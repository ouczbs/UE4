// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationModifier.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "ModifierOutputFilter.h"
#include "Editor/Transactor.h"
#include "UObject/UObjectIterator.h"

#include "UObject/ReleaseObjectVersion.h"
#include "Misc/MessageDialog.h"
#include "Editor/Transactor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/AnimObjectVersion.h"

#define LOCTEXT_NAMESPACE "AnimationModifier"

UAnimationModifier::UAnimationModifier()
	: PreviouslyAppliedModifier(nullptr)
{
}

void UAnimationModifier::ApplyToAnimationSequence(class UAnimSequence* InAnimationSequence)
{
	FEditorScriptExecutionGuard ScriptGuard;

	checkf(InAnimationSequence, TEXT("Invalid Animation Sequence supplied"));
	CurrentAnimSequence = InAnimationSequence;
	CurrentSkeleton = InAnimationSequence->GetSkeleton();

	// Filter to check for warnings / errors thrown from animation blueprint library (rudimentary approach for now)
	FCategoryLogOutputFilter OutputLog;
	OutputLog.SetAutoEmitLineTerminator(true);
	OutputLog.AddCategoryName("LogAnimationBlueprintLibrary");

	GLog->AddOutputDevice(&OutputLog);
		
	// Transact the modifier to prevent instance variables/data to change during applying
	FTransaction ModifierTransaction;
	ModifierTransaction.SaveObject(this);

	FTransaction AnimationDataTransaction;
	AnimationDataTransaction.SaveObject(CurrentAnimSequence);
	AnimationDataTransaction.SaveObject(CurrentSkeleton);

	/** In case this modifier has been previously applied, revert it using the serialised out version at the time */	
	if (PreviouslyAppliedModifier)
	{
		PreviouslyAppliedModifier->Modify();
		PreviouslyAppliedModifier->OnRevert(CurrentAnimSequence);
	}

	IAnimationDataController& Controller = CurrentAnimSequence->GetController();

	{
		IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ApplyModifierBracket", "Applying Animation Modifier"));
		/** Reverting and applying, populates the log with possible warnings and or errors to notify the user about */
		OnApply(CurrentAnimSequence);
	}

	// Apply transaction
	ModifierTransaction.BeginOperation();
	ModifierTransaction.Apply();
	ModifierTransaction.EndOperation();

	GLog->RemoveOutputDevice(&OutputLog);

	// Check if warnings or errors have occurred and show dialog to user to inform her about this
	const bool bWarningsOrErrors = OutputLog.ContainsWarnings() || OutputLog.ContainsErrors();
	bool bShouldRevert = false;
	if (bWarningsOrErrors)
	{
		static const FText WarningMessageFormat = FText::FromString("Modifier has generated warnings during a test run:\n\n{0}\nAre you sure you want to Apply it?");
		static const FText ErrorMessageFormat = FText::FromString("Modifier has generated errors (and warnings) during a test run:\n\n{0}\nResolve the Errors before trying to Apply!");

		EAppMsgType::Type MessageType = OutputLog.ContainsErrors() ? EAppMsgType::Ok : EAppMsgType::YesNo;
		const FText& MessageFormat = OutputLog.ContainsErrors() ? ErrorMessageFormat : WarningMessageFormat;
		const FText MessageTitle = FText::FromString("Modifier has Generated Warnings/Errors");
		bShouldRevert = (FMessageDialog::Open(MessageType, FText::FormatOrdered(MessageFormat, FText::FromString(OutputLog)), &MessageTitle) != EAppReturnType::Yes);
	}

	// Revert changes if necessary, otherwise post edit and refresh animation data
	if (bShouldRevert)
	{
		AnimationDataTransaction.BeginOperation();
		AnimationDataTransaction.Apply();
		AnimationDataTransaction.EndOperation();
		CurrentAnimSequence->RefreshCacheData();
	}
	else
	{
		/** Mark the previous modifier pending kill, as it will be replaced with the current modifier state */
		if (PreviouslyAppliedModifier)
		{
			PreviouslyAppliedModifier->MarkPendingKill();
		}

		PreviouslyAppliedModifier = DuplicateObject(this, GetOuter());

		CurrentAnimSequence->PostEditChange();
		CurrentSkeleton->PostEditChange();
		CurrentAnimSequence->RefreshCacheData();

		UpdateStoredRevisions();
	}
		
	// Finished
	CurrentAnimSequence = nullptr;
	CurrentSkeleton = nullptr;
}

