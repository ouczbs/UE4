// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerMode.h"
#include "SSceneOutliner.h"
#include "SDataLayerBrowser.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "DataLayerHierarchy.h"
#include "DataLayerActorTreeItem.h"
#include "DataLayerTreeItem.h"
#include "DataLayerDragDropOp.h"
#include "ISceneOutlinerHierarchy.h"
#include "SceneOutlinerMenuContext.h"
#include "ScopedTransaction.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "DragAndDrop/FolderDragDropOp.h"
#include "ActorMode.h"
#include "EditorActorFolders.h"
#include "Algo/Transform.h"
#include "ToolMenus.h"
#include "Selection.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "DataLayer"

FDataLayerModeParams::FDataLayerModeParams(SSceneOutliner* InSceneOutliner, SDataLayerBrowser* InDataLayerBrowser, const TWeakObjectPtr<UWorld>& InSpecifiedWorldToDisplay)
: SpecifiedWorldToDisplay(InSpecifiedWorldToDisplay)
, DataLayerBrowser(InDataLayerBrowser)
, SceneOutliner(InSceneOutliner)
{}

FDataLayerMode::FDataLayerMode(const FDataLayerModeParams& Params)
	: ISceneOutlinerMode(Params.SceneOutliner)
	, DataLayerBrowser(Params.DataLayerBrowser)
	, SpecifiedWorldToDisplay(Params.SpecifiedWorldToDisplay)
{
	SceneOutliner->AddFilter(MakeShared<TSceneOutlinerPredicateFilter<FDataLayerActorTreeItem>>(FDataLayerActorTreeItem::FFilterPredicate::CreateLambda([this](const AActor* Actor, const UDataLayer* DataLayer)
	{
		return FActorMode::IsActorDisplayable(SceneOutliner, Actor);
	}), FSceneOutlinerFilter::EDefaultBehaviour::Pass));

	DataLayerEditorSubsystem = UDataLayerEditorSubsystem::Get();
	Rebuild();
	const_cast<FSharedSceneOutlinerData&>(SceneOutliner->GetSharedData()).CustomDelete = FCustomSceneOutlinerDeleteDelegate::CreateRaw(this, &FDataLayerMode::DeleteItems);
}

int32 FDataLayerMode::GetTypeSortPriority(const ISceneOutlinerTreeItem& Item) const
{
	if (Item.IsA<FDataLayerTreeItem>())
	{
		return (int32)EItemSortOrder::DataLayer;
	}
	else if (Item.IsA<FDataLayerActorTreeItem>())
	{
		return (int32)EItemSortOrder::Actor;
	}
	// Warning: using actor mode with an unsupported item type!
	check(false);
	return -1;
}

bool FDataLayerMode::CanRenameItem(const ISceneOutlinerTreeItem& Item) const 
{
	if (Item.IsValid() && (Item.IsA<FDataLayerTreeItem>()))
	{
		FDataLayerTreeItem* DataLayerTreeItem = (FDataLayerTreeItem*)&Item;
		return !DataLayerTreeItem->GetDataLayer()->IsLocked();
	}
	return false;
}

FText FDataLayerMode::GetStatusText() const
{
	if (SelectedDataLayersSet.Num() == 1)
	{
		if (const UDataLayer* DataLayer = SelectedDataLayersSet.Array()[0].Get())
		{
			return FText::FromName(DataLayer->GetDataLayerLabel());
		}
	}
	else if (SelectedDataLayersSet.Num() > 1)
	{
		return FText::Format(LOCTEXT("DetailsMultipleDataLayersSelected", "{0} Data Layers Selected"), FText::AsNumber(SelectedDataLayersSet.Num()));
	}
	return LOCTEXT("DetailsNoDataLayerSelected", "No Data Layer Selected");
}

SDataLayerBrowser* FDataLayerMode::GetDataLayerBrowser() const
{
	return DataLayerBrowser;
}

