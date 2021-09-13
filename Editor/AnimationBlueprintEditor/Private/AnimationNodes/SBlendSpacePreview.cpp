// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationNodes/SBlendSpacePreview.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PersonaModule.h"
#include "Widgets/Layout/SBox.h"
#include "Modules/ModuleManager.h"
#include "AnimGraphNode_Base.h"

void SBlendSpacePreview::Construct(const FArguments& InArgs, UAnimGraphNode_Base* InNode)
{
	check(InNode);

	Node = InNode;

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");

	FBlendSpacePreviewArgs Args;

	Args.PreviewBlendSpace = MakeAttributeLambda([this](){ return CachedBlendSpace.Get(); });
	Args.PreviewPosition = MakeAttributeLambda([this]() { return CachedPosition; });
	Args.PreviewFilteredPosition = MakeAttributeLambda([this]() { return CachedFilteredPosition; });
	Args.OnGetBlendSpaceSampleName = InArgs._OnGetBlendSpaceSampleName;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredHeight_Lambda([this]()
		{
			return 100.0f;
		})
		.Visibility(this, &SBlendSpacePreview::GetBlendSpaceVisibility)
		[
			PersonaModule.CreateBlendSpacePreviewWidget(Args)
		]
	];

	RegisterActiveTimer(1.0f / 60.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
	{
		GetBlendSpaceInfo(CachedBlendSpace, CachedPosition, CachedFilteredPosition);
		return EActiveTimerReturnType::Continue;
	}));
}

EVisibility SBlendSpacePreview::GetBlendSpaceVisibility() const
{
	if (Node.Get() != nullptr)
	{
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node.Get()))
		{
			if (FProperty* Property = FKismetDebugUtilities::FindClassPropertyForNode(Blueprint, Node.Get()))
			{
				if (UObject* ActiveObject = Blueprint->GetObjectBeingDebugged())
				{
					return EVisibility::Visible;
				}
			}
		}
	}

	return EVisibility::Collapsed;
}

bool SBlendSpacePreview::GetBlendSpaceInfo(TWeakObjectPtr<const UBlendSpaceBase>& OutBlendSpace, FVector& OutPosition, FVector& OutFilteredPosition) const
{
	if (Node.Get() != nullptr)
	{
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node.Get()))
		{
			if (UObject* ActiveObject = Blueprint->GetObjectBeingDebugged())
			{
				if (UAnimBlueprintGeneratedClass* Class = Cast<UAnimBlueprintGeneratedClass>(ActiveObject->GetClass()))
				{
					if(int32* NodeIndexPtr = Class->GetAnimBlueprintDebugData().NodePropertyToIndexMap.Find(Node))
					{
						int32 AnimNodeIndex = *NodeIndexPtr;
						// reverse node index temporarily because of a bug in NodeGuidToIndexMap
						AnimNodeIndex = Class->GetAnimNodeProperties().Num() - AnimNodeIndex - 1;

						if (FAnimBlueprintDebugData::FBlendSpacePlayerRecord* DebugInfo = Class->GetAnimBlueprintDebugData().BlendSpacePlayerRecordsThisFrame.FindByPredicate(
							[AnimNodeIndex](const FAnimBlueprintDebugData::FBlendSpacePlayerRecord& InRecord){ return InRecord.NodeID == AnimNodeIndex; }))
						{
							OutBlendSpace = DebugInfo->BlendSpace.Get();
							OutPosition = DebugInfo->Position;
							OutFilteredPosition = DebugInfo->FilteredPosition;
							return true;
						}
					}
				}
			}
		}
	}

	OutBlendSpace = nullptr;
	OutPosition = FVector::ZeroVector;
	OutFilteredPosition = FVector::ZeroVector;
	return false;
}
