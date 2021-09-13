// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldAtlas.cpp
=============================================================================*/

#include "DistanceFieldAtlas.h"
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
#include "DistanceFieldDownsampling.h"
#include "GlobalShader.h"
#include "RenderGraph.h"
#include "MeshCardRepresentation.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "Async/Async.h"
#include "ObjectCacheContext.h"

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "AssetCompilingManager.h"
#include "MeshUtilities.h"
#include "StaticMeshCompiler.h"
#endif

#if WITH_EDITORONLY_DATA
#include "IMeshBuilderModule.h"
#endif

CSV_DEFINE_CATEGORY(DistanceField, false);

#if ENABLE_COOK_STATS
namespace DistanceFieldCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("DistanceField.Usage"), TEXT(""));
	});
}
#endif

static TAutoConsoleVariable<int32> CVarDistField(
	TEXT("r.GenerateMeshDistanceFields"),
	0,	
	TEXT("Whether to build distance fields of static meshes, needed for distance field AO, which is used to implement Movable SkyLight shadows.\n")
	TEXT("Enabling will increase mesh build times and memory usage.  Changing this value will cause a rebuild of all static meshes."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDistFieldRes(
	TEXT("r.DistanceFields.MaxPerMeshResolution"),
	512,	
	TEXT("Highest resolution (in one dimension) allowed for a single static mesh asset, used to cap the memory usage of meshes with a large scale.\n")
	TEXT("Changing this will cause all distance fields to be rebuilt.  Large values such as 512 can consume memory very quickly! (64Mb for one asset at 512)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarDistFieldResScale(
	TEXT("r.DistanceFields.DefaultVoxelDensity"),
	.2f,	
	TEXT("Determines how the default scale of a mesh converts into distance field voxel dimensions.\n")
	TEXT("Changing this will cause all distance fields to be rebuilt.  Large values can consume memory very quickly!"),
	ECVF_ReadOnly);

static int32 GHeightFieldAtlasTileSize = 64;
static FAutoConsoleVariableRef CVarHeightFieldAtlasTileSize(
	TEXT("r.HeightFields.AtlasTileSize"),
	GHeightFieldAtlasTileSize,
	TEXT("Suballocation granularity"),
	ECVF_RenderThreadSafe);

static int32 GHeightFieldAtlasDimInTiles = 16;
static FAutoConsoleVariableRef CVarHeightFieldAtlasDimInTiles(
	TEXT("r.HeightFields.AtlasDimInTiles"),
	GHeightFieldAtlasDimInTiles,
	TEXT("Number of tiles the atlas has in one dimension"),
	ECVF_RenderThreadSafe);

static int32 GHeightFieldAtlasDownSampleLevel = 2;
static FAutoConsoleVariableRef CVarHeightFieldAtlasDownSampleLevel(
	TEXT("r.HeightFields.AtlasDownSampleLevel"),
	GHeightFieldAtlasDownSampleLevel,
	TEXT("Max number of times a suballocation can be down-sampled"),
	ECVF_RenderThreadSafe);

static int32 GHFVisibilityAtlasTileSize = 64;
static FAutoConsoleVariableRef CVarHFVisibilityAtlasTileSize(
	TEXT("r.HeightFields.VisibilityAtlasTileSize"),
	GHFVisibilityAtlasTileSize,
	TEXT("Suballocation granularity"),
	ECVF_RenderThreadSafe);

static int32 GHFVisibilityAtlasDimInTiles = 8;
static FAutoConsoleVariableRef CVarHFVisibilityAtlasDimInTiles(
	TEXT("r.HeightFields.VisibilityAtlasDimInTiles"),
	GHFVisibilityAtlasDimInTiles,
	TEXT("Number of tiles the atlas has in one dimension"),
	ECVF_RenderThreadSafe);

static int32 GHFVisibilityAtlasDownSampleLevel = 2;
static FAutoConsoleVariableRef CVarHFVisibilityAtlasDownSampleLevel(
	TEXT("r.HeightFields.VisibilityAtlasDownSampleLevel"),
	GHFVisibilityAtlasDownSampleLevel,
	TEXT("Max number of times a suballocation can be down-sampled"),
	ECVF_RenderThreadSafe);

TGlobalResource<FLandscapeTextureAtlas> GHeightFieldTextureAtlas(FLandscapeTextureAtlas::SAT_Height);
TGlobalResource<FLandscapeTextureAtlas> GHFVisibilityTextureAtlas(FLandscapeTextureAtlas::SAT_Visibility);

FDistanceFieldAsyncQueue* GDistanceFieldAsyncQueue = NULL;

#if WITH_EDITOR

// DDC key for distance field data, must be changed when modifying the generation code or data format
#define DISTANCEFIELD_DERIVEDDATA_VER TEXT("CD4A6506-C64C-A229-AA56-2B0A414AE96B")

FString BuildDistanceFieldDerivedDataKey(const FString& InMeshKey)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.MaxPerMeshResolution"));
	const int32 PerMeshMax = CVar->GetValueOnAnyThread();
	const FString PerMeshMaxString = PerMeshMax == 128 ? TEXT("") : FString::Printf(TEXT("_%u"), PerMeshMax);

	static const auto CVarDensity = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.DistanceFields.DefaultVoxelDensity"));
	const float VoxelDensity = CVarDensity->GetValueOnAnyThread();
	const FString VoxelDensityString = VoxelDensity == .1f ? TEXT("") : FString::Printf(TEXT("_%.3f"), VoxelDensity);

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("DIST"),
		*FString::Printf(TEXT("%s_%s%s%s"), *InMeshKey, DISTANCEFIELD_DERIVEDDATA_VER, *PerMeshMaxString, *VoxelDensityString),
		TEXT(""));
}

#endif

#if WITH_EDITORONLY_DATA

void FDistanceFieldVolumeData::CacheDerivedData(const FString& InStaticMeshDerivedDataKey, const ITargetPlatform* TargetPlatform, UStaticMesh* Mesh, FStaticMeshRenderData& RenderData, UStaticMesh* GenerateSource, float DistanceFieldResolutionScale, bool bGenerateDistanceFieldAsIfTwoSided)
{
	FString DistanceFieldKey = BuildDistanceFieldDerivedDataKey(InStaticMeshDerivedDataKey);

	for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetStaticMaterials().Num(); MaterialIndex++)
	{
		FSignedDistanceFieldBuildMaterialData MaterialData;
		// Default material blend mode
		MaterialData.BlendMode = BLEND_Opaque;
		MaterialData.bTwoSided = false;

		UMaterialInterface* MaterialInterface = Mesh->GetStaticMaterials()[MaterialIndex].MaterialInterface;
		if (MaterialInterface)
		{
			MaterialData.BlendMode = MaterialInterface->GetBlendMode();
			MaterialData.bTwoSided = MaterialInterface->IsTwoSided();
		}

		DistanceFieldKey += FString::Printf(TEXT("_M%u_%u"), (uint32)MaterialData.BlendMode, MaterialData.bTwoSided ? 1 : 0);
	}

	TArray<uint8> DerivedData;

	COOK_STAT(auto Timer = DistanceFieldCookStats::UsageStats.TimeSyncWork());
	if (GetDerivedDataCacheRef().GetSynchronous(*DistanceFieldKey, DerivedData, Mesh->GetPathName()))
	{
		COOK_STAT(Timer.AddHit(DerivedData.Num()));
		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);
		Serialize(Ar, Mesh);

		BeginCacheMeshCardRepresentation(TargetPlatform, Mesh, RenderData, DistanceFieldKey, /*OptionalSourceMeshData*/ nullptr);
	}
	else
	{
		// We don't actually build the resource until later, so only track the cycles used here.
		COOK_STAT(Timer.TrackCyclesOnly());
		FAsyncDistanceFieldTask* NewTask = new FAsyncDistanceFieldTask;
		NewTask->DDCKey = DistanceFieldKey;
		check(Mesh && GenerateSource);
		NewTask->TargetPlatform = TargetPlatform;
		NewTask->StaticMesh = Mesh;
		NewTask->GenerateSource = GenerateSource;
		NewTask->DistanceFieldResolutionScale = DistanceFieldResolutionScale;
		NewTask->bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;
		NewTask->GeneratedVolumeData = new FDistanceFieldVolumeData();
		NewTask->GeneratedVolumeData->AssetName = Mesh->GetFName();
		NewTask->GeneratedVolumeData->bAsyncBuilding = true;

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
		if (Mesh->NaniteSettings.bEnabled)
		{
			IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForPlatform(TargetPlatform);
			if (!MeshBuilderModule.BuildMeshVertexPositions(Mesh, NewTask->SourceMeshData.TriangleIndices, NewTask->SourceMeshData.VertexPositions))
			{
				UE_LOG(LogStaticMesh, Error, TEXT("Failed to build static mesh. See previous line(s) for details."));
			}
		}

		GDistanceFieldAsyncQueue->AddTask(NewTask);
	}
}

