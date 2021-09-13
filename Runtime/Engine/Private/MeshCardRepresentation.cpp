// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshCardRepresentation.cpp
=============================================================================*/

#include "MeshCardRepresentation.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Misc/App.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshResources.h"
#include "ProfilingDebugging/CookStats.h"
#include "Templates/UniquePtr.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "Async/Async.h"
#include "ObjectCacheContext.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "DerivedDataCacheInterface.h"
#include "MeshUtilities.h"
#include "StaticMeshCompiler.h"
#endif

#if WITH_EDITORONLY_DATA
#include "IMeshBuilderModule.h"
#endif

#if ENABLE_COOK_STATS
namespace CardRepresentationCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("CardRepresentation.Usage"), TEXT(""));
	});
}
#endif

static TAutoConsoleVariable<int32> CVarCardRepresentation(
	TEXT("r.MeshCardRepresentation"),
	1,
	TEXT(""),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarCardRepresentationMinSurface(
	TEXT("r.MeshCardRepresentation.MinSurface"),
	0.2f,
	TEXT("Min percentage of surface treshold to spawn a new card, [0;1] range."),
	ECVF_ReadOnly);

FCardRepresentationAsyncQueue* GCardRepresentationAsyncQueue = NULL;

#if WITH_EDITOR

// DDC key for card representation data, must be changed when modifying the generation code or data format
#define CARDREPRESENTATION_DERIVEDDATA_VER TEXT("E8C7CF855B174FE195BE69EAF6000293")

FString BuildCardRepresentationDerivedDataKey(const FString& InMeshKey)
{
	const float MinSurfaceThreshold = CVarCardRepresentationMinSurface.GetValueOnAnyThread();

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("CARD"),
		*FString::Printf(TEXT("%s_%s_%.3f"), *InMeshKey, CARDREPRESENTATION_DERIVEDDATA_VER, MinSurfaceThreshold),
		TEXT(""));
}

#endif

#if WITH_EDITORONLY_DATA

void BeginCacheMeshCardRepresentation(const ITargetPlatform* TargetPlatform, UStaticMesh* StaticMeshAsset, FStaticMeshRenderData& RenderData, const FString& DistanceFieldKey, FSourceMeshDataForDerivedDataTask* OptionalSourceMeshData)
{
	static const auto CVarCards = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MeshCardRepresentation"));

	if (CVarCards->GetValueOnAnyThread() != 0)
	{
		FString Key = BuildCardRepresentationDerivedDataKey(DistanceFieldKey);
		if (RenderData.LODResources.IsValidIndex(0))
		{
			if (!RenderData.LODResources[0].CardRepresentationData)
			{
				RenderData.LODResources[0].CardRepresentationData = new FCardRepresentationData();
			}

			const FMeshBuildSettings& BuildSettings = StaticMeshAsset->GetSourceModel(0).BuildSettings;
			UStaticMesh* MeshToGenerateFrom = StaticMeshAsset;

			RenderData.LODResources[0].CardRepresentationData->CacheDerivedData(Key, TargetPlatform, StaticMeshAsset, MeshToGenerateFrom, BuildSettings.bGenerateDistanceFieldAsIfTwoSided, OptionalSourceMeshData);
		}
	}
}

