// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Component/ComponentElementWorldInterface.h"
#include "ComponentElementEditorWorldInterface.generated.h"

UCLASS()
class UComponentElementEditorWorldInterface : public UComponentElementWorldInterface
{
	GENERATED_BODY()

public:
	virtual void NotifyMovementStarted(const FTypedElementHandle& InElementHandle) override;
	virtual void NotifyMovementOngoing(const FTypedElementHandle& InElementHandle) override;
	virtual void NotifyMovementEnded(const FTypedElementHandle& InElementHandle) override;
	virtual bool CanDeleteElement(const FTypedElementHandle& InElementHandle) override;
	virtual bool DeleteElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions) override;
	virtual bool CanDuplicateElement(const FTypedElementHandle& InElementHandle) override;
	virtual void DuplicateElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements) override;
};
