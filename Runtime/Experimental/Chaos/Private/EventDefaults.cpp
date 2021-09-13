// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventDefaults.h"
#include "EventsData.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SkeletalMeshPhysicsProxy.h"
#include "PhysicsProxy/StaticMeshPhysicsProxy.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "ChaosSolversModule.h"
#include "Chaos/CollisionFilterData.h"

namespace Chaos
{
	template <typename Traits>
	void TEventDefaults<Traits>::RegisterSystemEvents(TEventManager<Traits>& EventManager)
	{
		RegisterCollisionEvent(EventManager);
		RegisterBreakingEvent(EventManager);
		RegisterTrailingEvent(EventManager);
		RegisterSleepingEvent(EventManager);
	}

	template <typename Traits>
	void TEventDefaults<Traits>::RegisterCollisionEvent(TEventManager<Traits>& EventManager)
	{
		EventManager.template RegisterEvent<FCollisionEventData>(EEventType::Collision, []
		(const Chaos::TPBDRigidsSolver<Traits>* Solver, FCollisionEventData& CollisionEventData)
		{
			check(Solver);
			SCOPE_CYCLE_COUNTER(STAT_GatherCollisionEvent);

			// #todo: This isn't working - SolverActor parameters are set on a solver but it is currently a different solver that is simulating!!
			//if (!Solver->GetEventFilters()->IsCollisionEventEnabled())
			//	return;

			FCollisionDataArray& AllCollisionsDataArray = CollisionEventData.CollisionData.AllCollisionsArray;
			TMap<IPhysicsProxyBase*, TArray<int32>>& AllCollisionsIndicesByPhysicsProxy = CollisionEventData.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap;

			
			if (CollisionEventData.CollisionData.TimeCreated != Solver->MTime)
			{
				AllCollisionsDataArray.Reset();
				AllCollisionsIndicesByPhysicsProxy.Reset();

				CollisionEventData.CollisionData.TimeCreated = Solver->MTime;
				CollisionEventData.PhysicsProxyToCollisionIndices.TimeCreated = Solver->MTime;
			}
			
			const auto* Evolution = Solver->GetEvolution();

			const FPBDCollisionConstraints& CollisionRule = Evolution->GetCollisionConstraints();


			const TPBDRigidParticles<float, 3>& Particles = Evolution->GetParticles().GetDynamicParticles();
			const TArrayCollectionArray<ClusterId>& ClusterIdsArray = Evolution->GetRigidClustering().GetClusterIdsArray();
#if TODO_REIMPLEMENT_RIGID_CLUSTERING
			const Chaos::TPBDRigidsSolver<Traits>::FClusteringType::FClusterMap& ParentToChildrenMap = Evolution->GetRigidClustering().GetChildrenMap();
#endif
			const typename Chaos::TPBDRigidClustering<typename TPBDRigidsSolver<Traits>::FPBDRigidsEvolution, FPBDCollisionConstraints, float, 3>::FClusterMap& ParentToChildrenMap = Evolution->GetRigidClustering().GetChildrenMap();

			if(CollisionRule.NumConstraints() > 0)
			{
				// Get the number of valid constraints (AccumulatedImpulse != 0.f and Phi < 0.f) from AllConstraintsArray
				TArray<const Chaos::FPBDCollisionConstraintHandle*> ValidCollisionHandles;
				ValidCollisionHandles.SetNumUninitialized(CollisionRule.NumConstraints());
				int32 NumValidCollisions = 0;
				const float MinDeltaVelocityForHitEvents = FChaosSolversModule::GetModule()->GetSettingsProvider().GetMinDeltaVelocityForHitEvents();
				for (const Chaos::FPBDCollisionConstraintHandle * ContactHandle : CollisionRule.GetConstConstraintHandles())
				{
					if (NumValidCollisions >= CollisionRule.NumConstraints())
					{
						break;
					}

					if (ContactHandle->GetType() == FCollisionConstraintBase::FType::SinglePoint ||
						ContactHandle->GetType() == FCollisionConstraintBase::FType::SinglePointSwept)
					{
						const FRigidBodyPointContactConstraint& Constraint = (ContactHandle->GetType() == FCollisionConstraintBase::FType::SinglePoint) ?
							ContactHandle->GetPointContact() :
							*ContactHandle->GetSweptPointContact().As<FRigidBodyPointContactConstraint>();
						
						if (ensure(!Constraint.AccumulatedImpulse.ContainsNaN() && FMath::IsFinite(Constraint.GetPhi())))
						{
							TGeometryParticleHandle<float, 3>* Particle0 = Constraint.Particle[0];
							TGeometryParticleHandle<float, 3>* Particle1 = Constraint.Particle[1];
							TKinematicGeometryParticleHandle<float, 3>* Body0 = Particle0->CastToKinematicParticle();

							// presently when a rigidbody or kinematic hits static geometry then Body1 is null
							TKinematicGeometryParticleHandle<float, 3>* Body1 = Particle1->CastToKinematicParticle();

							const FImplicitObject* Implicit0 = Constraint.Manifold.Implicit[0];
							const FImplicitObject* Implicit1 = Constraint.Manifold.Implicit[1];

							const FPerShapeData* Shape0 = Particle0->GetImplicitShape(Implicit0);
							const FPerShapeData* Shape1 = Particle1->GetImplicitShape(Implicit1);

							// If we don't have a filter - allow the notify, otherwise obey the filter flag
							const bool bFilter0Notify = Shape0 ? Shape0->GetSimData().HasFlag(EFilterFlags::ContactNotify) : true;
							const bool bFilter1Notify = Shape1 ? Shape1->GetSimData().HasFlag(EFilterFlags::ContactNotify) : true;

							if(!bFilter0Notify && !bFilter1Notify)
							{
								// No need to notify - engine didn't request notifications for either shape.
								continue;
							}

							if (!Constraint.AccumulatedImpulse.IsZero() && Body0)
							{
								if (ensure(!Constraint.GetLocation().ContainsNaN() &&
									!Constraint.GetNormal().ContainsNaN()) &&
									!Body0->V().ContainsNaN() &&
									!Body0->W().ContainsNaN() &&
									(Body1 == nullptr || ((!Body1->V().ContainsNaN()) && !Body1->W().ContainsNaN())))
								{
									ValidCollisionHandles[NumValidCollisions] = ContactHandle;
									NumValidCollisions++;
								}
							}
						}
					}
				}

				ValidCollisionHandles.SetNum(NumValidCollisions);

				if(ValidCollisionHandles.Num() > 0)
				{
					for (int32 IdxCollision = 0; IdxCollision < ValidCollisionHandles.Num(); ++IdxCollision)
					{
						if (ValidCollisionHandles[IdxCollision]->GetType() == FCollisionConstraintBase::FType::SinglePoint ||
							ValidCollisionHandles[IdxCollision]->GetType() == FCollisionConstraintBase::FType::SinglePointSwept)
						{
							const FRigidBodyPointContactConstraint& Constraint = (ValidCollisionHandles[IdxCollision]->GetType() == FCollisionConstraintBase::FType::SinglePoint) ? 
								ValidCollisionHandles[IdxCollision]->GetPointContact() :
								*ValidCollisionHandles[IdxCollision]->GetSweptPointContact().As<FRigidBodyPointContactConstraint>();

							TGeometryParticleHandle<float, 3>* Particle0 = Constraint.Particle[0];
							TGeometryParticleHandle<float, 3>* Particle1 = Constraint.Particle[1];

							TCollisionData<float, 3> Data;
							Data.Location = Constraint.GetLocation();
							Data.AccumulatedImpulse = Constraint.AccumulatedImpulse;
							Data.Normal = Constraint.GetNormal();
							Data.PenetrationDepth = Constraint.GetPhi();
							Data.Particle = Particle0;
							Data.Levelset = Particle1;

							if (TPBDRigidParticleHandle<float, 3> * Rigid0 = Particle0->CastToRigidParticle())
							{
								Data.DeltaVelocity1 = Rigid0->V() - Rigid0->PreV();
							}
							if (TPBDRigidParticleHandle<float, 3> * Rigid1 = Particle1->CastToRigidParticle())
							{
								Data.DeltaVelocity2 = Rigid1->V() - Rigid1->PreV();
							}

							// todo: do we need these anymore now we are storing the particles you can access all of this stuff from there
							// do we still need these now we have pointers to particles returned?
							TPBDRigidParticleHandle<float, 3>* PBDRigid0 = Particle0->CastToRigidParticle();
							if (PBDRigid0 && PBDRigid0->ObjectState() == EObjectStateType::Dynamic)
							{
								Data.Velocity1 = PBDRigid0->V();
								Data.AngularVelocity1 = PBDRigid0->W();
								Data.Mass1 = PBDRigid0->M();
							}

							TPBDRigidParticleHandle<float, 3>* PBDRigid1 = Particle1->CastToRigidParticle();
							if (PBDRigid1 && PBDRigid1->ObjectState() == EObjectStateType::Dynamic)
							{
								Data.Velocity2 = PBDRigid1->V();
								Data.AngularVelocity2 = PBDRigid1->W();
								Data.Mass2 = PBDRigid1->M();
							}

							IPhysicsProxyBase* const PhysicsProxy = Particle0->PhysicsProxy();
							IPhysicsProxyBase* const OtherPhysicsProxy = Particle1->PhysicsProxy();
							//Data.Material1 = nullptr; // #todo: provide UPhysicalMaterial for Particle
							//Data.Material2 = nullptr; // #todo: provide UPhysicalMaterial for Levelset

							const FSolverCollisionEventFilter* SolverCollisionEventFilter = Solver->GetEventFilters()->GetCollisionFilter();
							if (!SolverCollisionEventFilter->Enabled() || SolverCollisionEventFilter->Pass(Data))

							{
								const int32 NewIdx = AllCollisionsDataArray.Add(TCollisionData<float, 3>());
								TCollisionData<float, 3>& CollisionDataArrayItem = AllCollisionsDataArray[NewIdx];

								CollisionDataArrayItem = Data;

#if TODO_REIMPLEMENT_RIGID_CLUSTERING
								// If Constraint.ParticleIndex is a cluster store an index for a mesh in this cluster
								if (ClusterIdsArray[Constraint.ParticleIndex].NumChildren > 0)
								{
									int32 ParticleIndexMesh = GetParticleIndexMesh(ParentToChildrenMap, Constraint.ParticleIndex);
									ensure(ParticleIndexMesh != INDEX_NONE);
									CollisionDataArrayItem.ParticleIndexMesh = ParticleIndexMesh;
								}
								// If Constraint.LevelsetIndex is a cluster store an index for a mesh in this cluster
								if (ClusterIdsArray[Constraint.LevelsetIndex].NumChildren > 0)
								{
									int32 LevelsetIndexMesh = GetParticleIndexMesh(ParentToChildrenMap, Constraint.LevelsetIndex);
									ensure(LevelsetIndexMesh != INDEX_NONE);
									CollisionDataArrayItem.LevelsetIndexMesh = LevelsetIndexMesh;
								}
#endif

								// Add to AllCollisionsIndicesByPhysicsProxy
								AllCollisionsIndicesByPhysicsProxy.FindOrAdd(PhysicsProxy).Add(TEventManager<Traits>::EncodeCollisionIndex(NewIdx, false));

								if (OtherPhysicsProxy && OtherPhysicsProxy != PhysicsProxy)
								{
									AllCollisionsIndicesByPhysicsProxy.FindOrAdd(OtherPhysicsProxy).Add(TEventManager<Traits>::EncodeCollisionIndex(NewIdx, true));
								}
							}
						}
					}
				}
			}
		});
	}

