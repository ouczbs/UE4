// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Views/STreeView.h"
#include "Misc/TextFilter.h"

class FAssetThumbnailPool;
class FAssetPlacementPaletteItemModel;
class FMenuBuilder;
class FUICommandList;
class IDetailsView;
class UFoliageType;
struct FAssetData;
class UAssetPlacementSettings;
class IPropertyHandle;

typedef TSharedPtr<FAssetPlacementPaletteItemModel> FPlacementPaletteItemModelPtr;
typedef STreeView<FPlacementPaletteItemModelPtr> SPlacementTypeTreeView;
typedef STileView<FPlacementPaletteItemModelPtr> SPlacementTypeTileView;

/** The palette of Placement types available for use by the Placement edit mode */
class SAssetPlacementPalette : public SCompoundWidget
{
public:
	/** View modes supported by the palette */
	enum class EViewMode : uint8
	{
		Thumbnail,
		Tree
	};

	SLATE_BEGIN_ARGS(SAssetPlacementPalette) {}
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PalettePropertyHandle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SAssetPlacementPalette();

	// SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Updates the Placement palette, optionally doing a full rebuild of the items in the palette as well */
	void UpdatePalette(bool bRebuildItems = false);

	/** Refreshes the Placement palette */
	void RefreshPalette();

	/** @return True if the given view mode is the active view mode */
	bool IsActiveViewMode(EViewMode ViewMode) const;

	/** @return True if tooltips should be shown when hovering over Placement type items in the palette */
	bool ShouldShowTooltips() const;

	/** @return The current search filter text */
	FText GetSearchText() const;

private:
	/** Adds the Placement type asset to the instanced Placement actor's list of types. */
	void AddPlacementType(const FAssetData& AssetData);

	/**
	 * Removes all items from the palette.
	 */
	void ClearPalette();
	void OnClearPalette();
	void SetPaletteToAssetDataList(TArrayView<const FAssetData> InAssetDatas);

	/** Refreshes the active palette view widget */
	void RefreshActivePaletteViewWidget();

	/** Creates the palette views */
	TSharedRef<class SWidgetSwitcher> CreatePaletteViews();

	/** Adds the displayed name of the Placement type for filtering */
	void GetPaletteItemFilterString(FPlacementPaletteItemModelPtr PaletteItemModel, TArray<FString>& OutArray) const;

	/** Handles changes to the search filter text */
	void OnSearchTextChanged(const FText& InFilterText);

	bool ShouldFilterAsset(const FAssetData& InAssetData);

	void OnContentBrowserMirrorButtonClicked(ECheckBoxState InState);
	void OnContentBrowserSelectionChanged(const TArray<FAssetData>& NewSelectedAssets, bool bIsPrimaryBrowser);
	void SetupContentBrowserMirroring(bool bInMirrorContentBrowser);

	/** Sets the view mode of the palette */
	void SetViewMode(EViewMode NewViewMode);

	/** Sets whether to show tooltips when hovering over Placement type items in the palette */
	void ToggleShowTooltips();

	/** Switches the palette display between the tile and tree view */
	FReply OnToggleViewModeClicked();

	/** @return The index of the view widget to display */
	int32 GetActiveViewIndex() const;

	/** Creates the view options menu */
	TSharedRef<SWidget> GetViewOptionsMenuContent();

	TSharedPtr<SListView<FPlacementPaletteItemModelPtr>> GetActiveViewWidget() const;

	/** Gets the visibility of the "Drop Placement Here" prompt for when the palette is empty */
	EVisibility GetDropPlacementHintVisibility() const;

	/** Gets the visibility of the drag-drop zone overlay */
	EVisibility GetPlacementDropTargetVisibility() const;

	/** Handles dropping of a mesh or Placement type into the palette */
	FReply HandlePlacementDropped(const FGeometry& DropZoneGeometry, const FDragDropEvent& DragDropEvent);