void FCardRepresentationData::CacheDerivedData(const FString& InDDCKey, const ITargetPlatform* TargetPlatform, UStaticMesh* Mesh, UStaticMesh* GenerateSource, bool bGenerateDistanceFieldAsIfTwoSided, FSourceMeshDataForDerivedDataTask* OptionalSourceMeshData)
{
	TArray<uint8> DerivedData;

	COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeSyncWork());
	if (GetDerivedDataCacheRef().GetSynchronous(*InDDCKey, DerivedData, Mesh->GetPathName()))
	{
		COOK_STAT(Timer.AddHit(DerivedData.Num()));
		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);
		Ar << *this;
	}
	else
	{
		// We don't actually build the resource until later, so only track the cycles used here.
		COOK_STAT(Timer.TrackCyclesOnly());
		FAsyncCardRepresentationTask* NewTask = new FAsyncCardRepresentationTask;
		NewTask->DDCKey = InDDCKey;
		check(Mesh && GenerateSource);
		NewTask->StaticMesh = Mesh;
		NewTask->GenerateSource = GenerateSource;
		NewTask->GeneratedCardRepresentation = new FCardRepresentationData();
		NewTask->bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;

		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetStaticMaterials().Num(); MaterialIndex++)
		{
			FSignedDistanceFieldBuildMaterialData MaterialData;
			// Default material blend mode
			MaterialData.BlendMode = BLEND_Opaque;
			MaterialData.bTwoSided = false;

			if (Mesh->GetStaticMaterials()[MaterialIndex].MaterialInterface)
			{
				MaterialData.BlendMode = Mesh->GetStaticMaterials()[MaterialIndex].MaterialInterface->GetBlendMode();
				MaterialData.bTwoSided = Mesh->GetStaticMaterials()[MaterialIndex].MaterialInterface->IsTwoSided();
			}

			NewTask->MaterialBlendModes.Add(MaterialData);
		}

		// Nanite overrides source static mesh with a coarse representation. Need to load original data before we build the mesh SDF.
		if (OptionalSourceMeshData)
		{
			NewTask->SourceMeshData = *OptionalSourceMeshData;
		}
		else if (Mesh->NaniteSettings.bEnabled)
		{
			IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForPlatform(TargetPlatform);
			if (!MeshBuilderModule.BuildMeshVertexPositions(Mesh, NewTask->SourceMeshData.TriangleIndices, NewTask->SourceMeshData.VertexPositions))
			{
				UE_LOG(LogStaticMesh, Error, TEXT("Failed to build static mesh. See previous line(s) for details."));
			}
		}

		GCardRepresentationAsyncQueue->AddTask(NewTask);
	}
}

#endif

int32 GUseAsyncCardRepresentationBuildQueue = 1;
static FAutoConsoleVariableRef CVarCardRepresentationAsyncBuildQueue(
	TEXT("r.MeshCardRepresentation.Async"),
	GUseAsyncCardRepresentationBuildQueue,
	TEXT("."),
	ECVF_Default | ECVF_ReadOnly
	);

FCardRepresentationAsyncQueue::FCardRepresentationAsyncQueue() 
{
#if WITH_EDITOR
	MeshUtilities = NULL;

	const int32 MaxConcurrency = -1;
	// In Editor, we allow faster compilation by letting the asset compiler's scheduler organize work.
	ThreadPool = MakeUnique<FQueuedThreadPoolWrapper>(FAssetCompilingManager::Get().GetThreadPool(), MaxConcurrency, [](EQueuedWorkPriority) { return EQueuedWorkPriority::Lowest; });
#else
	const int32 MaxConcurrency = 1;
	ThreadPool = MakeUnique<FQueuedThreadPoolWrapper>(GThreadPool, MaxConcurrency, [](EQueuedWorkPriority) { return EQueuedWorkPriority::Lowest; });
#endif
}

FCardRepresentationAsyncQueue::~FCardRepresentationAsyncQueue()
{
}

void FAsyncCardRepresentationTaskWorker::DoWork()
{
	// Put on background thread to avoid interfering with game-thread bound tasks
	FQueuedThreadPoolTaskGraphWrapper TaskGraphWrapper(ENamedThreads::AnyBackgroundThreadNormalTask);
	GCardRepresentationAsyncQueue->Build(&Task, TaskGraphWrapper);
}