	template <typename Traits>
	void TEventDefaults<Traits>::RegisterBreakingEvent(TEventManager<Traits>& EventManager)
	{
		EventManager.template RegisterEvent<FBreakingEventData>(EEventType::Breaking, []
		(const Chaos::TPBDRigidsSolver<Traits>* Solver, FBreakingEventData& BreakingEventData)
		{
			check(Solver);
			SCOPE_CYCLE_COUNTER(STAT_GatherBreakingEvent);

			// #todo: This isn't working - SolverActor parameters are set on a solver but it is currently a different solver that is simulating!!
			if (!Solver->GetEventFilters()->IsBreakingEventEnabled())
				return;

			FBreakingDataArray& AllBreakingDataArray = BreakingEventData.BreakingData.AllBreakingsArray;
			TMap<IPhysicsProxyBase*, TArray<int32>>& AllBreakingIndicesByPhysicsProxy = BreakingEventData.PhysicsProxyToBreakingIndices.PhysicsProxyToIndicesMap;

			if (BreakingEventData.BreakingData.TimeCreated != Solver->MTime)
			{
				AllBreakingDataArray.Reset();
				AllBreakingIndicesByPhysicsProxy.Reset();
				BreakingEventData.BreakingData.TimeCreated = Solver->MTime;
			}

			const auto* Evolution = Solver->GetEvolution();
			const TPBDRigidParticles<float, 3>& Particles = Evolution->GetParticles().GetDynamicParticles();
			const TArray<TBreakingData<float, 3>>& AllBreakingsArray = Evolution->GetRigidClustering().GetAllClusterBreakings();
			const TArrayCollectionArray<ClusterId>& ClusterIdsArray = Evolution->GetRigidClustering().GetClusterIdsArray();

#if TODO_REIMPLEMENT_RIGID_CLUSTERING
			const Chaos::FPBDRigidsSolver::FClusteringType::FClusterMap& ParentToChildrenMap = Evolution->GetRigidClustering().GetChildrenMap();
#endif

			if(AllBreakingsArray.Num() > 0)
			{
				for(int32 Idx = 0; Idx < AllBreakingsArray.Num(); ++Idx)
				{
					// Since Clustered GCs can be unioned the particleIndex representing the union 
					// is not associated with a PhysicsProxy
					TPBDRigidParticleHandle<float, 3>* PBDRigid = AllBreakingsArray[Idx].Particle->CastToRigidParticle();
					if(PBDRigid)
					{
						if(ensure(!AllBreakingsArray[Idx].Location.ContainsNaN() &&
							!PBDRigid->V().ContainsNaN() &&
							!PBDRigid->W().ContainsNaN()))
						{
							TBreakingData<float, 3> BreakingData;
							BreakingData.Location = AllBreakingsArray[Idx].Location;
							BreakingData.Velocity = PBDRigid->V();
							BreakingData.AngularVelocity = PBDRigid->W();
							BreakingData.Mass = PBDRigid->M();
							BreakingData.Particle = PBDRigid;
							
							if(PBDRigid->Geometry()->HasBoundingBox())
							{
								BreakingData.BoundingBox = PBDRigid->Geometry()->BoundingBox();
							}

							const FSolverBreakingEventFilter* SolverBreakingEventFilter = Solver->GetEventFilters()->GetBreakingFilter();
							if(!SolverBreakingEventFilter->Enabled() || SolverBreakingEventFilter->Pass(BreakingData))
							{
								int32 NewIdx = AllBreakingDataArray.Add(TBreakingData<float, 3>());
								TBreakingData<float, 3>& BreakingDataArrayItem = AllBreakingDataArray[NewIdx];
								BreakingDataArrayItem = BreakingData;

								// Add to AllBreakingIndicesByPhysicsProxy
								AllBreakingIndicesByPhysicsProxy.FindOrAdd(BreakingData.Particle->PhysicsProxy()).Add(TEventManager<Traits>::EncodeCollisionIndex(NewIdx, false));

#if 0 // #todo
								// If AllBreakingsArray[Idx].ParticleIndex is a cluster store an index for a mesh in this cluster
								if(ClusterIdsArray[AllBreakingsArray[Idx].ParticleIndex].NumChildren > 0)
								{
									int32 ParticleIndexMesh = GetParticleIndexMesh(ParentToChildrenMap, AllBreakingsArray[Idx].ParticleIndex);
									ensure(ParticleIndexMesh != INDEX_NONE);
									BreakingDataArrayItem.ParticleIndexMesh = ParticleIndexMesh;
								}
#endif
								}
						}
					}
				}
			}
		
		});
	}

