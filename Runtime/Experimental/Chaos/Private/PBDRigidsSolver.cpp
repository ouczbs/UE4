// Copyright Epic Games, Inc. All Rights Reserved.

#include "PBDRigidsSolver.h"

#include "Async/AsyncWork.h"
#include "Chaos/ChaosArchive.h"
#include "Chaos/PBDCollisionConstraintsUtil.h"
#include "Chaos/Utilities.h"
#include "Chaos/ChaosDebugDraw.h"
#include "ChaosStats.h"
#include "ChaosSolversModule.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsProxy/SkeletalMeshPhysicsProxy.h"
#include "PhysicsProxy/StaticMeshPhysicsProxy.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "EventDefaults.h"
#include "EventsData.h"
#include "RewindData.h"
#include "ChaosSolverConfiguration.h"
#include "Chaos/PullPhysicsDataImp.h"
#include "Chaos/PhysicsSolverBaseImpl.h"

#include "ProfilingDebugging/CsvProfiler.h"

//PRAGMA_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY_STATIC(LogPBDRigidsSolver, Log, All);

// Stat Counters
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumDisabledParticles"), STAT_ChaosCounter_NumDisabledParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumParticles"), STAT_ChaosCounter_NumParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumDynamicParticles"), STAT_ChaosCounter_NumDynamicParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumActiveDynamicParticles"), STAT_ChaosCounter_NumActiveDynamicParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumKinematicParticles"), STAT_ChaosCounter_NumKinematicParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumStaticParticles"), STAT_ChaosCounter_NumStaticParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumGeomCollParticles"), STAT_ChaosCounter_NumGeometryCollectionParticles, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumIslands"), STAT_ChaosCounter_NumIslands, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumContacts"), STAT_ChaosCounter_NumContacts, STATGROUP_ChaosCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumJoints"), STAT_ChaosCounter_NumJoints, STATGROUP_ChaosCounters);

CSV_DEFINE_CATEGORY(ChaosCounters, true);


// DebugDraw CVars
#if CHAOS_DEBUG_DRAW

// Must be 0 when checked in...
#define CHAOS_SOLVER_ENABLE_DEBUG_DRAW 0

int32 ChaosSolverDebugDrawShapes = CHAOS_SOLVER_ENABLE_DEBUG_DRAW;
int32 ChaosSolverDebugDrawCollisions = CHAOS_SOLVER_ENABLE_DEBUG_DRAW;
int32 ChaosSolverDebugDrawBounds = 0;
int32 ChaosSolverDrawTransforms = 0;
int32 ChaosSolverDrawIslands = 0;
int32 ChaosSolverDrawShapesShowStatic = 1;
int32 ChaosSolverDrawShapesShowKinematic = 1;
int32 ChaosSolverDrawShapesShowDynamic = 1;
FAutoConsoleVariableRef CVarChaosSolverDrawShapes(TEXT("p.Chaos.Solver.DebugDrawShapes"), ChaosSolverDebugDrawShapes, TEXT("Draw Shapes (0 = never; 1 = end of frame)."));
FAutoConsoleVariableRef CVarChaosSolverDrawCollisions(TEXT("p.Chaos.Solver.DebugDrawCollisions"), ChaosSolverDebugDrawCollisions, TEXT("Draw Collisions (0 = never; 1 = end of frame)."));
FAutoConsoleVariableRef CVarChaosSolverDrawBounds(TEXT("p.Chaos.Solver.DebugDrawBounds"), ChaosSolverDebugDrawBounds, TEXT("Draw bounding volumes inside the broadphase (0 = never; 1 = end of frame)."));
FAutoConsoleVariableRef CVarChaosSolverDrawTransforms(TEXT("p.Chaos.Solver.DebugDrawTransforms"), ChaosSolverDrawTransforms, TEXT("Draw particle transforms (0 = never; 1 = end of frame)."));
FAutoConsoleVariableRef CVarChaosSolverDrawIslands(TEXT("p.Chaos.Solver.DebugDrawIslands"), ChaosSolverDrawIslands, TEXT("Draw solver islands (0 = never; 1 = end of frame)."));
FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesStatic(TEXT("p.Chaos.Solver.DebugDraw.ShowStatics"), ChaosSolverDrawShapesShowStatic, TEXT("If DebugDrawShapes is enabled, whether to show static objects"));
FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesKinematic(TEXT("p.Chaos.Solver.DebugDraw.ShowKinematics"), ChaosSolverDrawShapesShowKinematic, TEXT("If DebugDrawShapes is enabled, whether to show kinematic objects"));
FAutoConsoleVariableRef CVarChaosSolverDrawShapesShapesDynamic(TEXT("p.Chaos.Solver.DebugDraw.ShowDynamics"), ChaosSolverDrawShapesShowDynamic, TEXT("If DebugDrawShapes is enabled, whether to show dynamic objects"));

Chaos::DebugDraw::FChaosDebugDrawSettings ChaosSolverDebugDebugDrawSettings(
	/* ArrowSize =					*/ 10.0f,
	/* BodyAxisLen =				*/ 30.0f,
	/* ContactLen =					*/ 30.0f,
	/* ContactWidth =				*/ 6.0f,
	/* ContactPhiWidth =			*/ 0.0f,
	/* ContactOwnerWidth =			*/ 0.0f,
	/* ConstraintAxisLen =			*/ 30.0f,
	/* JointComSize =				*/ 2.0f,
	/* LineThickness =				*/ 1.0f,
	/* DrawScale =					*/ 1.0f,
	/* FontHeight =					*/ 10.0f,
	/* FontScale =					*/ 1.5f,
	/* ShapeThicknesScale =			*/ 1.0f,
	/* PointSize =					*/ 5.0f,
	/* VelScale =					*/ 0.0f,
	/* AngVelScale =				*/ 0.0f,
	/* ImpulseScale =				*/ 0.0f,
	/* InertiaScale =				*/ 1.0f,
	/* DrawPriority =				*/ 10.0f,
	/* bShowSimple =				*/ true,
	/* bShowComplex =				*/ false,
	/* bInShowLevelSetCollision =	*/ true
);
FAutoConsoleVariableRef CVarChaosSolverArrowSize(TEXT("p.Chaos.Solver.DebugDraw.ArrowSize"), ChaosSolverDebugDebugDrawSettings.ArrowSize, TEXT("ArrowSize."));
FAutoConsoleVariableRef CVarChaosSolverBodyAxisLen(TEXT("p.Chaos.Solver.DebugDraw.BodyAxisLen"), ChaosSolverDebugDebugDrawSettings.BodyAxisLen, TEXT("BodyAxisLen."));
FAutoConsoleVariableRef CVarChaosSolverContactLen(TEXT("p.Chaos.Solver.DebugDraw.ContactLen"), ChaosSolverDebugDebugDrawSettings.ContactLen, TEXT("ContactLen."));
FAutoConsoleVariableRef CVarChaosSolverContactWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactWidth"), ChaosSolverDebugDebugDrawSettings.ContactWidth, TEXT("ContactWidth."));
FAutoConsoleVariableRef CVarChaosSolverContactPhiWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactPhiWidth"), ChaosSolverDebugDebugDrawSettings.ContactPhiWidth, TEXT("ContactPhiWidth."));
FAutoConsoleVariableRef CVarChaosSolverContactOwnerWidth(TEXT("p.Chaos.Solver.DebugDraw.ContactOwnerWidth"), ChaosSolverDebugDebugDrawSettings.ContactOwnerWidth, TEXT("ContactOwnerWidth."));
FAutoConsoleVariableRef CVarChaosSolverConstraintAxisLen(TEXT("p.Chaos.Solver.DebugDraw.ConstraintAxisLen"), ChaosSolverDebugDebugDrawSettings.ConstraintAxisLen, TEXT("ConstraintAxisLen."));
FAutoConsoleVariableRef CVarChaosSolverLineThickness(TEXT("p.Chaos.Solver.DebugDraw.LineThickness"), ChaosSolverDebugDebugDrawSettings.LineThickness, TEXT("LineThickness."));
FAutoConsoleVariableRef CVarChaosSolverLineShapeThickness(TEXT("p.Chaos.Solver.DebugDraw.ShapeLineThicknessScale"), ChaosSolverDebugDebugDrawSettings.ShapeThicknesScale, TEXT("Shape lineThickness multiplier."));
FAutoConsoleVariableRef CVarChaosSolverPointSize(TEXT("p.Chaos.Solver.DebugDraw.PointSize"), ChaosSolverDebugDebugDrawSettings.PointSize, TEXT("Point size."));
FAutoConsoleVariableRef CVarChaosSolverVelScale(TEXT("p.Chaos.Solver.DebugDraw.VelScale"), ChaosSolverDebugDebugDrawSettings.VelScale, TEXT("If >0 show velocity when drawing particle transforms."));
FAutoConsoleVariableRef CVarChaosSolverAngVelScale(TEXT("p.Chaos.Solver.DebugDraw.AngVelScale"), ChaosSolverDebugDebugDrawSettings.AngVelScale, TEXT("If >0 show angular velocity when drawing particle transforms."));
FAutoConsoleVariableRef CVarChaosSolverImpulseScale(TEXT("p.Chaos.Solver.DebugDraw.ImpulseScale"), ChaosSolverDebugDebugDrawSettings.ImpulseScale, TEXT("If >0 show impulses when drawing collisions."));
FAutoConsoleVariableRef CVarChaosSolverInertiaScale(TEXT("p.Chaos.Solver.DebugDraw.InertiaScale"), ChaosSolverDebugDebugDrawSettings.InertiaScale, TEXT("When DebugDrawTransforms is enabled, show the mass-normalized inertia matrix scaled by this amount."));
FAutoConsoleVariableRef CVarChaosSolverScale(TEXT("p.Chaos.Solver.DebugDraw.Scale"), ChaosSolverDebugDebugDrawSettings.DrawScale, TEXT("Scale applied to all Chaos Debug Draw line lengths etc."));
FAutoConsoleVariableRef CVarChaosSolverShowSimple(TEXT("p.Chaos.Solver.DebugDraw.ShowSimple"), ChaosSolverDebugDebugDrawSettings.bShowSimpleCollision, TEXT("Whether to show simple collision is shape drawing is enabled"));
FAutoConsoleVariableRef CVarChaosSolverShowComplex(TEXT("p.Chaos.Solver.DebugDraw.ShowComplex"), ChaosSolverDebugDebugDrawSettings.bShowComplexCollision, TEXT("Whether to show complex collision is shape drawing is enabled"));
FAutoConsoleVariableRef CVarChaosSolverShowLevelSet(TEXT("p.Chaos.Solver.DebugDraw.ShowLevelSet"), ChaosSolverDebugDebugDrawSettings.bShowLevelSetCollision, TEXT("Whether to show levelset collision is shape drawing is enabled"));

