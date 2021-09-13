// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticMeshCompiler.h"

#if WITH_EDITOR

#include "Algo/NoneOf.h"
#include "AssetCompilingManager.h"
#include "Engine/StaticMesh.h"
#include "ObjectCacheContext.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "Misc/Optional.h"
#include "EngineModule.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/IQueuedWork.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Components/PrimitiveComponent.h"
#include "ContentStreaming.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Rendering/StaticLightingSystemInterface.h"
#include "AI/NavigationSystemBase.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "StaticMeshCompiler"

static AsyncCompilationHelpers::FAsyncCompilationStandardCVars CVarAsyncStaticMeshStandard(
	TEXT("StaticMesh"),
	TEXT("static meshes"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			FStaticMeshCompilingManager::Get().FinishAllCompilation();
		}
	));

static TAutoConsoleVariable<int32> CVarAsyncStaticMeshPlayInEditorMode(
	TEXT("Editor.AsyncStaticMeshPlayInEditorMode"),
	0,
	TEXT("0 - Wait until all static meshes are built before entering PIE. (Slowest but causes no visual or behavior artifacts.) \n")
	TEXT("1 - Wait until all static meshes affecting navigation and physics are built before entering PIE. (Some visuals might be missing during compilation.)\n")
	TEXT("2 - Wait only on static meshes affecting navigation and physics when they are close to the player. (Fastest while still preventing falling through the floor and going through objects.)\n"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAsyncStaticMeshPlayInEditorDistance(
	TEXT("Editor.AsyncStaticMeshPlayInEditorDistance"),
	2.0f,
	TEXT("Scale applied to the player bounding sphere to determine how far away to force meshes compilation before resuming play.\n")
	TEXT("The effect can be seen during play session when Editor.AsyncStaticMeshPlayInEditorDebugDraw = 1.\n"),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarAsyncStaticMeshDebugDraw(
	TEXT("Editor.AsyncStaticMeshPlayInEditorDebugDraw"),
	false,
	TEXT("0 - Debug draw for async static mesh compilation is disabled.\n")
	TEXT("1 - Debug draw for async static mesh compilation is enabled.\n")
	TEXT("The collision sphere around the player is drawn in white and can be adjusted with Editor.AsyncStaticMeshPlayInEditorDistance\n")
	TEXT("Any static meshes affecting the physics that are still being compiled will have their bounding box drawn in green.\n")
	TEXT("Any static meshes that were waited on due to being too close to the player will have their bounding box drawn in red for a couple of seconds."),
	ECVF_Default);

namespace StaticMeshCompilingManagerImpl
{
	static void EnsureInitializedCVars()
	{
		static bool bIsInitialized = false;

		if (!bIsInitialized)
		{
			bIsInitialized = true;

			AsyncCompilationHelpers::EnsureInitializedCVars(
				TEXT("staticmesh"),
				CVarAsyncStaticMeshStandard.AsyncCompilation,
				CVarAsyncStaticMeshStandard.AsyncCompilationMaxConcurrency,
				GET_MEMBER_NAME_CHECKED(UEditorExperimentalSettings, bEnableAsyncStaticMeshCompilation));
		}
	}
}

FStaticMeshCompilingManager::FStaticMeshCompilingManager()
	: Notification(LOCTEXT("StaticMeshes", "Static Meshes"))
{
	PostReachabilityAnalysisHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddRaw(this, &FStaticMeshCompilingManager::OnPostReachabilityAnalysis);
}

void FStaticMeshCompilingManager::OnPostReachabilityAnalysis()
{
	if (GetNumRemainingMeshes())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::CancelUnreachableMeshes);

		TArray<UStaticMesh*> PendingStaticMeshes;
		PendingStaticMeshes.Reserve(GetNumRemainingMeshes());

		for (auto Iterator = RegisteredStaticMesh.CreateIterator(); Iterator; ++Iterator)
		{
			UStaticMesh* StaticMesh = Iterator->GetEvenIfUnreachable();
			if (StaticMesh && StaticMesh->IsUnreachable())
			{
				UE_LOG(LogStaticMesh, Verbose, TEXT("Cancelling static mesh %s async compilation because it's being garbage collected"), *StaticMesh->GetName());

				if (StaticMesh->TryCancelAsyncTasks())
				{
					Iterator.RemoveCurrent();
				}
				else
				{
					PendingStaticMeshes.Add(StaticMesh);
				}
			}
		}

		FinishCompilation(PendingStaticMeshes);
	}
}

EQueuedWorkPriority FStaticMeshCompilingManager::GetBasePriority(UStaticMesh* InStaticMesh) const
{
	return EQueuedWorkPriority::Low;
}

FQueuedThreadPool* FStaticMeshCompilingManager::GetThreadPool() const
{
	static FQueuedThreadPoolDynamicWrapper* GStaticMeshThreadPool = nullptr;
	if (GStaticMeshThreadPool == nullptr)
	{
		StaticMeshCompilingManagerImpl::EnsureInitializedCVars();

		// Static meshes will be scheduled on the asset thread pool, where concurrency limits might by dynamically adjusted depending on memory constraints.
		GStaticMeshThreadPool = new FQueuedThreadPoolDynamicWrapper(FAssetCompilingManager::Get().GetThreadPool(), -1, [](EQueuedWorkPriority) { return EQueuedWorkPriority::Low; });

		AsyncCompilationHelpers::BindThreadPoolToCVar(
			GStaticMeshThreadPool,
			CVarAsyncStaticMeshStandard.AsyncCompilation,
			CVarAsyncStaticMeshStandard.AsyncCompilationResume,
			CVarAsyncStaticMeshStandard.AsyncCompilationMaxConcurrency
		);
	}

	return GStaticMeshThreadPool;
}

void FStaticMeshCompilingManager::Shutdown()
{
	bHasShutdown = true;
	if (GetNumRemainingMeshes())
	{
		check(IsInGameThread());
		TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::Shutdown)

		TArray<UStaticMesh*> PendingStaticMeshes;
		PendingStaticMeshes.Reserve(GetNumRemainingMeshes());

		for (TWeakObjectPtr<UStaticMesh>& WeakStaticMesh : RegisteredStaticMesh)
		{
			if (WeakStaticMesh.IsValid())
			{
				UStaticMesh* StaticMesh = WeakStaticMesh.Get();
				if (!StaticMesh->TryCancelAsyncTasks())
				{
					PendingStaticMeshes.Add(StaticMesh);
				}
			}
		}

		FinishCompilation(PendingStaticMeshes);
	}

	FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(PostReachabilityAnalysisHandle);
}

