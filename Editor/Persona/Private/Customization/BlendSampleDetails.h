// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

#include "Templates/Function.h"
#include "DetailWidgetRow.h"
#include "SAnimationBlendSpaceGridWidget.h"

struct FAssetData;
class FDetailWidgetRow;
class UBlendSpaceBase;
class UAnimGraphNode_BlendSpaceGraphBase;

class FBlendSampleDetails : public IDetailCustomization
{
public:
	FBlendSampleDetails(const class UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget, int32 InSampleIndex);

	static TSharedRef<IDetailCustomization> MakeInstance(const class UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget, int32 InSampleIndex)
	{
		return MakeShareable( new FBlendSampleDetails(InBlendSpace, InGridWidget, InSampleIndex) );
	}

	// Begin IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End IDetailCustomization interface
	
	static void GenerateBlendSampleWidget(TFunction<FDetailWidgetRow& (void)>InFunctor, FOnSampleMoved OnSampleMoved, const class UBlendSpaceBase* BlendSpace, const int32 SampleIndex, bool bShowLabel);

	static void GenerateAnimationWidget(FDetailWidgetRow& Row, const UBlendSpaceBase* BlendSpace, TSharedPtr<IPropertyHandle> AnimationProperty);

	static void GenerateSampleGraphWidget(FDetailWidgetRow& Row, UAnimGraphNode_BlendSpaceGraphBase* BlendSpaceNode, int32 SampleIndex);

	static bool ShouldFilterAssetStatic(const FAssetData& AssetData, const UBlendSpaceBase* BlendSpaceBase);

protected:
	/** Checks whether or not the specified asset should not be shown in the mini content browser when changing the animation */
	bool ShouldFilterAsset(const FAssetData& AssetData) const;
private:
	/** Pointer to the current parent blend space for the customized blend sample*/
	const class UBlendSpaceBase* BlendSpace;
	/** Parent grid widget object */
	SBlendSpaceGridWidget* GridWidget;
	/** Current sample index */
	int32 SampleIndex;
	/** Cached flags to check whether or not an additive animation type is compatible with the blend space*/	
	TMap<FString, bool> bValidAdditiveTypes;
};
