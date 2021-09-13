// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "EditorSupportDelegates.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "LevelUtils.h"
#include "Misc/MapErrors.h"
#include "ActorEditorUtils.h"
#include "EngineGlobals.h"

#if WITH_EDITOR

#include "Editor.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

#define LOCTEXT_NAMESPACE "ErrorChecking"

void AActor::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	FObjectProperty* ObjProp = CastField<FObjectProperty>(PropertyThatWillChange);
	UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(GetClass());
	if ( BPGC != nullptr && ObjProp != nullptr )
	{
		BPGC->UnbindDynamicDelegatesForProperty(this, ObjProp);
	}

	// During SIE, allow components to be unregistered here, and then reregistered and reconstructed in PostEditChangeProperty.
	if ((GEditor && GEditor->bIsSimulatingInEditor) || ReregisterComponentsWhenModified())
	{
		UnregisterAllComponents();
	}
}

bool AActor::CanEditChange(const FProperty* PropertyThatWillChange) const
{
	if ((PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Layers)) ||
		(PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, ActorGuid)))
	{
		return false;
	}

	const bool bIsGridPlacement = PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, GridPlacement);
	const bool bIsRuntimeGrid = PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, RuntimeGrid);
	const bool bIsDataLayers = PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, DataLayers);

	if (bIsGridPlacement || bIsRuntimeGrid || bIsDataLayers)
	{
		if (!IsTemplate())
		{
			if (UWorld* World = GetWorld())
			{
				const bool bIsPartitionedWorld = UWorld::HasSubsystem<UWorldPartitionSubsystem>(World);
				if (!bIsPartitionedWorld)
				{
					return false;
				}
			}
		}
	}

	if (bIsGridPlacement && (GetDefaultGridPlacement() != EActorGridPlacement::None))
	{
		return false;
	}

	if (bIsDataLayers && !SupportsDataLayer())
	{
		return false;
	}

	return Super::CanEditChange(PropertyThatWillChange);
}

static FName Name_RelativeLocation = USceneComponent::GetRelativeLocationPropertyName();
static FName Name_RelativeRotation = USceneComponent::GetRelativeRotationPropertyName();
static FName Name_RelativeScale3D = USceneComponent::GetRelativeScale3DPropertyName();

void AActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FName MemberPropertyName = MemberPropertyThatChanged != NULL ? MemberPropertyThatChanged->GetFName() : NAME_None;

	if (IsPropertyChangedAffectingDataLayers(PropertyChangedEvent))
	{
		FixupDataLayers();
	}
	
	const bool bTransformationChanged = (MemberPropertyName == Name_RelativeLocation || MemberPropertyName == Name_RelativeRotation || MemberPropertyName == Name_RelativeScale3D);

	// During SIE, allow components to reregistered and reconstructed in PostEditChangeProperty.
	// This is essential as construction is deferred during spawning / duplication when in SIE.
	if ((GEditor && GEditor->bIsSimulatingInEditor && GetWorld() != nullptr) || ReregisterComponentsWhenModified())
	{
		// In the Undo case we have an annotation storing information about constructed components and we do not want
		// to improperly apply out of date changes so we need to skip registration of all blueprint created components
		// and defer instance components attached to them until after rerun
		if (CurrentTransactionAnnotation.IsValid())
		{
			UnregisterAllComponents();

			TInlineComponentArray<UActorComponent*> Components;
			GetComponents(Components);

			Components.Sort([](UActorComponent& A, UActorComponent& B)
			{
				if (&B == B.GetOwner()->GetRootComponent())
				{
					return false;
				}
				if (USceneComponent* ASC = Cast<USceneComponent>(&A))
				{
					if (ASC->GetAttachParent() == &B)
					{
						return false;
					}
				}
				return true;
			});

			bool bRequiresReregister = false;
			for (UActorComponent* Component : Components)
			{
				if (Component->CreationMethod == EComponentCreationMethod::Native)
				{
					Component->RegisterComponent();
				}
				else if (Component->CreationMethod == EComponentCreationMethod::Instance)
				{
					USceneComponent* SC = Cast<USceneComponent>(Component);
					if (SC == nullptr || SC == RootComponent || (SC->GetAttachParent() && SC->GetAttachParent()->IsRegistered()))
					{
						Component->RegisterComponent();
					}
					else
					{
						bRequiresReregister = true;
					}
				}
				else
				{
					bRequiresReregister = true;
				}
			}

			RerunConstructionScripts();

			if (bRequiresReregister)
			{
				ReregisterAllComponents();
			}
		}
		else
		{
			UnregisterAllComponents();
			RerunConstructionScripts();
			ReregisterAllComponents();
		}
	}

	// Let other systems know that an actor was moved
	if (bTransformationChanged)
	{
		GEngine->BroadcastOnActorMoved( this );
	}

	FEditorSupportDelegates::UpdateUI.Broadcast();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AActor::PostEditMove(bool bFinished)
{
	if ( ReregisterComponentsWhenModified() && !FLevelUtils::IsMovingLevel())
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		if (bFinished || bRunConstructionScriptOnDrag || (Blueprint && Blueprint->bRunConstructionScriptOnDrag))
		{
			FNavigationLockContext NavLock(GetWorld(), ENavigationLockReason::AllowUnregister);
			RerunConstructionScripts();
		}
	}

	if (!FLevelUtils::IsMovingLevel())
	{
		GEngine->BroadcastOnActorMoving(this);
	}

	if ( bFinished )
	{
		UWorld* World = GetWorld();

		World->UpdateCullDistanceVolumes(this);
		World->bAreConstraintsDirty = true;

		FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();

		// Let other systems know that an actor was moved
		GEngine->BroadcastOnActorMoved( this );

		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	// If the root component was not just recreated by the construction script - call PostEditComponentMove on it
	if(RootComponent != NULL && !RootComponent->IsCreatedByConstructionScript())
	{
		RootComponent->PostEditComponentMove(bFinished);
	}

	if (bFinished)
	{
		FNavigationSystem::OnPostEditActorMove(*this);
	}
}