bool FStaticMeshCompilingManager::IsAsyncStaticMeshCompilationEnabled() const
{
	if (bHasShutdown)
	{
		return false;
	}

	StaticMeshCompilingManagerImpl::EnsureInitializedCVars();

	return CVarAsyncStaticMeshStandard.AsyncCompilation.GetValueOnAnyThread() != 0;
}

void FStaticMeshCompilingManager::UpdateCompilationNotification()
{
	Notification.Update(GetNumRemainingMeshes());
}

void FStaticMeshCompilingManager::PostCompilation(TArrayView<UStaticMesh* const> InStaticMeshes)
{
	if (InStaticMeshes.Num())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(OnAssetPostCompileEvent);

		TArray<FAssetCompileData> AssetsData;
		AssetsData.Reserve(InStaticMeshes.Num());

		for (UStaticMesh* StaticMesh : InStaticMeshes)
		{
			// Do not broadcast an event for unreachable objects
			if (!StaticMesh->IsUnreachable())
			{
				AssetsData.Emplace(StaticMesh);
			}
		}

		if (AssetsData.Num())
		{
			FAssetCompilingManager::Get().OnAssetPostCompileEvent().Broadcast(AssetsData);
		}
	}
}

void FStaticMeshCompilingManager::PostCompilation(UStaticMesh* StaticMesh)
{
	using namespace StaticMeshCompilingManagerImpl;
	
	// If AsyncTask is null here, the task got canceled so we don't need to do anything
	if (StaticMesh->AsyncTask && !IsEngineExitRequested())
	{
		check(IsInGameThread());
		TRACE_CPUPROFILER_EVENT_SCOPE(PostCompilation);

		FObjectCacheContextScope ObjectCacheScope;

		// The scope is important here to destroy the FStaticMeshAsyncBuildScope before broadcasting events
		{
			// Acquire the async task locally to protect against re-entrance
			TUniquePtr<FStaticMeshAsyncBuildTask> LocalAsyncTask = MoveTemp(StaticMesh->AsyncTask);
			LocalAsyncTask->EnsureCompletion();

			// Do not do anything else if the staticmesh is being garbage collected
			if (StaticMesh->IsUnreachable())
			{
				return;
			}

			UE_LOG(LogStaticMesh, Verbose, TEXT("Refreshing static mesh %s because it is ready"), *StaticMesh->GetName());

			FStaticMeshAsyncBuildScope AsyncBuildScope(StaticMesh);

			if (LocalAsyncTask->GetTask().PostLoadContext.IsValid())
			{
				StaticMesh->FinishPostLoadInternal(*LocalAsyncTask->GetTask().PostLoadContext);

				LocalAsyncTask->GetTask().PostLoadContext.Reset();
			}

			if (LocalAsyncTask->GetTask().BuildContext.IsValid())
			{
				TArray<UStaticMeshComponent*> ComponentsToUpdate;
				for (UStaticMeshComponent* Component : ObjectCacheScope.GetContext().GetStaticMeshComponents(StaticMesh))
				{
					ComponentsToUpdate.Add(Component);
				}

				StaticMesh->FinishBuildInternal(
					ComponentsToUpdate,
					LocalAsyncTask->GetTask().BuildContext->bHasRenderDataChanged,
					LocalAsyncTask->GetTask().BuildContext->bShouldComputeExtendedBounds
				);

				LocalAsyncTask->GetTask().BuildContext.Reset();
			}
		}

		for (UStaticMeshComponent* Component : ObjectCacheScope.GetContext().GetStaticMeshComponents(StaticMesh))
		{
			Component->PostStaticMeshCompilation();
		}

		// Generate an empty property changed event, to force the asset registry tag
		// to be refreshed now that RenderData is available.
		FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
		FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(StaticMesh, EmptyPropertyChangedEvent);
	}
}