#endif

bool ChaosSolverUseParticlePool = true;
FAutoConsoleVariableRef CVarChaosSolverUseParticlePool(TEXT("p.Chaos.Solver.UseParticlePool"), ChaosSolverUseParticlePool, TEXT("Whether or not to use dirty particle pool (Optim)"));

int32 ChaosSolverParticlePoolNumFrameUntilShrink = 30;
FAutoConsoleVariableRef CVarChaosSolverParticlePoolNumFrameUntilShrink(TEXT("p.Chaos.Solver.ParticlePoolNumFrameUntilShrink"), ChaosSolverParticlePoolNumFrameUntilShrink, TEXT("Num Frame until we can potentially shrink the pool"));

// Iteration count cvars
// These override the engine config if >= 0

int32 ChaosSolverIterations = -1;
FAutoConsoleVariableRef CVarChaosSolverIterations(TEXT("p.Chaos.Solver.Iterations"), ChaosSolverIterations, TEXT("Override umber of solver iterations (-1 to use config)"));

int32 ChaosSolverCollisionIterations = -1;
FAutoConsoleVariableRef CVarChaosSolverCollisionIterations(TEXT("p.Chaos.Solver.Collision.Iterations"), ChaosSolverCollisionIterations, TEXT("Override number of collision iterations per solver iteration (-1 to use config)"));

int32 ChaosSolverPushOutIterations = -1;
FAutoConsoleVariableRef CVarChaosSolverPushOutIterations(TEXT("p.Chaos.Solver.PushoutIterations"), ChaosSolverPushOutIterations, TEXT("Override number of solver pushout iterations (-1 to use config)"));

int32 ChaosSolverCollisionPushOutIterations = -1;
FAutoConsoleVariableRef CVarChaosSolverCollisionPushOutIterations(TEXT("p.Chaos.Solver.Collision.PushOutIterations"), ChaosSolverCollisionPushOutIterations, TEXT("Override number of collision iterations per solver iteration (-1 to use config)"));

int32 ChaosSolverJointPairIterations = -1;
FAutoConsoleVariableRef CVarChaosSolverJointPairIterations(TEXT("p.Chaos.Solver.Joint.PairIterations"), ChaosSolverJointPairIterations, TEXT("Override number of iterations per joint pair during a solver iteration (-1 to use config)"));

int32 ChaosSolverJointPushOutPairIterations = -1;
FAutoConsoleVariableRef CVarChaosSolverJointPushOutPairIterations(TEXT("p.Chaos.Solver.Joint.PushOutPairIterations"), ChaosSolverJointPushOutPairIterations, TEXT("Override number of push out iterations per joint during a solver iteration (-1 to use config)"));


// Collision detection cvars
// These override the engine config if >= 0
float ChaosSolverCullDistance = -1.0f;
FAutoConsoleVariableRef CVarChaosSolverCullDistance(TEXT("p.Chaos.Solver.Collision.CullDistance"), ChaosSolverCullDistance, TEXT("Override cull distance (if >= 0)"));

int32 ChaosSolverCleanupCommandsOnDestruction = 1;
FAutoConsoleVariableRef CVarChaosSolverCleanupCommandsOnDestruction(TEXT("p.Chaos.Solver.CleanupCommandsOnDestruction"), ChaosSolverCleanupCommandsOnDestruction, TEXT("Whether or not to run internal command queue cleanup on solver destruction (0 = no cleanup, >0 = cleanup all commands)"));

int32 ChaosSolverCollisionDeferNarrowPhase = 0;
FAutoConsoleVariableRef CVarChaosSolverCollisionDeferNarrowPhase(TEXT("p.Chaos.Solver.Collision.DeferNarrowPhase"), ChaosSolverCollisionDeferNarrowPhase, TEXT("Create contacts for all broadphase pairs, perform NarrowPhase later."));

// Allow one-shot or incremental manifolds where supported (which depends on shape pair types)
int32 ChaosSolverCollisionUseManifolds = 1;
FAutoConsoleVariableRef CVarChaosSolverCollisionUseManifolds(TEXT("p.Chaos.Solver.Collision.UseManifolds"), ChaosSolverCollisionUseManifolds, TEXT("Enable/Disable use of manifolds in collision."));

int32 ChaosVisualDebuggerEnable = 1;
FAutoConsoleVariableRef CVarChaosVisualDebuggerEnable(TEXT("p.Chaos.VisualDebuggerEnable"), ChaosVisualDebuggerEnable, TEXT("Enable/Disable pushing/saving data to the visual debugger"));

namespace Chaos
{

	template <typename Traits>
	class AdvanceOneTimeStepTask : public FNonAbandonableTask
	{
		friend class FAutoDeleteAsyncTask<AdvanceOneTimeStepTask>;
	public:
		AdvanceOneTimeStepTask(
			TPBDRigidsSolver<Traits>* Scene
			, const float DeltaTime
			, const FSubStepInfo& SubStepInfo)
			: MSolver(Scene)
			, MDeltaTime(DeltaTime)
			, MSubStepInfo(SubStepInfo)
		{
			UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("AdvanceOneTimeStepTask::AdvanceOneTimeStepTask()"));
		}

		void DoWork()
		{
			LLM_SCOPE(ELLMTag::Chaos);
			UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("AdvanceOneTimeStepTask::DoWork()"));
			MSolver->StartingSceneSimulation();