bool AActor::ReregisterComponentsWhenModified() const
{
	// For child actors, redirect to the parent's owner (we do the same in RerunConstructionScripts).
	if (const AActor* ParentActor = GetParentActor())
	{
		return ParentActor->ReregisterComponentsWhenModified();
	}

	return !bActorIsBeingConstructed && !IsTemplate() && !GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor) && GetWorld() != nullptr;
}

void AActor::DebugShowComponentHierarchy(  const TCHAR* Info, bool bShowPosition )
{	
	TArray<AActor*> ParentedActors;
	GetAttachedActors( ParentedActors );
	if( Info  )
	{
		UE_LOG( LogActor, Warning, TEXT("--%s--"), Info );
	}
	else
	{
		UE_LOG( LogActor, Warning, TEXT("--------------------------------------------------") );
	}
	UE_LOG( LogActor, Warning, TEXT("--------------------------------------------------") );
	UE_LOG( LogActor, Warning, TEXT("Actor [%x] (%s)"), this, *GetFName().ToString() );
	USceneComponent* SceneComp = GetRootComponent();
	if( SceneComp )
	{
		int32 NestLevel = 0;
		DebugShowOneComponentHierarchy( SceneComp, NestLevel, bShowPosition );			
	}
	else
	{
		UE_LOG( LogActor, Warning, TEXT("Actor has no root.") );		
	}
	UE_LOG( LogActor, Warning, TEXT("--------------------------------------------------") );
}

void AActor::DebugShowOneComponentHierarchy( USceneComponent* SceneComp, int32& NestLevel, bool bShowPosition )
{
	FString Nest = "";
	for (int32 iNest = 0; iNest < NestLevel ; iNest++)
	{
		Nest = Nest + "---->";	
	}
	NestLevel++;
	FString PosString;
	if( bShowPosition )
	{
		FVector Posn = SceneComp->GetComponentTransform().GetLocation();
		//PosString = FString::Printf( TEXT("{R:%f,%f,%f- W:%f,%f,%f}"), SceneComp->RelativeLocation.X, SceneComp->RelativeLocation.Y, SceneComp->RelativeLocation.Z, Posn.X, Posn.Y, Posn.Z );
		PosString = FString::Printf( TEXT("{R:%f- W:%f}"), SceneComp->GetRelativeLocation().Z, Posn.Z );
	}
	else
	{
		PosString = "";
	}
	AActor* OwnerActor = SceneComp->GetOwner();
	if( OwnerActor )
	{
		UE_LOG(LogActor, Warning, TEXT("%sSceneComp [%x] (%s) Owned by %s %s"), *Nest, SceneComp, *SceneComp->GetFName().ToString(), *OwnerActor->GetFName().ToString(), *PosString );
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("%sSceneComp [%x] (%s) No Owner"), *Nest, SceneComp, *SceneComp->GetFName().ToString() );
	}
	if( SceneComp->GetAttachParent())
	{
		if( bShowPosition )
		{
			FVector Posn = SceneComp->GetComponentTransform().GetLocation();
			//PosString = FString::Printf( TEXT("{R:%f,%f,%f- W:%f,%f,%f}"), SceneComp->RelativeLocation.X, SceneComp->RelativeLocation.Y, SceneComp->RelativeLocation.Z, Posn.X, Posn.Y, Posn.Z );
			PosString = FString::Printf( TEXT("{R:%f- W:%f}"), SceneComp->GetRelativeLocation().Z, Posn.Z );
		}
		else
		{
			PosString = "";
		}
		UE_LOG(LogActor, Warning, TEXT("%sAttachParent [%x] (%s) %s"), *Nest, SceneComp->GetAttachParent(), *SceneComp->GetAttachParent()->GetFName().ToString(), *PosString );
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("%s[NO PARENT]"), *Nest );
	}

	if( SceneComp->GetAttachChildren().Num() != 0 )
	{
		for (USceneComponent* EachSceneComp : SceneComp->GetAttachChildren())
		{			
			DebugShowOneComponentHierarchy(EachSceneComp,NestLevel, bShowPosition );
		}
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("%s[NO CHILDREN]"), *Nest );
	}
}