#endif

uint64 NextDistanceFieldVolumeDataId = 1;

FDistanceFieldVolumeData::FDistanceFieldVolumeData() :
	LocalSpaceMeshBounds(ForceInit),
	bMostlyTwoSided(false),
	bAsyncBuilding(false)
{
	Id = NextDistanceFieldVolumeDataId;
	NextDistanceFieldVolumeDataId++;
}

void FDistanceFieldVolumeData::Serialize(FArchive& Ar, UObject* Owner)
{
	// Note: this is derived data, no need for versioning (bump the DDC guid)
	Ar << LocalSpaceMeshBounds << bMostlyTwoSided << Mips << AlwaysLoadedMip;
	StreamableMips.Serialize(Ar, Owner, 0);
}

int32 GUseAsyncDistanceFieldBuildQueue = 1;
static FAutoConsoleVariableRef CVarAOAsyncBuildQueue(
	TEXT("r.AOAsyncBuildQueue"),
	GUseAsyncDistanceFieldBuildQueue,
	TEXT("Whether to asynchronously build distance field volume data from meshes."),
	ECVF_Default | ECVF_ReadOnly
	);

FAsyncDistanceFieldTask::FAsyncDistanceFieldTask()
	: StaticMesh(nullptr)
	, GenerateSource(nullptr)
	, DistanceFieldResolutionScale(0.0f)
	, bGenerateDistanceFieldAsIfTwoSided(false)
	, GeneratedVolumeData(nullptr)
{
}

FDistanceFieldAsyncQueue::FDistanceFieldAsyncQueue() 
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

FDistanceFieldAsyncQueue::~FDistanceFieldAsyncQueue()
{
}

void FAsyncDistanceFieldTaskWorker::DoWork()
{
	// Put on background thread to avoid interfering with game-thread bound tasks
	FQueuedThreadPoolTaskGraphWrapper TaskGraphWrapper(ENamedThreads::AnyBackgroundThreadNormalTask);
	GDistanceFieldAsyncQueue->Build(&Task, TaskGraphWrapper);
}

void FDistanceFieldAsyncQueue::CancelBackgroundTask(TArray<FAsyncDistanceFieldTask*> Tasks)
{
	// Do all the cancellation first to make sure none of these tasks
	// get scheduled as we're waiting for completion.
	for (FAsyncDistanceFieldTask* Task : Tasks)
	{
		if (Task->AsyncTask)
		{
			Task->AsyncTask->Cancel();
		}
	}

	for (FAsyncDistanceFieldTask* Task : Tasks)
	{
		if (Task->AsyncTask)
		{
			Task->AsyncTask->EnsureCompletion();
			Task->AsyncTask.Reset();
		}
	}
}

void FDistanceFieldAsyncQueue::StartBackgroundTask(FAsyncDistanceFieldTask* Task)
{
	check(Task->AsyncTask == nullptr);
	Task->AsyncTask = MakeUnique<FAsyncTask<FAsyncDistanceFieldTaskWorker>>(*Task);
	Task->AsyncTask->StartBackgroundTask(ThreadPool.Get(), EQueuedWorkPriority::Lowest);
}

void FDistanceFieldAsyncQueue::ProcessPendingTasks()
{
	FScopeLock Lock(&CriticalSection);
	TArray<FAsyncDistanceFieldTask*> Tasks = MoveTemp(PendingTasks);
	for (FAsyncDistanceFieldTask* Task : Tasks)
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

void FDistanceFieldAsyncQueue::AddTask(FAsyncDistanceFieldTask* Task)
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
		// If we're already in worker threads, there is no need to launch an async task.
		if (GUseAsyncDistanceFieldBuildQueue || !IsInGameThread())
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
	UE_LOG(LogStaticMesh,Fatal,TEXT("Tried to build a distance field without editor support (this should have been done during cooking)"));
#endif
}