void FDataLayerMode::OnItemAdded(FSceneOutlinerTreeItemPtr Item)
{
	if (const FDataLayerTreeItem* DataLayerItem = Item->CastTo<FDataLayerTreeItem>())
	{
		if (!Item->Flags.bIsFilteredOut)
		{
			if (SelectedDataLayersSet.Contains(DataLayerItem->GetDataLayer()))
			{
				SceneOutliner->AddToSelection({Item});
			}
		}
	}
	else if (const FDataLayerActorTreeItem* DataLayerActorTreeItem = Item->CastTo<FDataLayerActorTreeItem>())
	{
		if (SelectedDataLayerActors.Contains(FSelectedDataLayerActor(DataLayerActorTreeItem->GetDataLayer(), DataLayerActorTreeItem->GetActor())))
		{
			SceneOutliner->AddToSelection({Item});
		}
	}
}

void FDataLayerMode::OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item)
{
	if (const FDataLayerTreeItem* DataLayerItem = Item->CastTo<FDataLayerTreeItem>())
	{
		if (UDataLayer* DataLayer = DataLayerItem->GetDataLayer())
		{
			const FScopedTransaction Transaction(LOCTEXT("SelectActorsInDataLayer", "Select Actors in Data Layer"));
			GEditor->SelectNone(/*bNoteSelectionChange*/false, true);
			DataLayerEditorSubsystem->SelectActorsInDataLayer(DataLayer, /*bSelect*/true, /*bNotify*/true, /*bSelectEvenIfHidden*/true);
		}
	}
	else if (FDataLayerActorTreeItem* DataLayerActorItem = Item->CastTo<FDataLayerActorTreeItem>())
	{
		if (AActor * Actor = DataLayerActorItem->GetActor())
		{
			const FScopedTransaction Transaction(LOCTEXT("ClickingOnActor", "Clicking on Actor in Data Layer"));
			GEditor->GetSelectedActors()->Modify();
			GEditor->SelectNone(/*bNoteSelectionChange*/false, true);
			GEditor->SelectActor(Actor, /*bSelected*/true, /*bNotify*/true, /*bSelectEvenIfHidden*/true);
			GEditor->NoteSelectionChange();
			GEditor->MoveViewportCamerasToActor(*Actor, /*bActiveViewportOnly*/false);
		}
	}
}

void FDataLayerMode::DeleteItems(const TArray<TWeakPtr<ISceneOutlinerTreeItem>>& Items)
{
	TArray<UDataLayer*> DataLayersToDelete;
	TMap<UDataLayer*, TArray<AActor*>> ActorsToRemoveFromDataLayer;

	for (const TWeakPtr<ISceneOutlinerTreeItem>& Item : Items)
	{
		if (FDataLayerActorTreeItem* DataLayerActorItem = Item.Pin()->CastTo<FDataLayerActorTreeItem>())
		{
			UDataLayer* DataLayer = DataLayerActorItem->GetDataLayer();
			AActor* Actor = DataLayerActorItem->GetActor();
			if (DataLayer && !DataLayer->IsLocked() && Actor)
			{
				ActorsToRemoveFromDataLayer.FindOrAdd(DataLayer).Add(Actor);
			}
		}
		else if (FDataLayerTreeItem* DataLayerItem = Item.Pin()->CastTo< FDataLayerTreeItem>())
		{
			if (UDataLayer* DataLayer = DataLayerItem->GetDataLayer())
			{
				if (!DataLayer->IsLocked())
				{
					DataLayersToDelete.Add(DataLayer);
				}
			}
		}
	}

	if (!ActorsToRemoveFromDataLayer.IsEmpty())
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveActorsFromDataLayer", "Remove Actors from Data Layer"));
		for (const auto& ItPair : ActorsToRemoveFromDataLayer)
		{
			DataLayerEditorSubsystem->RemoveActorsFromDataLayer(ItPair.Value, ItPair.Key);
		}
	}
	else if (!DataLayersToDelete.IsEmpty())
	{
		int32 PrevDeleteCount = SelectedDataLayersSet.Num();
		for (UDataLayer* DataLayerToDelete : DataLayersToDelete)
		{
			SelectedDataLayersSet.Remove(DataLayerToDelete);
		}
		
		{
			const FScopedTransaction Transaction(LOCTEXT("DeleteDataLayers", "Delete Data Layers"));
			DataLayerEditorSubsystem->DeleteDataLayers(DataLayersToDelete);
		}

		if ((SelectedDataLayersSet.Num() != PrevDeleteCount) && DataLayerBrowser)
		{
			DataLayerBrowser->OnSelectionChanged(SelectedDataLayersSet);
		}
	}
}