FArchive& operator<<(FArchive& Ar, AActor::FActorRootComponentReconstructionData::FAttachedActorInfo& ActorInfo)
{
	enum class EVersion : uint8
	{
		InitialVersion = 0,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	EVersion Version = EVersion::LatestVersion;
	Ar << Version;

	if (Version > EVersion::LatestVersion)
	{
		Ar.SetError();
		return Ar;
	}

	Ar << ActorInfo.Actor;
	Ar << ActorInfo.AttachParent;
	Ar << ActorInfo.AttachParentName;
	Ar << ActorInfo.SocketName;
	Ar << ActorInfo.RelativeTransform;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, AActor::FActorRootComponentReconstructionData& RootComponentData)
{
	enum class EVersion : uint8
	{
		InitialVersion = 0,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	EVersion Version = EVersion::LatestVersion;
	Ar << Version;

	if (Version > EVersion::LatestVersion)
	{
		Ar.SetError();
		return Ar;
	}

	Ar << RootComponentData.Transform;

	if (Ar.IsSaving())
	{
		FQuat TransformRotationQuat = RootComponentData.TransformRotationCache.GetCachedQuat();
		Ar << TransformRotationQuat;
	}
	else if (Ar.IsLoading())
	{
		FQuat TransformRotationQuat;
		Ar << TransformRotationQuat;
		RootComponentData.TransformRotationCache.NormalizedQuatToRotator(TransformRotationQuat);
	}
	
	Ar << RootComponentData.AttachedParentInfo;
	
	Ar << RootComponentData.AttachedToInfo;

	return Ar;
}

TSharedRef<AActor::FActorTransactionAnnotation> AActor::FActorTransactionAnnotation::Create()
{
	return MakeShareable(new FActorTransactionAnnotation());
}

TSharedRef<AActor::FActorTransactionAnnotation> AActor::FActorTransactionAnnotation::Create(const AActor* InActor, const bool InCacheRootComponentData)
{
	return MakeShareable(new FActorTransactionAnnotation(InActor, FComponentInstanceDataCache(InActor), InCacheRootComponentData));
}

TSharedPtr<AActor::FActorTransactionAnnotation> AActor::FActorTransactionAnnotation::CreateIfRequired(const AActor* InActor, const bool InCacheRootComponentData)
{
	// Don't create a transaction annotation for something that has no instance data, or a root component that's created by a construction script
	FComponentInstanceDataCache TempComponentInstanceData(InActor);
	if (!TempComponentInstanceData.HasInstanceData())
	{
		USceneComponent* ActorRootComponent = InActor->GetRootComponent();
		if (!InCacheRootComponentData || !ActorRootComponent || !ActorRootComponent->IsCreatedByConstructionScript())
		{
			return nullptr;
		}
	}

	return MakeShareable(new FActorTransactionAnnotation(InActor, MoveTemp(TempComponentInstanceData), InCacheRootComponentData));
}

AActor::FActorTransactionAnnotation::FActorTransactionAnnotation()
	: bRootComponentDataCached(false)
{
}

AActor::FActorTransactionAnnotation::FActorTransactionAnnotation(const AActor* InActor, FComponentInstanceDataCache&& InComponentInstanceData, const bool InCacheRootComponentData)
	: ComponentInstanceData(MoveTemp(InComponentInstanceData))
{
	Actor = InActor;

	USceneComponent* ActorRootComponent = InActor->GetRootComponent();
	if (InCacheRootComponentData && ActorRootComponent && ActorRootComponent->IsCreatedByConstructionScript())
	{
		bRootComponentDataCached = true;
		RootComponentData.Transform = ActorRootComponent->GetComponentTransform();
		RootComponentData.Transform.SetTranslation(ActorRootComponent->GetComponentLocation()); // take into account any custom location
		RootComponentData.TransformRotationCache = ActorRootComponent->GetRelativeRotationCache();

		if (ActorRootComponent->GetAttachParent())
		{
			RootComponentData.AttachedParentInfo.Actor = ActorRootComponent->GetAttachParent()->GetOwner();
			RootComponentData.AttachedParentInfo.AttachParent = ActorRootComponent->GetAttachParent();
			RootComponentData.AttachedParentInfo.AttachParentName = ActorRootComponent->GetAttachParent()->GetFName();
			RootComponentData.AttachedParentInfo.SocketName = ActorRootComponent->GetAttachSocketName();
			RootComponentData.AttachedParentInfo.RelativeTransform = ActorRootComponent->GetRelativeTransform();
		}

		for (USceneComponent* AttachChild : ActorRootComponent->GetAttachChildren())
		{
			AActor* ChildOwner = (AttachChild ? AttachChild->GetOwner() : NULL);
			if (ChildOwner && ChildOwner != InActor)
			{
				// Save info about actor to reattach
				FActorRootComponentReconstructionData::FAttachedActorInfo Info;
				Info.Actor = ChildOwner;
				Info.SocketName = AttachChild->GetAttachSocketName();
				Info.RelativeTransform = AttachChild->GetRelativeTransform();
				RootComponentData.AttachedToInfo.Add(Info);
			}
		}
	}
	else
	{
		bRootComponentDataCached = false;
	}
}

void AActor::FActorTransactionAnnotation::AddReferencedObjects(FReferenceCollector& Collector)
{
	ComponentInstanceData.AddReferencedObjects(Collector);
}

void AActor::FActorTransactionAnnotation::Serialize(FArchive& Ar)
{
	enum class EVersion : uint8
	{
		InitialVersion = 0,
		WithInstanceCache,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	EVersion Version = EVersion::LatestVersion;
	Ar << Version;

	if (Version > EVersion::LatestVersion)
	{
		Ar.SetError();
		return;
	}

	// InitialVersion
	Ar << Actor;
	Ar << bRootComponentDataCached;
	if (bRootComponentDataCached)
	{
		Ar << RootComponentData;
	}
	// WithInstanceCache
	if (Ar.IsLoading())
	{
		ComponentInstanceData = FComponentInstanceDataCache(Actor.Get());
	}
	if (Version >= EVersion::WithInstanceCache)
	{
		ComponentInstanceData.Serialize(Ar);
	}
}

bool AActor::FActorTransactionAnnotation::HasInstanceData() const
{
	return (bRootComponentDataCached || ComponentInstanceData.HasInstanceData());
}

TSharedPtr<ITransactionObjectAnnotation> AActor::FactoryTransactionAnnotation(const ETransactionAnnotationCreationMode InCreationMode) const
{
	if (InCreationMode == ETransactionAnnotationCreationMode::DefaultInstance)
	{
		return FActorTransactionAnnotation::Create();
	}

	if (CurrentTransactionAnnotation.IsValid())
	{
		return CurrentTransactionAnnotation;
	}

	return FActorTransactionAnnotation::CreateIfRequired(this);
}

void AActor::PreEditUndo()
{
	// Check if this Actor needs to be re-instanced
	UClass* OldClass = GetClass();
	UClass* NewClass = OldClass->GetAuthoritativeClass();
	if (NewClass != OldClass)
	{
		// Empty the OwnedComponents array, it's filled with invalid information
		OwnedComponents.Empty();
	}

	// Since child actor components will rebuild themselves get rid of the Actor before we make changes
	TInlineComponentArray<UChildActorComponent*> ChildActorComponents;
	GetComponents(ChildActorComponents);

	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (ChildActorComponent->IsCreatedByConstructionScript())
		{
			ChildActorComponent->DestroyChildActor();
		}
	}

	// let navigation system know to not care about this actor anymore
	FNavigationSystem::RemoveActorData(*this);

	Super::PreEditUndo();
}

bool AActor::InternalPostEditUndo()
{
	// Check if this Actor needs to be re-instanced
	UClass* OldClass = GetClass();
	if (OldClass->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UClass* NewClass = OldClass->GetAuthoritativeClass();
		if (!ensure(NewClass != OldClass))
		{
			UE_LOG(LogActor, Warning, TEXT("WARNING: %s is out of date and is the same as its AuthoritativeClass during PostEditUndo!"), *OldClass->GetName());
		};

		// Early exit, letting anything more occur would be invalid due to the REINST_ class
		return false;
	}

	// Notify LevelBounds actor that level bounding box might be changed
	if (!IsTemplate())
	{
		if (ULevel* Level = GetLevel())
		{
			Level->MarkLevelBoundsDirty();
		}
	}

	// Restore OwnedComponents array
	if (!IsPendingKill())
	{
		ResetOwnedComponents();

		// BP created components are not serialized, so this should be cleared and will be filled in as the construction scripts are run
		BlueprintCreatedComponents.Reset();

		// notify navigation system
		FNavigationSystem::UpdateActorAndComponentData(*this);
	}
	else
	{
		FNavigationSystem::RemoveActorData(*this);
	}

	// This is a normal undo, so call super
	return true;
}

void AActor::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	if (TransactionEvent.HasOuterChange())
	{
		GEngine->BroadcastLevelActorOuterChanged(this, StaticFindObject(ULevel::StaticClass(), nullptr, *TransactionEvent.GetOriginalObjectOuterPathName().ToString()));
	}
}

void AActor::PostEditUndo()
{
	if (InternalPostEditUndo())
	{
		Super::PostEditUndo();
	}

	UWorld* World = GetWorld();
	if (World && World->Scene)
	{
		ENQUEUE_RENDER_COMMAND(UpdateAllPrimitiveSceneInfosCmd)([Scene = World->Scene](FRHICommandListImmediate& RHICmdList) {
			Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);
		});
	}
}

