// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/IToolkit.h"
#include "Framework/Commands/UICommandList.h"
#include "Toolkits/IToolkitHost.h"

class UInteractiveTool;
class UInteractiveToolManager;
class UEdMode;
class IDetailsView;
struct FDetailsViewArgs;
enum class EToolShutdownType;

/**
 * Base class for all toolkits (abstract).
 */
class UNREALED_API FBaseToolkit
	: public IToolkit
{
public:

	/** FBaseToolkit constructor */
	FBaseToolkit();

	/** Virtual destructor */
	virtual ~FBaseToolkit();

public:

	// IToolkit interface

	virtual FName GetToolkitContextFName() const override;
	virtual FText GetTabSuffix() const override { return FText::GetEmpty(); }
	virtual bool ProcessCommandBindings(const FKeyEvent& InKeyEvent) const override;
	virtual bool IsHosted() const override;
	virtual const TSharedRef<IToolkitHost> GetToolkitHost() const override;
	virtual void BringToolkitToFront() override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;
	virtual bool IsBlueprintEditor() const override;
	virtual TSharedRef<FWorkspaceItem> GetWorkspaceMenuCategory() const override { return WorkspaceMenuCategory.ToSharedRef(); }

	virtual FEditorModeTools& GetEditorModeManager() const = 0;

public:

	/** @return	Returns true if this is a world-centric asset editor.  That is, the user is editing the asset inline in a Level Editor app. */
	bool IsWorldCentricAssetEditor() const;	

	/** @returns Returns our toolkit command list */
	const TSharedRef<FUICommandList> GetToolkitCommands() const
	{
		return ToolkitCommands;
	}

protected:

	/** @return Returns the prefix string to use for tabs created for this toolkit.  In world-centric mode, tabs get a
	    name prefix to make them distinguishable from other tabs */
	FString GetTabPrefix() const;

	/** @return Returns the color to use for tabs created for this toolkit.  In world-centric mode, tabs may be colored to
	    make them more easy to distinguish compared to other tabs. */
	FLinearColor GetTabColorScale() const;

	// Creates the Editor mode manager for your class. Default is to create none, for legacy reasons.
	virtual void CreateEditorModeManager();

protected:

	/** Asset editing mode, set at creation-time and never changes */
	EToolkitMode::Type ToolkitMode;

	/** List of UI commands for this toolkit.  This should be filled in by the derived class! */
	TSharedRef<FUICommandList> ToolkitCommands;

	/** The host application for this editor.  If editing in world-centric mode, this is the level level editor that we're editing the asset within 
	    Use GetToolkitHost() method to access this member. */
	TWeakPtr<IToolkitHost> ToolkitHost;

	/** The workspace menu category of this toolkit */
	TSharedPtr<FWorkspaceItem> WorkspaceMenuCategory;
};



/**
 * This FModeToolkit just creates a basic UI panel that allows various InteractiveTools to
 * be initialized, and a DetailsView used to show properties of the active Tool.
 */
class UNREALED_API FModeToolkit
	: public FBaseToolkit
	, public TSharedFromThis<FModeToolkit>
{
public:

	/** Initializes the mode toolkit */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost);
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode);
	~FModeToolkit();

public:

	// FBaseToolkit overrides

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override { }
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override { }

public:

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override { return GetBaseToolkitName(); }
	virtual FText GetToolkitToolTipText() const override { return GetBaseToolkitName(); }
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual bool IsAssetEditor() const override;
	virtual const TArray<UObject*>* GetObjectsCurrentlyBeingEdited() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual FText GetEditorModeDisplayName() const override;
	virtual FSlateIcon GetEditorModeIcon() const override;
	virtual FEditorModeTools& GetEditorModeManager() const final;

	virtual TWeakObjectPtr<UEdMode> GetScriptableEditorMode() const final;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

	/** Returns the number of Mode specific tabs in the mode toolbar **/
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const {}

	/**
	 * @param PaletteIndex      The index of the ToolPalette to build
	 * @returns the name of Tool Palette
	 **/
	virtual FText GetToolPaletteDisplayName(FName Palette) const { return FText(); }

	/* Exclusive Tool Palettes only allow users to use tools from one palette at a time */
	virtual bool HasExclusiveToolPalettes() const { return true; }

	/* Integrated Tool Palettes show up in the same panel as their details */
	virtual bool HasIntegratedToolPalettes() const { return true; }

	/**
	 * @param PaletteIndex      The index of the ToolPalette to build
	 * @param ToolbarBuilder    The builder to use for given PaletteIndex
	**/
	virtual void BuildToolPalette(FName Palette, class FToolBarBuilder& ToolbarBuilder);

	virtual FText GetActiveToolDisplayName() const { return FText(); }
	virtual FText GetActiveToolMessage() const { return FText(); }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPaletteChanged, FName);
	FOnPaletteChanged& OnPaletteChanged() { return OnPaletteChangedDelegate; }

	virtual void OnToolPaletteChanged(FName PaletteName) {};
	void SetCurrentPalette(FName InName);
	FName GetCurrentPalette() const;
	void SetModeSettingsObject(UObject* InSettingsObject);

protected:
	bool CanStartTool(const FString& ToolTypeIdentifier);
	bool CanAcceptActiveTool();
	bool CanCancelActiveTool();
	bool CanCompleteActiveTool();

	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool);
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool);

	virtual void CustomizeModeDetailsViewArgs(FDetailsViewArgs& ArgsInOut) {}
	virtual void CustomizeDetailsViewArgs(FDetailsViewArgs& ArgsInOut) {}

protected:
	TSharedPtr<SWidget> ToolkitWidget;
	TSharedPtr<IDetailsView> ModeDetailsView;
	TSharedPtr<IDetailsView> DetailsView;

	TWeakObjectPtr<UEdMode> OwningEditorMode;

	FName CurrentPaletteName;
	FOnPaletteChanged OnPaletteChangedDelegate;
};