bool FStaticMeshCompilingManager::IsAsyncCompilationAllowed(UStaticMesh* StaticMesh) const
{
	return IsAsyncStaticMeshCompilationEnabled();
}

FStaticMeshCompilingManager& FStaticMeshCompilingManager::Get()
{
	static FStaticMeshCompilingManager Singleton;
	return Singleton;
}

int32 FStaticMeshCompilingManager::GetNumRemainingMeshes() const
{
	return RegisteredStaticMesh.Num();
}

void FStaticMeshCompilingManager::AddStaticMeshes(TArrayView<UStaticMesh* const> InStaticMeshes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::AddStaticMeshes)
	check(IsInGameThread());

	// Wait until we gather enough mesh to process
	// to amortize the cost of scanning components
	//ProcessStaticMeshes(32 /* MinBatchSize */);

	for (UStaticMesh* StaticMesh : InStaticMeshes)
	{
		check(StaticMesh->AsyncTask != nullptr);
		RegisteredStaticMesh.Emplace(StaticMesh);
	}
}

void FStaticMeshCompilingManager::FinishCompilation(TArrayView<UStaticMesh* const> InStaticMeshes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::FinishCompilation);

	// Allow calls from any thread if the meshes are already finished compiling.
	if (Algo::NoneOf(InStaticMeshes, &UStaticMesh::IsCompiling))
	{
		return;
	}

	check(IsInGameThread());

	TArray<UStaticMesh*> PendingStaticMeshes;
	PendingStaticMeshes.Reserve(InStaticMeshes.Num());

	int32 StaticMeshIndex = 0;
	for (UStaticMesh* StaticMesh : InStaticMeshes)
	{
		if (RegisteredStaticMesh.Contains(StaticMesh))
		{
			PendingStaticMeshes.Emplace(StaticMesh);
		}
	}

	if (PendingStaticMeshes.Num())
	{
		class FCompilableStaticMesh : public AsyncCompilationHelpers::TCompilableAsyncTask<FStaticMeshAsyncBuildTask>
		{
		public:
			FCompilableStaticMesh(UStaticMesh* InStaticMesh)
				: StaticMesh(InStaticMesh)
			{
			}

			FStaticMeshAsyncBuildTask* GetAsyncTask() override
			{
				return StaticMesh->AsyncTask.Get();
			}

			UStaticMesh* StaticMesh;
			FName GetName() override { return StaticMesh->GetFName(); }
		};

		TArray<FCompilableStaticMesh> CompilableStaticMeshes(PendingStaticMeshes);
		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableStaticMeshes](int32 Index)	-> AsyncCompilationHelpers::ICompilable& { return CompilableStaticMeshes[Index]; },
			CompilableStaticMeshes.Num(),
			LOCTEXT("StaticMeshes", "Static Meshes"),
			LogStaticMesh,
			[this](AsyncCompilationHelpers::ICompilable* Object)
			{
				UStaticMesh* StaticMesh = static_cast<FCompilableStaticMesh*>(Object)->StaticMesh;
				PostCompilation(StaticMesh);
				RegisteredStaticMesh.Remove(StaticMesh);
			}
		);

		PostCompilation(PendingStaticMeshes);
	}
}