FReply FDataLayerMode::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	const FSceneOutlinerItemSelection& Selection = SceneOutliner->GetSelection();

	// Rename key: Rename selected actors (not rebindable, because it doesn't make much sense to bind.)
	if (InKeyEvent.GetKey() == EKeys::F2)
	{
		if (Selection.Num() == 1)
		{
			FSceneOutlinerTreeItemPtr ItemToRename = Selection.SelectedItems[0].Pin();
			if (ItemToRename.IsValid() && CanRenameItem(*ItemToRename) && ItemToRename->CanInteract())
			{
				SceneOutliner->SetPendingRenameItem(ItemToRename);
				SceneOutliner->ScrollItemIntoView(ItemToRename);
			}
			return FReply::Handled();
		}
	}

	// F5 forces a full refresh
	else if (InKeyEvent.GetKey() == EKeys::F5)
	{
		SceneOutliner->FullRefresh();
		return FReply::Handled();
	}

	// Delete/BackSpace keys delete selected actors
	else if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
	{
		DeleteItems(Selection.SelectedItems);
		return FReply::Handled();

	}
	return FReply::Unhandled();
}

bool FDataLayerMode::ParseDragDrop(FSceneOutlinerDragDropPayload& OutPayload, const FDragDropOperation& Operation) const
{
	return !GetActorsFromOperation(Operation, true).IsEmpty();
}

FSceneOutlinerDragValidationInfo FDataLayerMode::ValidateDrop(const ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload) const
{
	TArray<AActor*> PayloadActors = GetActorsFromOperation(Payload.SourceOperation);
	if (!PayloadActors.IsEmpty())
	{
		for (AActor* Actor : PayloadActors)
		{
			if (!Actor->SupportsDataLayer())
			{
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, LOCTEXT("ActorCantBeAssignedToDataLayer", "Can't assign actors to Data Layer"));
			}
		}

		if (const FDataLayerTreeItem* DataLayerItem = DropTarget.CastTo<FDataLayerTreeItem>())
		{
			const UDataLayer* DataLayerTarget = DataLayerItem->GetDataLayer();
			if (!DataLayerTarget)
			{
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, FText());
			}
			
			if (DataLayerTarget->IsLocked())
			{
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, LOCTEXT("CantReassignLockedDataLayer", "Can't reassign actors from locked Data Layer"));
			}
			
			if (GetSelectedDataLayers(SceneOutliner).Num() > 1)
			{
				TArray<UDataLayer*> SelectedDataLayers = GetSelectedDataLayers(SceneOutliner);
				for (UDataLayer* SelectedDataLayer : SelectedDataLayers)
				{
					if (SelectedDataLayer->IsLocked())
					{
						return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::IncompatibleGeneric, LOCTEXT("CantReassignLockedDataLayer", "Can't reassign actors from locked Data Layer"));
					}
				}

				if (SceneOutliner->GetTree().IsItemSelected(const_cast<ISceneOutlinerTreeItem&>(DropTarget).AsShared()))
				{
					return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::Compatible, LOCTEXT("AssignToDataLayers", "Assign to Selected Data Layers"));
				}
			}

			return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::Compatible, FText::Format(LOCTEXT("AssignToDataLayer", "Assign to Data Layer \"{0}\""), FText::FromName(DataLayerTarget->GetDataLayerLabel())));
		}
		else
		{
			if (!PayloadActors[0]->HasDataLayers())
			{
				// Only allow actors not coming from the DataLayerBrowser
				return FSceneOutlinerDragValidationInfo(ESceneOutlinerDropCompatibility::Compatible, LOCTEXT("AssignToNewDataLayer", "Assign to New Data Layer"));
			}
		}
	}
	return FSceneOutlinerDragValidationInfo::Invalid();
}

