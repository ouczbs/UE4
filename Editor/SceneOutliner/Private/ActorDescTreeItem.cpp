// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorDescTreeItem.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerDragDrop.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ActorEditorUtils.h"
#include "ClassIconFinder.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"
#include "Logging/MessageLog.h"
#include "SSocketChooser.h"
#include "ToolMenus.h"
#include "LevelEditorViewport.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_ActorDescTreeItem"

const FSceneOutlinerTreeItemType FActorDescTreeItem::Type(&ISceneOutlinerTreeItem::Type);

struct SActorDescTreeLabel : FSceneOutlinerCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SActorDescTreeLabel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FActorDescTreeItem& ActorDescItem, ISceneOutliner& SceneOutliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
	{
		WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());

		TreeItemPtr = StaticCastSharedRef<FActorDescTreeItem>(ActorDescItem.AsShared());

		HighlightText = SceneOutliner.GetFilterHighlightText();

		TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

		auto MainContent = SNew(SHorizontalBox)
			// Main actor desc label
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
				.Text(this, &SActorDescTreeLabel::GetDisplayText)
				.ToolTipText(this, &SActorDescTreeLabel::GetTooltipText)
				.HighlightText(HighlightText)
				.ColorAndOpacity(this, &SActorDescTreeLabel::GetForegroundColor)
				.OnTextCommitted(this, &SActorDescTreeLabel::OnLabelCommitted)
				.OnVerifyTextChanged(this, &SActorDescTreeLabel::OnVerifyItemLabelChanged)
				.IsSelected(FIsSelected::CreateSP(&InRow, &STableRow<FSceneOutlinerTreeItemPtr>::IsSelectedExclusively))
				.IsReadOnly_Lambda([Item = ActorDescItem.AsShared(), this]()
			{
				return !CanExecuteRenameRequest(Item.Get());
			})
			]

		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.0f, 0.f, 3.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SActorDescTreeLabel::GetTypeText)
				.Visibility(this, &SActorDescTreeLabel::GetTypeTextVisibility)
				.HighlightText(HighlightText)
			];

		if (WeakSceneOutliner.Pin()->GetMode()->IsInteractive())
		{
			ActorDescItem.RenameRequestEvent.BindSP(InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode);
		}

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
						.Image(this, &SActorDescTreeLabel::GetIcon)
					.ToolTipText(this, &SActorDescTreeLabel::GetIconTooltip)
					.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f)
				[
					MainContent
				]
			];
	}

private:
	TWeakPtr<FActorDescTreeItem> TreeItemPtr;
	TAttribute<FText> HighlightText;

	FText GetDisplayText() const
	{
		if (TSharedPtr<FActorDescTreeItem> TreeItem = TreeItemPtr.Pin())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = TreeItem->ActorDescHandle.GetActorDesc())
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("ActorLabel"), FText::FromName(ActorDesc->GetActorLabel()));
				Args.Add(TEXT("UnloadedTag"), LOCTEXT("UnloadedActorLabel", "(Unloaded)"));
				return FText::Format(LOCTEXT("UnloadedActorDisplay", "{ActorLabel} {UnloadedTag}"), Args);
			}
		}
		return FText();
	}

	FText GetTooltipText() const
	{
		return FText();
	}

	FText GetTypeText() const
	{
		if (TSharedPtr<FActorDescTreeItem> TreeItem = TreeItemPtr.Pin())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = TreeItem->ActorDescHandle.GetActorDesc())
			{
				return FText::FromName(ActorDesc->GetActorClass()->GetFName());
			}
		}

		return FText();
	}

	EVisibility GetTypeTextVisibility() const
	{
		return HighlightText.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	const FSlateBrush* GetIcon() const
	{
		TSharedPtr<FActorDescTreeItem> TreeItem = TreeItemPtr.Pin();

		if (TreeItem.IsValid() && WeakSceneOutliner.IsValid())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = TreeItem->ActorDescHandle.GetActorDesc())
			{
				const FName IconName = ActorDesc->GetActorClass()->GetFName();

				const FSlateBrush* CachedBrush = WeakSceneOutliner.Pin()->GetCachedIconForClass(IconName);
				if (CachedBrush != nullptr)
				{
					return CachedBrush;
				}
				else if (IconName != NAME_None)
				{

					const FSlateBrush* FoundSlateBrush = FSlateIconFinder::FindIconForClass(ActorDesc->GetActorClass()).GetIcon();
					WeakSceneOutliner.Pin()->CacheIconForClass(IconName, FoundSlateBrush);
					return FoundSlateBrush;
				}
			}
		}

		return nullptr;
	}

	const FSlateBrush* GetIconOverlay() const
	{
		return nullptr;
	}

	FText GetIconTooltip() const
	{
		return FText();
	}

	virtual FSlateColor GetForegroundColor() const override
	{
		if (const auto TreeItem = TreeItemPtr.Pin())
		{
			if (auto BaseColor = FSceneOutlinerCommonLabelData::GetForegroundColor(*TreeItem))
			{
				return BaseColor.GetValue();
			}
		}

		return FSceneOutlinerCommonLabelData::DarkColor;
	}

	bool OnVerifyItemLabelChanged(const FText&, FText&)
	{
		// don't allow label change for unloaded actor items
		return false;
	}

	void OnLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo)
	{
		// not supported.
	}
};