void FDistanceFieldAsyncQueue::CancelBuild(UStaticMesh* StaticMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistanceFieldAsyncQueue::CancelBuild)

	TArray<FAsyncDistanceFieldTask*> TasksToCancel;
	{
		FScopeLock Lock(&CriticalSection);
		TArray<FAsyncDistanceFieldTask*> Tasks = MoveTemp(PendingTasks);
		PendingTasks.Reserve(Tasks.Num());
		for (FAsyncDistanceFieldTask* Task : Tasks)
		{
			if (Task->GenerateSource != StaticMesh && Task->StaticMesh != StaticMesh)
			{
				PendingTasks.Add(Task);
			}
		}

		Tasks = MoveTemp(ReferencedTasks);
		ReferencedTasks.Reserve(Tasks.Num());
		for (FAsyncDistanceFieldTask* Task : Tasks)
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
	for (FAsyncDistanceFieldTask* Task : TasksToCancel)
	{
		if (Task->GeneratedVolumeData != nullptr)
		{
			// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
			BeginCleanup(Task->GeneratedVolumeData);
		}

		delete Task;
	}
}

void FDistanceFieldAsyncQueue::CancelAllOutstandingBuilds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistanceFieldAsyncQueue::CancelAllOutstandingBuilds)

	TArray<FAsyncDistanceFieldTask*> OutstandingTasks;
	{
		FScopeLock Lock(&CriticalSection);
		PendingTasks.Empty();
		OutstandingTasks = MoveTemp(ReferencedTasks);
	}

	CancelBackgroundTask(OutstandingTasks);
	for (FAsyncDistanceFieldTask* Task : OutstandingTasks)
	{
		delete Task;
	}
}

void FDistanceFieldAsyncQueue::RescheduleBackgroundTask(FAsyncDistanceFieldTask* InTask, EQueuedWorkPriority InPriority)
{
	if (InTask->AsyncTask)
	{
		if (InTask->AsyncTask->GetPriority() != InPriority)
		{
			InTask->AsyncTask->Reschedule(GThreadPool, InPriority);
		}
	}
}

void FDistanceFieldAsyncQueue::BlockUntilBuildComplete(UStaticMesh* StaticMesh, bool bWarnIfBlocked)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistanceFieldAsyncQueue::BlockUntilBuildComplete)

	// We will track the wait time here, but only the cycles used.
	// This function is called whether or not an async task is pending, 
	// so we have to look elsewhere to properly count how many resources have actually finished building.
	COOK_STAT(auto Timer = DistanceFieldCookStats::UsageStats.TimeAsyncWait());
	COOK_STAT(Timer.TrackCyclesOnly());
	bool bReferenced = false;
	bool bHadToBlock = false;
	double StartTime = 0;

	TSet<UStaticMesh*> RequiredFinishCompilation;
	do 
	{
		ProcessAsyncTasks();

		bReferenced = false;
		RequiredFinishCompilation.Reset();

		// Reschedule the tasks we're waiting on as highest prio
		{
			FScopeLock Lock(&CriticalSection);
			for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
			{
				if (ReferencedTasks[TaskIndex]->StaticMesh == StaticMesh ||
					ReferencedTasks[TaskIndex]->GenerateSource == StaticMesh)
				{
					bReferenced = true;

					// If the task we are waiting on depends on other static meshes
					// we need to force finish them too.
#if WITH_EDITOR
					
					if (ReferencedTasks[TaskIndex]->GenerateSource != nullptr &&
						ReferencedTasks[TaskIndex]->GenerateSource->IsCompiling())
					{
						RequiredFinishCompilation.Add(ReferencedTasks[TaskIndex]->GenerateSource);
					}

					if (ReferencedTasks[TaskIndex]->StaticMesh != nullptr &&
						ReferencedTasks[TaskIndex]->StaticMesh->IsCompiling())
					{
						RequiredFinishCompilation.Add(ReferencedTasks[TaskIndex]->StaticMesh);
					}
#endif

					RescheduleBackgroundTask(ReferencedTasks[TaskIndex], EQueuedWorkPriority::Highest);
				}
			}
		}

#if WITH_EDITOR
		// Call the finish compilation outside of the critical section since those compilations
		// might need to register new distance field tasks which also uses the critical section.
		if (RequiredFinishCompilation.Num())
		{
			FStaticMeshCompilingManager::Get().FinishCompilation(RequiredFinishCompilation.Array());
		}
#endif

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
		UE_LOG(LogStaticMesh, Display, TEXT("Main thread blocked for %.3fs for async distance field build of %s to complete!  This can happen if the mesh is rebuilt excessively."),
			(float)(FPlatformTime::Seconds() - StartTime), 
			*StaticMesh->GetName());
	}
}

void FDistanceFieldAsyncQueue::BlockUntilAllBuildsComplete()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistanceFieldAsyncQueue::BlockUntilAllBuildsComplete)
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

void FDistanceFieldAsyncQueue::Build(FAsyncDistanceFieldTask* Task, FQueuedThreadPool& BuildThreadPool)
{
#if WITH_EDITOR
	// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg FProperty or serialized)
	if (Task->StaticMesh && Task->GenerateSource)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FDistanceFieldAsyncQueue::Build);

		const FStaticMeshLODResources& LODModel = Task->GenerateSource->GetRenderData()->LODResources[0];
		MeshUtilities->GenerateSignedDistanceFieldVolumeData(
			Task->StaticMesh->GetName(),
			Task->SourceMeshData,
			LODModel,
			BuildThreadPool,
			Task->MaterialBlendModes,
			Task->GenerateSource->GetRenderData()->Bounds,
			Task->DistanceFieldResolutionScale,
			Task->bGenerateDistanceFieldAsIfTwoSided,
			*Task->GeneratedVolumeData);
	}

    CompletedTasks.Push(Task);
#endif
}