TArray<AActor*> FDataLayerMode::GetActorsFromOperation(const FDragDropOperation& Operation, bool bOnlyFindFirst) const
{
	TSet<AActor*> Actors;

	auto GetActorsFromFolderOperation = [&Actors, bOnlyFindFirst](const FFolderDragDropOp& FolderOp)
	{
		if (bOnlyFindFirst && Actors.Num())
		{
			return;
		}

		if (UWorld* World = FolderOp.World.Get())
		{
			TArray<TWeakObjectPtr<AActor>> ActorsToDrop;
			FActorFolders::GetWeakActorsFromFolders(*World, FolderOp.Folders, ActorsToDrop);
			for (const auto& Actor : ActorsToDrop)
			{
				if (AActor* ActorPtr = Actor.Get())
				{
					Actors.Add(ActorPtr);
					if (bOnlyFindFirst)
					{
						break;
					}
				}
			}
		}
	};

	auto GetActorsFromActorOperation = [&Actors, bOnlyFindFirst](const FActorDragDropOp& ActorOp)
	{
		if (bOnlyFindFirst && Actors.Num())
		{
			return;
		}
		for (const auto& Actor : ActorOp.Actors)
		{
			if (AActor* ActorPtr = Actor.Get())
			{
				Actors.Add(ActorPtr);
				if (bOnlyFindFirst)
				{
					break;
				}
			}
		}
	};

	if (Operation.IsOfType<FActorDragDropOp>())
	{
		const FActorDragDropOp& ActorDragOp = StaticCast<const FActorDragDropOp&>(Operation);
		GetActorsFromActorOperation(ActorDragOp);
	}
	if (Operation.IsOfType<FFolderDragDropOp>())
	{
		const FFolderDragDropOp& FolderDragOp = StaticCast<const FFolderDragDropOp&>(Operation);
		GetActorsFromFolderOperation(FolderDragOp);
	}
	if (Operation.IsOfType<FCompositeDragDropOp>())
	{
		const FCompositeDragDropOp& CompositeDragOp = StaticCast<const FCompositeDragDropOp&>(Operation);
		if (TSharedPtr<const FActorDragDropOp> ActorSubOp = CompositeDragOp.GetSubOp<FActorDragDropOp>())
		{
			GetActorsFromActorOperation(*ActorSubOp);
		}
		if (TSharedPtr<const FFolderDragDropOp> FolderSubOp = CompositeDragOp.GetSubOp<FFolderDragDropOp>())
		{
			GetActorsFromFolderOperation(*FolderSubOp);
		}
	}
	return Actors.Array();
}

void FDataLayerMode::OnDrop(ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload, const FSceneOutlinerDragValidationInfo& ValidationInfo) const
{
	TArray<AActor*> ActorsToAdd = GetActorsFromOperation(Payload.SourceOperation);
	if (ActorsToAdd.IsEmpty())
	{
		return;
	}

	if (const FDataLayerTreeItem* DataLayerItem = DropTarget.CastTo<FDataLayerTreeItem>())
	{
		if (UDataLayer* DataLayer = DataLayerItem->GetDataLayer())
		{
			if (SceneOutliner->GetTree().IsItemSelected(const_cast<ISceneOutlinerTreeItem&>(DropTarget).AsShared()))
			{
				TArray<UDataLayer*> AllSelectedDataLayers = GetSelectedDataLayers(SceneOutliner);
				if (AllSelectedDataLayers.Num() > 1)
				{
					const FScopedTransaction Transaction(LOCTEXT("DataLayerOutlinerAddActorsToDataLayers", "Add Actors to Data Layers"));
					DataLayerEditorSubsystem->AddActorsToDataLayers(ActorsToAdd, AllSelectedDataLayers);
					return;
				}
			}

			const FScopedTransaction Transaction(LOCTEXT("DataLayerOutlinerAddActorsToDataLayer", "Add Actors to Data Layer"));
			DataLayerEditorSubsystem->AddActorsToDataLayer(ActorsToAdd, DataLayer);
		}
	}
	else
	{
		if (!ActorsToAdd[0]->HasDataLayers())
		{
			// Only allow actors not coming from the DataLayerBrowser
			const FScopedTransaction Transaction(LOCTEXT("AddSelectedActorsToNewDataLayer", "Add Actors to New Data Layer"));
			if (UDataLayer* NewDataLayer = DataLayerEditorSubsystem->CreateDataLayer())
			{
				DataLayerEditorSubsystem->AddActorsToDataLayer(ActorsToAdd, NewDataLayer);
			}
		}
	}
}

