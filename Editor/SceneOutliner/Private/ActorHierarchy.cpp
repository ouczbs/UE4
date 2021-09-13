// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorHierarchy.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldTreeItem.h"
#include "ActorTreeItem.h"
#include "ActorDescTreeItem.h"
#include "ComponentTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "ISceneOutlinerMode.h"
#include "ActorEditorUtils.h"
#include "LevelUtils.h"
#include "GameFramework/WorldSettings.h"
#include "EditorActorFolders.h"
#include "EditorFolderUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

TUniquePtr<FActorHierarchy> FActorHierarchy::Create(ISceneOutlinerMode* Mode, const TWeakObjectPtr<UWorld>& World)
{
	FActorHierarchy* Hierarchy = new FActorHierarchy(Mode, World);

	GEngine->OnLevelActorAdded().AddRaw(Hierarchy, &FActorHierarchy::OnLevelActorAdded);
	GEngine->OnLevelActorDeleted().AddRaw(Hierarchy, &FActorHierarchy::OnLevelActorDeleted);
	GEngine->OnLevelActorDetached().AddRaw(Hierarchy, &FActorHierarchy::OnLevelActorDetached);
	GEngine->OnLevelActorAttached().AddRaw(Hierarchy, &FActorHierarchy::OnLevelActorAttached);
	GEngine->OnLevelActorFolderChanged().AddRaw(Hierarchy, &FActorHierarchy::OnLevelActorFolderChanged);
	GEngine->OnLevelActorListChanged().AddRaw(Hierarchy, &FActorHierarchy::OnLevelActorListChanged);

	if (World.IsValid())
	{
		if (World->PersistentLevel)
		{
			World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddRaw(Hierarchy, &FActorHierarchy::OnLoadedActorAdded);
			World->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.AddRaw(Hierarchy, &FActorHierarchy::OnLoadedActorRemoved);
		}

		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			WorldPartition->OnActorDescAddedEvent.AddRaw(Hierarchy, &FActorHierarchy::OnActorDescAdded);
			WorldPartition->OnActorDescRemovedEvent.AddRaw(Hierarchy, &FActorHierarchy::OnActorDescRemoved);
		}
	}

	FWorldDelegates::LevelAddedToWorld.AddRaw(Hierarchy, &FActorHierarchy::OnLevelAdded);
	FWorldDelegates::LevelRemovedFromWorld.AddRaw(Hierarchy, &FActorHierarchy::OnLevelRemoved);

	auto& Folders = FActorFolders::Get();
	Folders.OnFolderCreate.AddRaw(Hierarchy, &FActorHierarchy::OnBroadcastFolderCreate);
	Folders.OnFolderMove.AddRaw(Hierarchy, &FActorHierarchy::OnBroadcastFolderMove);
	Folders.OnFolderDelete.AddRaw(Hierarchy, &FActorHierarchy::OnBroadcastFolderDelete);

	return TUniquePtr<FActorHierarchy>(Hierarchy);
}

FActorHierarchy::FActorHierarchy(ISceneOutlinerMode* Mode, const TWeakObjectPtr<UWorld>& World)
	: ISceneOutlinerHierarchy(Mode)
	, RepresentingWorld(World)
{
}

FActorHierarchy::~FActorHierarchy()
{
	if (GEngine)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
		GEngine->OnLevelActorDetached().RemoveAll(this);
		GEngine->OnLevelActorAttached().RemoveAll(this);
		GEngine->OnLevelActorFolderChanged().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
	}

	if (RepresentingWorld.IsValid())
	{
		if (RepresentingWorld->PersistentLevel)
		{
			RepresentingWorld->PersistentLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
			RepresentingWorld->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.RemoveAll(this);
		}

		if (UWorldPartition* WorldPartition = RepresentingWorld->GetWorldPartition())
		{
			WorldPartition->OnActorDescAddedEvent.RemoveAll(this);
			WorldPartition->OnActorDescRemovedEvent.RemoveAll(this);
		}
	}

	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);


	if (FActorFolders::IsAvailable())
	{
		auto& Folders = FActorFolders::Get();
		Folders.OnFolderCreate.RemoveAll(this);
		Folders.OnFolderMove.RemoveAll(this);
		Folders.OnFolderDelete.RemoveAll(this);
	}
}