	/** @returns true if there are any items in the palette. */
	bool HasAnyItemInPalette() const;

private:	// CONTEXT MENU

	/** @return the SWidget containing the context menu */
	TSharedPtr<SWidget> ConstructPlacementTypeContextMenu();

	/** Handler for 'Show in CB' command  */
	void OnShowPlacementTypeInCB();

private:	// THUMBNAIL VIEW

	/** Creates a thumbnail tile for the given Placement type */
	TSharedRef<ITableRow> GenerateTile(FPlacementPaletteItemModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the scaled thumbnail tile size */
	float GetScaledThumbnailSize() const;

	/** Gets the current scale of the thumbnail tiles */
	float GetThumbnailScale() const;

	/** Sets the current scale of the thumbnail tiles */
	void SetThumbnailScale(float InScale);

	/** Gets whether the thumbnail scaling slider is visible */
	EVisibility GetThumbnailScaleSliderVisibility() const;

private:	// TREE VIEW

	/** Generates a row widget for Placement mesh item */
	TSharedRef<ITableRow> TreeViewGenerateRow(FPlacementPaletteItemModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void TreeViewGetChildren(FPlacementPaletteItemModelPtr Item, TArray<FPlacementPaletteItemModelPtr>& OutChildren);

	/** Text for Placement meshes list header */
	FText GetTypeColumnHeaderText() const;

	/** Mesh list sorting support */
	EColumnSortMode::Type GetMeshColumnSortMode() const;
	void OnTypeColumnSortModeChanged(EColumnSortPriority::Type InPriority, const FName& InColumnName, EColumnSortMode::Type InSortMode);

private:
	/** Handles the click for the uneditable blueprint Placement type warning */
	void OnEditPlacementTypeBlueprintHyperlinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);

private:
	/** Active timer handler to update the items in the palette */
	EActiveTimerReturnType UpdatePaletteItems(double InCurrentTime, float InDeltaTime);

	/** Active timer handler to refresh the palette */
	EActiveTimerReturnType RefreshPaletteItems(double InCurrentTime, float InDeltaTime);

private:
	typedef TTextFilter<FPlacementPaletteItemModelPtr> PlacementTypeTextFilter;
	TSharedPtr<PlacementTypeTextFilter> TypeFilter;

	/** All the items in the palette (unfiltered) */
	TArray<FPlacementPaletteItemModelPtr> PaletteItems;

	/** The filtered list of types to display in the palette */
	TArray<FPlacementPaletteItemModelPtr> FilteredItems;

	/** Switches between the thumbnail and tree views */
	TSharedPtr<class SWidgetSwitcher> WidgetSwitcher;

	/** The header row of the Placement mesh tree */
	TSharedPtr<class SHeaderRow> TreeViewHeaderRow;

	/** Placement type thumbnails widget  */
	TSharedPtr<SPlacementTypeTileView> TileViewWidget;

	/** Placement type tree widget  */
	TSharedPtr<SPlacementTypeTreeView> TreeViewWidget;

	/** Placement mesh details widget  */
	TSharedPtr<class IDetailsView> DetailsWidget;

	TSharedPtr<IPropertyHandle> PalettePropertyHandle;

	/** Placement items search box widget */
	TSharedPtr<class SSearchBox> SearchBoxPtr;

	/** Command list for binding functions for the context menu. */
	TSharedPtr<FUICommandList> UICommandList;

	/** Thumbnail pool for rendering mesh thumbnails */
	TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;

	bool bItemsNeedRebuild = true;
	bool bShowFullTooltips = true;
	bool bIsRebuildTimerRegistered = true;
	bool bIsRefreshTimerRegistered = true;
	bool bIsMirroringContentBrowser = true;
	EViewMode ActiveViewMode;
	EColumnSortMode::Type ActiveSortOrder = EColumnSortMode::Type::Ascending;

	float PaletteThumbnailScale = .3f;
};
