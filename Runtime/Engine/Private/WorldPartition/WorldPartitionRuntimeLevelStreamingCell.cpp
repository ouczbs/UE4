// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "WorldPartition/WorldPartition.h"
#include "Engine/World.h"

#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "WorldPartition/HLOD/HLODActor.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionLevelStreamingPolicy.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionPackageCache.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#endif

UWorldPartitionRuntimeLevelStreamingCell::UWorldPartitionRuntimeLevelStreamingCell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LevelStreaming(nullptr)
{
}

EWorldPartitionRuntimeCellState UWorldPartitionRuntimeLevelStreamingCell::GetCurrentState() const
{
	if (LevelStreaming)
	{
		ULevelStreaming::ECurrentState CurrentStreamingState = LevelStreaming->GetCurrentState();
		if (CurrentStreamingState == ULevelStreaming::ECurrentState::LoadedVisible)
		{
			return EWorldPartitionRuntimeCellState::Activated;
		}
		else if (CurrentStreamingState >= ULevelStreaming::ECurrentState::LoadedNotVisible)
		{
			return EWorldPartitionRuntimeCellState::Loaded;
		}
	}
	
	//@todo_ow: Now that actors are moved to the persistent level, remove the AlwaysLoaded cell (it's always empty)
	return IsAlwaysLoaded() ? EWorldPartitionRuntimeCellState::Activated : EWorldPartitionRuntimeCellState::Unloaded;
}

UWorldPartitionLevelStreamingDynamic* UWorldPartitionRuntimeLevelStreamingCell::GetLevelStreaming() const
{
	return LevelStreaming;
}

EStreamingStatus UWorldPartitionRuntimeLevelStreamingCell::GetLevelStreamingStatus() const
{
	check(LevelStreaming)
	return LevelStreaming->GetLevelStreamingStatus();
}

FLinearColor UWorldPartitionRuntimeLevelStreamingCell::GetDebugColor() const
{
	FLinearColor Color = LevelStreaming ? ULevelStreaming::GetLevelStreamingStatusColor(GetLevelStreamingStatus()) : FLinearColor::Black;
	Color.A = 0.25f / (Level + 1);
	return Color;
}

void UWorldPartitionRuntimeLevelStreamingCell::SetIsAlwaysLoaded(bool bInIsAlwaysLoaded)
{
	Super::SetIsAlwaysLoaded(bInIsAlwaysLoaded);
	if (LevelStreaming)
	{
		LevelStreaming->SetShouldBeAlwaysLoaded(true);
	}
}

#if WITH_EDITOR

void UWorldPartitionRuntimeLevelStreamingCell::AddActorToCell(const FWorldPartitionActorDescView& ActorDescView, uint32 InContainerID, const FTransform& InContainerTransform, const UActorDescContainer* InContainer)
{
	check(!ActorDescView.GetActorIsEditorOnly());
	Packages.Emplace(ActorDescView.GetActorPackage(), ActorDescView.GetActorPath(), InContainerID, InContainerTransform, InContainer->GetContainerPackage());
}

