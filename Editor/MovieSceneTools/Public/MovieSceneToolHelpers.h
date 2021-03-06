// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Curves/RichCurve.h"
#include "Math/InterpCurvePoint.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "ISequencer.h"
#include "Logging/TokenizedMessage.h"
#include "MovieSceneTranslator.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneCaptureSettings.h"
#include "SEnumCombo.h"
#include "Animation/AnimSequence.h"
#include "INodeAndChannelMappings.h"


class ISequencer;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneSequence;
class UInterpTrackMoveAxis;
struct FMovieSceneObjectBindingID;
class UMovieSceneTrack;
struct FMovieSceneEvaluationTrack;
class UMovieSceneUserImportFBXSettings;
class UMovieSceneUserImportFBXControlRigSettings;
struct FMovieSceneFloatValue;
class INodeNameAdapter;
struct FMovieSceneSequenceTransform;
class UAnimSeqExportOption;
template<typename ChannelType> struct TMovieSceneChannelData;
enum class EVisibilityBasedAnimTickOption : uint8;
struct FActorForWorldTransforms;

namespace fbxsdk
{
	class FbxCamera;
	class FbxNode;
}
namespace UnFbx
{
	class FFbxImporter;
	class FFbxCurvesAPI;
};

struct FFBXInOutParameters
{
	bool bConvertSceneBackup;
	bool bConvertSceneUnitBackup;
	bool bForceFrontXAxisBackup;
};

//callback's used by skel mesh recorders

DECLARE_DELEGATE(FInitAnimationCB);
DECLARE_DELEGATE(FStartAnimationCB);
DECLARE_DELEGATE_OneParam(FTickAnimationCB, float);
DECLARE_DELEGATE(FEndAnimationCB);

//Skel Mesh Recorder to set up and restore various parameters on the skelmesh
struct MOVIESCENETOOLS_API FSkelMeshRecorderState
{
public:
	FSkelMeshRecorderState() {}
	~FSkelMeshRecorderState() {}

	void Init(USkeletalMeshComponent* InComponent);
	void FinishRecording();


public:
	TWeakObjectPtr<USkeletalMeshComponent> SkelComp;;

	/** Original ForcedLodModel setting on the SkelComp, so we can modify it and restore it when we are done. */
	int CachedSkelCompForcedLodModel;

	/** Used to store/restore update flag when recording */
	EVisibilityBasedAnimTickOption CachedVisibilityBasedAnimTickOption;

	/** Used to store/restore URO when recording */
	bool bCachedEnableUpdateRateOptimizations;
};

class MOVIESCENETOOLS_API MovieSceneToolHelpers
{
public:

