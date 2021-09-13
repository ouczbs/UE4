// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "KismetPins/SGraphPinObject.h"

/////////////////////////////////////////////////////
// SGraphPinStruct

class SGraphPinStruct : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SGraphPinStruct) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	// Called when a new struct was picked via the asset picker
	void OnPickedNewStruct(const UScriptStruct* ChosenStruct);

	//~ Begin SGraphPinObject Interface
	virtual FReply OnClickUse() override;
	virtual bool AllowSelfPinWidget() const override { return false; }
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	virtual FText GetDefaultComboText() const override;
	virtual FOnClicked GetOnUseButtonDelegate() override;
	//~ End SGraphPinObject Interface
};
