// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldTreeItem.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/SToolTip.h"
#include "EditorStyleSet.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerDragDrop.h"
#include "SSceneOutliner.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "EditorActorFolders.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"
#include "IDocumentation.h"
#include "FolderTreeItem.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_WorldTreeItem"

namespace SceneOutliner
{
	FText GetWorldDescription(UWorld* World)
	{
		FText Description;
		if (World)
		{
			FText PostFix;
			const FWorldContext* WorldContext = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() == World)
				{
					WorldContext = &Context;
					break;
				}
			}

			if (World->WorldType == EWorldType::PIE)
			{
				switch (World->GetNetMode())
				{
				case NM_Client:
					if (WorldContext)
					{
						PostFix = FText::Format(LOCTEXT("ClientPostfixFormat", "(Client {0})"), FText::AsNumber(WorldContext->PIEInstance - 1));
					}
					else
					{
						PostFix = LOCTEXT("ClientPostfix", "(Client)");
					}
					break;
				case NM_DedicatedServer:
				case NM_ListenServer:
					PostFix = LOCTEXT("ServerPostfix", "(Server)");
					break;
				case NM_Standalone:
					PostFix = LOCTEXT("PlayInEditorPostfix", "(Play In Editor)");
					break;
				}
			}
			else if (World->WorldType == EWorldType::Editor)
			{
				PostFix = LOCTEXT("EditorPostfix", "(Editor)");
			}

			Description = FText::Format(LOCTEXT("WorldFormat", "{0} {1}"), FText::FromString(World->GetFName().GetPlainNameString()), PostFix);
		}

		return Description;
	}
}

const FSceneOutlinerTreeItemType FWorldTreeItem::Type(&ISceneOutlinerTreeItem::Type);

struct SWorldTreeLabel : FSceneOutlinerCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SWorldTreeLabel) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, FWorldTreeItem& WorldItem, ISceneOutliner& SceneOutliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
	{
		TreeItemPtr = StaticCastSharedRef<FWorldTreeItem>(WorldItem.AsShared());
		WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());

		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FSceneOutlinerDefaultTreeItemMetrics::IconPadding())
			[
				SNew(SBox)
				.WidthOverride(FSceneOutlinerDefaultTreeItemMetrics::IconSize())
				.HeightOverride(FSceneOutlinerDefaultTreeItemMetrics::IconSize())
				[
					SNew(SImage)
					.Image(FSlateIconFinder::FindIconBrushForClass(UWorld::StaticClass()))
					.ColorAndOpacity(FSlateColor::UseForeground())
					.ToolTipText(LOCTEXT("WorldIcon_Tooltip", "World"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SWorldTreeLabel::GetDisplayText)
				.HighlightText(SceneOutliner.GetFilterHighlightText())
				.ColorAndOpacity(this, &SWorldTreeLabel::GetForegroundColor)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
					TAttribute<FText>(this, &SWorldTreeLabel::GetTooltipText),
					nullptr,
					TEXT("Shared/LevelEditor/SceneOutliner"),
					TEXT("WorldSettingsLabel")
				))
			]
		];
	}

private:
	TWeakPtr<FWorldTreeItem> TreeItemPtr;

	FText GetDisplayText() const
	{
		auto Item = TreeItemPtr.Pin();
		return Item.IsValid() ? FText::FromString(Item->GetDisplayString()) : FText();
	}

	FText GetTooltipText() const
	{
		auto Item = TreeItemPtr.Pin();
		FText PersistentLevelDisplayName = Item.IsValid() ? FText::FromString(Item->GetWorldName()) : FText();
		if (Item->CanInteract())
		{
			return FText::Format(LOCTEXT("WorldLabel_Tooltip", "The world settings for {0}, double-click to edit"), PersistentLevelDisplayName);
		}
		else
		{
			return FText::Format(LOCTEXT("WorldLabel_TooltipNonInteractive", "The world {0}"), PersistentLevelDisplayName);
		}
	}

	FSlateColor GetForegroundColor() const
	{
		if (auto BaseColor = FSceneOutlinerCommonLabelData::GetForegroundColor(*TreeItemPtr.Pin()))
		{
			return BaseColor.GetValue();
		}

		return FSlateColor::UseForeground();
	}
};

FWorldTreeItem::FWorldTreeItem(UWorld* InWorld)
	: ISceneOutlinerTreeItem(Type)
	, World(InWorld)
	, ID(InWorld)
{
}

FWorldTreeItem::FWorldTreeItem(TWeakObjectPtr<UWorld> InWorld)
	: ISceneOutlinerTreeItem(Type)
	, World(InWorld)
	, ID(InWorld.Get())
{
}

FSceneOutlinerTreeItemID FWorldTreeItem::GetID() const
{
	return ID;
}

FString FWorldTreeItem::GetDisplayString() const
{
	if (UWorld* WorldPtr = World.Get())
	{
		return SceneOutliner::GetWorldDescription(WorldPtr).ToString();
	}
	return FString();
}

FString FWorldTreeItem::GetWorldName() const
{
	if (UWorld* WorldPtr = World.Get())
	{
		return World->GetFName().GetPlainNameString();
	}
	return FString();
}

bool FWorldTreeItem::CanInteract() const
{
	if (UWorld* WorldPtr = World.Get())
	{
		return Flags.bInteractive && WorldPtr->WorldType == EWorldType::Editor;
	}
	return Flags.bInteractive;
}

void FWorldTreeItem::GenerateContextMenu(UToolMenu* Menu, SSceneOutliner& Outliner)
{
	auto SharedOutliner = StaticCastSharedRef<SSceneOutliner>(Outliner.AsShared());
	
	const FSlateIcon WorldSettingsIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.WorldProperties.Tab");
	const FSlateIcon NewFolderIcon(FEditorStyle::GetStyleSetName(), "SceneOutliner.NewFolderIcon");
	FToolMenuSection& Section = Menu->AddSection("Section");
	Section.AddMenuEntry("CreateFolder", LOCTEXT("CreateFolder", "Create Folder"), FText(), NewFolderIcon, FUIAction(FExecuteAction::CreateSP(&Outliner, &SSceneOutliner::CreateFolder)));
	Section.AddMenuEntry("OpenWorldSettings", LOCTEXT("OpenWorldSettings", "World Settings"), FText(), WorldSettingsIcon, FExecuteAction::CreateSP(this, &FWorldTreeItem::OpenWorldSettings));
}

TSharedRef<SWidget> FWorldTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
{
	return SNew(SWorldTreeLabel, *this, Outliner, InRow);
}

void FWorldTreeItem::OpenWorldSettings() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(FName("WorldSettingsTab"));	
}

#undef LOCTEXT_NAMESPACE
