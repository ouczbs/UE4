// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ISceneOutlinerTreeItem.h"
#include "UObject/ObjectKey.h"

/** A tree item that represents an actor in the world */
struct SCENEOUTLINER_API FActorTreeItem : ISceneOutlinerTreeItem
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(bool, FFilterPredicate, const AActor*);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractivePredicate, const AActor*);
		
	bool Filter(FFilterPredicate Pred) const
	{
		return Pred.Execute(Actor.Get());
	}

	bool GetInteractiveState(FInteractivePredicate Pred) const 
	{
		return Pred.Execute(Actor.Get());
	}

	/** The actor this tree item is associated with. */
	mutable TWeakObjectPtr<AActor> Actor;

	/** Constant identifier for this tree item */
	const FObjectKey ID;

	/** Static type identifier for this tree item class */
	static const FSceneOutlinerTreeItemType Type;

	/** Construct this item from an actor */
	FActorTreeItem(AActor* InActor);

	/* Begin ISceneOutlinerTreeItem Implementation */
	virtual bool IsValid() const override { return Actor.IsValid(); }
	virtual FSceneOutlinerTreeItemID GetID() const override;
	virtual FString GetDisplayString() const override;
	virtual bool CanInteract() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override;
	virtual void OnVisibilityChanged(const bool bNewVisibility) override;
	virtual bool HasVisibilityInfo() const override { return true; }
	virtual bool GetVisibility() const override;
	virtual void OnLabelChanged() override;
	/* End ISceneOutlinerTreeItem Implementation */
public:
	/** true if this item exists in both the current world and PIE. */
	bool bExistsInCurrentWorldAndPIE;
	/** Cached actor label */
	FString ActorLabel;
};