// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_SequencePlayer.h"

#include "AnimEncoding.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimTrace.h"
#include "Animation/AnimPoseSearchProvider.h"

#define LOCTEXT_NAMESPACE "AnimNode_SequencePlayer"

/////////////////////////////////////////////////////
// FAnimSequencePlayerNode

float FAnimNode_SequencePlayer::GetCurrentAssetTime()
{
	return InternalTimeAccumulator;
}

float FAnimNode_SequencePlayer::GetCurrentAssetTimePlayRateAdjusted()
{
	const float SequencePlayRate = (Sequence ? Sequence->RateScale : 1.f);
	const float AdjustedPlayRate = PlayRateScaleBiasClamp.ApplyTo(FMath::IsNearlyZero(PlayRateBasis) ? 0.f : (PlayRate / PlayRateBasis), 0.f);
	const float EffectivePlayrate = SequencePlayRate * AdjustedPlayRate;
	return (EffectivePlayrate < 0.0f) ? GetCurrentAssetLength() - InternalTimeAccumulator : InternalTimeAccumulator;
}

float FAnimNode_SequencePlayer::GetCurrentAssetLength()
{
	return Sequence ? Sequence->GetPlayLength() : 0.0f;
}

void FAnimNode_SequencePlayer::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);

	GetEvaluateGraphExposedInputs().Execute(Context);

	if (Sequence && !ensureMsgf(!Sequence->IsA<UAnimMontage>(), TEXT("Sequence players do not support anim montages.")))
	{
		Sequence = nullptr;
	}

	InternalTimeAccumulator = StartPosition;
	PlayRateScaleBiasClamp.Reinitialize();

	if (Sequence != nullptr)
	{
		const float EffectiveStartPosition = GetEffectiveStartPosition(Context);

		InternalTimeAccumulator = FMath::Clamp(EffectiveStartPosition, 0.f, Sequence->GetPlayLength());
		const float AdjustedPlayRate = PlayRateScaleBiasClamp.ApplyTo(FMath::IsNearlyZero(PlayRateBasis) ? 0.f : (PlayRate / PlayRateBasis), 0.f);
		const float EffectivePlayrate = Sequence->RateScale * AdjustedPlayRate;
		if ((EffectiveStartPosition == 0.f) && (EffectivePlayrate < 0.f))
		{
			InternalTimeAccumulator = Sequence->GetPlayLength();
		}
	}
}

void FAnimNode_SequencePlayer::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);
}

void FAnimNode_SequencePlayer::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateAssetPlayer);

	GetEvaluateGraphExposedInputs().Execute(Context);

	if (Sequence && !ensureMsgf(!Sequence->IsA<UAnimMontage>(), TEXT("Sequence players do not support anim montages.")))
	{
		Sequence = nullptr;
	}

	if ((Sequence != nullptr) && (Context.AnimInstanceProxy->IsSkeletonCompatible(Sequence->GetSkeleton())))
	{
		InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.f, Sequence->GetPlayLength());
		const float AdjustedPlayRate = PlayRateScaleBiasClamp.ApplyTo(FMath::IsNearlyZero(PlayRateBasis) ? 0.f : (PlayRate / PlayRateBasis), Context.GetDeltaTime());
		CreateTickRecordForNode(Context, Sequence, bLoopAnimation, AdjustedPlayRate);
	}

#if WITH_EDITORONLY_DATA
	if (FAnimBlueprintDebugData* DebugData = Context.AnimInstanceProxy->GetAnimBlueprintDebugData())
	{
		DebugData->RecordSequencePlayer(Context.GetCurrentNodeId(), GetAccumulatedTime(), Sequence != nullptr ? Sequence->GetPlayLength() : 0.0f, Sequence != nullptr ? Sequence->GetNumberOfSampledKeys() : 0);
	}
#endif

	TRACE_ANIM_SEQUENCE_PLAYER(Context, *this);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Name"), Sequence != nullptr ? Sequence->GetFName() : NAME_None);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Sequence"), Sequence);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Playback Time"), InternalTimeAccumulator);
}

void FAnimNode_SequencePlayer::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	if ((Sequence != nullptr) && (Output.AnimInstanceProxy->IsSkeletonCompatible(Sequence->GetSkeleton())))
	{
		const bool bExpectedAdditive = Output.ExpectsAdditivePose();
		const bool bIsAdditive = Sequence->IsValidAdditive();

		if (bExpectedAdditive && !bIsAdditive)
		{
			FText Message = FText::Format(LOCTEXT("AdditiveMismatchWarning", "Trying to play a non-additive animation '{0}' into a pose that is expected to be additive in anim instance '{1}'"), FText::FromString(Sequence->GetName()), FText::FromString(Output.AnimInstanceProxy->GetAnimInstanceName()));
			Output.LogMessage(EMessageSeverity::Warning, Message);
		}

		FAnimationPoseData AnimationPoseData(Output);
		Sequence->GetAnimationPose(AnimationPoseData, FAnimExtractContext(InternalTimeAccumulator, Output.AnimInstanceProxy->ShouldExtractRootMotion()));
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_SequencePlayer::OverrideAsset(UAnimationAsset* NewAsset)
{
	if (UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(NewAsset))
	{
		Sequence = AnimSequence;
	}
}

void FAnimNode_SequencePlayer::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), Sequence ? *Sequence->GetName() : TEXT("NULL"), InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine, true);
}

float FAnimNode_SequencePlayer::GetTimeFromEnd(float CurrentNodeTime)
{
	return Sequence->GetPlayLength() - CurrentNodeTime;
}

float FAnimNode_SequencePlayer::GetEffectiveStartPosition(const FAnimationBaseContext& Context) const
{
	// Override the start position if pose matching is enabled
	if (Sequence != nullptr && bStartFromMatchingPose)
	{
		UE::Anim::IPoseSearchProvider* PoseSearchProvider = UE::Anim::IPoseSearchProvider::Get();
		if (PoseSearchProvider)
		{
			UE::Anim::IPoseSearchProvider::FSearchResult Result = PoseSearchProvider->Search(Context, Sequence);
			if (Result.PoseIdx >= 0)
			{
				return Result.TimeOffsetSeconds;
			}
		}
	}

	return StartPosition;
}

#undef LOCTEXT_NAMESPACE