FSceneOutlinerTreeItemPtr FActorHierarchy::FindParent(const ISceneOutlinerTreeItem& Item, const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items) const
{
	if (Item.IsA<FWorldTreeItem>())
	{
		return nullptr;
	}
	else if (const FActorTreeItem* ActorTreeItem = Item.CastTo<FActorTreeItem>())
	{
		if (AActor* Actor = ActorTreeItem->Actor.Get())
		{
			if (const AActor* ParentActor = Actor->GetSceneOutlinerParent())
			{
				if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(ParentActor))
				{
					return *ParentItem;
				} // IF Parent can be listed in SceneOutliner return nullptr so it gets created
				else if(ParentActor->IsListedInSceneOutliner())
				{
					return nullptr;
				}
			}

			if (const ULevelInstanceSubsystem* LevelInstanceSubsystem = RepresentingWorld->GetSubsystem<ULevelInstanceSubsystem>())
			{
				if (const ALevelInstance* OwningLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(Actor))
				{
					const ALevelInstance* LevelInstanceActor = Cast<ALevelInstance>(Actor);
					const bool bIsAnEditingLevelInstance = LevelInstanceActor ? LevelInstanceActor->IsEditing() : false;
					// Parent this to a LevelInstance if the parent LevelInstance is being edited or if this is a sub LevelInstance which is being edited
					if (bShowingLevelInstances || (OwningLevelInstance->IsEditing() || bIsAnEditingLevelInstance))
					{
						if (const FSceneOutlinerTreeItemPtr* OwningLevelInstanceItem = Items.Find(OwningLevelInstance))
						{
							return *OwningLevelInstanceItem;
						}
						else
						{
							return nullptr;
						}
					}
				}
			}

			if (Mode->ShouldShowFolders() && !Actor->GetFolderPath().IsNone())
			{
				if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(Actor->GetFolderPath()))
				{
					return *ParentItem;
				}
				else
				{
					return nullptr;
				}
			}

			// Default to the world
			if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(RepresentingWorld.Get()))
			{
				return *ParentItem;
			}
		}
	}
	else if (const FFolderTreeItem* FolderItem = Item.CastTo<FFolderTreeItem>())
	{
		// We should never call FindParents on a folder item if folders are not being shown
		check(Mode->ShouldShowFolders());

		const FName ParentPath = FEditorFolderUtils::GetParentPath(FolderItem->Path);

		const FSceneOutlinerTreeItemPtr* ParentItem = nullptr;
		// If the folder has no parent path, it must be parented to the root world
		if (ParentPath.IsNone())
		{
			ParentItem = Items.Find(RepresentingWorld.Get());
		}
		else
		{
			ParentItem = Items.Find(FEditorFolderUtils::GetParentPath(FolderItem->Path));
		}

		if (ParentItem)
		{
			return *ParentItem;
		}
	}
	else if (const FComponentTreeItem* ComponentTreeItem = Item.CastTo<FComponentTreeItem>())
	{
		const AActor* Owner = ComponentTreeItem->Component->GetOwner();
		if (Owner)
		{
			if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(Owner))
			{
				return *ParentItem;
			}
		}
	}
	else if (const FActorDescTreeItem* ActorDescItem = Item.CastTo<FActorDescTreeItem>())
	{
		const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.GetActorDesc();

		if (ActorDesc)
		{
			const FName FolderPath = ActorDesc->GetFolderPath();
			if (!FolderPath.IsNone())
			{
				if (const FSceneOutlinerTreeItemPtr* UnloadedActorItem = Items.Find(FolderPath))
				{
					return *UnloadedActorItem;
				}
			}
		}
		// Default to the world
		if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(RepresentingWorld.Get()))
		{
			return *ParentItem;
		}
	}
	return nullptr;
}

void FActorHierarchy::CreateComponentItems(const AActor* Actor, TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	check(Actor);
	// Add all this actors components if showing components and the owning actor was created
	if (bShowingComponents)
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr)
			{
				if (FSceneOutlinerTreeItemPtr ComponentItem = Mode->CreateItemFor<FComponentTreeItem>(Component))
				{
					OutItems.Add(ComponentItem);
				}
			}
		}
	}
}

