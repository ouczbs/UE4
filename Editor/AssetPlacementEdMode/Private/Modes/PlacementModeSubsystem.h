// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "UObject/ObjectPtr.h"

#include "PlacementModeSubsystem.generated.h"

struct FPaletteItem;
struct FTypedElementHandle;
class UAssetPlacementSettings;

UCLASS(Transient)
class UPlacementModeSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// End USubsystem Interface

	// @returns the settings object for the mode for sharing across all tools and tool builders.
	const UAssetPlacementSettings* GetModeSettingsObject() const;

	/**
	 * Verifies if the given element handle is supported by the current mode settings' palette.
	 *
	 * @returns true if the element can be placed by the mode.
	 */
	bool DoesCurrentPaletteSupportElement(const FTypedElementHandle& InElementToCheck) const;

	/**
	 * Adds the given palette item to the current palette.
	 * 
	 * @returns a pointer to the added item, if the add was successful.
	 */
	TSharedPtr<FPaletteItem> AddPaletteItem(const FAssetData& InAssetData);

	/**
	 * Clears all items from the current palette.
	 */
	void ClearPalette();

	/**
	 * Updates the settings object to use the content browser's active selection as the palette.
	 */
	void SetUseContentBrowserAsPalette(bool bInUseContentBrowser);

protected:
	UPROPERTY()
	TObjectPtr<UAssetPlacementSettings> ModeSettings;
};