			if(MDeltaTime > 0)	//if delta time is 0 we are flushing data, user callbacks should not be triggered because there is no sim
			{
				MSolver->ApplyCallbacks_Internal(MSolver->GetSolverTime(), MDeltaTime);	//question: is SolverTime the right thing to pass in here?
			}
			MSolver->GetEvolution()->GetRigidClustering().ResetAllClusterBreakings();

			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateParams);
				Chaos::TPBDPositionConstraints<float, 3> PositionTarget; // Dummy for now
				TMap<int32, int32> TargetedParticles;
				{
					MSolver->FieldParameterUpdateCallback(PositionTarget, TargetedParticles);
				}

				for (FGeometryCollectionPhysicsProxy* GeoclObj : MSolver->GetGeometryCollectionPhysicsProxies_Internal())
				{
					GeoclObj->FieldParameterUpdateCallback(MSolver);
				}

				MSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().ProcessPendingQueues();
			}

			{
				//SCOPE_CYCLE_COUNTER(STAT_BeginFrame);
				//MSolver->StartFrameCallback(MDeltaTime, MSolver->GetSolverTime());
			}


			if(FRewindData* RewindData = MSolver->GetRewindData())
			{
				RewindData->AdvanceFrame(MDeltaTime,[Evolution = MSolver->GetEvolution()]()
				{
					return Evolution->CreateExternalResimCache();
				});
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_EvolutionAndKinematicUpdate);

				// This outer loop can potentially cause the system to lose energy over integration
				// in a couple of different cases.
				//
				// * If we have a timestep that's smaller than MinDeltaTime, then we just won't step.
				//   Yes, we'll lose some teeny amount of energy, but we'll avoid 1/dt issues.
				//
				// * If we have used all of our substeps but still have time remaining, then some
				//   energy will be lost.
				const float MinDeltaTime = MSolver->GetMinDeltaTime_External();
				const float MaxDeltaTime = MSolver->GetMaxDeltaTime_External();
				int32 StepsRemaining = MSubStepInfo.bSolverSubstepped ? 1 : MSolver->GetMaxSubSteps_External();
				float TimeRemaining = MDeltaTime;
				bool bFirstStep = true;
				while (StepsRemaining > 0 && TimeRemaining > MinDeltaTime)
				{
					--StepsRemaining;
					const float DeltaTime = MaxDeltaTime > 0.f ? FMath::Min(TimeRemaining, MaxDeltaTime) : TimeRemaining;
					TimeRemaining -= DeltaTime;

					{
						MSolver->FieldForcesUpdateCallback();
					}

					for (FGeometryCollectionPhysicsProxy* GeoCollectionObj : MSolver->GetGeometryCollectionPhysicsProxies_Internal())
					{
						GeoCollectionObj->FieldForcesUpdateCallback(MSolver);
					}

					if(FRewindData* RewindData = MSolver->GetRewindData())
					{
						//todo: make this work with sub-stepping
						MSolver->GetEvolution()->SetCurrentStepResimCache(bFirstStep ? RewindData->GetCurrentStepResimCache() : nullptr);
					}

					MSolver->GetEvolution()->AdvanceOneTimeStep(DeltaTime, MSubStepInfo);
					MSolver->PostEvolutionVDBPush();
					bFirstStep = false;
				}

				// Editor will tick with 0 DT, this will guarantee acceleration structure is still processing even if we don't advance evolution.
				if (MDeltaTime < MinDeltaTime)
				{
					MSolver->GetEvolution()->ComputeIntermediateSpatialAcceleration();
				}


#if CHAOS_CHECKED
				// If time remains, then log why we have lost energy over the timestep.
				if (TimeRemaining > 0.f)
				{
					if (StepsRemaining == 0)
					{
						UE_LOG(LogPBDRigidsSolver, Warning, TEXT("AdvanceOneTimeStepTask::DoWork() - Energy lost over %fs due to too many substeps over large timestep"), TimeRemaining);
					}
					else
					{
						UE_LOG(LogPBDRigidsSolver, Warning, TEXT("AdvanceOneTimeStepTask::DoWork() - Energy lost over %fs due to small timestep remainder"), TimeRemaining);
					}
				}