void FDistanceFieldAsyncQueue::AddReferencedObjects(FReferenceCollector& Collector)
{	
	FScopeLock Lock(&CriticalSection);
	for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
	{
		// Make sure none of the UObjects referenced by the async tasks are GC'ed during the task
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->StaticMesh);
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->GenerateSource);
	}
}

FString FDistanceFieldAsyncQueue::GetReferencerName() const
{
	return TEXT("FDistanceFieldAsyncQueue");
}

void FDistanceFieldAsyncQueue::ProcessAsyncTasks(bool bLimitExecutionTime)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistanceFieldAsyncQueue::ProcessAsyncTasks);

	ProcessPendingTasks();

	FObjectCacheContextScope ObjectCacheScope;
	const double MaxProcessingTime = 0.016f;
	double StartTime = FPlatformTime::Seconds();
	while (!bLimitExecutionTime || (FPlatformTime::Seconds() - StartTime) < MaxProcessingTime)
	{
		FAsyncDistanceFieldTask* Task = CompletedTasks.Pop();
		if (Task == nullptr)
		{
			break;
		}

		// We want to count each resource built from a DDC miss, so count each iteration of the loop separately.
		COOK_STAT(auto Timer = DistanceFieldCookStats::UsageStats.TimeSyncWork());

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

		// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg FProperty or serialized)
		if (Task->StaticMesh)
		{
			Task->GeneratedVolumeData->bAsyncBuilding = false;
			FDistanceFieldVolumeData* OldVolumeData = Task->StaticMesh->GetRenderData()->LODResources[0].DistanceFieldData;

			// Assign the new volume data, this is safe because the render thread makes a copy of the pointer at scene proxy creation time.
			Task->StaticMesh->GetRenderData()->LODResources[0].DistanceFieldData = Task->GeneratedVolumeData;

			// Renderstates are not initialized between UStaticMesh::PreEditChange() and UStaticMesh::PostEditChange()
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

			if (OldVolumeData)
			{
				// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
				BeginCleanup(OldVolumeData);
			}

			{
				TArray<uint8> DerivedData;
				// Save built distance field volume to DDC
				FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
				Task->StaticMesh->GetRenderData()->LODResources[0].DistanceFieldData->Serialize(Ar, Task->StaticMesh);
				GetDerivedDataCacheRef().Put(*Task->DDCKey, DerivedData, Task->StaticMesh->GetPathName());
				COOK_STAT(Timer.AddMiss(DerivedData.Num()));
			}

			BeginCacheMeshCardRepresentation(Task->TargetPlatform, Task->StaticMesh, *Task->StaticMesh->GetRenderData(), Task->DDCKey, &Task->SourceMeshData);
		}

		delete Task;
	}
#endif
}

void FDistanceFieldAsyncQueue::Shutdown()
{
	CancelAllOutstandingBuilds();

	UE_LOG(LogStaticMesh, Log, TEXT("Abandoning remaining async distance field tasks for shutdown"));
	ThreadPool->Destroy();
}

FLandscapeTextureAtlas::FLandscapeTextureAtlas(ESubAllocType InSubAllocType)
	: MaxDownSampleLevel(0)
	, Generation(0)
	, SubAllocType(InSubAllocType)
{}

void FLandscapeTextureAtlas::InitializeIfNeeded()
{
	const bool bHeight = SubAllocType == SAT_Height;
	const uint32 LocalTileSize = bHeight ? GHeightFieldAtlasTileSize : GHFVisibilityAtlasTileSize;
	const uint32 LocalDimInTiles = bHeight ? GHeightFieldAtlasDimInTiles : GHFVisibilityAtlasDimInTiles;
	const uint32 LocalDownSampleLevel = bHeight ? GHeightFieldAtlasDownSampleLevel : GHFVisibilityAtlasDownSampleLevel;

	if (!AtlasTextureRHI
		|| AddrSpaceAllocator.TileSize != LocalTileSize
		|| AddrSpaceAllocator.DimInTiles != LocalDimInTiles
		|| MaxDownSampleLevel != LocalDownSampleLevel)
	{
		AddrSpaceAllocator.Init(LocalTileSize, 1, LocalDimInTiles);

		for (int32 Idx = 0; Idx < PendingStreamingTextures.Num(); ++Idx)
		{
			UTexture2D* Texture = PendingStreamingTextures[Idx];
			Texture->bForceMiplevelsToBeResident = false;
		}
		PendingStreamingTextures.Empty();

		for (TSet<FAllocation>::TConstIterator It(CurrentAllocations); It; ++It)
		{
			check(!PendingAllocations.Contains(*It));
			PendingAllocations.Add(*It);
		}

		CurrentAllocations.Reset();

		const uint32 SizeX = AddrSpaceAllocator.DimInTexels;
		const uint32 SizeY = AddrSpaceAllocator.DimInTexels;
		const ETextureCreateFlags Flags = TexCreate_ShaderResource | TexCreate_UAV;
		const EPixelFormat Format = bHeight ? PF_R8G8 : PF_G8;
		FRHIResourceCreateInfo CreateInfo(bHeight ? TEXT("HeightFieldAtlas") : TEXT("VisibilityAtlas"));

		AtlasTextureRHI = RHICreateTexture2D(SizeX, SizeY, Format, 1, 1, Flags, CreateInfo);
		AtlasUAVRHI = RHICreateUnorderedAccessView(AtlasTextureRHI, 0);

		MaxDownSampleLevel = LocalDownSampleLevel;
		++Generation;
	}
}

void FLandscapeTextureAtlas::AddAllocation(UTexture2D* Texture, uint32 VisibilityChannel)
{
	check(Texture);

	FAllocation* Found = CurrentAllocations.Find(Texture);
	if (Found)
	{
		++Found->RefCount;
		return;
	}

	Found = FailedAllocations.Find(Texture);
	if (Found)
	{
		++Found->RefCount;
		return;
	}

	Found = PendingAllocations.Find(Texture);
	if (Found)
	{
		++Found->RefCount;
	}
	else
	{
		PendingAllocations.Add(FAllocation(Texture, VisibilityChannel));
	}
}