struct FWeakDataLayerActorSelector
{
	bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, TWeakObjectPtr<AActor>& DataOut) const
	{
		if (TSharedPtr<ISceneOutlinerTreeItem> ItemPtr = Item.Pin())
		{
			if (FDataLayerActorTreeItem* TypedItem = ItemPtr->CastTo<FDataLayerActorTreeItem>())
			{
				if (TypedItem->IsValid())
				{
					DataOut = TypedItem->Actor;
					return true;
				}
			}
		}
		return false;
	}
};

struct FWeakDataLayerSelector
{
	bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, TWeakObjectPtr<UDataLayer>& DataOut) const
	{
		if (TSharedPtr<ISceneOutlinerTreeItem> ItemPtr = Item.Pin())
		{
			if (FDataLayerTreeItem* TypedItem = ItemPtr->CastTo<FDataLayerTreeItem>())
			{
				if (TypedItem->IsValid())
				{
					DataOut = TypedItem->GetDataLayer();
					return true;
				}
			}
		}
		return false;
	}
};

TSharedPtr<FDragDropOperation> FDataLayerMode::CreateDragDropOperation(const TArray<FSceneOutlinerTreeItemPtr>& InTreeItems) const
{
	FSceneOutlinerDragDropPayload DraggedObjects(InTreeItems);

	TSharedPtr<FSceneOutlinerDragDropOp> OutlinerOp = MakeShareable(new FSceneOutlinerDragDropOp());

	if (DraggedObjects.Has<FDataLayerActorTreeItem>())
	{
		TSharedPtr<FActorDragDropOp> ActorOperation = MakeShareable(new FActorDragDropOp);
		ActorOperation->Init(DraggedObjects.GetData<TWeakObjectPtr<AActor>>(FWeakDataLayerActorSelector()));
		OutlinerOp->AddSubOp(ActorOperation);
	}

	if (DraggedObjects.Has<FDataLayerTreeItem>())
	{
		TSharedPtr<FDataLayerDragDropOp> DataLayerOperation = MakeShareable(new FDataLayerDragDropOp);
		TArray<TWeakObjectPtr<UDataLayer>> DataLayers = DraggedObjects.GetData<TWeakObjectPtr<UDataLayer>>(FWeakDataLayerSelector());
		for (const auto& DataLayer : DataLayers)
		{
			if (DataLayer.IsValid())
			{
				DataLayerOperation->DataLayerLabels.Add(DataLayer->GetDataLayerLabel());
			}
		}
		DataLayerOperation->Construct();
		OutlinerOp->AddSubOp(DataLayerOperation);
	}

	OutlinerOp->Construct();
	return OutlinerOp;
}

static const FName DefaultContextBaseMenuName("DataLayerOutliner.DefaultContextMenuBase");
static const FName DefaultContextMenuName("DataLayerOutliner.DefaultContextMenu");