void FActorHierarchy::CreateWorldChildren(UWorld* World, TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	check(World);

	if (Mode->ShouldShowFolders())
	{
		// Add any folders which might match the current search terms
		for (const auto& Pair : FActorFolders::Get().GetFolderPropertiesForWorld(*World))
		{
			if (FSceneOutlinerTreeItemPtr FolderItem = Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(Pair.Key, World)))
			{
				OutItems.Add(FolderItem);
			}
		}
	}

	const ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>();
	// Create all actor items
	for (FActorIterator ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		// If we are not showing LevelInstances, LevelInstance sub actor items should not be created unless they belong to a LevelInstance which is being edited
		if (LevelInstanceSubsystem)
		{
			if (const ALevelInstance* ParentLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(Actor))
			{
				if (!bShowingLevelInstances && !ParentLevelInstance->IsEditing())
				{
					continue;
				}
			}
		}

		if (FSceneOutlinerTreeItemPtr ActorItem = Mode->CreateItemFor<FActorTreeItem>(Actor))
		{
			OutItems.Add(ActorItem);

			// Create all component items
			CreateComponentItems(Actor, OutItems);
		}
	}

	if (bShowingUnloadedActors)
	{
		if (UWorldPartitionSubsystem* WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>())
		{
			UWorldPartition* WorldPartition = World->GetWorldPartition();
			check(WorldPartition);
			WorldPartitionSubsystem->ForEachActorDesc(AActor::StaticClass(), [this, WorldPartition, &OutItems](const FWorldPartitionActorDesc* ActorDesc)
                {
                    if (ActorDesc != nullptr && !ActorDesc->IsLoaded())
                    {
                        if (const FSceneOutlinerTreeItemPtr ActorDescItem = Mode->CreateItemFor<FActorDescTreeItem>(FActorDescTreeItem(ActorDesc->GetGuid(), WorldPartition)))
                        {
                            OutItems.Add(ActorDescItem);
                        }
                    }
                    return true;
                });
		}
	}
}

void FActorHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	check(RepresentingWorld.IsValid());

	if (FSceneOutlinerTreeItemPtr WorldItem = Mode->CreateItemFor<FWorldTreeItem>(RepresentingWorld))
	{
		OutItems.Add(WorldItem);
	}

	UWorld* RepresentingWorldPtr = RepresentingWorld.Get();
	check(RepresentingWorldPtr);

	// Create world children regardless of if a world item was created
	CreateWorldChildren(RepresentingWorldPtr, OutItems);
}

void FActorHierarchy::CreateChildren(const FSceneOutlinerTreeItemPtr& Item, TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const
{
	if (FWorldTreeItem* WorldItem = Item->CastTo<FWorldTreeItem>())
	{
		check(WorldItem->World == RepresentingWorld);
		CreateWorldChildren(WorldItem->World.Get(), OutChildren);
	}
	else if (const FActorTreeItem* ParentActorItem = Item->CastTo<FActorTreeItem>())
	{
		AActor* ParentActor = ParentActorItem->Actor.Get();
		check(ParentActor->GetWorld() == RepresentingWorld);

		CreateComponentItems(ParentActor, OutChildren);

		TArray<AActor*> ChildActors;

		if (const ALevelInstance* LevelInstanceParentActor = Cast<ALevelInstance>(ParentActor))
		{
			const ULevelInstanceSubsystem* LevelInstanceSubsystem = RepresentingWorld->GetSubsystem<ULevelInstanceSubsystem>();
			check(LevelInstanceSubsystem);

			LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstanceParentActor, [this, LevelInstanceParentActor, LevelInstanceSubsystem, &ChildActors](AActor* SubActor)
				{
					const ALevelInstance* LevelInstanceActor = Cast<ALevelInstance>(SubActor);
					const bool bIsAnEditingLevelInstance = LevelInstanceActor ? LevelInstanceSubsystem->IsEditingLevelInstance(LevelInstanceActor) : false;
					if (bShowingLevelInstances || (LevelInstanceSubsystem->IsEditingLevelInstance(LevelInstanceParentActor) || bIsAnEditingLevelInstance))
					{
						ChildActors.Add(SubActor);
					}
					return true;
				});
		}
		else
		{
			TFunction<bool(AActor*)> GetAttachedActors = [&ChildActors, &GetAttachedActors](AActor* Child)
			{
				ChildActors.Add(Child);
				Child->ForEachAttachedActors(GetAttachedActors);

				// Always continue
				return true;
			};

			// Grab all direct/indirect children of an actor
			ParentActor->ForEachAttachedActors(GetAttachedActors);
		}

		for (auto ChildActor : ChildActors)
		{
			if (FSceneOutlinerTreeItemPtr ChildActorItem = Mode->CreateItemFor<FActorTreeItem>(ChildActor))
			{
				OutChildren.Add(ChildActorItem);

				CreateComponentItems(ChildActor, OutChildren);
			}
		}
	}
	else if (FActorFolderTreeItem* FolderItem = Item->CastTo<FActorFolderTreeItem>())
	{
		check(Mode->ShouldShowFolders());

		for (const auto& Pair : FActorFolders::Get().GetFolderPropertiesForWorld(*FolderItem->World))
		{
			if (FEditorFolderUtils::PathIsChildOf(Pair.Key, FolderItem->Path))
			{
				if (FSceneOutlinerTreeItemPtr NewFolderItem = Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(Pair.Key, FolderItem->World)))
				{
					OutChildren.Add(NewFolderItem);
				}
			}
		}
	}
}

