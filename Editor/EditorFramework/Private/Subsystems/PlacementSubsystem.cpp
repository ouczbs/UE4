// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/PlacementSubsystem.h"

#include "Factories/AssetFactoryInterface.h"

#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementSelectionSet.h"

#include "Misc/CoreDelegates.h"
#include "UObject/UObjectIterator.h"

void UPlacementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UPlacementSubsystem::RegisterPlacementFactories);
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UPlacementSubsystem::UnregisterPlacementFactories);
}

void UPlacementSubsystem::Deinitialize()
{
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

TArray<FTypedElementHandle> UPlacementSubsystem::PlaceAsset(const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	return PlaceAssets(MakeArrayView( {InPlacementInfo} ), InPlacementOptions);
}

TArray<FTypedElementHandle> UPlacementSubsystem::PlaceAssets(TArrayView<const FAssetPlacementInfo> InPlacementInfos, const FPlacementOptions& InPlacementOptions)
{
	TGuardValue<bool> bShouldCreatePreviewElements(bIsCreatingPreviewElements, InPlacementOptions.bIsCreatingPreviewElements);
	TMap<IAssetFactoryInterface*, TArray<FTypedElementHandle>> PlacedElementsByFactory;

	for (const FAssetPlacementInfo& PlacementInfo : InPlacementInfos)
	{
		const FAssetData& AssetData = PlacementInfo.AssetToPlace;
		TScriptInterface<IAssetFactoryInterface> FactoryInterface = PlacementInfo.FactoryOverride;
		if (!FactoryInterface)
		{
			FactoryInterface = FindAssetFactoryFromAssetData(AssetData);
		}

		if (!FactoryInterface || !FactoryInterface->CanPlaceElementsFromAssetData(AssetData))
		{
			continue;
		}

		if (!PlacedElementsByFactory.Contains(&*FactoryInterface))
		{
			FactoryInterface->BeginPlacement(InPlacementOptions);
			PlacedElementsByFactory.Add(&*FactoryInterface);
		}

		FAssetPlacementInfo AdjustedPlacementInfo = PlacementInfo;
		if (!FactoryInterface->PrePlaceAsset(AdjustedPlacementInfo, InPlacementOptions))
		{
			continue;
		}

		TArray<FTypedElementHandle> PlacedHandles = FactoryInterface->PlaceAsset(AdjustedPlacementInfo, InPlacementOptions);
		if (PlacedHandles.Num())
		{
			FactoryInterface->PostPlaceAsset(PlacedHandles, PlacementInfo, InPlacementOptions);
			PlacedElementsByFactory[&*FactoryInterface].Append(MoveTemp(PlacedHandles));
		}
	}

	TArray<FTypedElementHandle> PlacedElements;
	for (TMap<IAssetFactoryInterface*, TArray<FTypedElementHandle>>::ElementType& FactoryAndPlacedElementsPair : PlacedElementsByFactory)
	{
		FactoryAndPlacedElementsPair.Key->EndPlacement(FactoryAndPlacedElementsPair.Value, InPlacementOptions);
		PlacedElements.Append(MoveTemp(FactoryAndPlacedElementsPair.Value));
	}

	return PlacedElements;
}

TScriptInterface<IAssetFactoryInterface> UPlacementSubsystem::FindAssetFactoryFromAssetData(const FAssetData& InAssetData)
{
	for (const TScriptInterface<IAssetFactoryInterface>& AssetFactory : AssetFactories)
	{
		if (AssetFactory && AssetFactory->CanPlaceElementsFromAssetData(InAssetData))
		{
			return AssetFactory;
		}
	}

	return nullptr;
}

bool UPlacementSubsystem::IsCreatingPreviewElements() const
{
	return bIsCreatingPreviewElements;
}

void UPlacementSubsystem::RegisterPlacementFactories()
{
	for (TObjectIterator<UClass> ObjectIt; ObjectIt; ++ObjectIt)
	{
		UClass* TestClass = *ObjectIt;
		if (TestClass->ImplementsInterface(UAssetFactoryInterface::StaticClass()))
		{
			if (!TestClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
			{
				TScriptInterface<IAssetFactoryInterface> NewFactory = NewObject<UObject>(this, TestClass);
				AssetFactories.Add(NewFactory);
			}
		}
	}
}

void UPlacementSubsystem::UnregisterPlacementFactories()
{
	AssetFactories.Empty();
}
