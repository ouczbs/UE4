// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProceduralFoliageEditorLibrary.h"
#include "ProceduralFoliageVolume.h"
#include "ProceduralFoliageComponent.h"
#include "FoliageEdMode.h"
#include "Algo/Transform.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "ProceduralFoliageEditorLibrary"

UProceduralFoliageEditorLibrary::UProceduralFoliageEditorLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProceduralFoliageEditorLibrary::ResimulateProceduralFoliageVolumes(const TArray<AProceduralFoliageVolume*>& ProceduralFoliageVolumes)
{
	TArray<UProceduralFoliageComponent*> Components;
	Components.Reserve(ProceduralFoliageVolumes.Num());
	Algo::Transform(ProceduralFoliageVolumes, Components, [](AProceduralFoliageVolume* PFV) { return PFV->ProceduralComponent; });
	ResimulateProceduralFoliageComponents(Components);
}

void UProceduralFoliageEditorLibrary::ClearProceduralFoliageVolumes(const TArray<AProceduralFoliageVolume*>& ProceduralFoliageVolumes)
{
	TArray<UProceduralFoliageComponent*> Components;
	Components.Reserve(ProceduralFoliageVolumes.Num());
	Algo::Transform(ProceduralFoliageVolumes, Components, [](AProceduralFoliageVolume* PFV) { return PFV->ProceduralComponent; });
	ClearProceduralFoliageComponents(Components);
}

void UProceduralFoliageEditorLibrary::ClearProceduralFoliageComponents(const TArray<UProceduralFoliageComponent*>& ProceduralFoliageComponents)
{
	FScopedSlowTask SlowTask(ProceduralFoliageComponents.Num(), LOCTEXT("ClearProceduralFoliageComponents", "Clearing Procedural Foliage Components"));
	SlowTask.MakeDialogDelayed(0.5f);
	for (UProceduralFoliageComponent* Component : ProceduralFoliageComponents)
	{
		SlowTask.EnterProgressFrame(1);
		if (Component)
		{
			Component->RemoveProceduralContent();
		}
	}
}

void UProceduralFoliageEditorLibrary::ResimulateProceduralFoliageComponents(const TArray<UProceduralFoliageComponent*>& ProceduralFoliageComponents)
{
	FScopedSlowTask SlowTask(ProceduralFoliageComponents.Num(), LOCTEXT("ResimulateProceduralFoliageComponents", "Resimulating Procedural Foliage Components"));
	SlowTask.MakeDialogDelayed(0.5f);
	for (UProceduralFoliageComponent* Component : ProceduralFoliageComponents)
	{
		SlowTask.EnterProgressFrame(1);
		if (Component && Component->ResimulateProceduralFoliage([Component](const TArray<FDesiredFoliageInstance>& DesiredFoliageInstances)
			{
				FFoliagePaintingGeometryFilter OverrideGeometryFilter;
				OverrideGeometryFilter.bAllowLandscape = Component->bAllowLandscape;
				OverrideGeometryFilter.bAllowStaticMesh = Component->bAllowStaticMesh;
				OverrideGeometryFilter.bAllowBSP = Component->bAllowBSP;
				OverrideGeometryFilter.bAllowFoliage = Component->bAllowFoliage;
				OverrideGeometryFilter.bAllowTranslucent = Component->bAllowTranslucent;
				FEdModeFoliage::AddInstances(Component->GetWorld(), DesiredFoliageInstances, OverrideGeometryFilter, true);
			}))
		{
			// If no instances were spawned, inform the user
			if (!Component->HasSpawnedAnyInstances())
			{
				FNotificationInfo Info(LOCTEXT("NothingSpawned_Notification", "Unable to spawn instances. Ensure a large enough surface exists within the volume."));
				Info.bUseLargeFont = false;
				Info.bFireAndForget = true;
				Info.bUseThrobber = false;
				Info.bUseSuccessFailIcons = true;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE