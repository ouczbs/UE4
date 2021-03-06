// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "AnimNode_BlendSpacePlayer.generated.h"

class UBlendSpaceBase;

//@TODO: Comment
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendSpacePlayer : public FAnimNode_AssetPlayerBase
{
	GENERATED_USTRUCT_BODY()

	// @return the current sample coordinates that this node is using to sample the blendspace
	FVector GetPosition() const { return FVector(X, Y, Z); }

	// @return the current sample coordinates after going through the filtering
	FVector GetFilteredPosition() const { return BlendFilter.GetFilterLastOutput(); }

public:
	// The X coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Coordinates, meta=(PinShownByDefault))
	float X;

	// The Y coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates, meta = (PinShownByDefault))
	float Y;

	// The Z coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates, meta = (PinHiddenByDefault))
	float Z;

	// The play rate multiplier. Can be negative, which will cause the animation to play in reverse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DefaultValue = "1.0", PinHiddenByDefault))
	float PlayRate;

	// Should the animation continue looping when it reaches the end?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DefaultValue = "true", PinHiddenByDefault))
	bool bLoop;

	// Whether we should reset the current play time when the blend space changes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bResetPlayTimeWhenBlendSpaceChanges;

	// The start up position in [0, 1], it only applies when reinitialized
	// if you loop, it will still start from 0.f after finishing the round
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta = (DefaultValue = "0.f", PinHiddenByDefault))
	float StartPosition;

	// The blendspace asset to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	TObjectPtr<UBlendSpaceBase> BlendSpace;

protected:

	FBlendFilter BlendFilter;

	TArray<FBlendSampleData> BlendSampleDataCache;

	UPROPERTY(Transient)
	TObjectPtr<UBlendSpaceBase> PreviousBlendSpace;

public:	
	FAnimNode_BlendSpacePlayer();

	// FAnimNode_AssetPlayerBase interface
	virtual float GetCurrentAssetTime();
	virtual float GetCurrentAssetTimePlayRateAdjusted();
	virtual float GetCurrentAssetLength();
	// End of FAnimNode_AssetPlayerBase interface

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void OverrideAsset(UAnimationAsset* NewAsset) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	float GetTimeFromEnd(float CurrentTime);

	UAnimationAsset* GetAnimAsset();

protected:
	void UpdateInternal(const FAnimationUpdateContext& Context);

private:
	void Reinitialize(bool bResetTime = true);

	const FBlendSampleData* GetHighestWeightedSample() const;
};
