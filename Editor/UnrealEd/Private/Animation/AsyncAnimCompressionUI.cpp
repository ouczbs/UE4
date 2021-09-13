// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncAnimCompressionUI.h"
#include "GlobalEditorNotification.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Animation/AnimCompressionDerivedDataPublic.h"
#include "Animation/AnimSequence.h"

/** Global notification object. */

/** Notification class for asynchronous shader compiling. */
class FAnimCompressionNotificationImpl : public FGlobalEditorProgressNotification
{
public:
	FAnimCompressionNotificationImpl()
		: FGlobalEditorProgressNotification(NSLOCTEXT("AsyncAnimCompression", "AnimCompressionInProgress", "Compressing Animations"))
	{}
protected:
	/** FGlobalEditorProgressNotification interface */
	virtual int32 UpdateProgress()
	{
		const int32 RemainingJobs = GAsyncCompressedAnimationsTracker ? GAsyncCompressedAnimationsTracker->GetNumRemainingJobs() : 0;
		if (RemainingJobs > 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("AnimsToCompress"), FText::AsNumber(RemainingJobs));
			UpdateProgressMessage(FText::Format(NSLOCTEXT("AsyncAnimCompression", "AnimCompressionInProgressFormat", "Compressing Animations ({AnimsToCompress})"), Args));

		}

		return RemainingJobs;
	}
};

FAnimCompressionNotificationImpl GAnimCompressionNotification;