void FStaticMeshCompilingManager::FinishCompilationsForGame()
{
	if (GetNumRemainingMeshes())
	{
		FObjectCacheContextScope ObjectCacheScope;
		// Supports both Game and PIE mode
		const bool bIsPlaying = 
			(GWorld && !GWorld->IsEditorWorld()) ||
			(GEditor && GEditor->PlayWorld && !GEditor->IsSimulateInEditorInProgress());

		if (bIsPlaying)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::FinishCompilationsForGame);

			const int32 PlayInEditorMode = CVarAsyncStaticMeshPlayInEditorMode.GetValueOnGameThread();
			
			const bool bShowDebugDraw = CVarAsyncStaticMeshDebugDraw.GetValueOnGameThread();

			TSet<const UWorld*> PIEWorlds;
			TMultiMap<const UWorld*, FBoxSphereBounds> WorldActors;
			
			float RadiusScale = CVarAsyncStaticMeshPlayInEditorDistance.GetValueOnGameThread();
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (WorldContext.WorldType == EWorldType::PIE || WorldContext.WorldType == EWorldType::Game)
				{
					UWorld* World = WorldContext.World();
					PIEWorlds.Add(World);

					// Extract all pawns of the world to support player/bots local and remote.
					if (PlayInEditorMode == 2)
					{
						for (TActorIterator<APawn> It(World); It; ++It)
						{
							const APawn* Pawn = *It;
							if (Pawn)
							{
								FBoxSphereBounds ActorBounds;
								Pawn->GetActorBounds(true, ActorBounds.Origin, ActorBounds.BoxExtent);
								ActorBounds.SphereRadius = ActorBounds.BoxExtent.GetMax() * RadiusScale;
								WorldActors.Emplace(World, ActorBounds);

								if (bShowDebugDraw)
								{
									DrawDebugSphere(World, ActorBounds.Origin, ActorBounds.SphereRadius, 10, FColor::White);
								}
							}
						}
					}
				}
			}
			
			TSet<UStaticMesh*> StaticMeshToCompile;
			TArray<FBoxSphereBounds, TInlineAllocator<16>> ActorsBounds;
			for (const UStaticMeshComponent* Component : ObjectCacheScope.GetContext().GetStaticMeshComponents())
			{
				if (Component->IsRegistered() &&
					PIEWorlds.Contains(Component->GetWorld()) &&
					Component->GetStaticMesh() != nullptr &&
					RegisteredStaticMesh.Contains(Component->GetStaticMesh()) &&
					(PlayInEditorMode == 0 || Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Component->IsNavigationRelevant() || Component->bAlwaysCreatePhysicsState || Component->CanCharacterStepUpOn != ECB_No))
				{
					const FBoxSphereBounds ComponentBounds = Component->Bounds.GetBox();
					const UWorld* ComponentWorld = Component->GetWorld();

					if (PlayInEditorMode == 2)
					{
						ActorsBounds.Reset();
						WorldActors.MultiFind(ComponentWorld, ActorsBounds);
						
						bool bStaticMeshComponentCollided = false;
						if (ActorsBounds.Num())
						{
							for (const FBoxSphereBounds& ActorBounds : ActorsBounds)
							{
								if (FMath::SphereAABBIntersection(ActorBounds.Origin, ActorBounds.SphereRadius * ActorBounds.SphereRadius, ComponentBounds.GetBox()))
								{
									if (bShowDebugDraw)
									{
										DrawDebugBox(ComponentWorld, ComponentBounds.Origin, ComponentBounds.BoxExtent, FColor::Red, false, 10.0f);
									}
								
									bool bIsAlreadyInSet = false;
									StaticMeshToCompile.Add(Component->GetStaticMesh(), &bIsAlreadyInSet);
									if (!bIsAlreadyInSet)
									{
										UE_LOG(
											LogStaticMesh,
											Display,
											TEXT("Waiting on static mesh %s being ready because it affects collision/navigation and is near a player/bot"),
											*Component->GetStaticMesh()->GetFullName()
										);
									}
									bStaticMeshComponentCollided = true;
									break;
								}
							}
						}

						if (bShowDebugDraw && !bStaticMeshComponentCollided)
						{
							DrawDebugBox(ComponentWorld, ComponentBounds.Origin, ComponentBounds.BoxExtent, FColor::Green);
						}
					}
					else 
					{
						bool bIsAlreadyInSet = false;
						StaticMeshToCompile.Add(Component->GetStaticMesh(), &bIsAlreadyInSet);
						if (!bIsAlreadyInSet)
						{
							if (PlayInEditorMode == 0)
							{
								UE_LOG(LogStaticMesh, Display, TEXT("Waiting on static mesh %s being ready before playing"), *Component->GetStaticMesh()->GetFullName());
							}
							else
							{
								UE_LOG(LogStaticMesh, Display, TEXT("Waiting on static mesh %s being ready because it affects collision/navigation"), *Component->GetStaticMesh()->GetFullName());
							}
						}
					}
				}
			}

			if (StaticMeshToCompile.Num())
			{
				FinishCompilation(StaticMeshToCompile.Array());
			}
		}
	}
}

