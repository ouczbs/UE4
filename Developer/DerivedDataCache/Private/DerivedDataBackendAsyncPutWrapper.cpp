// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBackendAsyncPutWrapper.h"
#include "MemoryDerivedDataBackend.h"

/** 
 * Async task to handle the fire and forget async put
 */
class FCachePutAsyncWorker
{
public:
	/** Cache Key for the put to InnerBackend **/
	FString								CacheKey;
	/** Data for the put to InnerBackend **/
	TArray<uint8>						Data;
	/** Backend to use for storage, my responsibilities are about async puts **/
	FDerivedDataBackendInterface*		InnerBackend;
	/** Memory based cache to clear once the put is finished **/
	FDerivedDataBackendInterface*		InflightCache;
	/** We remember outstanding puts so that we don't do them redundantly **/
	FThreadSet*							FilesInFlight;
	/**If true, then do not attempt skip the put even if CachedDataProbablyExists returns true **/
	bool								bPutEvenIfExists;
	/** Usage stats to track thread times. */
	FDerivedDataCacheUsageStats&        UsageStats;

	/** Constructor
	*/
	FCachePutAsyncWorker(const TCHAR* InCacheKey, TArrayView<const uint8> InData, FDerivedDataBackendInterface* InInnerBackend, bool InbPutEvenIfExists, FDerivedDataBackendInterface* InInflightCache, FThreadSet* InInFilesInFlight, FDerivedDataCacheUsageStats& InUsageStats)
		: CacheKey(InCacheKey)
		, Data(InData.GetData(), InData.Num())
		, InnerBackend(InInnerBackend)
		, InflightCache(InInflightCache)
		, FilesInFlight(InInFilesInFlight)
		, bPutEvenIfExists(InbPutEvenIfExists)
		, UsageStats(InUsageStats)
	{
		check(InnerBackend);
	}
		
	/** Call the inner backend and when that completes, remove the memory cache */
	void DoWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DDCPut_DoWork);
		COOK_STAT(auto Timer = UsageStats.TimePut());

		using EPutStatus = FDerivedDataBackendInterface::EPutStatus;
		EPutStatus Status = EPutStatus::NotCached;

		if (!bPutEvenIfExists && InnerBackend->CachedDataProbablyExists(*CacheKey))
		{
			Status = EPutStatus::Cached;
		}
		else
		{
			Status = InnerBackend->PutCachedData(*CacheKey, Data, bPutEvenIfExists);
			COOK_STAT(Timer.AddHit(Data.Num()));
		}

		if (InflightCache)
		{
			// if the data was not cached synchronously, retry
			if (Status != EPutStatus::Cached)
			{
				// retry after a brief wait
				FPlatformProcess::SleepNoStats(0.2f);

				if (Status == EPutStatus::Executing && InnerBackend->CachedDataProbablyExists(*CacheKey))
				{
					Status = EPutStatus::Cached;
				}
				else
				{
					Status = InnerBackend->PutCachedData(*CacheKey, Data, /*bPutEvenIfExists*/ false);
				}
			}

			switch (Status)
			{
			case EPutStatus::Cached:
				// remove this from the in-flight cache because the inner cache contains the data
				InflightCache->RemoveCachedData(*CacheKey, /*bTransient*/ false);
				break;
			case EPutStatus::NotCached:
				UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Put failed, keeping in memory copy %s."), *InnerBackend->GetName(), *CacheKey);
				if (uint32 ErrorCode = FPlatformMisc::GetLastError())
				{
					TCHAR ErrorBuffer[1024];
					FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, ErrorCode);
					UE_LOG(LogDerivedDataCache, Display, TEXT("Failed to write %s to %s. Error: %u (%s)"), *CacheKey, *InnerBackend->GetName(), ErrorCode, ErrorBuffer);
				}
				break;
			case EPutStatus::Executing:
				UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Put not finished executing, keeping in memory copy %s."), *InnerBackend->GetName(), *CacheKey);
				break;
			default:
				break;
			}
		}

		FilesInFlight->Remove(CacheKey);
		FDerivedDataBackend::Get().AddToAsyncCompletionCounter(-1);
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Completed AsyncPut of %s."), *InnerBackend->GetName(), *CacheKey);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCachePutAsyncWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	/** Indicates to the thread pool that this task is abandonable */
	bool CanAbandon()
	{
		return true;
	}

	/** Abandon routine, we need to remove the item from the in flight cache because something might be waiting for that */
	void Abandon()
	{
		if (InflightCache)
		{
			InflightCache->RemoveCachedData(*CacheKey, /*bTransient=*/ false); // we can remove this from the temp cache, since the real cache will hit now
		}
		FilesInFlight->Remove(CacheKey);
		FDerivedDataBackend::Get().AddToAsyncCompletionCounter(-1);
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Abandoned AsyncPut of %s."), *InnerBackend->GetName(), *CacheKey);
	}
};

