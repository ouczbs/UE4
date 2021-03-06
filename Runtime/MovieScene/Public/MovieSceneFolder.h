// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "MovieSceneFolder.generated.h"

class UMovieSceneTrack;

/** Represents a folder used for organizing objects in tracks in a movie scene. */
UCLASS(DefaultToInstanced)
class MOVIESCENE_API UMovieSceneFolder : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the name of this folder. */
	FName GetFolderName() const;

	/** Sets the name of this folder. Automatically calls Modify() on the folder object. */
	void SetFolderName( FName InFolderName );

	/** Gets the folders contained by this folder. */
	const TArray<UMovieSceneFolder*>& GetChildFolders() const;

	/** Adds a child folder to this folder. Automatically calls Modify() on the folder object. */
	void AddChildFolder( UMovieSceneFolder* InChildFolder );

	/** Removes a child folder from this folder. Automatically calls Modify() on the folder object. */
	void RemoveChildFolder( UMovieSceneFolder* InChildFolder );

	/** Gets the master tracks contained by this folder. */
	const TArray<UMovieSceneTrack*>& GetChildMasterTracks() const;

	/** Adds a master track to this folder. Automatically calls Modify() on the folder object. */
	void AddChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Removes a master track from this folder. Automatically calls Modify() on the folder object. */
	void RemoveChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Gets the guids for the object bindings contained by this folder. */
	const TArray<FGuid>& GetChildObjectBindings() const;

	/** Adds a guid for an object binding to this folder. Automatically calls Modify() on the folder object. */
	void AddChildObjectBinding(const FGuid& InObjectBinding );

	/** Removes a guid for an object binding from this folder. Automatically calls Modify() on the folder object. */
	void RemoveChildObjectBinding( const FGuid& InObjectBinding );

	/** Called after this object has been deserialized */
	virtual void PostLoad() override;

	/** Searches for a guid in this folder and it's child folders, if found returns the folder containing the guid. */
	UMovieSceneFolder* FindFolderContaining(const FGuid& InObjectBinding);


	virtual void Serialize( FArchive& Archive );

#if WITH_EDITORONLY_DATA
	/**
	 * Get this folder's color.
	 *
	 * @return The folder color.
	 */
	const FColor& GetFolderColor() const
	{
		return FolderColor;
	}

	/**
	 * Set this folder's color. Does not call Modify() on the folder object for legacy reasons.
	 *
	 * @param InFolderColor The folder color to set.
	 */
	void SetFolderColor(const FColor& InFolderColor)
	{
		FolderColor = InFolderColor;
	}

	/**
	 * Get this folder's desired sorting order 
	 */
	int32 GetSortingOrder() const
	{
		return SortingOrder;
	}

	/**
	 * Set this folder's desired sorting order. Does not call Modify() internally for legacy reasons.
	 *
	 * @param InSortingOrder The higher the value the further down the list the folder will be.
	 */
	void SetSortingOrder(const int32 InSortingOrder)
	{
		SortingOrder = InSortingOrder;
	}
#endif

private:
	/** The name of this folder. */
	UPROPERTY()
	FName FolderName;

	/** The folders contained by this folder. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneFolder>> ChildFolders;

	/** The master tracks contained by this folder. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneTrack>> ChildMasterTracks;

	/** The guid strings used to serialize the guids for the object bindings contained by this folder. */
	UPROPERTY()
	TArray<FString> ChildObjectBindingStrings;

#if WITH_EDITORONLY_DATA
	/** This folder's color */
	UPROPERTY(EditAnywhere, Category=General, DisplayName=Color)
	FColor FolderColor;

	/** This folder's desired sorting order */
	UPROPERTY()
	int32 SortingOrder;
#endif

	/** The guids for the object bindings contained by this folder. */
	TArray<FGuid> ChildObjectBindings;
};