void AActor::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	CurrentTransactionAnnotation = StaticCastSharedPtr<FActorTransactionAnnotation>(TransactionAnnotation);

	if (InternalPostEditUndo())
	{
		Super::PostEditUndo(TransactionAnnotation);
	}
}

// @todo: Remove this hack once we have decided on the scaling method to use.
bool AActor::bUsePercentageBasedScaling = false;

void AActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	if( RootComponent != NULL )
	{
		FTransform NewTransform = GetRootComponent()->GetComponentTransform();
		NewTransform.SetTranslation(NewTransform.GetTranslation() + DeltaTranslation);
		GetRootComponent()->SetWorldTransform(NewTransform);
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyTranslation %s has no root component"), *GetName() );
	}
}

void AActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	if( RootComponent != NULL )
	{
		FRotator Rot = RootComponent->GetAttachParent() != NULL ? GetActorRotation() : RootComponent->GetRelativeRotation();
		FRotator ActorRotWind, ActorRotRem;
		Rot.GetWindingAndRemainder(ActorRotWind, ActorRotRem);
		const FQuat ActorQ = ActorRotRem.Quaternion();
		const FQuat DeltaQ = DeltaRotation.Quaternion();

		FRotator NewActorRotRem;
		if(RootComponent->GetAttachParent() != NULL )
		{
			//first we get the new rotation in relative space.
			const FQuat ResultQ = DeltaQ * ActorQ;
			NewActorRotRem = FRotator(ResultQ);
			FRotator DeltaRot = NewActorRotRem - ActorRotRem;
			FRotator NewRotation = Rot + DeltaRot;
			FQuat NewRelRotation = NewRotation.Quaternion();
			NewRelRotation = RootComponent->GetRelativeRotationFromWorld(NewRelRotation);
			NewActorRotRem = FRotator(NewRelRotation);
			//now we need to get current relative rotation to find the diff
			Rot = RootComponent->GetRelativeRotation();
			Rot.GetWindingAndRemainder(ActorRotWind, ActorRotRem);
		}
		else
		{
			const FQuat ResultQ = DeltaQ * ActorQ;
			NewActorRotRem = FRotator(ResultQ);
		}

		ActorRotRem.SetClosestToMe(NewActorRotRem);
		FRotator DeltaRot = NewActorRotRem - ActorRotRem;
		DeltaRot.Normalize();
		RootComponent->SetRelativeRotationExact( Rot + DeltaRot );
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyRotation %s has no root component"), *GetName() );
	}
}


