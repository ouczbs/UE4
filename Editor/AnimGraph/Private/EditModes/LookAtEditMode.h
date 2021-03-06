// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealWidgetFwd.h"
#include "AnimNodeEditMode.h"

class FLookAtEditMode : public FAnimNodeEditMode
{
public:
	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
	virtual FVector GetWidgetLocation() const override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual FName GetSelectedBone() const override;
	virtual void DoTranslation(FVector& InTranslation) override;

private:
	struct FAnimNode_LookAt* RuntimeNode;
	class UAnimGraphNode_LookAt* GraphNode;
};