void FCardRepresentationAsyncQueue::CancelBackgroundTask(TArray<FAsyncCardRepresentationTask*> Tasks)
{
	// Do all the cancellation first to make sure none of these tasks
	// get scheduled as we're waiting for completion.
	for (FAsyncCardRepresentationTask* Task : Tasks)
	{
		if (Task->AsyncTask)
		{
			Task->AsyncTask->Cancel();
		}
	}

	for (FAsyncCardRepresentationTask* Task : Tasks)
	{
		if (Task->AsyncTask)
		{
			Task->AsyncTask->EnsureCompletion();
			Task->AsyncTask.Reset();
		}
	}
}

void FCardRepresentationAsyncQueue::StartBackgroundTask(FAsyncCardRepresentationTask* Task)
{
	check(Task->AsyncTask == nullptr);
	Task->AsyncTask = MakeUnique<FAsyncTask<FAsyncCardRepresentationTaskWorker>>(*Task);
	Task->AsyncTask->StartBackgroundTask(ThreadPool.Get(), EQueuedWorkPriority::Lowest);
}

void FCardRepresentationAsyncQueue::ProcessPendingTasks()
{
	FScopeLock Lock(&CriticalSection);
	TArray<FAsyncCardRepresentationTask*> Tasks = MoveTemp(PendingTasks);
	for (FAsyncCardRepresentationTask* Task : Tasks)
	{
		if (Task->GenerateSource && Task->GenerateSource->IsCompiling())
		{
			PendingTasks.Add(Task);
		}
		else
		{
			StartBackgroundTask(Task);
		}
	}
}

void FCardRepresentationAsyncQueue::AddTask(FAsyncCardRepresentationTask* Task)
{
#if WITH_EDITOR
	if (!MeshUtilities)
	{
		MeshUtilities = &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	}
	
	{
		// Array protection when called from multiple threads
		FScopeLock Lock(&CriticalSection);
		ReferencedTasks.Add(Task);
	}

	// The Source Mesh's RenderData is not yet ready, postpone the build
	if (Task->GenerateSource->IsCompiling())
	{
		// Array protection when called from multiple threads
		FScopeLock Lock(&CriticalSection);
		PendingTasks.Add(Task);
	}
	else
	{
		// If we're already in worker threads there is no need to launch an async task.
		if (GUseAsyncCardRepresentationBuildQueue || !IsInGameThread())
		{
			StartBackgroundTask(Task);
		}
		else
		{
			// To avoid deadlocks, we must queue the inner build tasks on another thread pool, so use the task graph.
			// Put on background thread to avoid interfering with game-thread bound tasks
			FQueuedThreadPoolTaskGraphWrapper TaskGraphWrapper(ENamedThreads::AnyBackgroundThreadNormalTask);
			Build(Task, TaskGraphWrapper);
		}
	}
#else
	UE_LOG(LogStaticMesh,Fatal,TEXT("Tried to build a card representation without editor support (this should have been done during cooking)"));
#endif
}

void FCardRepresentationAsyncQueue::CancelBuild(UStaticMesh* StaticMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelBuild);

	TArray<FAsyncCardRepresentationTask*> TasksToCancel;
	{
		FScopeLock Lock(&CriticalSection);
		TArray<FAsyncCardRepresentationTask*> Tasks = MoveTemp(PendingTasks);
		PendingTasks.Reserve(Tasks.Num());
		for (FAsyncCardRepresentationTask* Task : Tasks)
		{
			if (Task->GenerateSource != StaticMesh && Task->StaticMesh != StaticMesh)
			{
				PendingTasks.Add(Task);
			}
		}

		Tasks = MoveTemp(ReferencedTasks);
		ReferencedTasks.Reserve(Tasks.Num());
		for (FAsyncCardRepresentationTask* Task : Tasks)
		{
			if (Task->GenerateSource != StaticMesh && Task->StaticMesh != StaticMesh)
			{
				ReferencedTasks.Add(Task);
			}
			else
			{
				TasksToCancel.Add(Task);
			}
		}
	}

	CancelBackgroundTask(TasksToCancel);
	for (FAsyncCardRepresentationTask* Task : TasksToCancel)
	{
		if (Task->GeneratedCardRepresentation != nullptr)
		{
			// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
			BeginCleanup(Task->GeneratedCardRepresentation);
		}

		delete Task;
	}
}