void FStaticMeshCompilingManager::FinishAllCompilation()
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::FinishAllCompilation)

	if (GetNumRemainingMeshes())
	{
		TArray<UStaticMesh*> PendingStaticMeshes;
		PendingStaticMeshes.Reserve(GetNumRemainingMeshes());

		for (TWeakObjectPtr<UStaticMesh>& StaticMesh : RegisteredStaticMesh)
		{
			if (StaticMesh.IsValid())
			{
				PendingStaticMeshes.Add(StaticMesh.Get());
			}
		}

		FinishCompilation(PendingStaticMeshes);
	}
}

void FStaticMeshCompilingManager::Reschedule()
{
	using namespace StaticMeshCompilingManagerImpl;
	if (RegisteredStaticMesh.Num() > 1)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::Reschedule);

		FObjectCacheContextScope ObjectCacheScope;
		TSet<UStaticMesh*> StaticMeshesToProcess;
		for (TWeakObjectPtr<UStaticMesh>& StaticMesh : RegisteredStaticMesh)
		{
			if (StaticMesh.IsValid())
			{
				StaticMeshesToProcess.Add(StaticMesh.Get());
			}
		}

		TMap<UStaticMesh*, float> DistanceToEditingViewport;
		{
			if (StaticMeshesToProcess.Num() > 1)
			{
				const int32 NumViews = IStreamingManager::Get().GetNumViews();
			
				const FStreamingViewInfo* BestViewInfo = nullptr;
				for (int32 ViewIndex = 0; ViewIndex < NumViews; ++ViewIndex)
				{
					const FStreamingViewInfo& ViewInfo = IStreamingManager::Get().GetViewInformation(ViewIndex);
					if (BestViewInfo == nullptr || ViewInfo.BoostFactor > BestViewInfo->BoostFactor)
					{
						BestViewInfo = &ViewInfo;
					}
				}

				const FVector Location = BestViewInfo ? BestViewInfo->ViewOrigin : FVector(0.0f, 0.0f, 0.0f);
				{
					for (const UStaticMeshComponent* StaticMeshComponent : ObjectCacheScope.GetContext().GetStaticMeshComponents())
					{
						if (StaticMeshComponent->IsRegistered() && StaticMeshesToProcess.Contains(StaticMeshComponent->GetStaticMesh()))
						{
							FVector ComponentLocation = StaticMeshComponent->GetComponentLocation();
							float& Dist = DistanceToEditingViewport.FindOrAdd(StaticMeshComponent->GetStaticMesh(), FLT_MAX);
							float ComponentDist = Location.Dist(ComponentLocation, Location);
							if (ComponentDist < Dist)
							{
								Dist = ComponentDist;
							}
						}
					}
				}
			}

			if (DistanceToEditingViewport.Num())
			{
				StaticMeshesToProcess.Sort(
					[&DistanceToEditingViewport](const UStaticMesh& Lhs, const UStaticMesh& Rhs)
					{
						const float* ResultA = DistanceToEditingViewport.Find(&Lhs);
						const float* ResultB = DistanceToEditingViewport.Find(&Rhs);

						const float FinalResultA = ResultA ? *ResultA : FLT_MAX;
						const float FinalResultB = ResultB ? *ResultB : FLT_MAX;
						return FinalResultA < FinalResultB;
					}
				);
			}

			if (DistanceToEditingViewport.Num())
			{
				FQueuedThreadPoolDynamicWrapper* QueuedThreadPool = (FQueuedThreadPoolDynamicWrapper*)GetThreadPool();
				QueuedThreadPool->Sort(
					[&DistanceToEditingViewport](const IQueuedWork* Lhs, const IQueuedWork* Rhs)
					{
						const FStaticMeshAsyncBuildTask* TaskA = (const FStaticMeshAsyncBuildTask*)Lhs;
						const FStaticMeshAsyncBuildTask* TaskB = (const FStaticMeshAsyncBuildTask*)Rhs;

						const float* ResultA = DistanceToEditingViewport.Find(TaskA->StaticMesh);
						const float* ResultB = DistanceToEditingViewport.Find(TaskB->StaticMesh);

						const float FinalResultA = ResultA ? *ResultA : FLT_MAX;
						const float FinalResultB = ResultB ? *ResultB : FLT_MAX;
						return FinalResultA < FinalResultB;
					}
				);
			}
		}
	}
}