void FLandscapeTextureAtlas::RemoveAllocation(UTexture2D* Texture)
{
	FSetElementId Id = PendingAllocations.FindId(Texture);
	if (Id.IsValidId())
	{
		check(PendingAllocations[Id].RefCount);
		if (!--PendingAllocations[Id].RefCount)
		{
			check(!PendingStreamingTextures.Contains(Texture));
			PendingAllocations.Remove(Id);
		}
		return;
	}

	Id = FailedAllocations.FindId(Texture);
	if (Id.IsValidId())
	{
		check(FailedAllocations[Id].RefCount);
		if (!--FailedAllocations[Id].RefCount)
		{
			check(!PendingStreamingTextures.Contains(Texture));
			FailedAllocations.Remove(Id);
		}
		return;
	}

	Id = CurrentAllocations.FindId(Texture);
	if (Id.IsValidId())
	{
		FAllocation& Allocation = CurrentAllocations[Id];
		check(Allocation.RefCount && Allocation.Handle != INDEX_NONE);
		if (!--Allocation.RefCount)
		{
			AddrSpaceAllocator.Free(Allocation.Handle);
			PendingStreamingTextures.Remove(Texture);
			CurrentAllocations.Remove(Id);
		}
	}
}

class FUploadLandscapeTextureToAtlasCS : public FGlobalShader
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FSharedParameters, )
		SHADER_PARAMETER(FUintVector4, UpdateRegionOffsetAndSize)
		SHADER_PARAMETER(FVector4, SourceScaleBias)
		SHADER_PARAMETER(uint32, SourceMipBias)
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceTextureSampler)
	END_SHADER_PARAMETER_STRUCT()

	FUploadLandscapeTextureToAtlasCS() = default;

	FUploadLandscapeTextureToAtlasCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
	}

	static constexpr uint32 ThreadGroupSizeX = 8;
	static constexpr uint32 ThreadGroupSizeY = 8;
};

class FUploadHeightFieldToAtlasCS : public FUploadLandscapeTextureToAtlasCS
{
	DECLARE_GLOBAL_SHADER(FUploadHeightFieldToAtlasCS);
	SHADER_USE_PARAMETER_STRUCT(FUploadHeightFieldToAtlasCS, FUploadLandscapeTextureToAtlasCS);

	using FPermutationDomain = FShaderPermutationNone;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FUploadLandscapeTextureToAtlasCS::FSharedParameters, SharedParams)
		SHADER_PARAMETER_UAV(RWTexture2D<float2>, RWHeightFieldAtlas)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FUploadHeightFieldToAtlasCS, "/Engine/Private/HeightFieldAtlasManagement.usf", "UploadHeightFieldToAtlasCS", SF_Compute);

class FUploadVisibilityToAtlasCS : public FUploadLandscapeTextureToAtlasCS
{
	DECLARE_GLOBAL_SHADER(FUploadVisibilityToAtlasCS);
	SHADER_USE_PARAMETER_STRUCT(FUploadVisibilityToAtlasCS, FUploadLandscapeTextureToAtlasCS);

	using FPermutationDomain = FShaderPermutationNone;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FUploadLandscapeTextureToAtlasCS::FSharedParameters, SharedParams)
		SHADER_PARAMETER(FVector4, VisibilityChannelMask)
		SHADER_PARAMETER_UAV(RWTexture2D<float>, RWVisibilityAtlas)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FUploadVisibilityToAtlasCS, "/Engine/Private/HeightFieldAtlasManagement.usf", "UploadVisibilityToAtlasCS", SF_Compute);

uint32 FLandscapeTextureAtlas::CalculateDownSampleLevel(uint32 SizeX, uint32 SizeY) const
{
	const uint32 TileSize = AddrSpaceAllocator.TileSize;

	for (uint32 CurLevel = 0; CurLevel <= MaxDownSampleLevel; ++CurLevel)
	{
		const uint32 DownSampledSizeX = SizeX >> CurLevel;
		const uint32 DownSampledSizeY = SizeY >> CurLevel;

		if (DownSampledSizeX <= TileSize && DownSampledSizeY <= TileSize)
		{
			return CurLevel;
		}
	}

	return MaxDownSampleLevel;
}

