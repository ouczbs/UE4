// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/DefaultEdMode.h"

#include "EditorModes.h"
#include "EditorStyleSet.h"
#include "Textures/SlateIcon.h"
#include "LevelEditorViewport.h"
#include "Elements/Framework/TypedElementList.h"

class FLevelEditorSelectModeWidgetHelper : public FLegacyEdModeWidgetHelper
{
public:
	virtual bool ShouldDrawWidget() const override
	{
		if (GCurrentLevelEditingViewportClient)
		{
			const UTypedElementList* ElementsToManipulate = GCurrentLevelEditingViewportClient->GetElementsToManipulate();
			return ElementsToManipulate->Num() > 0;
		}
		return false;
	}
};

UEdModeDefault::UEdModeDefault()
{
	Info = FEditorModeInfo(
		FBuiltinEditorModes::EM_Default,
		NSLOCTEXT("DefaultMode", "DisplayName", "Select"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.SelectMode", "LevelEditor.SelectMode.Small"),
		true, 0);
}

bool UEdModeDefault::UsesPropertyWidgets() const
{
	return true;
}

bool UEdModeDefault::UsesToolkits() const
{
	return false;
}

TSharedRef<FLegacyEdModeWidgetHelper> UEdModeDefault::CreateWidgetHelper()
{
	return MakeShared<FLevelEditorSelectModeWidgetHelper>();
}