void FCardRepresentationAsyncQueue::CancelAllOutstandingBuilds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelAllOutstandingBuilds);

	TArray<FAsyncCardRepresentationTask*> OutstandingTasks;
	{
		FScopeLock Lock(&CriticalSection);
		PendingTasks.Empty();
		OutstandingTasks = MoveTemp(ReferencedTasks);
	}

	CancelBackgroundTask(OutstandingTasks);
	for (FAsyncCardRepresentationTask* Task : OutstandingTasks)
	{
		delete Task;
	}
}

void FCardRepresentationAsyncQueue::RescheduleBackgroundTask(FAsyncCardRepresentationTask* InTask, EQueuedWorkPriority InPriority)
{
	if (InTask->AsyncTask)
	{
		if (InTask->AsyncTask->GetPriority() != InPriority)
		{
			InTask->AsyncTask->Reschedule(GThreadPool, InPriority);
		}
	}
}

void FCardRepresentationAsyncQueue::BlockUntilBuildComplete(UStaticMesh* StaticMesh, bool bWarnIfBlocked)
{
	// We will track the wait time here, but only the cycles used.
	// This function is called whether or not an async task is pending, 
	// so we have to look elsewhere to properly count how many resources have actually finished building.
	COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeAsyncWait());
	COOK_STAT(Timer.TrackCyclesOnly());
	bool bReferenced = false;
	bool bHadToBlock = false;
	double StartTime = 0;

#if WITH_EDITOR
	FStaticMeshCompilingManager::Get().FinishCompilation({ StaticMesh });
#endif

	do 
	{
		ProcessAsyncTasks();

		bReferenced = false;

		{
			FScopeLock Lock(&CriticalSection);
			for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
			{
				if (ReferencedTasks[TaskIndex]->StaticMesh == StaticMesh ||
					ReferencedTasks[TaskIndex]->GenerateSource == StaticMesh)
				{
					bReferenced = true;
					RescheduleBackgroundTask(ReferencedTasks[TaskIndex], EQueuedWorkPriority::Highest);
				}
			}
		}

		if (bReferenced)
		{
			if (!bHadToBlock)
			{
				StartTime = FPlatformTime::Seconds();
			}

			bHadToBlock = true;
			FPlatformProcess::Sleep(.01f);
		}
	} 
	while (bReferenced);

	if (bHadToBlock &&
		bWarnIfBlocked
#if WITH_EDITOR
		&& !FAutomationTestFramework::Get().GetCurrentTest() // HACK - Don't output this warning during automation test
#endif
		)
	{
		UE_LOG(LogStaticMesh, Display, TEXT("Main thread blocked for %.3fs for async card representation build of %s to complete!  This can happen if the mesh is rebuilt excessively."),
			(float)(FPlatformTime::Seconds() - StartTime), 
			*StaticMesh->GetName());
	}
}

void FCardRepresentationAsyncQueue::BlockUntilAllBuildsComplete()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::BlockUntilAllBuildsComplete)
	do 
	{
#if WITH_EDITOR
		FStaticMeshCompilingManager::Get().FinishAllCompilation();
#endif
		// Reschedule as highest prio since we're explicitly waiting on them
		{
			FScopeLock Lock(&CriticalSection);
			for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
			{
				RescheduleBackgroundTask(ReferencedTasks[TaskIndex], EQueuedWorkPriority::Highest);
			}
		}

		ProcessAsyncTasks();
		FPlatformProcess::Sleep(.01f);
	} 
	while (GetNumOutstandingTasks() > 0);
}