void AActor::EditorApplyScale( const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown )
{
	if( RootComponent != NULL )
	{
		const FVector CurrentScale = GetRootComponent()->GetRelativeScale3D();

		// @todo: Remove this hack once we have decided on the scaling method to use.
		FVector ScaleToApply;

		if( AActor::bUsePercentageBasedScaling )
		{
			ScaleToApply = CurrentScale * (FVector(1.0f) + DeltaScale);
		}
		else
		{
			ScaleToApply = CurrentScale + DeltaScale;
		}

		GetRootComponent()->SetRelativeScale3D(ScaleToApply);

		if (PivotLocation)
		{
			const FVector CurrentScaleSafe(CurrentScale.X ? CurrentScale.X : 1.0f,
										   CurrentScale.Y ? CurrentScale.Y : 1.0f,
										   CurrentScale.Z ? CurrentScale.Z : 1.0f);

			const FRotator ActorRotation = GetActorRotation();
			const FVector WorldDelta = GetActorLocation() - (*PivotLocation);
			const FVector LocalDelta = (ActorRotation.GetInverse()).RotateVector(WorldDelta);
			const FVector LocalScaledDelta = LocalDelta * (ScaleToApply / CurrentScaleSafe);
			const FVector WorldScaledDelta = ActorRotation.RotateVector(LocalScaledDelta);

			GetRootComponent()->SetWorldLocation(WorldScaledDelta + (*PivotLocation));
		}
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyTranslation %s has no root component"), *GetName() );
	}

	FEditorSupportDelegates::UpdateUI.Broadcast();
}


void AActor::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
	const FRotationMatrix TempRot( GetActorRotation() );
	const FVector New0( TempRot.GetScaledAxis( EAxis::X ) * MirrorScale );
	const FVector New1( TempRot.GetScaledAxis( EAxis::Y ) * MirrorScale );
	const FVector New2( TempRot.GetScaledAxis( EAxis::Z ) * MirrorScale );
	// Revert the handedness of the rotation, but make up for it in the scaling.
	// Arbitrarily choose the X axis to remain fixed.
	const FMatrix NewRot( -New0, New1, New2, FVector::ZeroVector );

	if( RootComponent != NULL )
	{
		GetRootComponent()->SetRelativeRotationExact( NewRot.Rotator() );
		FVector Loc = GetActorLocation();
		Loc -= PivotLocation;
		Loc *= MirrorScale;
		Loc += PivotLocation;
		GetRootComponent()->SetRelativeLocation( Loc );

		FVector Scale3D = GetRootComponent()->GetRelativeScale3D();
		Scale3D.X = -Scale3D.X;
		GetRootComponent()->SetRelativeScale3D(Scale3D);
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyMirror %s has no root component"), *GetName() );
	}
}

void AActor::EditorGetUnderlyingActors(TSet<AActor*>& OutUnderlyingActors) const
{
	TInlineComponentArray<UChildActorComponent*> ChildActorComponents;
	GetComponents(ChildActorComponents);

	OutUnderlyingActors.Reserve(OutUnderlyingActors.Num() + ChildActorComponents.Num());
	
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (AActor* ChildActor = ChildActorComponent->GetChildActor())
		{
			bool bAlreadySet = false;
			OutUnderlyingActors.Add(ChildActor, &bAlreadySet);
			if (!bAlreadySet)
			{
				ChildActor->EditorGetUnderlyingActors(OutUnderlyingActors);
			}
		}
	}
}

bool AActor::IsHiddenEd() const
{
	// If any of the standard hide flags are set, return true
	if( bHiddenEdLayer || !bEditable || ( GIsEditor && ( IsTemporarilyHiddenInEditor() || bHiddenEdLevel ) ) )
	{
		return true;
	}
	// Otherwise, it's visible
	return false;
}

void AActor::SetIsTemporarilyHiddenInEditor( bool bIsHidden )
{
	if( bHiddenEdTemporary != bIsHidden )
	{
		bHiddenEdTemporary = bIsHidden;
		MarkComponentsRenderStateDirty();
	}
}

bool AActor::IsEditable() const
{
	return bEditable;
}

bool AActor::IsSelectable() const
{
	return true;
}