	/**
	 * Trim section at the given time
	 *
	 * @param Sections The sections to trim
	 * @param Time	The time at which to trim
	 * @param bTrimLeft Trim left or trim right
	 * @param bDeleteKeys Delete keys outside the split ranges
	 */
	static void TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, FQualifiedFrameTime Time, bool bTrimLeft, bool bDeleteKeys);

	/**
	 * Trim or extend section at the given time
	 *
	 * @param Track The track that contains the sections to trim
	 * @param RowIndex Optional row index to trim, otherwise trims sections with all row indices
	 * @param Time	The time at which to trim
	 * @param bTrimOrExtendleft Trim or extend left or right
	 * @param bDeleteKeys Delete keys outside the split ranges
	 */
	static void TrimOrExtendSection(UMovieSceneTrack* Track, TOptional<int32> RowIndex, FQualifiedFrameTime Time, bool bTrimOrExtendLeft, bool bDeleteKeys);

	/**
	 * Splits sections at the given time
	 *
	 * @param Sections The sections to split
	 * @param Time	The time at which to split
	 * @param bDeleteKeys Delete keys outside the split ranges
	 */
	static void SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, FQualifiedFrameTime Time, bool bDeleteKeys);

	/**
	 * Parse a shot name into its components.
	 *
	 * @param ShotName The shot name to parse
	 * @param ShotPrefix The parsed shot prefix
	 * @param ShotNumber The parsed shot number
	 * @param TakeNumber The parsed take number
	 * @return Whether the shot name was parsed successfully
	 */
	static bool ParseShotName(const FString& ShotName, FString& ShotPrefix, uint32& ShotNumber, uint32& TakeNumber);

	/**
	 * Compose a shot name given its components.
	 *
	 * @param ShotPrefix The shot prefix to use
	 * @param ShotNumber The shot number to use
	 * @param TakeNumber The take number to use
	 * @return The composed shot name
	 */
	static FString ComposeShotName(const FString& ShotPrefix, uint32 ShotNumber, uint32 TakeNumber);

	/**
	 * Generate a new shot package
	 *
	 * @param SequenceMovieScene The sequence movie scene for the new shot
	 * @param NewShotName The new shot name
	 * @return The new shot path
	 */
	static FString GenerateNewShotPath(UMovieScene* SequenceMovieScene, FString& NewShotName);

	/**
	 * Generate a new shot name
	 *
	 * @param AllSections All the sections in the given shot track
	 * @param Time The time to generate the new shot name at
	 * @return The new shot name
	 */
	static FString GenerateNewShotName(const TArray<UMovieSceneSection*>& AllSections, FFrameNumber Time);

	/**
	 * Gather takes - level sequence assets that have the same shot prefix and shot number in the same asset path (directory)
	 * 
	 * @param Section The section to gather takes from
	 * @param AssetData The gathered asset take data
	 * @param OutCurrentTakeNumber The current take number of the section
	 */
	static void GatherTakes(const UMovieSceneSection* Section, TArray<FAssetData>& AssetData, uint32& OutCurrentTakeNumber);


	/**
	 * Get the take number for the given asset
	 *
	 * @param Section The section to gather the take number from
	 * @param AssetData The take asset to search for
	 * @param OutTakeNumber The take number for the given asset
	 * @return Whether the take number was found
	 */
	static bool GetTakeNumber(const UMovieSceneSection* Section, FAssetData AssetData, uint32& OutTakeNumber);

	/**
	 * Set the take number for the given asset
	 *
	 * @param Section The section to set the take number on
	 * @param InTakeNumber The take number for the given asset
	 * @return Whether the take number could be set
	 */
	static bool SetTakeNumber(const UMovieSceneSection* Section, uint32 InTakeNumber);

	/**
	 * Get the next available row index for the section so that it doesn't overlap any other sections in time.
	 *
	 * @param InTrack The track to find the next available row on
	 * @param InSection The section
	 * @return The next available row index
	 */
	static int32 FindAvailableRowIndex(UMovieSceneTrack* InTrack, UMovieSceneSection* InSection);

	/**
	 * Generate a combobox for editing enum values
	 *
	 * @param Enum The enum to make the combobox from
	 * @param CurrentValue The current value to display
	 * @param OnSelectionChanged Delegate fired when selection is changed
	 * @return The new widget
	 */
	static TSharedRef<SWidget> MakeEnumComboBox(const UEnum* Enum, TAttribute<int32> CurrentValue, SEnumComboBox::FOnEnumSelectionChanged OnSelectionChanged);


	/**
	 * Show Import EDL Dialog
	 *
	 * @param InMovieScene The movie scene to import the edl into
	 * @param InFrameRate The frame rate to import the EDL at
	 * @param InOpenDirectory Optional directory path to open from. If none given, a dialog will pop up to prompt the user
	 * @return Whether the import was successful
	 */
	static bool ShowImportEDLDialog(UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InOpenDirectory = TEXT(""));

	/**
	 * Show Export EDL Dialog
	 *
	 * @param InMovieScene The movie scene with the cinematic shot track and audio tracks to export
	 * @param InFrameRate The frame rate to export the EDL at
	 * @param InSaveDirectory Optional directory path to save to. If none given, a dialog will pop up to prompt the user
	 * @param InHandleFrames The number of handle frames to include for each shot.
	 * @param MovieExtension The movie extension for the shot filenames (ie. .avi, .mov, .mp4)
	 * @return Whether the export was successful
	 */
	static bool ShowExportEDLDialog(const UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InSaveDirectory = TEXT(""), int32 InHandleFrames = 8, FString InMovieExtension = TEXT(".avi"));

	/**
	* Import movie scene formats
	*
	* @param InImporter The movie scene importer.
	* @param InMovieScene The movie scene to import the format into
	* @param InFrameRate The frame rate to import the format at
	* @param InOpenDirectory Optional directory path to open from. If none given, a dialog will pop up to prompt the user
	* @return Whether the import was successful
	*/
	static bool MovieSceneTranslatorImport(FMovieSceneImporter* InImporter, UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InOpenDirectory = TEXT(""));

	/**
	* Export movie scene formats
	*
	* @param InExporter The movie scene exporter.
	* @param InMovieScene The movie scene with the cinematic shot track and audio tracks to export
	* @param InFrameRate The frame rate to export the AAF at
	* @param InSaveDirectory Optional directory path to save to. If none given, a dialog will pop up to prompt the user
	* @param InHandleFrames The number of handle frames to include for each shot.
	* @return Whether the export was successful
	*/
	static bool MovieSceneTranslatorExport(FMovieSceneExporter* InExporter, const UMovieScene* InMovieScene, const FMovieSceneCaptureSettings& Settings);

	/** 
	* Log messages and display error message window for MovieScene translators
	*
	* @param InTranslator The movie scene importer or exporter.
	* @param InContext The context used to gather error, warning or info messages during import or export.
	* @param bDisplayMessages Whether to open the message log window after adding the message.
	*/
	static void MovieSceneTranslatorLogMessages(FMovieSceneTranslator* InTranslator, TSharedRef<FMovieSceneTranslatorContext> InContext, bool bDisplayMessages);

	/**
	* Log error output for MovieScene translators
	*
	* @param InTranslator The movie scene importer or exporter.
	* @param InContext The context used to gather error, warning or info messages during import or export.
	*/
	static void MovieSceneTranslatorLogOutput(FMovieSceneTranslator* InTranslator, TSharedRef<FMovieSceneTranslatorContext> InContext);

	/**
	* Export FBX
	*
	* @param World The world to export from
	* @param InMovieScene The movie scene to export frome
	* @param MoviePlayer to use
	* @param Bindings The sequencer binding map
	* @param NodeNameAdaptor Adaptor to look up actor names.
	* @param InFBXFileName the fbx file name.
	* @param Template Movie scene sequence id.
	* @param RootToLocalTransform The root to local transform time.
	* @return Whether the export was successful
	*/
	static bool ExportFBX(UWorld* World, UMovieScene* MovieScene, IMovieScenePlayer* Player, TArray<FGuid>& Bindings, INodeNameAdapter& NodeNameAdapter, FMovieSceneSequenceIDRef& Template,  const FString& InFBXFileName, FMovieSceneSequenceTransform& RootToLocalTransform);

	/**
	* Import FBX with dialog
	*
	* @param InMovieScene The movie scene to import the fbx into
	* @param InObjectBindingNameMap The object binding to name map to map import fbx animation onto
	* @param bCreateCameras Whether to allow creation of cameras if found in the fbx file.
	* @return Whether the import was successful
	*/
	static bool ImportFBXWithDialog(UMovieSceneSequence* InSequence, ISequencer& InSequencer, const TMap<FGuid, FString>& InObjectBindingNameMap, TOptional<bool> bCreateCameras);

	/**
	* Get FBX Ready for Import. This make sure the passed in file make be imported. After calling this call ImportReadiedFbx. It returns out some parameters that we forcably change so we reset them later.
	*
	* @param ImportFileName The filename to import into
	* @param ImportFBXSettings FBX Import Settings to enforce.
	* @param OutFBXParams Paremter to pass back to ImportReadiedFbx
	* @return Whether the fbx file was ready and is ready to be import.
	*/
	static bool ReadyFBXForImport(const FString&  ImportFilename, UMovieSceneUserImportFBXSettings* ImportFBXSettings, FFBXInOutParameters& OutFBXParams);

	/**
	* Import into an FBX scene that has been readied already, via the ReadyFBXForImport call. We do this as two pass in case the client want's to do something, like create camera's, before actually
	* loading the data.
	*
	* @param World The world to import the fbx into
	* @param InMovieScene The movie scene to import the fbx into
	* @param ObjectBindingMap Map relating binding id's to track names. 
	* @param TemplateID Id of the sequence that contains the objects being imported onto 
	* @param ImportFBXSettings FBX Import Settings to enforce.
	* @param InFBXParams Paremter from ImportReadiedFbx used to reset some internal fbx settings that we override.
	* @return Whether the fbx file was ready and is ready to be import.
	*/

	static bool ImportFBXIfReady(UWorld* World, UMovieSceneSequence* InSequence, IMovieScenePlayer* Player, FMovieSceneSequenceIDRef TemplateID, TMap<FGuid, FString>& ObjectBindingMap, UMovieSceneUserImportFBXSettings* ImportFBXSettings,
		const FFBXInOutParameters& InFBXParams);

	/**
	* Import FBX Camera to existing camera's
	*
	* @param FbxImporter The Fbx importer
	* @param InMovieScene The movie scene to import the fbx into
	* @param Player The player we are getting objects from.
	* @param TemplateID Id of the sequence that contains the objects being imported onto
	* @param InObjectBindingNameMap The object binding to name map to map import fbx animation onto
	* @param bCreateCameras Whether to allow creation of cameras if found in the fbx file.
	* @param bNotifySlate  If an issue show's up, whether or not to notify the UI.
	* @return Whether the import was successful
	*/

	static void ImportFBXCameraToExisting(UnFbx::FFbxImporter* FbxImporter, UMovieSceneSequence* InSequence, IMovieScenePlayer* Player, FMovieSceneSequenceIDRef TemplateID, TMap<FGuid, FString>& InObjectBindingMap, bool bMatchByNameOnly, bool bNotifySlate);

	/**
	* Import FBX Node to existing actor/node
	*
	* @param NodeName Name of fbx node/actor
	* @param CurvesApi Existing FBX curves coming in
	* @param InMovieScene The movie scene to import the fbx into
	* @param Player The player we are getting objects from.
	* @param TemplateID Id of the sequence template ID.
	* @param ObjectBinding Guid of the object we are importing onto.
	* @return Whether the import was successful
	*/
	static bool ImportFBXNode(FString NodeName, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieSceneSequence* InSequence, IMovieScenePlayer* Player, FMovieSceneSequenceIDRef TemplateID, FGuid ObjectBinding);


	/**
	*  Camera track was added, we usually do extra things, like add a Camera Cut tracks
	*
	* @param MovieScene MovieScene to add Camera.
	* @param CameraGuid CameraGuid  Guid of the camera that was added.
	* @param FrameNumber FrameNumber it's added at.
	*/
	static void CameraAdded(UMovieScene* MovieScene, FGuid CameraGuid, FFrameNumber FrameNumber);

	/**
	* Import FBX Camera to existing camera's
	*
	* @param CameraNode The Fbx camera
	* @param InCameraActor UE actor
	*/
	static void CopyCameraProperties(fbxsdk::FbxCamera* CameraNode, AActor* InCameraActor);


	/*
	 * Rich curve interpolation to matinee interpolation
	 *
	 * @param InterpMode The rich curve interpolation to convert
	 * @return The converted matinee interpolation
	 */
	static EInterpCurveMode RichCurveInterpolationToMatineeInterpolation( ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode);

	/*
	 * Copy key data to move axis
	 *
	 * @param KeyData The key data to copy from
	 * @param MoveAxis The move axis to copy to
	 * @param FrameRate The frame rate of the source channel
	 */
	static void CopyKeyDataToMoveAxis(const TMovieSceneChannelData<FMovieSceneFloatValue>& KeyData, UInterpTrackMoveAxis* MoveAxis, FFrameRate FrameRate);

	/*
	 * Export the object binding to a camera anim
	 *
	 * @param InMovieScene The movie scene to export the object binding from
	 * @param InObjectBinding The object binding to export
	 * @return The exported camera anim asset
	 */
	static UObject* ExportToCameraAnim(UMovieScene* InMovieScene, FGuid& InObjectBinding);


	/*
	 * Export the SkelMesh to an Anim Sequence for specified MovieScene and Player
	 *
	 * @param AnimSequence The sequence to save to.
	 * @param ExportOptions The options to use when saving.
	 * @param MovieScene The movie scene to export the object binding from
	 * @param Player The Player to evaluate
	 * @param SkelMesh The Player to evaluate
	 * @param Template ID of the sequence template.
	 * @param RootToLocalTransform Transform Offset to apply to exported anim sequence.
	 * @return Whether or not it succeeds

	*/
	static bool ExportToAnimSequence(UAnimSequence* AnimSequence, UAnimSeqExportOption* ExportOptions, UMovieScene* MovieScene, IMovieScenePlayer* Player,
		USkeletalMeshComponent* SkelMesh, FMovieSceneSequenceIDRef& Template, FMovieSceneSequenceTransform& RootToLocalTransform);

	/*
	 * Bake the SkelMesh to a generic object wich implements a set of callbacks
	 *
	 * @param MovieScene The movie scene to export the object binding from
	 * @param Player The Player to evaluate
	 * @param SkelMesh The Player to evaluate
	 * @param Template ID of the sequence template.
	 * @param RootToLocalTransform Transform Offset to apply to exported anim sequence.
	 * @param InitCallback Callback before it starts running, maybe performance heavy.
	 * @param StartCallback Callback right before starting if needed, should be lightweight.
	 * @param TickCallback Callback per tick where you can bake the skelmesh.
	 * @param EndCallback Callback at end to finalize the baking.
	 * @return Whether or not it succeeds
	
	*/

	static bool BakeToSkelMeshToCallbacks(UMovieScene* MovieScene, IMovieScenePlayer* Player,
		USkeletalMeshComponent* SkelMesh, FMovieSceneSequenceIDRef& Template, FMovieSceneSequenceTransform& RootToLocalTransform,
		FInitAnimationCB InitCallback, FStartAnimationCB StartCallback, FTickAnimationCB TickCallback, FEndAnimationCB EndCallback);


	/*
	 * @return Whether this object class has hidden mobility and can't be animated
	 */
	static bool HasHiddenMobility(const UClass* ObjectClass);
	
	/*
	* Get the Active EvaluationTrack for a given track. Will do a recompile if the track isn't valid
	*@param Sequencer The sequencer we are evaluating
	*@aram Track The movie scene track whose evaluation counterpart we want
	*@return Returns the evaluation track for the given movie scene track. May do a re-compile if needed.
	*/
	static const FMovieSceneEvaluationTrack* GetEvaluationTrack(ISequencer *Sequencer, const FGuid& TrackSignature);


	/*
	* Get the location at time for the specified transform evaluation track
	*@param Track The sequencer we are evaluating
	*@param Object The object that owns this track
	*@param KeyTime the time to evaluate
	*@param KeyPos The position at this time
	*@param KeyRot The rotation at this time
	*@param Sequencer The Sequence that owns this track
	*/
	static void GetLocationAtTime(const FMovieSceneEvaluationTrack* Track, UObject* Object, FFrameTime KeyTime, FVector& KeyPos, FRotator& KeyRot, const TSharedPtr<ISequencer>& Sequencer);
	
	/* Get the Parents (Scene/Actors) of this object.
	* @param Parents Returned Parents
	* @param InObject Object to find parents for
	*/
	static void GetParents(TArray<const UObject*>& Parents, const UObject* InObject);
	
	/* Return Reference Frame from the passed in paretns
	* @param Sequencer The Sequence that's driving these parents.
	* @param Parents Parents in sequencer to evaluate to find reference transforms
	* @param KeyTime Time to Evaluate At
	* @return Returns Reference Transform.
	*/
	static FTransform GetRefFrameFromParents(const TSharedPtr<ISequencer>& Sequencer, const TArray<const UObject*>& Parents, FFrameTime KeyTime);

	/* Return Return ParentTm for current Parent Object
	* @param CurrentRefTM Current Referemnce TM
	* @param Sequencer The Sequence that's driving these parents.
	* @param ParentObject The Parent
	* @param KeyTime Time to Evaluate At
	* @return Returns true if succesful in evaluating the parent in the sequencer and getting a transform.
	*/
	static bool GetParentTM(FTransform& CurrentRefTM, const TSharedPtr<ISequencer>& Sequencer, UObject* ParentObject, FFrameTime KeyTime);

	/*
	 * Get the fbx cameras from the requested parent node
	 */
	static void GetCameras(fbxsdk::FbxNode* Parent, TArray<fbxsdk::FbxCamera*>& Cameras);

	/*
	 * Get the fbx camera name
	 */
	static FString GetCameraName(fbxsdk::FbxCamera* InCamera);

	/*
	 * Import FBX into Channels With Dialog
	 */
	static bool ImportFBXIntoChannelsWithDialog(const TSharedRef<ISequencer>& InSequencer, TArray<FFBXNodeAndChannels>* NodeAndChannels);

	/*
	* Import FBX into Channels
	*/	
	static bool ImportFBXIntoControlRigChannels(UMovieScene* MovieScene, const FString& ImportFilename,  UMovieSceneUserImportFBXControlRigSettings *ControlRigSettings,
		TArray<FFBXNodeAndChannels>* NodeAndChannels, const TArray<FName>& SelectedControlNames, FFrameRate FrameRate);

	/*
	* Acquire first SkeletalMeshComponent from the Object
	* @param BoundObject Object to get SkeletalMeshComponent from.If actor checks it's components, if component checks itself then child components.
	* @return Returns the USkeletalMeshComponent if one is found
	*/
	static USkeletalMeshComponent* AcquireSkeletalMeshFromObject(UObject* BoundObject);
	/*
	*  Get an actors word transforms at the specified times
	* @param Sequencer Sequencer to evaluation
    * @param ActorForWorldTransforms The actor and possible component and socket that we want to get the world transforms for.
	* @param Frames The times we want to get the world transforms
	* @param OutWorldTransforms The calculated world transforms, one for each specified frame.
	*/
	static void GetActorWorldTransforms(ISequencer* Sequencer, const FActorForWorldTransforms& Actors, const TArray<FFrameNumber>& Frames, TArray<FTransform>& OutWorldTransforms);


};