ULevelStreaming* UWorldPartitionRuntimeLevelStreamingCell::CreateLevelStreaming(const FString& InPackageName) const
{
	if (GetActorCount() > 0)
	{
		const UWorldPartition* WorldPartition = GetOuterUWorldPartition();
		UWorld* OuterWorld = WorldPartition->GetTypedOuter<UWorld>();
		UWorld* OwningWorld = WorldPartition->GetWorld();
		
		const FName LevelStreamingName = FName(*FString::Printf(TEXT("WorldPartitionLevelStreaming_%s"), *GetName()));
		const UClass* LevelStreamingClass = UWorldPartitionLevelStreamingDynamic::StaticClass();
		
		// When called by Commandlet (PopulateGeneratedPackageForCook), LevelStreaming's outer is set to Cell/WorldPartition's outer to prevent warnings when saving Cell Levels (Warning: Obj in another map). 
		// At runtime, LevelStreaming's outer will be properly set to the main world (see UWorldPartitionRuntimeLevelStreamingCell::Activate).
		UWorld* LevelStreamingOuterWorld = IsRunningCommandlet() ? OuterWorld : OwningWorld;
		ULevelStreaming* NewLevelStreaming = NewObject<ULevelStreaming>(LevelStreamingOuterWorld, LevelStreamingClass, LevelStreamingName, RF_NoFlags, NULL);
		FString PackageName = !InPackageName.IsEmpty() ? InPackageName : UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(GetFName(), OuterWorld);
		TSoftObjectPtr<UWorld> WorldAsset(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *PackageName, *OuterWorld->GetName())));
		NewLevelStreaming->SetWorldAsset(WorldAsset);
		// Transfer WorldPartition's transform to Level
		NewLevelStreaming->LevelTransform = WorldPartition->GetInstanceTransform();

		if (UWorldPartitionLevelStreamingDynamic* WorldPartitionLevelStreamingDynamic = Cast<UWorldPartitionLevelStreamingDynamic>(NewLevelStreaming))
		{
			WorldPartitionLevelStreamingDynamic->Initialize(*this);
		}

		if (OwningWorld->IsPlayInEditor() && OwningWorld->GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor) && OwningWorld->GetPackage()->PIEInstanceID != INDEX_NONE)
		{
			// When renaming for PIE, make sure to keep World's name so that linker can properly remap with Package's instancing context
			NewLevelStreaming->RenameForPIE(OwningWorld->GetPackage()->PIEInstanceID, /*bKeepWorldAssetName*/true);
		}

		return NewLevelStreaming;
	}

	return nullptr;
}

void UWorldPartitionRuntimeLevelStreamingCell::LoadActorsForCook()
{
	FWorldPartitionPackageCache PackageCache;
	verify(FWorldPartitionLevelHelper::LoadActors(nullptr, Packages, PackageCache, [](bool){}, /*bLoadForPlay=*/false, /*bLoadAsync=*/false));
}

void UWorldPartitionRuntimeLevelStreamingCell::MoveAlwaysLoadedContentToPersistentLevel()
{
	check(IsAlwaysLoaded());
	if (GetActorCount() > 0)
	{
		LoadActorsForCook();

		UWorld* OuterWorld = GetOuterUWorldPartition()->GetTypedOuter<UWorld>();
		FWorldPartitionLevelHelper::MoveExternalActorsToLevel(Packages, OuterWorld->PersistentLevel);

		// Empty cell's package list (this ensures that no one can rely on cell's content).
		Packages.Empty();
	}
}

bool UWorldPartitionRuntimeLevelStreamingCell::PopulateGeneratedPackageForCook(UPackage* InPackage, const FString& InPackageCookName)
{
	check(!IsAlwaysLoaded());
	if (!InPackage || InPackageCookName.IsEmpty())
	{
		return false;
	}

	if (GetActorCount() > 0)
	{
		UWorldPartition* WorldPartition = GetOuterUWorldPartition();
		UWorld* OuterWorld = WorldPartition->GetTypedOuter<UWorld>();
		ULevelStreaming* NewLevelStreaming = CreateLevelStreaming(InPackageCookName);
		check(NewLevelStreaming)

		// Load cell Actors
		LoadActorsForCook();
		
		LevelStreaming = Cast<UWorldPartitionLevelStreamingDynamic>(NewLevelStreaming);
		ULevel* NewLevel = FWorldPartitionLevelHelper::CreateEmptyLevelForRuntimeCell(OuterWorld, NewLevelStreaming->GetWorldAsset().ToString(), InPackage);
		check(NewLevel->GetPackage() == InPackage);
		FWorldPartitionLevelHelper::MoveExternalActorsToLevel(Packages, NewLevel);
		// Remap Level's SoftObjectPaths
		FWorldPartitionLevelHelper::RemapLevelSoftObjectPaths(NewLevel, WorldPartition);
	}
	return true;
}

int32 UWorldPartitionRuntimeLevelStreamingCell::GetActorCount() const
{
	return Packages.Num();
}

