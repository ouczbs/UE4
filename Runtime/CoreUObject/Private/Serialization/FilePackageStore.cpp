// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/PackageStore.h"
#include "Serialization/MappedName.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryReader.h"
#include "IO/IoDispatcher.h"
#include "IO/IoContainerId.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Async/Async.h"
#include "Misc/CommandLine.h"

/*
 * File/container based package store.
 */
class FFilePackageStore final
	: public IPackageStore
{
public:
	FFilePackageStore(FIoDispatcher& InIoDispatcher)
		: IoDispatcher(InIoDispatcher) { }

	virtual ~FFilePackageStore() { }

	virtual void Initialize() override
	{
		// Setup culture
		{
			FInternationalization& Internationalization = FInternationalization::Get();
			FString CurrentCulture = Internationalization.GetCurrentCulture()->GetName();
			FParse::Value(FCommandLine::Get(), TEXT("CULTURE="), CurrentCulture);
			CurrentCultureNames = Internationalization.GetPrioritizedCultureNames(CurrentCulture);
		}

		LoadContainers(IoDispatcher.GetMountedContainers().Array());
		IoDispatcher.OnContainerMounted().AddRaw(this, &FFilePackageStore::OnContainerMounted);
	}

	virtual bool DoesPackageExist(FPackageId PackageId) override
	{
		return nullptr != GetPackageEntry(PackageId);
	}

	virtual const FPackageStoreEntry* GetPackageEntry(FPackageId PackageId) override
	{
		FScopeLock Lock(&PackageNameMapsCritical);
		FPackageStoreEntry* Entry = StoreEntriesMap.FindRef(PackageId);
		return Entry;
	}

	virtual FPackageId GetRedirectedPackageId(FPackageId PackageId) override
	{
		FScopeLock Lock(&PackageNameMapsCritical);
		FPackageId RedirectedId = RedirectsPackageMap.FindRef(PackageId);
		return RedirectedId;
	}

	virtual bool IsRedirect(FPackageId PackageId) override
	{
		return TargetRedirectIds.Contains(PackageId);
	}

private:
	void LoadContainers(TArrayView<const FIoContainerId> Containers)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LoadContainers);

		int32 ContainersToLoad = 0;

		for (const FIoContainerId& ContainerId : Containers)
		{
			if (ContainerId.IsValid())
			{
				++ContainersToLoad;
			}
		}

		if (!ContainersToLoad)
		{
			return;
		}

		TAtomic<int32> Remaining(ContainersToLoad);

		FEvent* Event = FPlatformProcess::GetSynchEventFromPool();
		FIoBatch IoBatch = IoDispatcher.NewBatch();

		for (const FIoContainerId& ContainerId : Containers)
		{
			if (!ContainerId.IsValid())
			{
				continue;
			}

			TUniquePtr<FLoadedContainer>& LoadedContainerPtr = LoadedContainers.FindOrAdd(ContainerId);
			if (!LoadedContainerPtr)
			{
				LoadedContainerPtr.Reset(new FLoadedContainer);
			}
			FLoadedContainer& LoadedContainer = *LoadedContainerPtr;

			UE_LOG(LogStreaming, Log, TEXT("Loading mounted container ID '0x%llX'"), ContainerId.Value());
			LoadedContainer.bValid = true;

			FIoChunkId HeaderChunkId = CreateIoChunkId(ContainerId.Value(), 0, EIoChunkType::ContainerHeader);
			IoBatch.ReadWithCallback(HeaderChunkId, FIoReadOptions(), IoDispatcherPriority_High, [this, &Remaining, Event, &LoadedContainer, ContainerId](TIoStatusOr<FIoBuffer> Result)
			{
				// Execution method Thread will run the async block synchronously when multithreading is NOT supported
				const EAsyncExecution ExecutionMethod = FPlatformProcess::SupportsMultithreading() ? EAsyncExecution::TaskGraph : EAsyncExecution::Thread;

				if (!Result.IsOk())
				{
					if (EIoErrorCode::NotFound == Result.Status().GetErrorCode())
					{
						UE_LOG(LogStreaming, Warning, TEXT("Header for container '0x%llX' not found."), ContainerId.Value());
					}
					else
					{
						UE_LOG(LogStreaming, Fatal, TEXT("Failed reading header for container '0x%llX' (%s)"), ContainerId.Value(), *Result.Status().ToString());
					}

					if (--Remaining == 0)
					{
						Event->Trigger();
					}
					return;
				}

				Async(ExecutionMethod, [this, &Remaining, Event, IoBuffer = Result.ConsumeValueOrDie(), &LoadedContainer]()
				{
					LLM_SCOPE(ELLMTag::AsyncLoading);

					FMemoryReaderView Ar(MakeArrayView(IoBuffer.Data(), IoBuffer.DataSize()));

					FContainerHeader ContainerHeader;
					Ar << ContainerHeader;

					const bool bHasContainerLocalNameMap = ContainerHeader.Names.Num() > 0;
					if (bHasContainerLocalNameMap)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(LoadContainerNameMap);
						LoadedContainer.ContainerNameMap.Reset(new FNameMap());
						LoadedContainer.ContainerNameMap->Load(ContainerHeader.Names, ContainerHeader.NameHashes, FMappedName::EType::Container);
					}

					LoadedContainer.PackageCount = ContainerHeader.PackageCount;
					LoadedContainer.StoreEntries = MoveTemp(ContainerHeader.StoreEntries);
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(AddPackages);
						FScopeLock Lock(&PackageNameMapsCritical);

						TArrayView<FPackageStoreEntry> StoreEntries(reinterpret_cast<FPackageStoreEntry*>(LoadedContainer.StoreEntries.GetData()), LoadedContainer.PackageCount);

						int32 Index = 0;
						StoreEntriesMap.Reserve(StoreEntriesMap.Num() + LoadedContainer.PackageCount);
						for (FPackageStoreEntry& ContainerEntry : StoreEntries)
						{
							const FPackageId& PackageId = ContainerHeader.PackageIds[Index];

							FPackageStoreEntry*& GlobalEntry = StoreEntriesMap.FindOrAdd(PackageId);
							if (!GlobalEntry)
							{
								GlobalEntry = &ContainerEntry;
							}
							++Index;
						}

						{
							TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackageStoreLocalization);
							const FSourceToLocalizedPackageIdMap* LocalizedPackages = nullptr;
							for (const FString& CultureName : CurrentCultureNames)
							{
								LocalizedPackages = ContainerHeader.CulturePackageMap.Find(CultureName);
								if (LocalizedPackages)
								{
									break;
								}
							}

							if (LocalizedPackages)
							{
								for (auto& Pair : *LocalizedPackages)
								{
									const FPackageId& SourceId = Pair.Key;
									const FPackageId& LocalizedId = Pair.Value;
									RedirectsPackageMap.Emplace(SourceId, LocalizedId);
									TargetRedirectIds.Add(LocalizedId);
								}
							}
						}

						{
							TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackageStoreRedirects);
							for (const TPair<FPackageId, FPackageId>& Redirect : ContainerHeader.PackageRedirects)
							{
								const FPackageId& SourceId = Redirect.Key;
								const FPackageId& RedirectedId = Redirect.Value;
								RedirectsPackageMap.Emplace(SourceId, RedirectedId);
								TargetRedirectIds.Add(RedirectedId);
							}
						}
					}

					if (--Remaining == 0)
					{
						Event->Trigger();
					}
				});
			});
		}

		IoBatch.Issue();
		Event->Wait();
		FPlatformProcess::ReturnSynchEventToPool(Event);

		ApplyRedirects(RedirectsPackageMap);
	}

	void OnContainerMounted(const FIoContainerId& ContainerId)
	{
		LLM_SCOPE(ELLMTag::AsyncLoading);
		LoadContainers(MakeArrayView(&ContainerId, 1));
	}

	void ApplyRedirects(const TMap<FPackageId, FPackageId>& Redirects)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ApplyRedirects);

		FScopeLock Lock(&PackageNameMapsCritical);

		if (Redirects.Num() == 0)
		{
			return;
		}

		for (auto It = Redirects.CreateConstIterator(); It; ++It)
		{
			const FPackageId& SourceId = It.Key();
			const FPackageId& RedirectId = It.Value();
			check(RedirectId.IsValid());
			FPackageStoreEntry* RedirectEntry = StoreEntriesMap.FindRef(RedirectId);
			check(RedirectEntry);
			FPackageStoreEntry*& PackageEntry = StoreEntriesMap.FindOrAdd(SourceId);
			if (RedirectEntry)
			{
				PackageEntry = RedirectEntry;
			}
		}

		for (auto It = StoreEntriesMap.CreateIterator(); It; ++It)
		{
			FPackageStoreEntry* StoreEntry = It.Value();

			for (FPackageId& ImportedPackageId : StoreEntry->ImportedPackages)
			{
				if (const FPackageId* RedirectId = Redirects.Find(ImportedPackageId))
				{
					ImportedPackageId = *RedirectId;
				}
			}
		}
	}

	struct FLoadedContainer
	{
		TUniquePtr<FNameMap> ContainerNameMap;
		TArray<uint8> StoreEntries; //FPackageStoreEntry[PackageCount];
		uint32 PackageCount = 0;
		bool bValid = false;
	};

	FIoDispatcher& IoDispatcher;
	TMap<FIoContainerId, TUniquePtr<FLoadedContainer>> LoadedContainers;

	TArray<FString> CurrentCultureNames;

	FCriticalSection PackageNameMapsCritical;

	TMap<FPackageId, FPackageStoreEntry*> StoreEntriesMap;
	TMap<FPackageId, FPackageId> RedirectsPackageMap;
	TSet<FPackageId> TargetRedirectIds;
	int32 NextCustomPackageIndex = 0;
};

TUniquePtr<IPackageStore> MakeFilePackageStore(FIoDispatcher& IoDispatcher)
{
	return MakeUnique<FFilePackageStore>(IoDispatcher);
}