void FLandscapeTextureAtlas::UpdateAllocations(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type InFeatureLevel)
{
	InitializeIfNeeded();

	TArray<FPendingUpload, TInlineAllocator<8>> PendingUploads;

	auto AllocSortPred = [](const FAllocation& A, const FAllocation& B)
	{
		const int32 SizeA = FMath::Max(A.SourceTexture->GetSizeX(), A.SourceTexture->GetSizeY());
		const int32 SizeB = FMath::Max(B.SourceTexture->GetSizeX(), B.SourceTexture->GetSizeY());
		return SizeA < SizeB;
	};

	for (int32 Idx = 0; Idx < PendingStreamingTextures.Num(); ++Idx)
	{
		UTexture2D* SourceTexture = PendingStreamingTextures[Idx];
		const uint32 SizeX = SourceTexture->GetSizeX();
		const uint32 SizeY = SourceTexture->GetSizeY();
		const uint32 DownSampleLevel = CalculateDownSampleLevel(SizeX, SizeY);
		const uint32 NumMissingMips = SourceTexture->GetNumMips() - SourceTexture->GetNumResidentMips();

		if (NumMissingMips <= DownSampleLevel)
		{
			SourceTexture->bForceMiplevelsToBeResident = false;
			const uint32 SourceMipBias = DownSampleLevel - NumMissingMips;
			const FAllocation* Allocation = CurrentAllocations.Find(SourceTexture);
			check(Allocation && Allocation->Handle != INDEX_NONE);
			const uint32 VisibilityChannel = Allocation->VisibilityChannel;
			PendingUploads.Emplace(SourceTexture, SizeX >> DownSampleLevel, SizeY >> DownSampleLevel, SourceMipBias, Allocation->Handle, VisibilityChannel);
			PendingStreamingTextures.RemoveAtSwap(Idx--);
		}
	}

	if (PendingAllocations.Num())
	{
		PendingAllocations.Sort(AllocSortPred);
		bool bAllocFailed = false;

		for (TSet<FAllocation>::TConstIterator It(PendingAllocations); It; ++It)
		{
			FAllocation TmpAllocation = *It;

			if (!bAllocFailed)
			{
				UTexture2D* SourceTexture = TmpAllocation.SourceTexture;
				const uint32 SizeX = SourceTexture->GetSizeX();
				const uint32 SizeY = SourceTexture->GetSizeY();
				const uint32 DownSampleLevel = CalculateDownSampleLevel(SizeX, SizeY);
				const uint32 DownSampledSizeX = SizeX >> DownSampleLevel;
				const uint32 DownSampledSizeY = SizeY >> DownSampleLevel;
				const uint32 Handle = AddrSpaceAllocator.Alloc(DownSampledSizeX, DownSampledSizeY);
				const uint32 VisibilityChannel = TmpAllocation.VisibilityChannel;

				if (Handle == INDEX_NONE)
				{
					FailedAllocations.Add(TmpAllocation);
					bAllocFailed = true;
					continue;
				}

				const uint32 NumMissingMips = SourceTexture->GetNumMips() - SourceTexture->GetNumResidentMips();
				const uint32 SourceMipBias = NumMissingMips > DownSampleLevel ? 0 : DownSampleLevel - NumMissingMips;

				if (NumMissingMips > DownSampleLevel)
				{
					SourceTexture->bForceMiplevelsToBeResident = true;
					check(!PendingStreamingTextures.Contains(SourceTexture));
					PendingStreamingTextures.Add(SourceTexture);
				}

				TmpAllocation.Handle = Handle;
				CurrentAllocations.Add(TmpAllocation);
				PendingUploads.Emplace(SourceTexture, DownSampledSizeX, DownSampledSizeY, SourceMipBias, Handle, VisibilityChannel);
			}
			else
			{
				FailedAllocations.Add(TmpAllocation);
			}
		}

		if (bAllocFailed)
		{
			FailedAllocations.Sort(AllocSortPred);
		}
		PendingAllocations.Empty();
	}

	if (FailedAllocations.Num())
	{
		for (TSet<FAllocation>::TIterator It(FailedAllocations); It; ++It)
		{
			FAllocation TmpAllocation = *It;
			UTexture2D* SourceTexture = TmpAllocation.SourceTexture;
			const uint32 SizeX = SourceTexture->GetSizeX();
			const uint32 SizeY = SourceTexture->GetSizeY();
			const uint32 DownSampleLevel = CalculateDownSampleLevel(SizeX, SizeY);
			const uint32 DownSampledSizeX = SizeX >> DownSampleLevel;
			const uint32 DownSampledSizeY = SizeY >> DownSampleLevel;
			const uint32 Handle = AddrSpaceAllocator.Alloc(DownSampledSizeX, DownSampledSizeY);
			const uint32 VisibilityChannel = TmpAllocation.VisibilityChannel;

			if (Handle == INDEX_NONE)
			{
				break;
			}

			const uint32 NumMissingMips = SourceTexture->GetNumMips() - SourceTexture->GetNumResidentMips();
			const uint32 SourceMipBias = NumMissingMips > DownSampleLevel ? 0 : DownSampleLevel - NumMissingMips;

			if (NumMissingMips > DownSampleLevel)
			{
				SourceTexture->bForceMiplevelsToBeResident = true;
				check(!PendingStreamingTextures.Contains(SourceTexture));
				PendingStreamingTextures.Add(SourceTexture);
			}

			TmpAllocation.Handle = Handle;
			CurrentAllocations.Add(TmpAllocation);
			PendingUploads.Emplace(SourceTexture, DownSampledSizeX, DownSampledSizeY, SourceMipBias, Handle, VisibilityChannel);
			It.RemoveCurrent();
		}
	}

	if (PendingUploads.Num())
	{
		AddPass(GraphBuilder, [this](FRHICommandList& RHICmdList)
		{
			RHICmdList.Transition(FRHITransitionInfo(AtlasUAVRHI, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
			RHICmdList.BeginUAVOverlap(AtlasUAVRHI);
		});

		if (SubAllocType == SAT_Height)
		{
			TShaderMapRef<FUploadHeightFieldToAtlasCS> ComputeShader(GetGlobalShaderMap(InFeatureLevel));
			for (int32 Idx = 0; Idx < PendingUploads.Num(); ++Idx)
			{
				typename FUploadHeightFieldToAtlasCS::FParameters* Parameters = GraphBuilder.AllocParameters<typename FUploadHeightFieldToAtlasCS::FParameters>();
				const FIntPoint UpdateRegion = PendingUploads[Idx].SetShaderParameters(Parameters, *this);
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("UploadHeightFieldToAtlas"),
					Parameters,
					ERDGPassFlags::Compute,
					[Parameters, ComputeShader, UpdateRegion](FRHICommandList& CmdList)
				{
					FComputeShaderUtils::Dispatch(CmdList, ComputeShader, *Parameters, FIntVector(UpdateRegion.X, UpdateRegion.Y, 1));
				});
			}
		}
		else
		{
			TShaderMapRef<FUploadVisibilityToAtlasCS> ComputeShader(GetGlobalShaderMap(InFeatureLevel));
			for (int32 Idx = 0; Idx < PendingUploads.Num(); ++Idx)
			{
				typename FUploadVisibilityToAtlasCS::FParameters* Parameters = GraphBuilder.AllocParameters<typename FUploadVisibilityToAtlasCS::FParameters>();
				const FIntPoint UpdateRegion = PendingUploads[Idx].SetShaderParameters(Parameters, *this);
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("UploadVisibilityToAtlas"),
					Parameters,
					ERDGPassFlags::Compute,
					[Parameters, ComputeShader, UpdateRegion](FRHICommandList& CmdList)
				{
					FComputeShaderUtils::Dispatch(CmdList, ComputeShader, *Parameters, FIntVector(UpdateRegion.X, UpdateRegion.Y, 1));
				});
			}
		}

		AddPass(GraphBuilder, [this](FRHICommandList& RHICmdList)
		{
			RHICmdList.EndUAVOverlap(AtlasUAVRHI);
			RHICmdList.Transition(FRHITransitionInfo(AtlasUAVRHI, ERHIAccess::UAVCompute, ERHIAccess::SRVGraphics));
		});
	}
}

void FLandscapeTextureAtlas::UpdateAllocations(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel)
{
	FMemMark Mark(FMemStack::Get());
	FRDGBuilder GraphBuilder(RHICmdList);
	UpdateAllocations(GraphBuilder, InFeatureLevel);
	GraphBuilder.Execute();
}

