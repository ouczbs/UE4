// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementClickDragToolBase.h"
#include "UObject/Object.h"
#include "ToolContextInterfaces.h"
#include "InteractiveToolManager.h"
#include "BaseGizmos/BrushStampIndicator.h"

void UPlacementClickDragToolBase::Setup()
{
	Super::Setup();

	RemoveToolPropertySource(BrushProperties);
}

double UPlacementClickDragToolBase::EstimateMaximumTargetDimension()
{
	return Super::EstimateMaximumTargetDimension();
}

void UPlacementClickDragToolBase::SetupBrushStampIndicator()
{
	Super::SetupBrushStampIndicator();

	BrushStampIndicator->bDrawRadiusCircle = false;
	BrushProperties->BrushSize = .5f;
	BrushProperties->BrushFalloffAmount = 1.f;
}