bool AActor::IsListedInSceneOutliner() const
{
	return bListedInSceneOutliner;
}

bool AActor::EditorCanAttachTo(const AActor* InParent, FText& OutReason) const
{
	return true;
}

AActor* AActor::GetSceneOutlinerParent() const
{
	return GetAttachParentActor();
}

class UHLODLayer* AActor::GetHLODLayer() const
{
	return HLODLayer;
}

void AActor::SetHLODLayer(class UHLODLayer* InHLODLayer)
{
	HLODLayer = InHLODLayer;
}

void AActor::SetPackageExternal(bool bExternal, bool bShouldDirty)
{
	if (bExternal == IsPackageExternal())
	{
		return;
	}

    // Mark the current actor & package as dirty
	Modify(bShouldDirty);

	UPackage* LevelPackage = GetLevel()->GetPackage();
	if (bExternal)
	{
		UPackage* NewActorPackage = ULevel::CreateActorPackage(LevelPackage, GetPathName());
		SetExternalPackage(NewActorPackage);
	}
	else 
	{
		UPackage* ActorPackage = GetExternalPackage();
		// Detach the linker from the actor package so that the actor won't keep references to it if we wanted to delete the package
		ResetLoaders(ActorPackage);
		SetExternalPackage(nullptr);
	}

	for (UActorComponent* ActorComponent : GetComponents())
	{
		if (ActorComponent && ActorComponent->IsRegistered())
		{
			ActorComponent->SetPackageExternal(bExternal, bShouldDirty);
		}
	}

	OnPackagingModeChanged.Broadcast(this, bExternal);
	
	// Mark the new actor package dirty
	MarkPackageDirty();
}

EActorGridPlacement AActor::GetDefaultGridPlacement() const
{
	if (GetClass()->GetClassFlags() & CLASS_NotPlaceable)
	{
		return EActorGridPlacement::AlwaysLoaded;
	}
	return EActorGridPlacement::None;
}

TUniquePtr<FWorldPartitionActorDesc> AActor::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FWorldPartitionActorDesc());
}

TUniquePtr<FWorldPartitionActorDesc> AActor::CreateActorDesc() const
{
	check(!HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject));
	
	TUniquePtr<FWorldPartitionActorDesc> ActorDesc(CreateClassActorDesc());
		
	ActorDesc->Init(this);
	
	return ActorDesc;
}

TUniquePtr<class FWorldPartitionActorDesc> AActor::CreateClassActorDesc(const TSubclassOf<AActor>& ActorClass)
{
	return CastChecked<AActor>(ActorClass->GetDefaultObject())->CreateClassActorDesc();
}

const FString& AActor::GetActorLabel(bool bCreateIfNone) const
{
	// If the label string is empty then we'll use the default actor label (usually the actor's class name.)
	// We actually cache the default name into our ActorLabel property.  This will be saved out with the
	// actor if the actor gets saved.  The reasons we like caching the name here is:
	//
	//		a) We can return it by const&	(performance)
	//		b) Calling GetDefaultActorLabel() is slow because of FName stuff  (performance)
	//		c) If needed, we could always empty the ActorLabel string if it matched the default
	//
	// Remember, ActorLabel is currently an editor-only property.

	if( ActorLabel.IsEmpty() && bCreateIfNone )
	{
		// Get the class
		UClass* ActorClass = GetClass();

		FString DefaultActorLabel = ActorClass->GetName();

		// Strip off the ugly "_C" suffix for Blueprint class actor instances
		if (Cast<UBlueprint>(ActorClass->ClassGeneratedBy))
		{
			DefaultActorLabel.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		}

		// We want the actor's label to be initially unique, if possible, so we'll use the number of the
		// actor's FName when creating the initially.  It doesn't actually *need* to be unique, this is just
		// an easy way to tell actors apart when observing them in a list.  The user can always go and rename
		// these labels such that they're no longer unique.
		if (!FActorSpawnUtils::IsGloballyUniqueName(GetFName()))
		{
			// Don't bother adding a suffix for number '0'
			const int32 NameNumber = NAME_INTERNAL_TO_EXTERNAL( GetFName().GetNumber() );
			if( NameNumber != 0 )
			{
				DefaultActorLabel.AppendInt(NameNumber);
			}
		}

		// Remember, there could already be an actor with the same label in the level.  But that's OK, because
		// actor labels aren't supposed to be unique.  We just try to make them unique initially to help
		// disambiguate when opening up a new level and there are hundreds of actors of the same type.
		ActorLabel = MoveTemp(DefaultActorLabel);
	}

	return ActorLabel;
}

void AActor::SetActorLabel(const FString& NewActorLabelDirty, bool bMarkDirty)
{
	// Clean up the incoming string a bit
	FString NewActorLabel = NewActorLabelDirty.TrimStartAndEnd();

	// Validate incoming string before proceeding
	FText OutErrorMessage;
	if (!FActorEditorUtils::ValidateActorName(FText::FromString(NewActorLabel), OutErrorMessage))
	{
		//Invalid actor name
		UE_LOG(LogActor, Warning, TEXT("SetActorLabel failed: %s"), *OutErrorMessage.ToString());
	}
	else
	{
		// First, update the actor label
		{
			// Has anything changed?
			if (FCString::Strcmp(*NewActorLabel, *GetActorLabel()) != 0)
			{
				// Store new label
				Modify(bMarkDirty);
				ActorLabel = MoveTemp(NewActorLabel);
			}
		}
	}

	FPropertyChangedEvent PropertyEvent( FindFProperty<FProperty>( AActor::StaticClass(), "ActorLabel" ) );
	PostEditChangeProperty(PropertyEvent);

	FCoreDelegates::OnActorLabelChanged.Broadcast(this);
}