uint32 FLandscapeTextureAtlas::GetAllocationHandle(UTexture2D* Texture) const
{
	const FAllocation* Allocation = CurrentAllocations.Find(Texture);
	return Allocation ? Allocation->Handle : INDEX_NONE;
}

FVector4 FLandscapeTextureAtlas::GetAllocationScaleBias(uint32 Handle) const
{
	return AddrSpaceAllocator.GetScaleBias(Handle);
}

void FLandscapeTextureAtlas::FSubAllocator::Init(uint32 InTileSize, uint32 InBorderSize, uint32 InDimInTiles)
{
	check(InDimInTiles && !(InDimInTiles & (InDimInTiles - 1)));

	TileSize = InTileSize;
	BorderSize = InBorderSize;
	TileSizeWithBorder = InTileSize + 2 * InBorderSize;
	DimInTiles = InDimInTiles;
	DimInTilesShift = FMath::CountBits(InDimInTiles - 1);
	DimInTilesMask = InDimInTiles - 1;
	DimInTexels = InDimInTiles * TileSizeWithBorder;
	MaxNumTiles = InDimInTiles * InDimInTiles;
	
	TexelSize = 1.f / DimInTexels;
	TileScale = TileSize * TexelSize;

	LevelOffsets.Empty();
	MarkerQuadTree.Empty();
	SubAllocInfos.Empty();

	uint32 NumBits = 0;
	for (uint32 Level = 1; Level <= DimInTiles; Level <<= 1)
	{
		const uint32 NumQuadsInLevel = Level * Level;
		LevelOffsets.Add(NumBits);
		NumBits += NumQuadsInLevel;
	}
	MarkerQuadTree.Add(false, NumBits);
}

uint32 FLandscapeTextureAtlas::FSubAllocator::Alloc(uint32 SizeX, uint32 SizeY)
{
	const uint32 NumTiles1D = FMath::DivideAndRoundUp(FMath::Max(SizeX, SizeY), TileSize);
	check(NumTiles1D <= DimInTiles);
	const uint32 NumLevels = LevelOffsets.Num();
	const uint32 Level = NumLevels - FMath::CeilLogTwo(NumTiles1D) - 1;
	const uint32 LevelOffset = LevelOffsets[Level];
	const uint32 QuadsInLevel1D = 1u << Level;
	const uint32 SearchEnd = LevelOffset + QuadsInLevel1D * QuadsInLevel1D;

	uint32 QuadIdx = LevelOffset;
	for (; QuadIdx < SearchEnd; ++QuadIdx)
	{
		if (!MarkerQuadTree[QuadIdx])
		{
			break;
		}
	}

	if (QuadIdx != SearchEnd)
	{
		const uint32 QuadIdxInLevel = QuadIdx - LevelOffset;
		
		uint32 ParentLevel = Level;
		uint32 ParentQuadIdxInLevel = QuadIdxInLevel;
		for (; ParentLevel != (uint32)-1; --ParentLevel)
		{
			const uint32 ParentLevelOffset = LevelOffsets[ParentLevel];
			const uint32 ParentQuadIdx = ParentLevelOffset + ParentQuadIdxInLevel;
			FBitReference Marker = MarkerQuadTree[ParentQuadIdx];
			if (Marker)
			{
				break;
			}
			Marker = true;
			ParentQuadIdxInLevel >>= 2;
		}

		uint32 ChildLevel = Level + 1;
		uint32 ChildQuadIdxInLevel = QuadIdxInLevel << 2;
		uint32 NumChildren = 4;
		for (; ChildLevel < NumLevels; ++ChildLevel)
		{
			const uint32 ChildQuadIdx = ChildQuadIdxInLevel + LevelOffsets[ChildLevel];
			for (uint32 Idx = 0; Idx < NumChildren; ++Idx)
			{
				check(!MarkerQuadTree[ChildQuadIdx + Idx]);
				MarkerQuadTree[ChildQuadIdx + Idx] = true;
			}
			ChildQuadIdxInLevel <<= 2;
			NumChildren <<= 2;
		}
		
		const uint32 QuadX = FMath::ReverseMortonCode2(QuadIdxInLevel);
		const uint32 QuadY = FMath::ReverseMortonCode2(QuadIdxInLevel >> 1);
		const uint32 QuadSizeInTiles1D = DimInTiles >> Level;
		const uint32 TileX = QuadX * QuadSizeInTiles1D;
		const uint32 TileY = QuadY * QuadSizeInTiles1D;

		FSubAllocInfo SubAllocInfo;
		SubAllocInfo.Level = Level;
		SubAllocInfo.QuadIdx = QuadIdx;
		SubAllocInfo.UVScaleBias.X = SizeX * TexelSize;
		SubAllocInfo.UVScaleBias.Y = SizeY * TexelSize;
		SubAllocInfo.UVScaleBias.Z = TileX / (float)DimInTiles + BorderSize * TexelSize;
		SubAllocInfo.UVScaleBias.W = TileY / (float)DimInTiles + BorderSize * TexelSize;

		return SubAllocInfos.Add(SubAllocInfo);
	}

	return INDEX_NONE;
}

void FLandscapeTextureAtlas::FSubAllocator::Free(uint32 Handle)
{
	check(SubAllocInfos.IsValidIndex(Handle));

	const FSubAllocInfo SubAllocInfo = SubAllocInfos[Handle];
	SubAllocInfos.RemoveAt(Handle);

	const uint32 Level = SubAllocInfo.Level;
	const uint32 QuadIdx = SubAllocInfo.QuadIdx;
	
	uint32 ChildLevel = Level;
	uint32 ChildIdxInLevel = QuadIdx - LevelOffsets[Level];
	uint32 NumChildren = 1;
	const uint32 NumLevels = LevelOffsets.Num();
	for (; ChildLevel < NumLevels; ++ChildLevel)
	{
		const uint32 ChildIdx = ChildIdxInLevel + LevelOffsets[ChildLevel];
		for (uint32 Idx = 0; Idx < NumChildren; ++Idx)
		{
			check(MarkerQuadTree[ChildIdx + Idx]);
			MarkerQuadTree[ChildIdx + Idx] = false;
		}
		ChildIdxInLevel <<= 2;
		NumChildren <<= 2;
	}

	uint32 TestIdxInLevel = (QuadIdx - LevelOffsets[Level]) & ~3u;
	uint32 ParentLevel = Level - 1;
	for (; ParentLevel != (uint32)-1; --ParentLevel)
	{
		const uint32 TestIdx = TestIdxInLevel + LevelOffsets[ParentLevel + 1];
		const bool bParentFree = !MarkerQuadTree[TestIdx] && !MarkerQuadTree[TestIdx + 1] && !MarkerQuadTree[TestIdx + 2] && !MarkerQuadTree[TestIdx + 3];
		if (!bParentFree)
		{
			break;
		}
		const uint32 ParentIdxInLevel = TestIdxInLevel >> 2;
		const uint32 ParentIdx = ParentIdxInLevel + LevelOffsets[ParentLevel];
		MarkerQuadTree[ParentIdx] = false;
		TestIdxInLevel = ParentIdxInLevel & ~3u;
	}
}

