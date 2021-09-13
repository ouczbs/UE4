// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Types/SlateEnums.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "Templates/SharedPointer.h"
#include "LevelInstance/LevelInstanceTypes.h"

//////////////////////////////////////////////////////////////////////////
// SNewLevelInstanceDialog

class SNewLevelInstanceDialog : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SNewLevelInstanceDialog) {}
		/** A pointer to the parent window */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(TArray<AActor*>, PivotActors)
	SLATE_END_ARGS()

	~SNewLevelInstanceDialog() {}

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The UI logic not specific to slate
	 */
	void Construct(const FArguments& InArgs);

	static const FVector2D DEFAULT_WINDOW_SIZE;

	bool ClickedOk() const { return bClickedOk; }
	
	const FNewLevelInstanceParams& GetCreationParams() const { return CreationParams; }

private:
	FReply OnOkClicked();
	bool IsOkEnabled() const;

	FReply OnCancelClicked();

	/** Pointer to the parent window, so we know to destroy it when done */
	TWeakPtr<SWindow> ParentWindowPtr;

	/** Dialog Result */
	FNewLevelInstanceParams CreationParams;

	/** Dialog Result */
	bool bClickedOk;
};

class FNewLevelInstanceParamsDetails : public IDetailCustomization
{
public:

	FNewLevelInstanceParamsDetails(TArray<AActor*> InPivotActors)
		: CreationParams(nullptr), PivotActors(InPivotActors)
	{};

	static TSharedRef<IDetailCustomization> MakeInstance(TArray<AActor*> InPivotActors)
	{
		return MakeShareable(new FNewLevelInstanceParamsDetails(InPivotActors));
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TSharedRef<SWidget> OnGeneratePivotActorWidget(AActor* Actor) const;
	FText GetSelectedPivotActorText() const;
	void OnSelectedPivotActorChanged(AActor* NewValue, ESelectInfo::Type SelectionType);
	bool IsPivotActorSelectionEnabled() const;

protected:
	FNewLevelInstanceParams* CreationParams;
	TArray<AActor*> PivotActors;
};