#endif
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_EventDataGathering);
				{
					SCOPE_CYCLE_COUNTER(STAT_FillProducerData);
					MSolver->GetEventManager()->FillProducerData(MSolver);
				}
				{
					SCOPE_CYCLE_COUNTER(STAT_FlipBuffersIfRequired);
					MSolver->GetEventManager()->FlipBuffersIfRequired();
				}
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_EndFrame);
				MSolver->GetEvolution()->EndFrame(MDeltaTime);
			}

			if(FRewindData* RewindData = MSolver->GetRewindData())
			{
				RewindData->FinishFrame();
			}

			MSolver->FinalizeCallbackData_Internal();

			MSolver->GetSolverTime() += MDeltaTime;
			MSolver->GetCurrentFrame()++;
			MSolver->PostTickDebugDraw(MDeltaTime);

			MSolver->UpdateStatCounters();

			//Editor ticks with 0 dt. We don't want to buffer any dirty data from this since it won't be consumed
			//TODO: handle this more gracefully
			if(MDeltaTime > 0)
			{
				MSolver->CompleteSceneSimulation();
			}
		}

	protected:

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(AdvanceOneTimeStepTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		TPBDRigidsSolver<Traits>* MSolver;
		float MDeltaTime;
		FSubStepInfo MSubStepInfo;
		TSharedPtr<FCriticalSection> PrevLock, CurrentLock;
		TSharedPtr<FEvent> PrevEvent, CurrentEvent;
	};

	template <typename Traits>
	TPBDRigidsSolver<Traits>::TPBDRigidsSolver(const EMultiBufferMode BufferingModeIn, UObject* InOwner)
		: Super(BufferingModeIn, BufferingModeIn == EMultiBufferMode::Single ? EThreadingModeTemp::SingleThread : EThreadingModeTemp::TaskGraph, InOwner, TraitToIdx<Traits>())
		, CurrentFrame(0)
		, MTime(0.0)
		, MLastDt(0.0)
		, bHasFloor(true)
		, bIsFloorAnalytic(false)
		, FloorHeight(0.f)
		, MEvolution(new FPBDRigidsEvolution(Particles, SimMaterials, &ContactModifiers, BufferingModeIn == Chaos::EMultiBufferMode::Single))
		, MEventManager(new TEventManager<Traits>(BufferingModeIn))
		, MSolverEventFilters(new FSolverEventFilters())
		, MDirtyParticlesBuffer(new FDirtyParticlesBuffer(BufferingModeIn, BufferingModeIn == Chaos::EMultiBufferMode::Single))
		, MCurrentLock(new FCriticalSection())
		, bUseCollisionResimCache(false)
		, JointConstraintRule(JointConstraints)
		, SuspensionConstraintRule(SuspensionConstraints)
		, PerSolverField(nullptr)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::PBDRigidsSolver()"));
		Reset();
		MEvolution->AddConstraintRule(&JointConstraintRule);
		MEvolution->AddConstraintRule(&SuspensionConstraintRule);

		MEvolution->SetInternalParticleInitilizationFunction(
			[this](const Chaos::TGeometryParticleHandle<float, 3>* OldParticle, Chaos::TGeometryParticleHandle<float, 3>* NewParticle) {
				IPhysicsProxyBase* Proxy = const_cast<IPhysicsProxyBase*>(OldParticle->PhysicsProxy());
				if (Chaos::TPBDRigidClusteredParticleHandle<float, 3>* NewClusteredParticle = NewParticle->CastToClustered())
				{
					NewClusteredParticle->AddPhysicsProxy(Proxy);
				}
				NewParticle->SetPhysicsProxy(Proxy);
			});
		
		JointConstraints.SetUpdateVelocityInApplyConstraints(true);
	}

	float MaxBoundsForTree = 10000;
	FAutoConsoleVariableRef CVarMaxBoundsForTree(
		TEXT("p.MaxBoundsForTree"),
		MaxBoundsForTree,
		TEXT("The max bounds before moving object into a large objects structure. Only applies on object registration")
		TEXT(""),
		ECVF_Default);

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::RegisterObject(FSingleParticlePhysicsProxy* Proxy)
	{
		LLM_SCOPE(ELLMTag::Chaos);

		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("TPBDRigidsSolver::RegisterObject()"));
		auto& RigidBody_External = Proxy->GetGameThreadAPI();

		if (RigidBody_External.Geometry() && RigidBody_External.Geometry()->HasBoundingBox() && RigidBody_External.Geometry()->BoundingBox().Extents().Max() >= MaxBoundsForTree)
		{
			RigidBody_External.SetSpatialIdx(FSpatialAccelerationIdx{ 1,0 });
		}
		if (!ensure(Proxy->GetParticle_LowLevel()->IsParticleValid()))
		{
			return;
		}

		// NOTE: Do we really need these lists of proxies if we can just
		// access them through the GTParticles list?
		

		RigidBody_External.SetUniqueIdx(GetEvolution()->GenerateUniqueIdx());
		TrackGTParticle_External(*Proxy->GetParticle_LowLevel());	//todo: remove this
		//Chaos::FParticlePropertiesData& RemoteParticleData = *DirtyPropertiesManager->AccessProducerBuffer()->NewRemoteParticleProperties();
		//Chaos::FShapeRemoteDataContainer& RemoteShapeContainer = *DirtyPropertiesManager->AccessProducerBuffer()->NewRemoteShapeContainer();

		Proxy->SetSolver(this);
		Proxy->GetParticle_LowLevel()->SetProxy(Proxy);
		AddDirtyProxy(Proxy);

		UpdateParticleInAccelerationStructure_External(Proxy->GetParticle_LowLevel(), /*bDelete=*/false);
	}

	int32 LogCorruptMap = 0;
	FAutoConsoleVariableRef CVarLogCorruptMap(TEXT("p.LogCorruptMap"), LogCorruptMap, TEXT(""));


	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UnregisterObject(FSingleParticlePhysicsProxy* Proxy)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("TPBDRigidsSolver::UnregisterObject()"));

		ClearGTParticle_External(*Proxy->GetParticle_LowLevel());	//todo: remove this

		UpdateParticleInAccelerationStructure_External(Proxy->GetParticle_LowLevel(), /*bDelete=*/true);

		// remove the proxy from the invalidation list
		RemoveDirtyProxy(Proxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		Proxy->SetSyncTimestamp(MarshallingManager.GetExternalTimestamp_External());

		// Null out the particle's proxy pointer
		Proxy->GetParticle_LowLevel()->SetProxy(nullptr);	//todo: use TUniquePtr for better ownership

		// Remove the proxy from the GT proxy map


		Chaos::FIgnoreCollisionManager& CollisionManager = GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager();
		{
			int32 ExternalTimestamp = GetMarshallingManager().GetExternalTimestamp_External();
			Chaos::FIgnoreCollisionManager::FDeactivationSet& PendingMap = CollisionManager.GetPendingDeactivationsForGameThread(ExternalTimestamp);
				PendingMap.Add(Proxy->GetGameThreadAPI().UniqueIdx());
			}

		// Enqueue a command to remove the particle and delete the proxy
		EnqueueCommandImmediate([Proxy, this]()
		{
			UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("TPBDRigidsSolver::UnregisterObject() ~ Dequeue"));

			// Generally need to remove stale events for particles that no longer exist
			GetEventManager()->template ClearEvents<FCollisionEventData>(EEventType::Collision, [Proxy]
			(FCollisionEventData& EventDataInOut)
			{
				Chaos::FCollisionDataArray const& CollisionData = EventDataInOut.CollisionData.AllCollisionsArray;
				if (CollisionData.Num() > 0)
				{
					check(Proxy);
					TArray<int32> const* const CollisionIndices = EventDataInOut.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.Find(Proxy);
					if (CollisionIndices)
					{
						EventDataInOut.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.Remove(Proxy);
					}
				}

			});

			// Get the physics thread-handle from the proxy, and then delete the proxy.
			//
			// NOTE: We have to delete the proxy from its derived version, because the
			// base destructor is protected. This makes everything just a bit uglier,
			// maybe that extra safety is not needed if we continue to contain all
			// references to proxy instances entirely in Chaos?
			TGeometryParticleHandle<float, 3>* Handle = Proxy->GetHandle_LowLevel();
			Proxy->SetHandle(nullptr);
			PendingDestroyPhysicsProxy.Add(Proxy);
			
			//If particle was created and destroyed before commands were enqueued just skip. I suspect we can skip entire lambda, but too much code to verify right now

			if(Handle)
			{
				// Remove from rewind data
				if(FRewindData* RewindData = GetRewindData())
				{
					RewindData->RemoveParticle(Handle->UniqueIdx());
				}
  
				// Use the handle to destroy the particle data
				GetEvolution()->DestroyParticle(Handle);
			}

		});

	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::RegisterObject(FGeometryCollectionPhysicsProxy* InProxy)
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("TPBDRigidsSolver::RegisterObject(FGeometryCollectionPhysicsProxy*)"));
		InProxy->SetSolver(this);
		InProxy->Initialize(GetEvolution());
		InProxy->NewData(); // Buffers data on the proxy.
		FParticlesType* InParticles = &GetParticles();

		// Finish registration on the physics thread...
		EnqueueCommandImmediate([InParticles, InProxy, this]()
		{
			UE_LOG(LogPBDRigidsSolver, Verbose, 
				TEXT("TPBDRigidsSolver::RegisterObject(FGeometryCollectionPhysicsProxy*)"));
			check(InParticles);
			InProxy->InitializeBodiesPT(this, *InParticles);
			GeometryCollectionPhysicsProxies_Internal.Add(InProxy);
		});
	}
	
	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UnregisterObject(FGeometryCollectionPhysicsProxy* InProxy)
	{
		check(InProxy);
		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		InProxy->SetSyncTimestamp(MarshallingManager.GetExternalTimestamp_External());

		RemoveDirtyProxy(InProxy);

		// Particles are removed from acceleration structure in FPhysScene_Chaos::RemoveObject.


		EnqueueCommandImmediate([InProxy, this]()
			{
				const TArray<Chaos::TPBDRigidClusteredParticleHandle<float, 3>*>& ParticleHandles = InProxy->GetSolverParticleHandles();
				
				GeometryCollectionPhysicsProxies_Internal.RemoveSingle(InProxy);
				InProxy->SyncBeforeDestroy();
				InProxy->OnRemoveFromSolver(this);
				InProxy->ResetDirtyIdx();
				PendingDestroyGeometryCollectionPhysicsProxy.Add(InProxy);
			});
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::RegisterObject(Chaos::FJointConstraint* GTConstraint)
	{
		FJointConstraintPhysicsProxy* JointProxy = new FJointConstraintPhysicsProxy(GTConstraint, nullptr);
		JointProxy->SetSolver(this);

		AddDirtyProxy(JointProxy);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UnregisterObject(Chaos::FJointConstraint* GTConstraint)
	{
		FJointConstraintPhysicsProxy* JointProxy = GTConstraint->GetProxy<FJointConstraintPhysicsProxy>();
		check(JointProxy);

		RemoveDirtyProxy(JointProxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		GTConstraint->GetProxy()->SetSyncTimestamp(MarshallingManager.GetExternalTimestamp_External());


		GTConstraint->SetProxy(static_cast<FJointConstraintPhysicsProxy*>(nullptr));

		GTConstraint->ReleaseKinematicEndPoint(this);

		FParticlesType* InParticles = &GetParticles();

		// Finish registration on the physics thread...
		EnqueueCommandImmediate([InParticles, JointProxy, this]()
			{
				JointProxy->DestroyOnPhysicsThread(this);
				JointConstraintPhysicsProxies_Internal.RemoveSingle(JointProxy);
				delete JointProxy;
			});
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::RegisterObject(Chaos::FSuspensionConstraint* GTConstraint)
	{
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = new FSuspensionConstraintPhysicsProxy(GTConstraint, nullptr);
		SuspensionProxy->SetSolver(this);

		AddDirtyProxy(SuspensionProxy);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UnregisterObject(Chaos::FSuspensionConstraint* GTConstraint)
	{
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = GTConstraint->GetProxy<FSuspensionConstraintPhysicsProxy>();
		check(SuspensionProxy);

		// mark proxy timestamp so we avoid trying to pull from sim after deletion
		SuspensionProxy->SetSyncTimestamp(MarshallingManager.GetExternalTimestamp_External());

		RemoveDirtyProxy(SuspensionProxy);

		GTConstraint->SetProxy(static_cast<FSuspensionConstraintPhysicsProxy*>(nullptr));

		FParticlesType* InParticles = &GetParticles();

		// Finish registration on the physics thread...
		EnqueueCommandImmediate([InParticles, SuspensionProxy, this]()
			{
				SuspensionProxy->DestroyOnPhysicsThread(this);
				delete SuspensionProxy;
			});
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::SetSuspensionTargetOnPhysicsThread(Chaos::FSuspensionConstraint* GTConstraint, const FVector& TargetPos, bool Enabled)
	{
		ensure(IsInPhysicsThreadContext());
		FSuspensionConstraintPhysicsProxy* SuspensionProxy = GTConstraint->GetProxy<FSuspensionConstraintPhysicsProxy>();
		check(SuspensionProxy);
		SuspensionProxy->UpdateTargetOnPhysicsThread(this, TargetPos, Enabled);
	}

	int32 RewindCaptureNumFrames = -1;
	FAutoConsoleVariableRef CVarRewindCaptureNumFrames(TEXT("p.RewindCaptureNumFrames"),RewindCaptureNumFrames,TEXT("The number of frames to capture rewind for. Requires restart of solver"));

	int32 UseResimCache = 0;
	FAutoConsoleVariableRef CVarUseResimCache(TEXT("p.UseResimCache"),UseResimCache,TEXT("Whether resim uses cache to skip work, requires recreating world to take effect"));

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::Reset()
	{
		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::Reset()"));

		MTime = 0;
		MLastDt = 0.0f;
		CurrentFrame = 0;
		SetMaxDeltaTime_External(1.0f);
		SetMinDeltaTime_External(SMALL_NUMBER);
		SetMaxSubSteps_External(1);
		MEvolution = TUniquePtr<FPBDRigidsEvolution>(new FPBDRigidsEvolution(Particles, SimMaterials, &ContactModifiers, BufferMode == EMultiBufferMode::Single)); 

		PerSolverField = MakeUnique<FPerSolverFieldSystem>();

		//todo: do we need this?
		//MarshallingManager.Reset();

		if(RewindCaptureNumFrames >= 0)
		{
			EnableRewindCapture(RewindCaptureNumFrames, bUseCollisionResimCache || !!UseResimCache);
		}

		MEvolution->SetCaptureRewindDataFunction([this](const TParticleView<TPBDRigidParticles<FReal,3>>& ActiveParticles)
		{
			FinalizeRewindData(ActiveParticles);
		});

		TEventDefaults<Traits>::RegisterSystemEvents(*GetEventManager());
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::ChangeBufferMode(Chaos::EMultiBufferMode InBufferMode)
	{
		// This seems unused inside the solver? #BH
		BufferMode = InBufferMode;

		SetThreadingMode_External(BufferMode == EMultiBufferMode::Single ? EThreadingModeTemp::SingleThread : EThreadingModeTemp::TaskGraph);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::StartingSceneSimulation()
	{
		LLM_SCOPE(ELLMTag::Chaos);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StartedSceneSimulation);

		GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().PopStorageData_Internal(GetEvolution()->LatestExternalTimestampConsumed_Internal);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::DestroyPendingProxies_Internal()
	{
		for (auto Proxy : PendingDestroyPhysicsProxy)
		{
			ensure(Proxy->GetHandle_LowLevel() == nullptr);	//should have already cleared this out
			delete Proxy;
		}
		PendingDestroyPhysicsProxy.Reset();

		bool bResetCollisionConstraints=false;
		for (auto Proxy : PendingDestroyGeometryCollectionPhysicsProxy)
		{
			//ensure(Proxy->GetHandle_LowLevel() == nullptr);	//should have already cleared this out
			MarshallingManager.GetCurrentPullData_Internal()->DirtyGeometryCollections.Reset();
			bResetCollisionConstraints = true;
			delete Proxy;
		}
		PendingDestroyGeometryCollectionPhysicsProxy.Reset();

		if(bResetCollisionConstraints)
			GetEvolution()->GetCollisionConstraints();
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::AdvanceSolverBy(const FReal DeltaTime, const FSubStepInfo& SubStepInfo)
	{
		const FReal StartSimTime = GetSolverTime();
		MEvolution->GetCollisionDetector().GetNarrowPhase().GetContext().bDeferUpdate = (ChaosSolverCollisionDeferNarrowPhase != 0);
		MEvolution->GetCollisionDetector().GetNarrowPhase().GetContext().bAllowManifolds = (ChaosSolverCollisionUseManifolds != 0);

		// Apply CVAR overrides if set
		{
			if (ChaosSolverIterations >= 0)
			{
				SetIterations(ChaosSolverIterations);
			}
			if (ChaosSolverCollisionIterations >= 0)
			{
				SetCollisionPairIterations(ChaosSolverCollisionIterations);
			}
			if (ChaosSolverPushOutIterations >= 0)
			{
				SetPushOutIterations(ChaosSolverPushOutIterations);
			}
			if (ChaosSolverCollisionPushOutIterations >= 0)
			{
				SetCollisionPushOutPairIterations(ChaosSolverCollisionPushOutIterations);
			}
			if (ChaosSolverJointPairIterations >= 0.0f)
			{
				SetJointPairIterations(ChaosSolverJointPairIterations);
			}
			if (ChaosSolverJointPushOutPairIterations >= 0.0f)
			{
				SetJointPushOutPairIterations(ChaosSolverJointPushOutPairIterations);
			}
			if (ChaosSolverCullDistance >= 0.0f)
			{
				SetCollisionCullDistance(ChaosSolverCullDistance);
			}
		}

		UE_LOG(LogPBDRigidsSolver, Verbose, TEXT("PBDRigidsSolver::Tick(%3.5f)"), DeltaTime);
		MLastDt = DeltaTime;
		EventPreSolve.Broadcast(DeltaTime);
		AdvanceOneTimeStepTask<Traits>(this, DeltaTime, SubStepInfo).DoWork();

		if(DeltaTime > 0)
		{
			//pass information back to external thread
			//we skip dt=0 case because sync data should be identical if dt = 0
			MarshallingManager.FinalizePullData_Internal(MEvolution->LatestExternalTimestampConsumed_Internal, StartSimTime, DeltaTime);
		}

		if(SubStepInfo.Step == SubStepInfo.NumSteps - 1)
		{
			//final step so we can destroy proxies
			DestroyPendingProxies_Internal();
		}
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::SetExternalTimestampConsumed_Internal(const int32 Timestamp)
	{
		MEvolution->LatestExternalTimestampConsumed_Internal = Timestamp;
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::SyncEvents_GameThread()
	{
		GetEventManager()->DispatchEvents();
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::PushPhysicsState(const FReal DeltaTime, const int32 NumSteps, const int32 NumExternalSteps)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PushPhysicsState);
		ensure(NumSteps > 0);
		ensure(NumExternalSteps > 0);
		//TODO: interpolate some data based on num steps

		FPushPhysicsData* PushData = MarshallingManager.GetProducerData_External();
		const FReal DynamicsWeight = FReal(1) / NumExternalSteps;
		FDirtySet* DirtyProxiesData = &PushData->DirtyProxiesDataBuffer;
		FDirtyPropertiesManager* Manager = &PushData->DirtyPropertiesManager;

		Manager->SetNumParticles(DirtyProxiesData->NumDirtyProxies());
		Manager->SetNumShapes(DirtyProxiesData->NumDirtyShapes());
		FShapeDirtyData* ShapeDirtyData = DirtyProxiesData->GetShapesDirtyData();
		auto ProcessProxyGT =[ShapeDirtyData, Manager, DirtyProxiesData](auto Proxy, int32 ParticleDataIdx, FDirtyProxy& DirtyProxy)
		{
			auto Particle = Proxy->GetParticle_LowLevel();
			Particle->SyncRemoteData(*Manager,ParticleDataIdx,DirtyProxy.ParticleData,DirtyProxy.ShapeDataIndices,ShapeDirtyData);
			Proxy->ClearAccumulatedData();
			Proxy->ResetDirtyIdx();
		};


		//todo: if we allocate remote data ahead of time we could go wide
		DirtyProxiesData->ParallelForEachProxy([&ProcessProxyGT, this, DynamicsWeight](int32 DataIdx, FDirtyProxy& Dirty)
		{
			switch(Dirty.Proxy->GetType())
			{
			case EPhysicsProxyType::SingleParticleProxy:
			{
				auto Proxy = static_cast<FSingleParticlePhysicsProxy*>(Dirty.Proxy);
				if(auto Rigid = Proxy->GetParticle_LowLevel()->CastToRigidParticle())
				{
					Rigid->ApplyDynamicsWeight(DynamicsWeight);
				}
				ProcessProxyGT(Proxy,DataIdx,Dirty);
				break;
			}
			case EPhysicsProxyType::GeometryCollectionType:
			{
				// Not invalid but doesn't currently use the remote data process
				break;
			}
			case EPhysicsProxyType::JointConstraintType:
			{
				auto Proxy = static_cast<FJointConstraintPhysicsProxy*>(Dirty.Proxy);
				Proxy->PushStateOnGameThread(this);
				break;
			}
			case EPhysicsProxyType::SuspensionConstraintType:
			{
				auto Proxy = static_cast<FSuspensionConstraintPhysicsProxy*>(Dirty.Proxy);
				Proxy->PushStateOnGameThread(this);
				break;
			}

			default:
			ensure(0 && TEXT("Unknown proxy type in physics solver."));
			}
		});

		GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().PushProducerStorageData_External(MarshallingManager.GetExternalTimestamp_External());

		MarshallingManager.Step_External(DeltaTime, NumSteps, GetSolverSubstep_External());
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::ProcessSinglePushedData_Internal(FPushPhysicsData& PushData)
	{
		FRewindData* RewindData = GetRewindData();

		FDirtySet* DirtyProxiesData = &PushData.DirtyProxiesDataBuffer;
		FDirtyPropertiesManager* Manager = &PushData.DirtyPropertiesManager;
		FShapeDirtyData* ShapeDirtyData = DirtyProxiesData->GetShapesDirtyData();

		auto ProcessProxyPT = [Manager,ShapeDirtyData,RewindData, this](auto& Proxy,int32 DataIdx,FDirtyProxy& Dirty,const auto& CreateHandleFunc)
		{
			const bool bIsNew = !Proxy->IsInitialized();
			if(bIsNew)
			{
				const auto* NonFrequentData = Dirty.ParticleData.FindNonFrequentData(*Manager,DataIdx);
				const FUniqueIdx* UniqueIdx = NonFrequentData ? &NonFrequentData->UniqueIdx() : nullptr;
				Proxy->SetHandle(CreateHandleFunc(UniqueIdx));

				auto Handle = Proxy->GetHandle_LowLevel();
				Handle->GTGeometryParticle() = Proxy->GetParticle_LowLevel();
			}

			if(RewindData)
			{
				//may want to remove branch by templatizing lambda
				if(RewindData->IsResim())
				{
					RewindData->PushGTDirtyData<true>(*Manager,DataIdx,Dirty);
				} else
				{
					RewindData->PushGTDirtyData<false>(*Manager,DataIdx,Dirty);
				}
			}

			Proxy->PushToPhysicsState(*Manager,DataIdx,Dirty,ShapeDirtyData,*GetEvolution());

			if(bIsNew)
			{
				auto Handle = Proxy->GetHandle_LowLevel();
				Handle->SetPhysicsProxy(Proxy);
				GetEvolution()->CreateParticle(Handle);
				Proxy->SetInitialized(true);
			}

			Dirty.Clear(*Manager,DataIdx,ShapeDirtyData);
		};

		if(RewindData)
		{
			RewindData->PrepareFrame(DirtyProxiesData->NumDirtyProxies());
		}

		//need to create new particle handles
		DirtyProxiesData->ForEachProxy([this,&ProcessProxyPT](int32 DataIdx,FDirtyProxy& Dirty)
		{
			switch(Dirty.Proxy->GetType())
			{
				case EPhysicsProxyType::SingleParticleProxy:
				{
					auto Proxy = static_cast<FSingleParticlePhysicsProxy*>(Dirty.Proxy);
					ProcessProxyPT(Proxy, DataIdx, Dirty, [this, &Dirty](const FUniqueIdx* UniqueIdx) -> TGeometryParticleHandle<FReal,3>*
					{
						switch (Dirty.ParticleData.GetParticleBufferType())
						{
							case EParticleType::Static: return Particles.CreateStaticParticles(1, UniqueIdx)[0];
							case EParticleType::Kinematic: return Particles.CreateKinematicParticles(1, UniqueIdx)[0];
							case EParticleType::Rigid: return Particles.CreateDynamicParticles(1, UniqueIdx)[0];
							default: check(false); return nullptr;
						}
					});
					break;
				}
			
				case EPhysicsProxyType::GeometryCollectionType:
				{
					// Currently no push needed for geometry collections and they handle the particle creation internally
					// #TODO This skips the rewind data push so GC will not be rewindable until resolved.
					Dirty.Proxy->ResetDirtyIdx();
					break;
				}
				case EPhysicsProxyType::JointConstraintType:
				case EPhysicsProxyType::SuspensionConstraintType:
				{
					// Pass until after all bodies are created. 
					break;
				}
				default:
				{
					ensure(0 && TEXT("Unknown proxy type in physics solver."));
					//Can't use, but we can still mark as "clean"
					Dirty.Proxy->ResetDirtyIdx();
				}
			}
		});

		//need to create new constraint handles
		DirtyProxiesData->ForEachProxy([this, &ProcessProxyPT](int32 DataIdx, FDirtyProxy& Dirty)
		{
			switch (Dirty.Proxy->GetType())
			{
			case EPhysicsProxyType::JointConstraintType:
			{
				auto JointProxy = static_cast<FJointConstraintPhysicsProxy*>(Dirty.Proxy);
				const bool bIsNew = !JointProxy->IsInitialized();
				if (bIsNew)
				{
					JointProxy->InitializeOnPhysicsThread(this);
					JointProxy->SetInitialized();
				}
				JointProxy->PushStateOnPhysicsThread(this);
				Dirty.Proxy->ResetDirtyIdx();
				break;
			}

			case EPhysicsProxyType::SuspensionConstraintType:
			{
				auto SuspensionProxy = static_cast<FSuspensionConstraintPhysicsProxy*>(Dirty.Proxy);
				const bool bIsNew = !SuspensionProxy->IsInitialized();
				if (bIsNew)
				{
					SuspensionProxy->InitializeOnPhysicsThread(this);
					SuspensionProxy->SetInitialized();
				}
				SuspensionProxy->PushStateOnPhysicsThread(this);
				Dirty.Proxy->ResetDirtyIdx();
				break;
			}

			}
		});

		{
			GetEvolution()->WakeIslands();
		}

		//MarshallingManager.FreeData_Internal(&PushData);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::ProcessPushedData_Internal(FPushPhysicsData& PushData)
	{
		//update callbacks
		SimCallbackObjects.Reserve(SimCallbackObjects.Num() + PushData.SimCallbackObjectsToAdd.Num());
		for(ISimCallbackObject* SimCallbackObject : PushData.SimCallbackObjectsToAdd)
		{
			SimCallbackObjects.Add(SimCallbackObject);
			if (SimCallbackObject->bContactModification)
			{
				ContactModifiers.Add(SimCallbackObject);
			}
		}

		//save any pending data for this particular interval
		for (const FSimCallbackInputAndObject& InputAndCallbackObj : PushData.SimCallbackInputs)
		{
			InputAndCallbackObj.CallbackObject->SetCurrentInput_Internal(InputAndCallbackObj.Input);
		}

		//remove any callbacks that are unregistered
		for (ISimCallbackObject* RemovedCallbackObject : PushData.SimCallbackObjectsToRemove)
		{
			RemovedCallbackObject->bPendingDelete = true;
		}

		for (int32 Idx = ContactModifiers.Num() - 1; Idx >= 0; --Idx)
		{
			ISimCallbackObject* Callback = ContactModifiers[Idx];
			if (Callback->bPendingDelete)
			{
				//will also be in SimCallbackObjects so we'll delete it in that loop
				ContactModifiers.RemoveAtSwap(Idx);
			}
		}

		for (int32 Idx = SimCallbackObjects.Num() - 1; Idx >= 0; --Idx)
		{
			ISimCallbackObject* Callback = SimCallbackObjects[Idx];
			if (Callback->bPendingDelete)
			{
				Callback->SetCurrentInput_Internal(nullptr);	//free any pending input
				delete Callback;
				SimCallbackObjects.RemoveAtSwap(Idx);
			}
		}

		ProcessSinglePushedData_Internal(PushData);

		//run any commands passed in. These don't generate outputs and are a one off so just do them here
		//note: commands run before sim callbacks. This is important for sub-stepping since we want each sub-step to have a consistent view
		//so for example if the user deletes a floor surface, we want all sub-steps to see that in the same way
		//also note, the commands run after data is marshalled over. This is important because data marshalling ensures any GT property changes are seen by command
		//for example a particle may not be created until marshalling occurs, and then a command could explicitly modify something like a collision setting
		for (FSimCallbackCommandObject* SimCallbackObject : PushData.SimCommands)
		{
			SimCallbackObject->PreSimulate_Internal();
			delete SimCallbackObject;
		}
		PushData.SimCommands.Reset();
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::CompleteSceneSimulation()
	{
		LLM_SCOPE(ELLMTag::Chaos);
		SCOPE_CYCLE_COUNTER(STAT_BufferPhysicsResults);

		EventPreBuffer.Broadcast(MLastDt);
		GetDirtyParticlesBuffer()->CaptureSolverData(this);
		BufferPhysicsResults();
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::BufferPhysicsResults()
	{
		//ensure(IsInPhysicsThread());
		TArray<FGeometryCollectionPhysicsProxy*> ActiveGC;
		ActiveGC.Reserve(GeometryCollectionPhysicsProxies_Internal.Num());

		FPullPhysicsData* PullData = MarshallingManager.GetCurrentPullData_Internal();

		TParticleView<TPBDRigidParticles<float, 3>>& DirtyParticles = GetParticles().GetDirtyParticlesView();

		//todo: should be able to go wide just add defaulted etc...
		{
			ensure(PullData->DirtyRigids.Num() == 0);	//we only fill this once per frame
			int32 BufferIdx = 0;
			PullData->DirtyRigids.Reserve(DirtyParticles.Num());

			for (Chaos::TPBDRigidParticleHandleImp<float, 3, false>& DirtyParticle : DirtyParticles)
			{
				IPhysicsProxyBase* Proxy = DirtyParticle.Handle()->PhysicsProxy();
				if(Proxy != nullptr)
				{
					switch(DirtyParticle.GetParticleType())
					{
						case Chaos::EParticleType::Rigid:
							{
								PullData->DirtyRigids.AddDefaulted();
								((FSingleParticlePhysicsProxy*)(Proxy))->BufferPhysicsResults(PullData->DirtyRigids.Last());
								break;
							}
						case Chaos::EParticleType::Kinematic:
						case Chaos::EParticleType::Static:
							ensure(false);
							break;
						case Chaos::EParticleType::GeometryCollection:
							ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(Proxy));
							break;
						case Chaos::EParticleType::Clustered:
							if (auto ClusterParticle = DirtyParticle.CastToClustered())
							{
								if (ClusterParticle->InternalCluster())
								{
									const TSet<IPhysicsProxyBase*> Proxies = ClusterParticle->PhysicsProxies();
									for (IPhysicsProxyBase* ClusterProxy : Proxies)
									{
										ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(ClusterProxy));
									}
								}
								else
								{
									ActiveGC.AddUnique((FGeometryCollectionPhysicsProxy*)(Proxy));
								}
							}
							break;
						default:
							check(false);
					}
				}	
			}
		}

		{
			ensure(PullData->DirtyGeometryCollections.Num() == 0);	//we only fill this once per frame
			PullData->DirtyGeometryCollections.Reserve(ActiveGC.Num());

			for (int32 Idx = 0; Idx < ActiveGC.Num(); ++Idx)
			{
				PullData->DirtyGeometryCollections.AddDefaulted();
				ActiveGC[Idx]->BufferPhysicsResults(this, PullData->DirtyGeometryCollections.Last());
			}
		}

		{
			ensure(PullData->DirtyJointConstraints.Num() == 0);	//we only fill this once per frame
			PullData->DirtyJointConstraints.Reserve(JointConstraintPhysicsProxies_Internal.Num());

			for(int32 Idx = 0; Idx < JointConstraintPhysicsProxies_Internal.Num(); ++Idx)
			{
				PullData->DirtyJointConstraints.AddDefaulted();
				JointConstraintPhysicsProxies_Internal[Idx]->BufferPhysicsResults(PullData->DirtyJointConstraints.Last());
			}
		}
		

		
		// Now that results have been buffered we have completed a solve step so we can broadcast that event
		EventPostSolve.Broadcast(MLastDt);

	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::BeginDestroy()
	{
		MEvolution->SetCanStartAsyncTasks(false);
	}
	
	// This function is not called during normal Engine execution.  
	// FPhysScene_ChaosInterface::EndFrame() calls 
	// FPhysScene_ChaosInterface::SyncBodies() instead, and then immediately afterwards 
	// calls FPBDRigidsSovler::SyncEvents_GameThread().  This function is used by tests,
	// however.
	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UpdateGameThreadStructures()
	{
		PullPhysicsStateForEachDirtyProxy_External([](auto){});
	}

	template <typename Traits>
	int32 TPBDRigidsSolver<Traits>::NumJointConstraints() const
	{
		return JointConstraints.NumConstraints();
	}
	
	template <typename Traits>
	int32 TPBDRigidsSolver<Traits>::NumCollisionConstraints() const
	{
		return GetEvolution()->GetCollisionConstraints().NumConstraints();
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UpdateStatCounters() const
	{
		// Particle counts
		SET_DWORD_STAT(STAT_ChaosCounter_NumDisabledParticles, GetEvolution()->GetParticles().GetAllParticlesView().Num() - GetEvolution()->GetParticles().GetNonDisabledView().Num());
		SET_DWORD_STAT(STAT_ChaosCounter_NumParticles, GetEvolution()->GetParticles().GetNonDisabledView().Num());
		SET_DWORD_STAT(STAT_ChaosCounter_NumDynamicParticles, GetEvolution()->GetParticles().GetNonDisabledDynamicView().Num());
		SET_DWORD_STAT(STAT_ChaosCounter_NumActiveDynamicParticles, GetEvolution()->GetParticles().GetActiveParticlesView().Num());
		SET_DWORD_STAT(STAT_ChaosCounter_NumKinematicParticles, GetEvolution()->GetParticles().GetActiveKinematicParticlesView().Num());
		SET_DWORD_STAT(STAT_ChaosCounter_NumStaticParticles, GetEvolution()->GetParticles().GetActiveStaticParticlesView().Num());
		SET_DWORD_STAT(STAT_ChaosCounter_NumGeometryCollectionParticles, GetEvolution()->GetParticles().GetGeometryCollectionParticles().Size());

		// Constraint counts
		SET_DWORD_STAT(STAT_ChaosCounter_NumIslands, GetEvolution()->GetConstraintGraph().NumIslands());
		SET_DWORD_STAT(STAT_ChaosCounter_NumContacts, NumCollisionConstraints());
		SET_DWORD_STAT(STAT_ChaosCounter_NumJoints, NumJointConstraints());

#if CSV_PROFILER
		// Particle counts
		CSV_CUSTOM_STAT(ChaosCounters, NumDisabledParticles, GetEvolution()->GetParticles().GetAllParticlesView().Num() - GetEvolution()->GetParticles().GetNonDisabledView().Num(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumParticles, GetEvolution()->GetParticles().GetNonDisabledView().Num(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumDynamicParticles, GetEvolution()->GetParticles().GetNonDisabledDynamicView().Num(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumKinematicParticles, GetEvolution()->GetParticles().GetActiveKinematicParticlesView().Num(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumStaticParticles, GetEvolution()->GetParticles().GetActiveStaticParticlesView().Num(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumGeometryCollectionParticles, (int)GetEvolution()->GetParticles().GetGeometryCollectionParticles().Size(), ECsvCustomStatOp::Accumulate);

		// Constraint counts
		CSV_CUSTOM_STAT(ChaosCounters, NumIslands, GetEvolution()->GetConstraintGraph().NumIslands(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumContacts, NumCollisionConstraints(), ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(ChaosCounters, NumJoints, NumJointConstraints(), ECsvCustomStatOp::Accumulate);
#endif
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::PostTickDebugDraw(FReal Dt) const
	{
#if CHAOS_DEBUG_DRAW
		QUICK_SCOPE_CYCLE_COUNTER(SolverDebugDraw);
		if (ChaosSolverDebugDrawShapes == 1)
		{
			if (ChaosSolverDrawShapesShowStatic)
			{
				DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetActiveStaticParticlesView(), FColor(128, 0, 0), &ChaosSolverDebugDebugDrawSettings);
			}
			if (ChaosSolverDrawShapesShowKinematic)
			{
				DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetActiveKinematicParticlesView(), FColor(64, 32, 0), &ChaosSolverDebugDebugDrawSettings);
			}
			if (ChaosSolverDrawShapesShowDynamic)
			{
				DebugDraw::DrawParticleShapes(FRigidTransform3(), Particles.GetNonDisabledDynamicView(), FColor(255, 255, 0), &ChaosSolverDebugDebugDrawSettings);
			}
		}
		if (ChaosSolverDebugDrawCollisions == 1) 
		{
			DebugDraw::DrawCollisions(FRigidTransform3(), GetEvolution()->GetCollisionConstraints(), 1.f, &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDebugDrawBounds == 1)
		{
			DebugDraw::DrawParticleBounds(FRigidTransform3(), Particles.GetAllParticlesView(), Dt, GetEvolution()->GetBroadPhase().GetBoundsThickness(), GetEvolution()->GetBroadPhase().GetBoundsVelocityInflation(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDrawTransforms == 1)
		{
			DebugDraw::DrawParticleTransforms(FRigidTransform3(), Particles.GetAllParticlesView(), &ChaosSolverDebugDebugDrawSettings);
		}
		if (ChaosSolverDrawIslands == 1)
		{
			DebugDraw::DrawConstraintGraph(FRigidTransform3(), GetEvolution()->GetCollisionConstraintsRule().GetGraphColor(), &ChaosSolverDebugDebugDrawSettings);
		}
#endif
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::PostEvolutionVDBPush() const
	{
#if CHAOS_VISUAL_DEBUGGER_ENABLED
		if (ChaosVisualDebuggerEnable)
		{
			const TGeometryParticleHandles<FReal, 3>&  AllParticleHandles = GetEvolution()->GetParticleHandles();
			for (uint32 ParticelIndex = 0; ParticelIndex < AllParticleHandles.Size(); ParticelIndex++)
			{
				const TUniquePtr<TGeometryParticleHandle<float, 3>>& ParticleHandle = AllParticleHandles.Handle(ParticelIndex);
				ChaosVisualDebugger::ParticlePositionLog(ParticleHandle->X());				
			}
		}
#endif
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UpdateMaterial(Chaos::FMaterialHandle InHandle, const Chaos::FChaosPhysicsMaterial& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		*SimMaterials.Get(InHandle.InnerHandle) = InNewData;
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::CreateMaterial(Chaos::FMaterialHandle InHandle, const Chaos::FChaosPhysicsMaterial& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		ensure(SimMaterials.Create(InNewData) == InHandle.InnerHandle);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::DestroyMaterial(Chaos::FMaterialHandle InHandle)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		SimMaterials.Destroy(InHandle.InnerHandle);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UpdateMaterialMask(Chaos::FMaterialMaskHandle InHandle, const Chaos::FChaosPhysicsMaterialMask& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		*SimMaterialMasks.Get(InHandle.InnerHandle) = InNewData;
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::CreateMaterialMask(Chaos::FMaterialMaskHandle InHandle, const Chaos::FChaosPhysicsMaterialMask& InNewData)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		ensure(SimMaterialMasks.Create(InNewData) == InHandle.InnerHandle);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::DestroyMaterialMask(Chaos::FMaterialMaskHandle InHandle)
	{
		TSolverSimMaterialScope<ELockType::Write> Scope(this);
		SimMaterialMasks.Destroy(InHandle.InnerHandle);
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::SyncQueryMaterials_External()
	{
		// Using lock on sim material is an imprefect workaround, we may block while physics thread is updating sim materials in callbacks.
		// QueryMaterials may be slightly stale. Need to rethink lifetime + ownership of materials for async case.
		//acquire external data lock
		FPhysicsSceneGuardScopedWrite ScopedWrite(GetExternalDataLock_External());
		TSolverSimMaterialScope<ELockType::Read> SimMatLock(this);
		
		QueryMaterials_External = SimMaterials;
		QueryMaterialMasks_External = SimMaterialMasks;
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::EnableRewindCapture(int32 NumFrames, bool InUseCollisionResimCache)
	{
		check(Traits::IsRewindable());
		MRewindData = MakeUnique<FRewindData>(NumFrames, InUseCollisionResimCache);
		bUseCollisionResimCache = InUseCollisionResimCache;
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::FinalizeRewindData(const TParticleView<TPBDRigidParticles<FReal,3>>& DirtyParticles)
	{
		using namespace Chaos;
		//Simulated objects must have their properties captured for rewind
		if(MRewindData && DirtyParticles.Num())
		{
			QUICK_SCOPE_CYCLE_COUNTER(RecordRewindData);

			MRewindData->PrepareFrameForPTDirty(DirtyParticles.Num());
			
			int32 DataIdx = 0;
			for(TPBDRigidParticleHandleImp<float,3,false>& DirtyParticle : DirtyParticles)
			{
				//may want to remove branch using templates outside loop
				if (MRewindData->IsResim())
				{
					MRewindData->PushPTDirtyData<true>(*DirtyParticle.Handle(), DataIdx++);
				}
				else
				{
					MRewindData->PushPTDirtyData<false>(*DirtyParticle.Handle(), DataIdx++);
				}
			}
		}
	}

	template <typename Traits>
	void TPBDRigidsSolver<Traits>::UpdateExternalAccelerationStructure_External(ISpatialAccelerationCollection<TAccelerationStructureHandle<FReal,3>,FReal,3>*& ExternalStructure)
	{
		GetEvolution()->UpdateExternalAccelerationStructure_External(ExternalStructure,*PendingSpatialOperations_External);
	}

	Chaos::FClusterCreationParameters<Chaos::FReal>::EConnectionMethod ToInternalConnectionMethod(EClusterUnionMethod InMethod)
	{
		using ETargetEnum = Chaos::FClusterCreationParameters<Chaos::FReal>::EConnectionMethod;
		switch(InMethod)
		{
		case EClusterUnionMethod::PointImplicit:
			return ETargetEnum::PointImplicit;
		case EClusterUnionMethod::DelaunayTriangulation:
			return ETargetEnum::DelaunayTriangulation;
		case EClusterUnionMethod::MinimalSpanningSubsetDelaunayTriangulation:
			return ETargetEnum::MinimalSpanningSubsetDelaunayTriangulation;
		case EClusterUnionMethod::PointImplicitAugmentedWithMinimalDelaunay:
			return ETargetEnum::PointImplicitAugmentedWithMinimalDelaunay;
		}

		return ETargetEnum::None;
	}

	template <typename Traits>
	void Chaos::TPBDRigidsSolver<Traits>::ApplyConfig(const FChaosSolverConfiguration& InConfig)
	{
		GetEvolution()->GetRigidClustering().SetClusterConnectionFactor(InConfig.ClusterConnectionFactor);
		GetEvolution()->GetRigidClustering().SetClusterUnionConnectionType(ToInternalConnectionMethod(InConfig.ClusterUnionConnectionType));
		SetIterations(InConfig.Iterations);
		SetCollisionPairIterations(InConfig.CollisionPairIterations);
		SetPushOutIterations(InConfig.PushOutIterations);
		SetCollisionPushOutPairIterations(InConfig.CollisionPushOutPairIterations);
		SetJointPairIterations(InConfig.JointPairIterations);
		SetJointPushOutPairIterations(InConfig.JointPushOutPairIterations);
		SetCollisionCullDistance(InConfig.CollisionCullDistance);
		SetGenerateCollisionData(InConfig.bGenerateCollisionData);
		SetGenerateBreakingData(InConfig.bGenerateBreakData);
		SetGenerateTrailingData(InConfig.bGenerateTrailingData);
		SetCollisionFilterSettings(InConfig.CollisionFilterSettings);
		SetBreakingFilterSettings(InConfig.BreakingFilterSettings);
		SetTrailingFilterSettings(InConfig.TrailingFilterSettings);
		SetUseContactGraph(InConfig.bGenerateContactGraph);
	}

	template <typename Traits>
	void Chaos::TPBDRigidsSolver<Traits>::FieldParameterUpdateCallback(
		Chaos::TPBDPositionConstraints<float, 3>& PositionTarget,
		TMap<int32, int32>& TargetedParticles)
	{
		GetPerSolverField().FieldParameterUpdateCallback(this, PositionTarget, TargetedParticles);
	}

	template <typename Traits>
	void Chaos::TPBDRigidsSolver<Traits>::FieldForcesUpdateCallback()
	{
		GetPerSolverField().FieldForcesUpdateCallback(this);
	}

#define EVOLUTION_TRAIT(Trait) template class CHAOS_API TPBDRigidsSolver<Trait>;
#include "Chaos/EvolutionTraits.inl"
#undef EVOLUTION_TRAIT

}; // namespace Chaos