FActorDescTreeItem::FActorDescTreeItem(const FGuid& InActorGuid, UActorDescContainer* Container)
	: ISceneOutlinerTreeItem(Type)
	, ActorDescHandle(InActorGuid, Container)
{
	ID = FSceneOutlinerTreeItemID(InActorGuid);
}

FSceneOutlinerTreeItemID FActorDescTreeItem::GetID() const
{
	return ID;
}

FString FActorDescTreeItem::GetDisplayString() const
{
	const FWorldPartitionActorDesc* ActorDesc = ActorDescHandle.GetActorDesc();
	return ActorDesc ? ActorDesc->GetActorLabel().ToString() : LOCTEXT("ActorLabelForMissingActor", "(Deleted Actor)").ToString();
}

bool FActorDescTreeItem::CanInteract() const
{
	return ActorDescHandle.IsValid();
}

TSharedRef<SWidget> FActorDescTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
{
	return SNew(SActorDescTreeLabel, *this, Outliner, InRow);
}

void FActorDescTreeItem::FocusActorBounds() const
{
	if (FWorldPartitionActorDesc const* ActorDesc = ActorDescHandle.GetActorDesc())
	{
		if (FLevelEditorViewportClient* LevelViewportClient = GCurrentLevelEditingViewportClient)
		{
			LevelViewportClient->FocusViewportOnBox(ActorDesc->GetBounds(), false);
		}	
	}
}

void FActorDescTreeItem::LoadUnloadedActor() const
{
	if (const FWorldPartitionActorDesc* ActorDesc = ActorDescHandle.GetActorDesc())
	{
		if (UActorDescContainer* ActorDescContainer = ActorDescHandle.GetActorDescContainer().Get())
		{
			new FWorldPartitionReference(ActorDescContainer, ActorDesc->GetGuid());
		}
	}
}

void FActorDescTreeItem::GenerateContextMenu(UToolMenu* Menu, SSceneOutliner&)
{
	FToolMenuSection& Section = Menu->AddSection("Section");
	Section.AddMenuEntry("FocusActorBounds", LOCTEXT("FocusActorBounds", "Focus Actor Bounds"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FActorDescTreeItem::FocusActorBounds)));
	Section.AddMenuEntry("LoadUnloadedActor", LOCTEXT("LoadUnloadedActor", "Load Unloaded Actor"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FActorDescTreeItem::LoadUnloadedActor)));
}

void FActorDescTreeItem::OnVisibilityChanged(const bool bNewVisibility)
{

}

bool FActorDescTreeItem::GetVisibility() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
