// Copyright Epic Games, Inc.All Rights Reserved.

#include "LevelAssetEditorViewportClient.h"
#include "UnrealWidget.h"

FLevelAssetEditorViewportClient::FLevelAssetEditorViewportClient(UInteractiveToolsContext* InToolsContext, FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
	, ToolsContext(InToolsContext)
{
	Widget->SetUsesEditorModeTools(ModeTools.Get());
}