void FStaticMeshCompilingManager::ProcessStaticMeshes(bool bLimitExecutionTime, int32 MinBatchSize)
{
	using namespace StaticMeshCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FStaticMeshCompilingManager::ProcessStaticMeshes);
	const int32 NumRemainingMeshes = GetNumRemainingMeshes();
	// Spread out the load over multiple frames but if too many meshes, convergence is more important than frame time
	const int32 MaxMeshUpdatesPerFrame = bLimitExecutionTime ? FMath::Max(64, NumRemainingMeshes / 10) : INT32_MAX;

	FObjectCacheContextScope ObjectCacheScope;
	if (NumRemainingMeshes && NumRemainingMeshes >= MinBatchSize)
	{
		TSet<UStaticMesh*> StaticMeshesToProcess;
		for (TWeakObjectPtr<UStaticMesh>& StaticMesh : RegisteredStaticMesh)
		{
			if (StaticMesh.IsValid())
			{
				StaticMeshesToProcess.Add(StaticMesh.Get());
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedStaticMeshes);

			const double TickStartTime = FPlatformTime::Seconds();

			TSet<TWeakObjectPtr<UStaticMesh>> StaticMeshesToPostpone;
			TArray<UStaticMesh*> ProcessedStaticMeshes;
			if (StaticMeshesToProcess.Num())
			{
				for (UStaticMesh* StaticMesh : StaticMeshesToProcess)
				{
					const bool bHasMeshUpdateLeft = ProcessedStaticMeshes.Num() <= MaxMeshUpdatesPerFrame;
					if (bHasMeshUpdateLeft && StaticMesh->IsAsyncTaskComplete())
					{
						PostCompilation(StaticMesh);
						ProcessedStaticMeshes.Add(StaticMesh);
					}
					else
					{
						StaticMeshesToPostpone.Emplace(StaticMesh);
					}
				}
			}

			RegisteredStaticMesh = MoveTemp(StaticMeshesToPostpone);

			PostCompilation(ProcessedStaticMeshes);
		}
	}
}

void FStaticMeshCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;
	FinishCompilationsForGame();

	Reschedule();

	ProcessStaticMeshes(bLimitExecutionTime);

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // #if WITH_EDITOR