FDerivedDataBackendAsyncPutWrapper::FDerivedDataBackendAsyncPutWrapper(FDerivedDataBackendInterface* InInnerBackend, bool bCacheInFlightPuts)
	: InnerBackend(InInnerBackend)
	, InflightCache(bCacheInFlightPuts ? (new FMemoryDerivedDataBackend(TEXT("AsyncPutCache"))) : NULL)
{
	check(InnerBackend);
}

/** return true if this cache is writable **/
bool FDerivedDataBackendAsyncPutWrapper::IsWritable()
{
	return InnerBackend->IsWritable();
}

FDerivedDataBackendInterface::ESpeedClass FDerivedDataBackendAsyncPutWrapper::GetSpeedClass()
{
	return InnerBackend->GetSpeedClass();
}

bool FDerivedDataBackendAsyncPutWrapper::CachedDataProbablyExists(const TCHAR* CacheKey)
{
	COOK_STAT(auto Timer = UsageStats.TimeProbablyExists());
	bool Result = (InflightCache && InflightCache->CachedDataProbablyExists(CacheKey)) || InnerBackend->CachedDataProbablyExists(CacheKey);
	COOK_STAT(if (Result) {	Timer.AddHit(0); });

	UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s CachedDataProbablyExists=%d for %s"), *GetName(), Result, CacheKey);
	return Result;
}

TBitArray<> FDerivedDataBackendAsyncPutWrapper::CachedDataProbablyExistsBatch(TConstArrayView<FString> CacheKeys)
{
	COOK_STAT(auto Timer = UsageStats.TimeProbablyExists());

	TBitArray<> Result;
	if (InflightCache)
	{
		Result = InflightCache->CachedDataProbablyExistsBatch(CacheKeys);
		check(Result.Num() == CacheKeys.Num());
		if (Result.CountSetBits() < CacheKeys.Num())
		{
			TBitArray<> InnerResult = InnerBackend->CachedDataProbablyExistsBatch(CacheKeys);
			check(InnerResult.Num() == CacheKeys.Num());
			Result.CombineWithBitwiseOR(InnerResult, EBitwiseOperatorFlags::MaintainSize);
		}
	}
	else
	{
		Result = InnerBackend->CachedDataProbablyExistsBatch(CacheKeys);
		check(Result.Num() == CacheKeys.Num());
	}

	COOK_STAT(if (Result.CountSetBits() == CacheKeys.Num()) { Timer.AddHit(0); });
	UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s CachedDataProbablyExists found %d/%d keys"), *GetName(), Result.CountSetBits(), CacheKeys.Num());
	return Result;
}

bool FDerivedDataBackendAsyncPutWrapper::TryToPrefetch(const TCHAR* CacheKey)
{
	COOK_STAT(auto Timer = UsageStats.TimePrefetch());

	bool SkipCheck = (!InflightCache && InflightCache->CachedDataProbablyExists(CacheKey));
	
	bool Hit = false;

	if (!SkipCheck)
	{
		Hit = InnerBackend->TryToPrefetch(CacheKey);
	}

	COOK_STAT(if (Hit) { Timer.AddHit(0); });
	return Hit;
}