FSceneOutlinerTreeItemPtr FActorHierarchy::CreateParentItem(const FSceneOutlinerTreeItemPtr& Item) const
{
	if (Item->IsA<FWorldTreeItem>())
	{
		return nullptr;
	}
	else if (const FActorTreeItem* ActorTreeItem = Item->CastTo<FActorTreeItem>())
	{
		if (const AActor* Actor = ActorTreeItem->Actor.Get())
		{
			if (AActor* ParentActor = Actor->GetSceneOutlinerParent())
			{
				return Mode->CreateItemFor<FActorTreeItem>(ParentActor, true);
			}

			// if this item belongs in a folder
			if (Mode->ShouldShowFolders() && !ActorTreeItem->Actor->GetFolderPath().IsNone())
			{
				return Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(ActorTreeItem->Actor->GetFolderPath(), ActorTreeItem->Actor->GetWorld()), true);
			}

			// If item belongs to a LevelInstance
			if (const ULevelInstanceSubsystem* LevelInstanceSubsystem = RepresentingWorld->GetSubsystem<ULevelInstanceSubsystem>())
			{
				if (ALevelInstance* ParentLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(Actor))
				{
					const ALevelInstance* LevelInstanceActor = Cast<ALevelInstance>(Actor);
					const bool bIsAnEditingLevelInstance = LevelInstanceActor ? LevelInstanceActor->IsEditing() : false;
					if (bShowingLevelInstances || (ParentLevelInstance->IsEditing() || bIsAnEditingLevelInstance))
					{
						return Mode->CreateItemFor<FActorTreeItem>(ParentLevelInstance, true);
					}
				}
			}

			// Default to the world
			UWorld* OwningWorld = ActorTreeItem->Actor->GetWorld();
			check(OwningWorld);
			return Mode->CreateItemFor<FWorldTreeItem>(OwningWorld, true);
		}
	}
	else if (const FComponentTreeItem* ComponentTreeItem = Item->CastTo<FComponentTreeItem>())
	{
		if (AActor* ParentActor = ComponentTreeItem->Component->GetOwner())
		{
			return Mode->CreateItemFor<FActorTreeItem>(ParentActor, true);
		}
	}
	else if (const FActorFolderTreeItem* FolderTreeItem = Item->CastTo<FActorFolderTreeItem>())
	{
		check(Mode->ShouldShowFolders());

		const FName ParentPath = FEditorFolderUtils::GetParentPath(FolderTreeItem->Path);
		if (ParentPath.IsNone())
		{
			UWorld* OwningWorld = FolderTreeItem->World.Get();
			check(OwningWorld);
			return Mode->CreateItemFor<FWorldTreeItem>(OwningWorld, true);
		}
		else
		{
			return Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(ParentPath, FolderTreeItem->World), true);
		}
	}
	else if (const FActorDescTreeItem* ActorDescItem = Item->CastTo<FActorDescTreeItem>())
	{
		if (const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.GetActorDesc())
		{
			const FName ActorDescPath = ActorDesc->GetFolderPath();
			if (Mode->ShouldShowFolders() && !ActorDescPath.IsNone())
			{
				return Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(ActorDescPath, RepresentingWorld.Get()), true);
			}
		}
	}

	return nullptr;
}

void FActorHierarchy::FullRefreshEvent()
{
	FSceneOutlinerHierarchyChangedData EventData;
	EventData.Type = FSceneOutlinerHierarchyChangedData::FullRefresh;

	HierarchyChangedEvent.Broadcast(EventData);
}