bool AActor::IsActorLabelEditable() const
{
	return bActorLabelEditable && !FActorEditorUtils::IsABuilderBrush(this);
}

void AActor::ClearActorLabel()
{
	ActorLabel.Reset();
}

const FName& AActor::GetFolderPath() const
{
	return FolderPath;
}

void AActor::SetFolderPath(const FName& NewFolderPath)
{
	if (!NewFolderPath.IsEqual(FolderPath, ENameCase::CaseSensitive))
	{
		Modify();

		FName OldPath = FolderPath;
		FolderPath = NewFolderPath;

		if (GEngine)
		{
			GEngine->BroadcastLevelActorFolderChanged(this, OldPath);
		}
	}
}

void AActor::SetFolderPath_Recursively(const FName& NewFolderPath)
{
	FActorEditorUtils::TraverseActorTree_ParentFirst(this, [&](AActor* InActor){
		InActor->SetFolderPath(NewFolderPath);
		return true;
	});
}

void AActor::CheckForDeprecated()
{
	if ( GetClass()->HasAnyClassFlags(CLASS_Deprecated) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetPathName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ActorIsObselete_Deprecated", "{ActorName} : Obsolete and must be removed! (Class is deprecated)" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorIsObselete));
	}
	// don't check to see if this is an abstract class if this is the CDO
	if ( !(GetFlags() & RF_ClassDefaultObject) && GetClass()->HasAnyClassFlags(CLASS_Abstract) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetPathName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ActorIsObselete_Abstract", "{ActorName} : Obsolete and must be removed! (Class is abstract)" ), Arguments) ) )
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorIsObselete));
	}
}

void AActor::CheckForErrors()
{
	int32 OldNumWarnings = FMessageLog("MapCheck").NumMessages(EMessageSeverity::Warning);
	CheckForDeprecated();
	if (OldNumWarnings < FMessageLog("MapCheck").NumMessages(EMessageSeverity::Warning))
	{
		return;
	}

	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComponent);
	if (PrimComp && (PrimComp->Mobility != EComponentMobility::Movable) && PrimComp->BodyInstance.bSimulatePhysics)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetPathName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_StaticPhysNone", "{ActorName} : Static object with bSimulatePhysics set to true" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticPhysNone));
	}

	if (RootComponent)
	{
		const FVector LocalRelativeScale3D = RootComponent->GetRelativeScale3D();
		if (FMath::IsNearlyZero(LocalRelativeScale3D.X * LocalRelativeScale3D.Y * LocalRelativeScale3D.Z))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ActorName"), FText::FromString(GetPathName()));
			FMessageLog("MapCheck").Error()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidDrawscale", "{ActorName} : Invalid DrawScale/DrawScale3D"), Arguments)))
				->AddToken(FMapErrorToken::Create(FMapErrors::InvalidDrawscale));
		}
	}

	// Route error checking to components.
	for (UActorComponent* ActorComponent : GetComponents())
	{
		if (ActorComponent && ActorComponent->IsRegistered())
		{
			ActorComponent->CheckForErrors();
		}
	}
}

bool AActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass( GetClass() );
	if (Blueprint)
	{
		Objects.AddUnique(Blueprint);
	}
	return true;
}