void UAnimationModifier::UpdateCompressedAnimationData()
{
	if (CurrentAnimSequence->DoesNeedRecompress())
	{
		CurrentAnimSequence->RequestSyncAnimRecompression(false);
	}
}

void UAnimationModifier::RevertFromAnimationSequence(class UAnimSequence* InAnimationSequence)
{
	FEditorScriptExecutionGuard ScriptGuard;

	/** Can only revert if previously applied, which means there should be a previous modifier */
	if (PreviouslyAppliedModifier)
	{
		checkf(InAnimationSequence, TEXT("Invalid Animation Sequence supplied"));
		CurrentAnimSequence = InAnimationSequence;
		CurrentSkeleton = InAnimationSequence->GetSkeleton();

		// Transact the modifier to prevent instance variables/data to change during reverting
		FTransaction Transaction;
		Transaction.SaveObject(this);

		PreviouslyAppliedModifier->Modify();

		IAnimationDataController& Controller = CurrentAnimSequence->GetController();

		{
			IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("RevertModifierBracket", "Reverting Animation Modifier"));
			PreviouslyAppliedModifier->OnRevert(CurrentAnimSequence);
		}

		// Apply transaction
		Transaction.BeginOperation();
		Transaction.Apply();
		Transaction.EndOperation();

	    CurrentAnimSequence->PostEditChange();
	    CurrentSkeleton->PostEditChange();
	    CurrentAnimSequence->RefreshCacheData();

		ResetStoredRevisions();

		// Finished
		CurrentAnimSequence = nullptr;
		CurrentSkeleton = nullptr;

		PreviouslyAppliedModifier->MarkPendingKill();
		PreviouslyAppliedModifier = nullptr;
	}
}

bool UAnimationModifier::IsLatestRevisionApplied() const
{
	return (AppliedGuid == RevisionGuid);
}

void UAnimationModifier::PostInitProperties()
{
	Super::PostInitProperties();
	UpdateNativeRevisionGuid();

	// Ensure we always have a valid guid
	if (!RevisionGuid.IsValid())
	{
		UpdateRevisionGuid(GetClass());
		MarkPackageDirty();
	}
}

void UAnimationModifier::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	/** Back-wards compatibility, assume the current modifier as previously applied */
	if (Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::SerializeAnimModifierState)
	{
		PreviouslyAppliedModifier = DuplicateObject(this, GetOuter());
	}
}

const USkeleton* UAnimationModifier::GetSkeleton()
{
	return CurrentSkeleton;
}

void UAnimationModifier::UpdateRevisionGuid(UClass* ModifierClass)
{
	RevisionGuid = FGuid::NewGuid();

	// Native classes are more difficult?
	for (TObjectIterator<UAnimationModifier> It; It; ++It)
	{
		if (*It != this && It->GetClass() == ModifierClass)
		{
			It->SetInstanceRevisionGuid(RevisionGuid);
		}
	}
}

void UAnimationModifier::UpdateNativeRevisionGuid()
{
	UClass* Class = GetClass();
	// Check if this is the class default object
	if (this == GetDefault<UAnimationModifier>(Class))
	{
		// If so check whether or not the config stored revision matches the natively defined one
		if (StoredNativeRevision != GetNativeClassRevision())
		{
			// If not update the blueprint revision GUID
			UpdateRevisionGuid(Class);
			StoredNativeRevision = GetNativeClassRevision();

			MarkPackageDirty();

			// Save the new native revision to config files
			SaveConfig();
			UpdateDefaultConfigFile();
		}
	}
}

int32 UAnimationModifier::GetNativeClassRevision() const
{
	// Overriden in derrived classes to perform native revisioning
	return 0;
}

const UAnimSequence* UAnimationModifier::GetAnimationSequence()
{
	return CurrentAnimSequence;
}

void UAnimationModifier::UpdateStoredRevisions()
{
	AppliedGuid = RevisionGuid;
}

void UAnimationModifier::ResetStoredRevisions()
{
	AppliedGuid.Invalidate();
}

void UAnimationModifier::SetInstanceRevisionGuid(FGuid Guid)
{
	RevisionGuid = Guid;
}

#undef LOCTEXT_NAMESPACE