FVector4 FLandscapeTextureAtlas::FSubAllocator::GetScaleBias(uint32 Handle) const
{
	check(SubAllocInfos.IsValidIndex(Handle));
	return SubAllocInfos[Handle].UVScaleBias;
}

FIntPoint FLandscapeTextureAtlas::FSubAllocator::GetStartOffset(uint32 Handle) const
{
	check(SubAllocInfos.IsValidIndex(Handle));
	const FSubAllocInfo& Info = SubAllocInfos[Handle];
	const uint32 QuadIdxInLevel = Info.QuadIdx - LevelOffsets[Info.Level];
	const uint32 QuadX = FMath::ReverseMortonCode2(QuadIdxInLevel);
	const uint32 QuadY = FMath::ReverseMortonCode2(QuadIdxInLevel >> 1);
	const uint32 QuadSizeInTexels1D = (DimInTiles >> Info.Level) * TileSizeWithBorder;	
	return FIntPoint(QuadX * QuadSizeInTexels1D, QuadY * QuadSizeInTexels1D);
}

FLandscapeTextureAtlas::FAllocation::FAllocation()
	: SourceTexture(nullptr)
	, Handle(INDEX_NONE)
	, VisibilityChannel(0)
	, RefCount(0)
{}

FLandscapeTextureAtlas::FAllocation::FAllocation(UTexture2D* InTexture, uint32 InVisibilityChannel)
	: SourceTexture(InTexture)
	, Handle(INDEX_NONE)
	, VisibilityChannel(InVisibilityChannel)
	, RefCount(1)
{}

FLandscapeTextureAtlas::FPendingUpload::FPendingUpload(UTexture2D* Texture, uint32 SizeX, uint32 SizeY, uint32 MipBias, uint32 InHandle, uint32 Channel)
	: SourceTexture(Texture->Resource->TextureRHI)
	, SizesAndMipBias(FIntVector(SizeX, SizeY, MipBias))
	, VisibilityChannel(Channel)
	, Handle(InHandle)
{}

FIntPoint FLandscapeTextureAtlas::FPendingUpload::SetShaderParameters(void* ParamsPtr, const FLandscapeTextureAtlas& Atlas) const
{
	if (Atlas.SubAllocType == SAT_Height)
	{
		typename FUploadHeightFieldToAtlasCS::FParameters* Params = (typename FUploadHeightFieldToAtlasCS::FParameters*)ParamsPtr;
		Params->RWHeightFieldAtlas = Atlas.AtlasUAVRHI;
		return SetCommonShaderParameters(&Params->SharedParams, Atlas);
	}
	else
	{
		typename FUploadVisibilityToAtlasCS::FParameters* Params = (typename FUploadVisibilityToAtlasCS::FParameters*)ParamsPtr;
		FVector4 ChannelMask(ForceInitToZero);
		ChannelMask[VisibilityChannel] = 1.f;
		Params->VisibilityChannelMask = ChannelMask;
		Params->RWVisibilityAtlas = Atlas.AtlasUAVRHI;
		return SetCommonShaderParameters(&Params->SharedParams, Atlas);
	}
}

FIntPoint FLandscapeTextureAtlas::FPendingUpload::SetCommonShaderParameters(void* ParamsPtr, const FLandscapeTextureAtlas& Atlas) const
{
	const uint32 DownSampledSizeX = SizesAndMipBias.X;
	const uint32 DownSampledSizeY = SizesAndMipBias.Y;
	const uint32 SourceMipBias = SizesAndMipBias.Z;
	const float InvDownSampledSizeX = 1.f / DownSampledSizeX;
	const float InvDownSampledSizeY = 1.f / DownSampledSizeY;
	const uint32 BorderSize = Atlas.AddrSpaceAllocator.BorderSize;
	const uint32 UpdateRegionSizeX = DownSampledSizeX + 2 * BorderSize;
	const uint32 UpdateRegionSizeY = DownSampledSizeY + 2 * BorderSize;
	const FIntPoint StartOffset = Atlas.AddrSpaceAllocator.GetStartOffset(Handle);

	typename FUploadLandscapeTextureToAtlasCS::FSharedParameters* CommonParams = (typename FUploadLandscapeTextureToAtlasCS::FSharedParameters*)ParamsPtr;
	CommonParams->UpdateRegionOffsetAndSize = FUintVector4(StartOffset.X, StartOffset.Y, UpdateRegionSizeX, UpdateRegionSizeY);
	CommonParams->SourceScaleBias = FVector4(InvDownSampledSizeX, InvDownSampledSizeY, (.5f - BorderSize) * InvDownSampledSizeX, (.5f - BorderSize) * InvDownSampledSizeY);
	CommonParams->SourceMipBias = SourceMipBias;
	CommonParams->SourceTexture = SourceTexture;
	CommonParams->SourceTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	const uint32 NumGroupsX = FMath::DivideAndRoundUp(UpdateRegionSizeX, FUploadLandscapeTextureToAtlasCS::ThreadGroupSizeX);
	const uint32 NumGroupsY = FMath::DivideAndRoundUp(UpdateRegionSizeY, FUploadLandscapeTextureToAtlasCS::ThreadGroupSizeY);
	return FIntPoint(NumGroupsX, NumGroupsY);
}