EDataValidationResult AActor::IsDataValid(TArray<FText>& ValidationErrors)
{
	// Do not run asset validation on external actors, validation will be caught through map check
	if (IsPackageExternal())
	{
		return EDataValidationResult::NotValidated;
	}

	bool bSuccess = CheckDefaultSubobjects();
	if (!bSuccess)
	{
		FText ErrorMsg = FText::Format(LOCTEXT("IsDataValid_Failed_CheckDefaultSubobjectsInternal", "{0} failed CheckDefaultSubobjectsInternal()"), FText::FromString(GetName()));
		ValidationErrors.Add(ErrorMsg);
	}

	int32 OldNumMapWarningsAndErrors = FMessageLog("MapCheck").NumMessages(EMessageSeverity::Warning);
	CheckForErrors();
	int32 NewNumMapWarningsAndErrors = FMessageLog("MapCheck").NumMessages(EMessageSeverity::Warning);
	if (NewNumMapWarningsAndErrors != OldNumMapWarningsAndErrors)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FText ErrorMsg = FText::Format(LOCTEXT("IsDataValid_Failed_CheckForErrors", "{ActorName} is not valid. See the MapCheck log messages for details."), Arguments);
		ValidationErrors.Add(ErrorMsg);
		bSuccess = false;
	}

	EDataValidationResult Result = bSuccess ? EDataValidationResult::Valid : EDataValidationResult::Invalid;

	// check the components
	for (UActorComponent* Component : GetComponents())
	{
		if (Component)
		{
			// if any component is invalid, our result is invalid
			// in the future we may want to update this to say that the actor was not validated if any of its components returns EDataValidationResult::NotValidated
			EDataValidationResult ComponentResult = Component->IsDataValid(ValidationErrors);
			if (ComponentResult == EDataValidationResult::Invalid)
			{
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}

//---------------------------------------------------------------------------
// DataLayers (begin)

bool AActor::AddDataLayer(const UDataLayer* DataLayer)
{
	bool bActorWasModified = false;
	if (SupportsDataLayer() && DataLayer && !ContainsDataLayer(DataLayer))
	{
		if (!bActorWasModified)
		{
			Modify();
			bActorWasModified = true;
		}

		DataLayers.Emplace(DataLayer->GetFName());
	}
	return bActorWasModified;
}

bool AActor::RemoveDataLayer(const UDataLayer* DataLayer)
{
	bool bActorWasModified = false;
	if (ContainsDataLayer(DataLayer))
	{
		if (!bActorWasModified)
		{
			Modify();
			bActorWasModified = true;
		}

		DataLayers.Remove(FActorDataLayer(DataLayer->GetFName()));
	}
	return bActorWasModified;
}

bool AActor::ContainsDataLayer(const UDataLayer* DataLayer) const
{
	return DataLayer && DataLayers.Contains(FActorDataLayer(DataLayer->GetFName()));
}

bool AActor::HasDataLayers() const
{
	return DataLayers.Num() > 0;
}

bool AActor::HasValidDataLayers() const
{
	if (const AWorldDataLayers* WorldDataLayers = AWorldDataLayers::Get(GetWorld()))
	{
		for (const FActorDataLayer& DataLayer : DataLayers)
		{
			if (const UDataLayer* DataLayerObject = WorldDataLayers->GetDataLayerFromName(DataLayer.Name))
			{
				return true;
			}
		}
	}
	return false;
}

bool AActor::HasAllDataLayers(const TArray<const UDataLayer*>& InDataLayers) const
{
	if (DataLayers.Num() < InDataLayers.Num())
	{
		return false;
	}

	for (const UDataLayer* DataLayer : InDataLayers)
	{
		if (!ContainsDataLayer(DataLayer))
		{
			return false;
		}
	}
	return true;
}

TArray<FName> AActor::GetDataLayerNames() const
{
	const AWorldDataLayers* WorldDataLayers = AWorldDataLayers::Get(GetWorld());
	return WorldDataLayers ? WorldDataLayers->GetDataLayerNames(DataLayers) : TArray<FName>();
}

TArray<const UDataLayer*> AActor::GetDataLayerObjects() const
{
	const AWorldDataLayers* WorldDataLayers = AWorldDataLayers::Get(GetWorld());
	return WorldDataLayers ? WorldDataLayers->GetDataLayerObjects(DataLayers) : TArray<const UDataLayer*>();
}

bool AActor::HasAnyOfDataLayers(const TArray<FName>& DataLayerNames) const
{
	for (const FActorDataLayer& DataLayer : DataLayers)
	{
		if (DataLayerNames.Contains(DataLayer.Name))
		{
			return true;
		}
	}
	return false;
}

void AActor::FixupDataLayers()
{
	if (!SupportsDataLayer())
	{
		DataLayers.Empty();
	}

	if (const AWorldDataLayers* WorldDataLayers = AWorldDataLayers::Get(GetWorld()))
	{
		TSet<FName> ExistingDataLayers;
		for (int32 Index = 0; Index < DataLayers.Num();)
		{
			const FName& DataLayer = DataLayers[Index].Name;
			if (!WorldDataLayers->GetDataLayerFromName(DataLayer) || ExistingDataLayers.Contains(DataLayer))
			{
				DataLayers.RemoveAtSwap(Index);
			}
			else
			{
				ExistingDataLayers.Add(DataLayer);
				++Index;
			}
		}
	}
}

bool AActor::IsPropertyChangedAffectingDataLayers(FPropertyChangedEvent& PropertyChangedEvent) const
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
		const FName MemberPropertyName = MemberPropertyThatChanged != NULL ? MemberPropertyThatChanged->GetFName() : NAME_None;

		static const FName NAME_DataLayers = GET_MEMBER_NAME_CHECKED(AActor, DataLayers);
		static const FName NAME_FActorDataLayerName = GET_MEMBER_NAME_CHECKED(FActorDataLayer, Name);

		if (MemberPropertyName == NAME_DataLayers &&
			PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet &&
			PropertyChangedEvent.Property->GetFName() == NAME_FActorDataLayerName)
		{
			return true;
		}
		else
		{
			const FName PropertyName = PropertyChangedEvent.GetPropertyName();
			if (PropertyName == NAME_DataLayers && (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet))
			{
				return true;
			}
		}
	}
	return false;
}

bool AActor::IsValidForDataLayer() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bIsPartitionedActor = UWorld::HasSubsystem<UWorldPartitionSubsystem>(World);
	const bool bIsInEditorWorld = World->WorldType == EWorldType::Editor;
	const bool bIsBuilderBrush = FActorEditorUtils::IsABuilderBrush(this);
	const bool bIsHidden = GetClass()->GetDefaultObject<AActor>()->bHiddenEd;
	const bool bIsValid = !bIsHidden && !bIsBuilderBrush && bIsInEditorWorld && bIsPartitionedActor;

	return bIsValid;
}

// DataLayers (end)
//---------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