void FActorHierarchy::OnLevelActorAdded(AActor* InActor)
{
	if (InActor != nullptr && RepresentingWorld.Get() == InActor->GetWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
		EventData.Items.Add(Mode->CreateItemFor<FActorTreeItem>(InActor));
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnLevelActorDeleted(AActor* InActor)
{
	if (InActor != nullptr && RepresentingWorld.Get() == InActor->GetWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
		EventData.ItemIDs.Add(InActor);
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnLevelActorAttached(AActor* InActor, const AActor* InParent)
{
	if (InActor != nullptr && RepresentingWorld.Get() == InActor->GetWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Moved;
		EventData.ItemIDs.Add(InActor);
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnLevelActorDetached(AActor* InActor, const AActor* InParent)
{
	if (InActor != nullptr && RepresentingWorld.Get() == InActor->GetWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Moved;
		EventData.ItemIDs.Add(InActor);
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnLoadedActorAdded(AActor& InActor)
{
	OnLevelActorAdded(&InActor);

	FSceneOutlinerHierarchyChangedData EventData;
	EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
	EventData.ItemIDs.Add(InActor.GetActorGuid());
	HierarchyChangedEvent.Broadcast(EventData);
}

void FActorHierarchy::OnLoadedActorRemoved(AActor& InActor)
{
	OnLevelActorDeleted(&InActor);

	if (bShowingUnloadedActors)
	{
		if (UWorldPartition* WorldPartition = RepresentingWorld->GetWorldPartition())
		{
			const FGuid& ActorGuid = InActor.GetActorGuid();
			if (WorldPartition->GetActorDesc(ActorGuid) != nullptr)
			{
				FSceneOutlinerHierarchyChangedData EventData;
				EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
				EventData.Items.Add(Mode->CreateItemFor<FActorDescTreeItem>(FActorDescTreeItem(ActorGuid, WorldPartition)));
				HierarchyChangedEvent.Broadcast(EventData);
			}
		}
	}
}

void FActorHierarchy::OnActorDescAdded(FWorldPartitionActorDesc* ActorDesc)
{
	if (bShowingUnloadedActors && ActorDesc && !ActorDesc->IsLoaded())
	{
		if (UWorldPartition* WorldPartition = RepresentingWorld->GetWorldPartition())
		{
			FSceneOutlinerHierarchyChangedData EventData;
			EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
			EventData.Items.Add(Mode->CreateItemFor<FActorDescTreeItem>(FActorDescTreeItem(ActorDesc->GetGuid(), WorldPartition)));
			HierarchyChangedEvent.Broadcast(EventData);
		}
	}
}

void FActorHierarchy::OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc)
{
	if (bShowingUnloadedActors && ActorDesc)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
		EventData.ItemIDs.Add(ActorDesc->GetGuid());
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnComponentsUpdated()
{
	FullRefreshEvent();
}

void FActorHierarchy::OnLevelActorListChanged()
{
	FullRefreshEvent();
}

void FActorHierarchy::OnLevelAdded(ULevel* InLevel, UWorld* InWorld)
{
	if (InLevel != nullptr && RepresentingWorld.Get() == InWorld)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Added;

		EventData.Items.Reserve(InLevel->Actors.Num());
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor != nullptr)
			{
				EventData.Items.Add(Mode->CreateItemFor<FActorTreeItem>(Actor));
			}
		}
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnLevelRemoved(ULevel* InLevel, UWorld* InWorld)
{
	if (InLevel != nullptr && RepresentingWorld.Get() == InWorld)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;

		EventData.ItemIDs.Reserve(InLevel->Actors.Num());
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor != nullptr)
			{
				EventData.ItemIDs.Add(Actor);
			}
		}
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

/** Called when a folder is to be created */
void FActorHierarchy::OnBroadcastFolderCreate(UWorld& InWorld, FName NewPath)
{
	if (Mode->ShouldShowFolders() && RepresentingWorld.Get() == &InWorld)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
		EventData.Items.Add(Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(NewPath, &InWorld)));
		EventData.ItemActions = SceneOutliner::ENewItemAction::Select | SceneOutliner::ENewItemAction::Rename;
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

/** Called when a folder is to be moved */
void FActorHierarchy::OnBroadcastFolderMove(UWorld& InWorld, FName OldPath, FName NewPath)
{
	if (Mode->ShouldShowFolders() && RepresentingWorld.Get() == &InWorld)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::FolderMoved;
		EventData.ItemIDs.Add(OldPath);
		EventData.NewPaths.Add(NewPath);
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

/** Called when a folder is to be deleted */
void FActorHierarchy::OnBroadcastFolderDelete(UWorld& InWorld, FName Path)
{
	if (Mode->ShouldShowFolders() && RepresentingWorld.Get() == &InWorld)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
		EventData.ItemIDs.Add(Path);
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FActorHierarchy::OnLevelActorFolderChanged(const AActor* InActor, FName OldPath)
{
	if (Mode->ShouldShowFolders() && RepresentingWorld.Get() == InActor->GetWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Moved;
		EventData.ItemIDs.Add(FSceneOutlinerTreeItemID(InActor));
		HierarchyChangedEvent.Broadcast(EventData);
	}
}