// Helper to make spawnables persist throughout the export process and then restore properly afterwards
struct MOVIESCENETOOLS_API FSpawnableRestoreState
{
	FSpawnableRestoreState(UMovieScene* MovieScene);
	~FSpawnableRestoreState();

	bool bWasChanged;
	TMap<FGuid, ESpawnOwnership> SpawnOwnershipMap;
	TWeakObjectPtr<UMovieScene> WeakMovieScene;
};

class FTrackEditorBindingIDPicker : public FMovieSceneObjectBindingIDPicker
{
public:
	FTrackEditorBindingIDPicker(FMovieSceneSequenceID InLocalSequenceID, TWeakPtr<ISequencer> InSequencer)
		: FMovieSceneObjectBindingIDPicker(InLocalSequenceID, InSequencer)
	{
		Initialize();
	}

	DECLARE_EVENT_OneParam(FTrackEditorBindingIDPicker, FOnBindingPicked, FMovieSceneObjectBindingID)
	FOnBindingPicked& OnBindingPicked()
	{
		return OnBindingPickedEvent;
	}

	using FMovieSceneObjectBindingIDPicker::GetPickerMenu;

private:

	virtual UMovieSceneSequence* GetSequence() const override { return WeakSequencer.Pin()->GetFocusedMovieSceneSequence(); }
	virtual void SetCurrentValue(const FMovieSceneObjectBindingID& InBindingId) override { OnBindingPickedEvent.Broadcast(InBindingId); }
	virtual FMovieSceneObjectBindingID GetCurrentValue() const override { return FMovieSceneObjectBindingID(); }

	FOnBindingPicked OnBindingPickedEvent;
};