FString UWorldPartitionRuntimeLevelStreamingCell::GetPackageNameToCreate() const
{
	const UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	UWorld* OuterWorld = WorldPartition->GetTypedOuter<UWorld>();
	return UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(GetFName(), OuterWorld);
}

#endif

UWorldPartitionLevelStreamingDynamic* UWorldPartitionRuntimeLevelStreamingCell::GetOrCreateLevelStreaming() const
{
#if WITH_EDITOR
	if (GetActorCount() == 0)
	{
		return nullptr;
	}

	if (!LevelStreaming)
	{
		LevelStreaming = Cast<UWorldPartitionLevelStreamingDynamic>(CreateLevelStreaming());
	}
	check(LevelStreaming);
#else
	// In Runtime, always loaded cell level is handled by World directly
	check(LevelStreaming || IsAlwaysLoaded());
#endif

#if !WITH_EDITOR
	// In Runtime, prepare LevelStreaming for activation
	if (LevelStreaming)
	{
		// Setup pre-created LevelStreaming's outer to the WorldPartition owning world
		const UWorldPartition* WorldPartition = GetOuterUWorldPartition();
		UWorld* OwningWorld = WorldPartition->GetWorld();
		if (LevelStreaming->GetWorld() != OwningWorld)
		{
			LevelStreaming->Rename(nullptr, OwningWorld);
		}

		// Transfer WorldPartition's transform to LevelStreaming
		LevelStreaming->LevelTransform = WorldPartition->GetInstanceTransform();

		// When Partition outer level is an instance, make sure to also generate unique cell level instance name
		ULevel* PartitionLevel = WorldPartition->GetTypedOuter<ULevel>();
		if (PartitionLevel->IsInstancedLevel())
		{
			FString PackageShortName = FPackageName::GetShortName(PartitionLevel->GetPackage());
			FString InstancedLevelPackageName = FString::Printf(TEXT("%s_InstanceOf_%s"), *LevelStreaming->PackageNameToLoad.ToString(), *PackageShortName);
			LevelStreaming->SetWorldAssetByPackageName(FName(InstancedLevelPackageName));
		}
	}
#endif

	if (LevelStreaming)
	{
		LevelStreaming->OnLevelShown.AddUniqueDynamic(this, &UWorldPartitionRuntimeLevelStreamingCell::OnLevelShown);
		LevelStreaming->OnLevelHidden.AddUniqueDynamic(this, &UWorldPartitionRuntimeLevelStreamingCell::OnLevelHidden);
	}

	return LevelStreaming;
}

void UWorldPartitionRuntimeLevelStreamingCell::Load() const
{
	if (UWorldPartitionLevelStreamingDynamic* LocalLevelStreaming = GetOrCreateLevelStreaming())
	{
		LocalLevelStreaming->Load();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::Activate() const
{
	if (UWorldPartitionLevelStreamingDynamic* LocalLevelStreaming = GetOrCreateLevelStreaming())
	{
		LocalLevelStreaming->Activate();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::Unload() const
{
#if WITH_EDITOR
	if (GetActorCount() == 0)
	{
		return;
	}
	check(LevelStreaming);
#else
	// In Runtime, always loaded cell level is handled by World directly
	check(LevelStreaming || IsAlwaysLoaded());
#endif

	if (LevelStreaming)
	{
		LevelStreaming->Unload();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::Deactivate() const
{
#if WITH_EDITOR
	if (GetActorCount() == 0)
	{
		return;
	}
	check(LevelStreaming);
#else
	// In Runtime, always loaded cell level is handled by World directly
	check(LevelStreaming || IsAlwaysLoaded());
#endif

	if (LevelStreaming)
	{
		LevelStreaming->Deactivate();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::OnLevelShown()
{
	check(LevelStreaming);
	LevelStreaming->GetWorld()->GetSubsystem<UHLODSubsystem>()->OnCellShown(this);
}

void UWorldPartitionRuntimeLevelStreamingCell::OnLevelHidden()
{
	check(LevelStreaming);
	LevelStreaming->GetWorld()->GetSubsystem<UHLODSubsystem>()->OnCellHidden(this);
}