TArray<UDataLayer*> FDataLayerMode::GetSelectedDataLayers(SSceneOutliner* InSceneOutliner) const
{
	FSceneOutlinerItemSelection ItemSelection(InSceneOutliner->GetSelection());
	TArray<FDataLayerTreeItem*> SelectedDataLayerItems;
	ItemSelection.Get<FDataLayerTreeItem>(SelectedDataLayerItems);
	TArray<UDataLayer*> ValidSelectedDataLayers;
	Algo::TransformIf(SelectedDataLayerItems, ValidSelectedDataLayers, [](const auto Item) { return Item && Item->GetDataLayer(); }, [](const auto Item) { return Item->GetDataLayer(); });
	return MoveTemp(ValidSelectedDataLayers);
}

void FDataLayerMode::RegisterContextMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	if (!ToolMenus->IsMenuRegistered(DefaultContextBaseMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(DefaultContextBaseMenuName);

		Menu->AddDynamicSection("DataLayerDynamicSection", FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
		{
			USceneOutlinerMenuContext* Context = InMenu->FindContext<USceneOutlinerMenuContext>();
			if (!Context || !Context->SceneOutliner.IsValid())
			{
				return;
			}

			SSceneOutliner* SceneOutliner = Context->SceneOutliner.Pin().Get();
			TArray<UDataLayer*> SelectedDataLayers = GetSelectedDataLayers(SceneOutliner);

			TArray<const UDataLayer*> AllDataLayers;
			if (const AWorldDataLayers* WorldDataLayers = AWorldDataLayers::Get(RepresentingWorld.Get()))
			{
				WorldDataLayers->ForEachDataLayer([&AllDataLayers](UDataLayer* DataLayer)
				{
					AllDataLayers.Add(DataLayer);
					return true;
				});
			}

			{
				FToolMenuSection& Section = InMenu->AddSection("DataLayers", LOCTEXT("DataLayers", "Data Layers"));
				Section.AddMenuEntry("CreateEmptyDataLayer", LOCTEXT("CreateEmptyDataLayer", "Create Empty Data Layer"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							const FScopedTransaction Transaction(LOCTEXT("CreateEmptyDataLayer", "Create Empty Data Layer"));
							DataLayerEditorSubsystem->CreateDataLayer();
							})
					));

				Section.AddMenuEntry("AddSelectedActorsToNewDataLayer", LOCTEXT("AddSelectedActorsToNewDataLayer", "Add Selected Actors to New Data Layer"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							const FScopedTransaction Transaction(LOCTEXT("AddSelectedActorsToNewDataLayer", "Add Selected Actors to New Data Layer"));
							if (UDataLayer* NewDataLayer = DataLayerEditorSubsystem->CreateDataLayer())
							{
								DataLayerEditorSubsystem->AddSelectedActorsToDataLayer(NewDataLayer);
							}}),
						FCanExecuteAction::CreateLambda([=] { return GEditor->GetSelectedActorCount() > 0; })
					));

				Section.AddMenuEntry("AddSelectedActorsToSelectedDataLayers", LOCTEXT("AddSelectedActorsToSelectedDataLayers", "Add Selected Actors to Selected Data Layers"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!SelectedDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("AddSelectedActorsToSelectedDataLayers", "Add Selected Actors to Selected Data Layers"));
								DataLayerEditorSubsystem->AddSelectedActorsToDataLayers(SelectedDataLayers);
							}}),
						FCanExecuteAction::CreateLambda([=] { return !SelectedDataLayers.IsEmpty() && GEditor->GetSelectedActorCount() > 0; })
					));

				Section.AddSeparator("SectionsSeparator");

				Section.AddMenuEntry("RemoveSelectedActorsFromSelectedDataLayers", LOCTEXT("RemoveSelectedActorsFromSelectedDataLayers", "Remove Selected Actors from Selected Data Layers"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!SelectedDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("RemoveSelectedActorsFromSelectedDataLayers", "Remove Selected Actors from Selected Data Layers"));
								DataLayerEditorSubsystem->RemoveSelectedActorsFromDataLayers(SelectedDataLayers);
							}}),
						FCanExecuteAction::CreateLambda([=] { return !SelectedDataLayers.IsEmpty() && GEditor->GetSelectedActorCount() > 0; })
					));

				Section.AddMenuEntry("DeleteSelectedDataLayers", LOCTEXT("DeleteSelectedDataLayers", "Delete Selected Data Layers"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!SelectedDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("DeleteSelectedDataLayers", "Delete Selected Data Layers"));
								DataLayerEditorSubsystem->DeleteDataLayers(SelectedDataLayers);
							}}),
						FCanExecuteAction::CreateLambda([=] { 
							for (UDataLayer* SelectedDataLayer : SelectedDataLayers)
							{
								if (SelectedDataLayer->IsLocked())
								{
									return false;
								}
							}
							return !SelectedDataLayers.IsEmpty();
						})
					));

				Section.AddMenuEntry("RenameSelectedDataLayer", LOCTEXT("RenameSelectedDataLayer", "Rename Selected Data Layer"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							if (SelectedDataLayers.Num() == 1)
							{
								FSceneOutlinerTreeItemPtr ItemToRename = SceneOutliner->GetTreeItem(SelectedDataLayers[0]);
								if (ItemToRename.IsValid() && CanRenameItem(*ItemToRename) && ItemToRename->CanInteract())
								{
									SceneOutliner->SetPendingRenameItem(ItemToRename);
									SceneOutliner->ScrollItemIntoView(ItemToRename);
								}
							}}),
						FCanExecuteAction::CreateLambda([=] { return (SelectedDataLayers.Num() == 1) && !SelectedDataLayers[0]->IsLocked(); })
					));

				Section.AddSeparator("SectionsSeparator");
			}

			{
				FToolMenuSection& Section = InMenu->AddSection("DataLayerSelection", LOCTEXT("DataLayerSelection", "Selection"));

				Section.AddMenuEntry("SelectActorsInDataLayers", LOCTEXT("SelectActorsInDataLayers", "Select Actors in Data Layers"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!SelectedDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("SelectActorsInDataLayers", "Select Actors in Data Layers"));
								GEditor->SelectNone(/*bNoteSelectionChange*/false, /*bDeselectBSPSurfs*/true);
								DataLayerEditorSubsystem->SelectActorsInDataLayers(SelectedDataLayers, /*bSelect*/true, /*bNotify*/true, /*bSelectEvenIfHidden*/true);
							}}),
						FCanExecuteAction::CreateLambda([=] { return !SelectedDataLayers.IsEmpty(); })
					));

				Section.AddMenuEntry("AppendActorsToSelection", LOCTEXT("AppendActorsToSelection", "Append Actors in Data Layer to Selection"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!SelectedDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("AppendActorsToSelection", "Append Actors in Data Layer to Selection"));
								DataLayerEditorSubsystem->SelectActorsInDataLayers(SelectedDataLayers, /*bSelect*/true, /*bNotify*/true, /*bSelectEvenIfHidden*/true);
							}}),
						FCanExecuteAction::CreateLambda([=] { return !SelectedDataLayers.IsEmpty(); })
					));

				Section.AddMenuEntry("DeselectActors", LOCTEXT("DeselectActors", "Deselect Actors in Data Layer"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!SelectedDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("DeselectActors", "Deselect Actors in Data Layer"));
								DataLayerEditorSubsystem->SelectActorsInDataLayers(SelectedDataLayers, /*bSelect*/false, /*bNotifySelectActors*/true);
							}}),
						FCanExecuteAction::CreateLambda([=] { return !SelectedDataLayers.IsEmpty(); })
					));
			}

			{
				FToolMenuSection& Section = InMenu->AddSection("DataLayerVisibility", LOCTEXT("DataLayerVisibility", "Visibility"));

				Section.AddMenuEntry("MakeAllDataLayersVisible", LOCTEXT("MakeAllDataLayersVisible", "Make All Data Layers Visible"), FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							check(!AllDataLayers.IsEmpty());
							{
								const FScopedTransaction Transaction(LOCTEXT("MakeAllDataLayersVisible", "Make All Data Layers Visible"));
								DataLayerEditorSubsystem->MakeAllDataLayersVisible();
							}}),
						FCanExecuteAction::CreateLambda([=] { return !AllDataLayers.IsEmpty(); })
					));
			}
		}));
	}

	if (!ToolMenus->IsMenuRegistered(DefaultContextMenuName))
	{
		ToolMenus->RegisterMenu(DefaultContextMenuName, DefaultContextBaseMenuName);
	}
}