	template <typename Traits>
	void TEventDefaults<Traits>::RegisterTrailingEvent(TEventManager<Traits>& EventManager)
	{
		EventManager.template RegisterEvent<FTrailingEventData>(EEventType::Trailing, []
		(const Chaos::TPBDRigidsSolver<Traits>* Solver, FTrailingEventData& TrailingEventData)
		{
			check(Solver);

			// #todo: This isn't working - SolverActor parameters are set on a solver but it is currently a different solver that is simulating!!
			if (!Solver->GetEventFilters()->IsTrailingEventEnabled())
				return;

			const auto* Evolution = Solver->GetEvolution();

			const TArrayCollectionArray<ClusterId>& ClusterIdsArray = Evolution->GetRigidClustering().GetClusterIdsArray();
#if TODO_REIMPLEMENT_RIGID_CLUSTERING
			const TMap<uint32, TUniquePtr<TArray<uint32>>>& ParentToChildrenMap = Evolution->GetRigidClustering().GetChildrenMap();
#endif
			FTrailingDataArray& AllTrailingsDataArray = TrailingEventData.TrailingData.AllTrailingsArray;
			TMap<IPhysicsProxyBase*, TArray<int32>>& AllTrailingIndicesByPhysicsProxy = TrailingEventData.PhysicsProxyToTrailingIndices.PhysicsProxyToIndicesMap;

			if (TrailingEventData.TrailingData.TimeCreated != Solver->MTime)
			{
				AllTrailingsDataArray.Reset();
				AllTrailingIndicesByPhysicsProxy.Reset();

				TrailingEventData.TrailingData.TimeCreated = Solver->MTime;
				TrailingEventData.PhysicsProxyToTrailingIndices.TimeCreated = Solver->MTime;
			}

			const TArray<TPBDRigidParticleHandle<Chaos::FReal, 3>*>& ActiveParticlesArray = Evolution->GetParticles().GetActiveParticlesArray();

			for (TPBDRigidParticleHandle<Chaos::FReal, 3>* ActiveParticle : ActiveParticlesArray)
			{

				if (ensure(FMath::IsFinite(ActiveParticle->InvM())))
				{
					if (ActiveParticle->InvM() != 0.f &&
						ActiveParticle->Geometry() &&
						ActiveParticle->Geometry()->HasBoundingBox())
					{
						if (ensure(!ActiveParticle->X().ContainsNaN() &&
							!ActiveParticle->V().ContainsNaN() &&
							!ActiveParticle->W().ContainsNaN() &&
							FMath::IsFinite(ActiveParticle->M())))
						{
							TTrailingData<float, 3> TrailingData;
							TrailingData.Location = ActiveParticle->X();
							TrailingData.Velocity = ActiveParticle->V();
							TrailingData.AngularVelocity = ActiveParticle->W();
							TrailingData.Mass = ActiveParticle->M();

							TrailingData.Particle = ActiveParticle;
							
							if (ActiveParticle->Geometry()->HasBoundingBox())
							{
								TrailingData.BoundingBox = ActiveParticle->Geometry()->BoundingBox();
							}

							const FSolverTrailingEventFilter* SolverTrailingEventFilter = Solver->GetEventFilters()->GetTrailingFilter();
							if (!SolverTrailingEventFilter->Enabled() || SolverTrailingEventFilter->Pass(TrailingData))
							{
								int32 NewIdx = AllTrailingsDataArray.Add(TTrailingData<float, 3>());
								TTrailingData<float, 3>& TrailingDataArrayItem = AllTrailingsDataArray[NewIdx];
								TrailingDataArrayItem = TrailingData;

								// Add to AllTrailingIndicesByPhysicsProxy
								AllTrailingIndicesByPhysicsProxy.FindOrAdd(TrailingData.Particle->PhysicsProxy()).Add(TEventManager<Traits>::EncodeCollisionIndex(NewIdx, false));

								// If IdxParticle is a cluster store an index for a mesh in this cluster
#if 0
								if (ClusterIdsArray[IdxParticle].NumChildren > 0)
								{
									int32 ParticleIndexMesh = GetParticleIndexMesh(ParentToChildrenMap, IdxParticle);
									ensure(ParticleIndexMesh != INDEX_NONE);
									TrailingDataArrayItem.ParticleIndexMesh = ParticleIndexMesh;
								}
#endif
							}
						}
					}
				}
			}
		});
	}