/*
	Determine if we would cache this by asking all our inner layers
*/
bool FDerivedDataBackendAsyncPutWrapper::WouldCache(const TCHAR* CacheKey, TArrayView<const uint8> InData)
{
	return InnerBackend->WouldCache(CacheKey, InData);
}

bool FDerivedDataBackendAsyncPutWrapper::ApplyDebugOptions(FBackendDebugOptions& InOptions)
{
	return InnerBackend->ApplyDebugOptions(InOptions);
}

bool FDerivedDataBackendAsyncPutWrapper::GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData)
{
	COOK_STAT(auto Timer = UsageStats.TimeGet());
	if (InflightCache && InflightCache->GetCachedData(CacheKey, OutData))
	{
		COOK_STAT(Timer.AddHit(OutData.Num()));
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s CacheHit from InFlightCache on %s"), *GetName(), CacheKey);
		return true;
	}

	bool bSuccess = InnerBackend->GetCachedData(CacheKey, OutData);
	if (bSuccess)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s Cache hit on %s"), *GetName(), CacheKey);
		COOK_STAT(Timer.AddHit(OutData.Num()));
	}
	else
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s Cache miss on %s"), *GetName(), CacheKey);
	}
	return bSuccess;
}

FDerivedDataBackendInterface::EPutStatus FDerivedDataBackendAsyncPutWrapper::PutCachedData(const TCHAR* CacheKey, TArrayView<const uint8> InData, bool bPutEvenIfExists)
{
	COOK_STAT(auto Timer = PutSyncUsageStats.TimePut());

	if (!InnerBackend->IsWritable())
	{
		return EPutStatus::NotCached; // no point in continuing down the chain
	}
	const bool bAdded = FilesInFlight.AddIfNotExists(CacheKey);
	if (!bAdded)
	{
		return EPutStatus::Executing; // if it is already on its way, we don't need to send it again
	}
	if (InflightCache)
	{
		if (InflightCache->CachedDataProbablyExists(CacheKey))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s skipping out of key already in in-flight cache %s"), *GetName(), CacheKey);
			return EPutStatus::Executing; // if it is already on its way, we don't need to send it again
		}
		InflightCache->PutCachedData(CacheKey, InData, true); // temp copy stored in memory while the async task waits to complete
		COOK_STAT(Timer.AddHit(InData.Num()));
	}

	UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s queueing %s for put"), *GetName(), CacheKey);

	FDerivedDataBackend::Get().AddToAsyncCompletionCounter(1);
	(new FAutoDeleteAsyncTask<FCachePutAsyncWorker>(CacheKey, InData, InnerBackend, bPutEvenIfExists, InflightCache.Get(), &FilesInFlight, UsageStats))->StartBackgroundTask(GDDCIOThreadPool, EQueuedWorkPriority::Low);

	return EPutStatus::Executing;
}

void FDerivedDataBackendAsyncPutWrapper::RemoveCachedData(const TCHAR* CacheKey, bool bTransient)
{
	if (!InnerBackend->IsWritable())
	{
		return; // no point in continuing down the chain
	}
	while (FilesInFlight.Exists(CacheKey))
	{
		FPlatformProcess::Sleep(0.0f); // this is an exception condition (corruption), spin and wait for it to clear
	}
	if (InflightCache)
	{
		InflightCache->RemoveCachedData(CacheKey, bTransient);
	}
	InnerBackend->RemoveCachedData(CacheKey, bTransient);

	UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s removed %s"), *GetName(), CacheKey)
}

void FDerivedDataBackendAsyncPutWrapper::GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath)
{
	COOK_STAT(
		{
		UsageStatsMap.Add(GraphPath + TEXT(": AsyncPut"), UsageStats);
		UsageStatsMap.Add(GraphPath + TEXT(": AsyncPutSync"), PutSyncUsageStats);
		if (InnerBackend)
		{
			InnerBackend->GatherUsageStats(UsageStatsMap, GraphPath + TEXT(". 0"));
		}
		if (InflightCache)
		{
			InflightCache->GatherUsageStats(UsageStatsMap, GraphPath + TEXT(". 1"));
		}
	});
}