TSharedPtr<SWidget> FDataLayerMode::CreateContextMenu()
{
	RegisterContextMenu();

	FSceneOutlinerItemSelection ItemSelection(SceneOutliner->GetSelection());

	USceneOutlinerMenuContext* ContextObject = NewObject<USceneOutlinerMenuContext>();
	ContextObject->SceneOutliner = StaticCastSharedRef<SSceneOutliner>(SceneOutliner->AsShared());
	ContextObject->bShowParentTree = SceneOutliner->GetSharedData().bShowParentTree;
	ContextObject->NumSelectedItems = ItemSelection.Num();
	FToolMenuContext Context(ContextObject);

	FName MenuName = DefaultContextMenuName;
	SceneOutliner->GetSharedData().ModifyContextMenu.ExecuteIfBound(MenuName, Context);

	// Build up the menu for a selection
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, Context);
	for (const FToolMenuSection& Section : Menu->Sections)
	{
		if (Section.Blocks.Num() > 0)
		{
			return ToolMenus->GenerateWidget(Menu);
		}
	}

	return nullptr;
}

TUniquePtr<ISceneOutlinerHierarchy> FDataLayerMode::CreateHierarchy()
{
	TUniquePtr<FDataLayerHierarchy> ActorHierarchy = FDataLayerHierarchy::Create(this, RepresentingWorld);
	return ActorHierarchy;
}

