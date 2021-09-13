// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditorModesActions.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LevelEditorModesActions, Log, All);

#define LOCTEXT_NAMESPACE "LevelEditorModesActions"

/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FLevelEditorModesCommands::RegisterCommands()
{
	EditorModeCommands.Empty();

	int editorMode = 0;
	static const TArray<FKey, TInlineAllocator<9>> EdModeKeys = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };

	for ( const FEditorModeInfo& Mode : GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetEditorModeInfoOrderedByPriority())
	{
		FName EditorModeCommandName = FName(*(FString("EditorMode.") + Mode.ID.ToString()));

		TSharedPtr<FUICommandInfo> EditorModeCommand = 
			FInputBindingManager::Get().FindCommandInContext(GetContextName(), EditorModeCommandName);

		// If a command isn't yet registered for this mode, we need to register one.
		if ( !EditorModeCommand.IsValid() )
		{
			FFormatNamedArguments Args;
			FText ModeName = Mode.Name;
			if (ModeName.IsEmpty())
			{
				ModeName = FText::FromName(Mode.ID);
			}
			Args.Add(TEXT("Mode"), ModeName);
			const FText Tooltip = FText::Format( NSLOCTEXT("LevelEditor", "ModeTooltipF", "Activate {Mode} Editing Mode"), Args );

			FInputChord DefaultKeyBinding;
			if (Mode.IsVisible() && editorMode < EdModeKeys.Num())
			{
				DefaultKeyBinding = FInputChord(EModifierKey::Shift, EdModeKeys[editorMode]);
				++editorMode;
			}

			FUICommandInfo::MakeCommandInfo(
				this->AsShared(),
				EditorModeCommand,
				EditorModeCommandName,
				ModeName,
				Tooltip,
				Mode.IconBrush,
				EUserInterfaceActionType::ToggleButton,
				DefaultKeyBinding);

			EditorModeCommands.Add(EditorModeCommand);
		}
	}
}

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