	template <typename Traits>
	void TEventDefaults<Traits>::RegisterSleepingEvent(TEventManager<Traits>& EventManager)
	{
		EventManager.template RegisterEvent<FSleepingEventData>(EEventType::Sleeping, []
		(const Chaos::TPBDRigidsSolver<Traits>* Solver, FSleepingEventData& SleepingEventData)
		{
			check(Solver);
			SCOPE_CYCLE_COUNTER(STAT_GatherSleepingEvent);

			const auto* Evolution = Solver->GetEvolution();

			FSleepingDataArray& EventSleepDataArray = SleepingEventData.SleepingData;
			EventSleepDataArray.Reset();

			Chaos::TPBDRigidsSolver<Traits>* NonConstSolver = const_cast<Chaos::TPBDRigidsSolver<Traits>*>(Solver);

			NonConstSolver->Particles.GetDynamicParticles().GetSleepDataLock().ReadLock();
			auto& SolverSleepingData = NonConstSolver->Particles.GetDynamicParticles().GetSleepData();
			for(const TSleepData<float, 3>& SleepData : SolverSleepingData)
			{
				if(SleepData.Particle)
				{
					FGeometryParticle* Particle = SleepData.Particle->GTGeometryParticle();
					if (Particle && SleepData.Particle->PhysicsProxy())
					{
						int32 NewIdx = EventSleepDataArray.Add(TSleepingData<float, 3>());
						TSleepingData<float, 3>& SleepingDataArrayItem = EventSleepDataArray[NewIdx];
						SleepingDataArrayItem.Particle = Particle;
						SleepingDataArrayItem.Sleeping = SleepData.Sleeping;
					}
				}
			}
			NonConstSolver->Particles.GetDynamicParticles().GetSleepDataLock().ReadUnlock();

			NonConstSolver->Particles.GetDynamicParticles().ClearSleepData();


		});
	}

#define EVOLUTION_TRAIT(Trait) template class TEventDefaults<Trait>;
#include "Chaos/EvolutionTraits.inl"
#undef EVOLUTION_TRAIT
}