void FCardRepresentationAsyncQueue::Build(FAsyncCardRepresentationTask* Task, FQueuedThreadPool& BuildThreadPool)
{
#if WITH_EDITOR
	
	// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg UProperty or serialized)
	if (Task->StaticMesh && Task->GenerateSource)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::Build);

		const FStaticMeshLODResources& LODModel = Task->GenerateSource->GetRenderData()->LODResources[0];

		Task->bSuccess = MeshUtilities->GenerateCardRepresentationData(
			Task->StaticMesh->GetName(),
			Task->SourceMeshData,
			LODModel,
			BuildThreadPool,
			Task->MaterialBlendModes,
			Task->GenerateSource->GetRenderData()->Bounds,
			Task->GenerateSource->GetRenderData()->LODResources[0].DistanceFieldData,
			Task->bGenerateDistanceFieldAsIfTwoSided,
			*Task->GeneratedCardRepresentation);
	}

    CompletedTasks.Push(Task);

#endif
}

void FCardRepresentationAsyncQueue::AddReferencedObjects(FReferenceCollector& Collector)
{	
	FScopeLock Lock(&CriticalSection);
	for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
	{
		// Make sure none of the UObjects referenced by the async tasks are GC'ed during the task
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->StaticMesh);
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->GenerateSource);
	}
}

FString FCardRepresentationAsyncQueue::GetReferencerName() const
{
	return TEXT("FCardRepresentationAsyncQueue");
}

void FCardRepresentationAsyncQueue::ProcessAsyncTasks(bool bLimitExecutionTime)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::ProcessAsyncTasks);

	ProcessPendingTasks();

	FObjectCacheContextScope ObjectCacheScope;
	const double MaxProcessingTime = 0.016f;
	double StartTime = FPlatformTime::Seconds();
	while (!bLimitExecutionTime || (FPlatformTime::Seconds() - StartTime) < MaxProcessingTime)
	{
		FAsyncCardRepresentationTask* Task = CompletedTasks.Pop();
		if (Task == nullptr)
		{
			break;
		}

		// We want to count each resource built from a DDC miss, so count each iteration of the loop separately.
		COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeSyncWork());

		bool bWasCancelled = false;
		{
			FScopeLock Lock(&CriticalSection);
			bWasCancelled = ReferencedTasks.Remove(Task) == 0;
		}

		if (bWasCancelled)
		{
			continue;
		}

		if (Task->AsyncTask)
		{
			Task->AsyncTask->EnsureCompletion();
			Task->AsyncTask.Reset();
		}

		// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg UProperty or serialized)
		if (Task->StaticMesh && Task->bSuccess)
		{
			FCardRepresentationData* OldCardData = Task->StaticMesh->GetRenderData()->LODResources[0].CardRepresentationData;

			// Assign the new data, this is safe because the render threads makes a copy of the pointer at scene proxy creation time.
			Task->StaticMesh->GetRenderData()->LODResources[0].CardRepresentationData = Task->GeneratedCardRepresentation;

			// Any already created render state needs to be dirtied
			if (Task->StaticMesh->GetRenderData()->IsInitialized())
			{
				for (UStaticMeshComponent* Component : ObjectCacheScope.GetContext().GetStaticMeshComponents(Task->StaticMesh))
				{
					if (Component->IsRegistered() && Component->IsRenderStateCreated())
					{
						Component->MarkRenderStateDirty();
					}
				}
			}

			// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
			BeginCleanup(OldCardData);

			{
				TArray<uint8> DerivedData;
				// Save built data to DDC
				FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
				Ar << *(Task->StaticMesh->GetRenderData()->LODResources[0].CardRepresentationData);
				GetDerivedDataCacheRef().Put(*Task->DDCKey, DerivedData, Task->StaticMesh->GetPathName());
				COOK_STAT(Timer.AddMiss(DerivedData.Num()));
			}
		}

		delete Task;
	}
#endif
}

void FCardRepresentationAsyncQueue::Shutdown()
{
	CancelAllOutstandingBuilds();

	UE_LOG(LogStaticMesh, Log, TEXT("Abandoning remaining async card representation tasks for shutdown"));
	ThreadPool->Destroy();
}