// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementFillTool.h"
#include "UObject/Object.h"
#include "ToolContextInterfaces.h"
#include "InteractiveToolManager.h"

constexpr TCHAR UPlacementModeFillTool::ToolName[];

bool UPlacementModeFillToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UPlacementModeFillToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UPlacementModeFillTool>(SceneState.ToolManager, UPlacementModeFillTool::ToolName);
}