void FDataLayerMode::OnItemSelectionChanged(FSceneOutlinerTreeItemPtr TreeItem, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection)
{
	SelectedDataLayersSet.Empty();
	SelectedDataLayerActors.Empty();
	Selection.ForEachItem<FDataLayerTreeItem>([this](const FDataLayerTreeItem& Item) { SelectedDataLayersSet.Add(Item.GetDataLayer()); });
	Selection.ForEachItem<FDataLayerActorTreeItem>([this](const FDataLayerActorTreeItem& Item) { SelectedDataLayerActors.Add(FSelectedDataLayerActor(Item.GetDataLayer(), Item.GetActor())); });
	if (DataLayerBrowser)
	{
		DataLayerBrowser->OnSelectionChanged(SelectedDataLayersSet);
	}
}

void FDataLayerMode::Rebuild()
{
	ChooseRepresentingWorld();
	
	Hierarchy = CreateHierarchy();
}

void FDataLayerMode::ChooseRepresentingWorld()
{
	// Select a world to represent

	RepresentingWorld = nullptr;

	// If a specified world was provided, represent it
	if (SpecifiedWorldToDisplay.IsValid())
	{
		RepresentingWorld = SpecifiedWorldToDisplay.Get();
	}

	// check if the user-chosen world is valid and in the editor contexts

	if (!RepresentingWorld.IsValid() && UserChosenWorld.IsValid())
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UserChosenWorld.Get() == Context.World())
			{
				RepresentingWorld = UserChosenWorld.Get();
				break;
			}
		}
	}

	// If the user did not manually select a world, try to pick the most suitable world context
	if (!RepresentingWorld.IsValid())
	{
		// ideally we want a PIE world that is standalone or the first client
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && Context.WorldType == EWorldType::PIE)
			{
				if (World->GetNetMode() == NM_Standalone)
				{
					RepresentingWorld = World;
					break;
				}
				else if (World->GetNetMode() == NM_Client && Context.PIEInstance == 2)	// Slightly dangerous: assumes server is always PIEInstance = 1;
				{
					RepresentingWorld = World;
					break;
				}
			}
		}
	}

	if (RepresentingWorld == nullptr)
	{
		// still not world so fallback to old logic where we just prefer PIE over Editor

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				RepresentingWorld = Context.World();
				break;
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				RepresentingWorld = Context.World();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
