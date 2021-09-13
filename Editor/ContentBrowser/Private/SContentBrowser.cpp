// Copyright Epic Games, Inc. All Rights Reserved.


#include "SContentBrowser.h"
#include "Factories/Factory.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UICommandList.h"
#include "Algo/Transform.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"
#include "Settings/ContentBrowserSettings.h"
#include "Settings/EditorSettings.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetRegistryModule.h"
#include "AssetRegistryState.h"
#include "AssetToolsModule.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "ContentBrowserLog.h"
#include "FrontendFilters.h"
#include "ContentBrowserPluginFilters.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "ContentBrowserDataSource.h"
#include "SourcesSearch.h"
#include "SFilterList.h"
#include "SPathView.h"
#include "SCollectionView.h"
#include "SAssetView.h"
#include "AssetContextMenu.h"
#include "NewAssetOrClassContextMenu.h"
#include "PathContextMenu.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserCommands.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "IAddContentDialogModule.h"
#include "UObject/GCObjectScopeGuard.h"
#include "Engine/Selection.h"
#include "AddToProjectConfig.h"
#include "GameProjectGenerationModule.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"
#include "IContentBrowserDataModule.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "ToolMenu.h"
#include "Widgets/Input/SExpandableButton.h"
#include "SExpandableSearchArea.h"
#include "SEditorHeaderButton.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

const FString SContentBrowser::SettingsIniSection = TEXT("ContentBrowser");

namespace ContentBrowserSourcesWidgetSwitcherIndex
{
	static const int32 PathView = 0;
	static const int32 CollectionsView = 1;
}

SContentBrowser::~SContentBrowser()
{
	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll( this );

	// Remove listeners for when collections/paths are renamed/deleted
	if (FCollectionManagerModule::IsModuleAvailable())
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		CollectionManagerModule.Get().OnCollectionRenamed().RemoveAll(this);
		CollectionManagerModule.Get().OnCollectionDestroyed().RemoveAll(this);
	}

	if (IContentBrowserDataModule* ContentBrowserDataModule = IContentBrowserDataModule::GetPtr())
	{
		if (UContentBrowserDataSubsystem* ContentBrowserData = ContentBrowserDataModule->GetSubsystem())
		{
			ContentBrowserData->OnItemDataUpdated().RemoveAll(this);
		}
	}

	if (bIsPrimaryBrowser && GEditor)
	{
		if (USelection* EditorSelection = GEditor->GetSelectedObjects())
		{
			EditorSelection->DeselectAll();
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SContentBrowser::Construct( const FArguments& InArgs, const FName& InInstanceName, const FContentBrowserConfig* Config )
{
	if ( InArgs._ContainingTab.IsValid() )
	{
		// For content browsers that are placed in tabs, save settings when the tab is closing.
		ContainingTab = InArgs._ContainingTab;
		InArgs._ContainingTab->SetOnPersistVisualState( SDockTab::FOnPersistVisualState::CreateSP( this, &SContentBrowser::OnContainingTabSavingVisualState ) );
		InArgs._ContainingTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateSP( this, &SContentBrowser::OnContainingTabClosed ) );
		InArgs._ContainingTab->SetOnTabActivated( SDockTab::FOnTabActivatedCallback::CreateSP( this, &SContentBrowser::OnContainingTabActivated ) );
	}
	

 	bIsLocked = InArgs._InitiallyLocked;
	bCanSetAsPrimaryBrowser = Config != nullptr ? Config->bCanSetAsPrimaryBrowser : true;
	bIsDrawer = InArgs._IsDrawer;

	HistoryManager.SetOnApplyHistoryData(FOnApplyHistoryData::CreateSP(this, &SContentBrowser::OnApplyHistoryData));
	HistoryManager.SetOnUpdateHistoryData(FOnUpdateHistoryData::CreateSP(this, &SContentBrowser::OnUpdateHistoryData));

	PathContextMenu = MakeShareable(new FPathContextMenu( AsShared() ));
	PathContextMenu->SetOnRenameFolderRequested(FPathContextMenu::FOnRenameFolderRequested::CreateSP(this, &SContentBrowser::OnRenameRequested));
	PathContextMenu->SetOnFolderDeleted(FPathContextMenu::FOnFolderDeleted::CreateSP(this, &SContentBrowser::OnOpenedFolderDeleted));
	PathContextMenu->SetOnFolderFavoriteToggled(FPathContextMenu::FOnFolderFavoriteToggled::CreateSP(this, &SContentBrowser::ToggleFolderFavorite));
	FrontendFilters = MakeShareable(new FAssetFilterCollectionType());
	TextFilter = MakeShareable( new FFrontendFilter_Text() );

	PluginPathFilters = MakeShareable(new FPluginFilterCollectionType());

	SourcesSearch = MakeShared<FSourcesSearch>();
	SourcesSearch->Initialize();
	SourcesSearch->SetHintText(LOCTEXT("SearchPathsHint", "Search Paths"));

	CollectionSearch = MakeShared<FSourcesSearch>();
	CollectionSearch->Initialize();
	CollectionSearch->SetHintText(LOCTEXT("CollectionsViewSearchBoxHint", "Search Collections"));

	CollectionViewPtr = SNew(SCollectionView)
		.OnCollectionSelected(this, &SContentBrowser::CollectionSelected)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserCollections")))
		.AllowCollectionDrag(true)
		.AllowQuickAssetManagement(true)
		.IsDocked(this, &SContentBrowser::IsCollectionViewDocked)
		.ExternalSearch(CollectionSearch);

	static const FName DefaultForegroundName("DefaultForeground");

	BindCommands();
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SContentBrowser::OnContentBrowserSettingsChanged);

	// Currently this controls the asset count 
	const bool bShowBottomToolbar = Config != nullptr ? Config->bShowBottomToolbar : true;

	AssetViewPtr = SNew(SAssetView)
		.ThumbnailLabel(Config != nullptr ? Config->ThumbnailLabel : EThumbnailLabel::ClassName)
		//.ThumbnailScale(Config != nullptr ? Config->ThumbnailScale : 0.18f)
		.InitialViewType(Config != nullptr ? Config->InitialAssetViewType : EAssetViewType::Tile)
		.OnNewItemRequested(this, &SContentBrowser::OnNewItemRequested)
		.OnItemSelectionChanged(this, &SContentBrowser::OnItemSelectionChanged, EContentBrowserViewContext::AssetView)
		.OnItemsActivated(this, &SContentBrowser::OnItemsActivated)
		.OnGetItemContextMenu(this, &SContentBrowser::GetItemContextMenu, EContentBrowserViewContext::AssetView)
		.OnItemRenameCommitted(this, &SContentBrowser::OnItemRenameCommitted)
		.FrontendFilters(FrontendFilters)
		.HighlightedText(this, &SContentBrowser::GetHighlightedText)
		.ShowBottomToolbar(bShowBottomToolbar)
		.ShowViewOptions(false)  // We control this for the main content browser
		.AllowThumbnailEditMode(true)
		.AllowThumbnailHintLabel(false)
		.CanShowFolders(Config != nullptr ? Config->bCanShowFolders : true)
		.CanShowClasses(Config != nullptr ? Config->bCanShowClasses : true)
		.CanShowRealTimeThumbnails(Config != nullptr ? Config->bCanShowRealTimeThumbnails : true)
		.CanShowDevelopersFolder(Config != nullptr ? Config->bCanShowDevelopersFolder : true)
		.CanShowFavorites(true)
		.CanDockCollections(true)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserAssets")))
		.OwningContentBrowser(SharedThis(this))
		.OnSearchOptionsChanged(this, &SContentBrowser::HandleAssetViewSearchOptionsChanged)
		.FillEmptySpaceInTileView(true);


	TSharedRef<SWidget> ViewOptions = SNullWidget::NullWidget;

	// Note, for backwards compatibility ShowBottomToolbar controls the visibility of view options so we respect that here
	if (bShowBottomToolbar)
	{
		ViewOptions =
			SNew(SComboButton)
			.ContentPadding(0)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.OnGetMenuContent(AssetViewPtr.ToSharedRef(), &SAssetView::GetViewButtonContent)
			.HasDownArrow(false)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Settings", "Settings"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0, 0, 0, 0 )
		[
			SNew( SBorder )
			.Padding( FMargin( 3 ) )
			.BorderImage(bIsDrawer ? FStyleDefaults::GetNoBrush() : FAppStyle::Get().GetBrush("Brushes.Panel"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(5, 0, 0, 0)
				[
					CreateToolBar(Config)
				]
				// History Back Button
				+SHorizontalBox::Slot()
				.Padding(10, 0, 0, 0)
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.ButtonStyle(FEditorStyle::Get(), "SimpleButton")
					.ToolTipText( this, &SContentBrowser::GetHistoryBackTooltip )
					.ContentPadding( FMargin(1, 0) )
					.OnClicked(this, &SContentBrowser::BackClicked)
					.IsEnabled(this, &SContentBrowser::IsBackEnabled)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserHistoryBack")))
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowLeft"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				// History Forward Button
				+ SHorizontalBox::Slot()
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.ButtonStyle(FEditorStyle::Get(), "SimpleButton")
					.ToolTipText( this, &SContentBrowser::GetHistoryForwardTooltip )
					.ContentPadding( FMargin(1, 0) )
					.OnClicked(this, &SContentBrowser::ForwardClicked)
					.IsEnabled(this, &SContentBrowser::IsForwardEnabled)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserHistoryForward")))
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowRight"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				// Path picker
				+ SHorizontalBox::Slot()
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Fill)
				[
					SAssignNew( PathPickerButton, SComboButton )
					.Visibility( ( Config != nullptr ? Config->bUsePathPicker : true ) ? EVisibility::Visible : EVisibility::Collapsed )
					.ButtonStyle(FEditorStyle::Get(), "SimpleButton")
					.ToolTipText( LOCTEXT( "PathPickerTooltip", "Choose a path" ) )
					.OnGetMenuContent( this, &SContentBrowser::GetPathPickerContent )
					.HasDownArrow( false )
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserPathPicker")))
					.ContentPadding(FMargin(1, 0))
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				// Path
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding(2, 0, 0, 0)
				[
					SAssignNew(PathBreadcrumbTrail, SBreadcrumbTrail<FString>)
					.ButtonContentPadding(FMargin(2, 2))
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.DelimiterImage(FAppStyle::Get().GetBrush("Icons.ChevronRight"))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.ShowLeadingDelimiter(false)
					.OnCrumbClicked(this, &SContentBrowser::OnPathClicked)
					.HasCrumbMenuContent(this, &SContentBrowser::OnHasCrumbDelimiterContent)
					.GetCrumbMenuContent(this, &SContentBrowser::OnGetCrumbDelimiterContent)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserPath")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					CreateLockButton(Config)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					CreateDrawerDockButton(Config)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				.HAlign(HAlign_Right)
				[
					ViewOptions
				]
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(2.0f)
		]

		// Assets/tree
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f)
		[
			// The tree/assets splitter
			SAssignNew(PathAssetSplitterPtr, SSplitter)
			.PhysicalSplitterHandleSize(2.0f)

			// Sources View
			+ SSplitter::Slot()
			.Value(0.15f)
			[
				SNew(SBox)
				.Padding(FMargin(4.f))
				.Visibility(this, &SContentBrowser::GetSourcesViewVisibility)
				[
					SNew(SBorder)
					.Padding(FMargin(0))
					.BorderImage(FEditorStyle::GetBrush("Brushes.Recessed"))

					[
						// Note: If adding more widgets here, fix ContentBrowserSourcesWidgetSwitcherIndex and the code that uses it!
						SAssignNew(SourcesWidgetSwitcher, SWidgetSwitcher)

						// Paths View
						+SWidgetSwitcher::Slot()
						[
							SAssignNew(PathFavoriteSplitterPtr, SSplitter)
							.Clipping(EWidgetClipping::ClipToBounds)
							.PhysicalSplitterHandleSize(1.0f)
							.HitDetectionSplitterHandleSize(3.0f)
							.Orientation(EOrientation::Orient_Vertical)
							.MinimumSlotHeight(26.0f)
							.Visibility( this, &SContentBrowser::GetSourcesViewVisibility )
							+SSplitter::Slot()
							.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SContentBrowser::GetFavoritesAreaSizeRule))
							.Value(0.2f)
							[
								CreateFavoritesView(Config)
							]
								
							+SSplitter::Slot()
							.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SContentBrowser::GetPathAreaSizeRule))
							.Value(0.8f)
							[
								CreatePathView(Config)
							]

							+SSplitter::Slot()
							.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SContentBrowser::GetCollectionsAreaSizeRule))
							.Value(0.4f)
							[
								CreateDockedCollectionsView(Config)
							]
						]

						// Collections View
						+SWidgetSwitcher::Slot()
						[
							SNew(SBox)
							.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
							[
								CollectionViewPtr.ToSharedRef()
							]
						]
					]
				]
			]

			// Asset View
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				CreateAssetView(Config)
			]
		]
	];

	ExtendViewOptionsMenu(Config);

	AssetContextMenu = MakeShared<FAssetContextMenu>(AssetViewPtr);
	AssetContextMenu->BindCommands(Commands);
	AssetContextMenu->SetOnShowInPathsViewRequested( FAssetContextMenu::FOnShowInPathsViewRequested::CreateSP(this, &SContentBrowser::OnShowInPathsViewRequested) );
	AssetContextMenu->SetOnRenameRequested( FAssetContextMenu::FOnRenameRequested::CreateSP(this, &SContentBrowser::OnRenameRequested) );
	AssetContextMenu->SetOnDuplicateRequested( FAssetContextMenu::FOnDuplicateRequested::CreateSP(this, &SContentBrowser::OnDuplicateRequested) );
	AssetContextMenu->SetOnEditRequested( FAssetContextMenu::FOnEditRequested::CreateSP(this, &SContentBrowser::OnEditRequested) );
	AssetContextMenu->SetOnAssetViewRefreshRequested( FAssetContextMenu::FOnAssetViewRefreshRequested::CreateSP( this, &SContentBrowser::OnAssetViewRefreshRequested) );
	FavoritePathViewPtr->SetTreeTitle(LOCTEXT("Favorites", "Favorites"));
	if( Config != nullptr && Config->SelectedCollectionName.Name != NAME_None )
	{
		// Select the specified collection by default
		FSourcesData DefaultSourcesData( Config->SelectedCollectionName );
		TArray<FString> SelectedPaths;
		AssetViewPtr->SetSourcesData( DefaultSourcesData );
	}
	else
	{
		// Select /Game by default
		FSourcesData DefaultSourcesData(FName("/Game"));
		TArray<FString> SelectedPaths;
		TArray<FString> SelectedFavoritePaths;
		SelectedPaths.Add(TEXT("/Game"));
		PathViewPtr->SetSelectedPaths(SelectedPaths);
		AssetViewPtr->SetSourcesData(DefaultSourcesData);
		FavoritePathViewPtr->SetSelectedPaths(SelectedFavoritePaths);
	}

	// Set the initial history data
	HistoryManager.AddHistoryData();

	// Load settings if they were specified
	this->InstanceName = InInstanceName;
	LoadSettings(InInstanceName);

	if( Config != nullptr )
	{
		// Make sure the sources view is initially visible if we were asked to show it
		if( ( bSourcesViewExpanded && ( !Config->bExpandSourcesView || !Config->bUseSourcesView ) ) ||
			( !bSourcesViewExpanded && Config->bExpandSourcesView && Config->bUseSourcesView ) )
		{
			SourcesViewExpandClicked();
		}
	}
	else
	{
		// in case we do not have a config, see what the global default settings are for the Sources Panel
		if (!bSourcesViewExpanded && GetDefault<UContentBrowserSettings>()->bOpenSourcesPanelByDefault)
		{
			SourcesViewExpandClicked();
		}
	}

	// Bindings to manage history when items are deleted
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	CollectionManagerModule.Get().OnCollectionRenamed().AddSP(this, &SContentBrowser::HandleCollectionRenamed);
	CollectionManagerModule.Get().OnCollectionDestroyed().AddSP(this, &SContentBrowser::HandleCollectionRemoved);
	CollectionManagerModule.Get().OnCollectionUpdated().AddSP(this, &SContentBrowser::HandleCollectionUpdated);

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	ContentBrowserData->OnItemDataUpdated().AddSP(this, &SContentBrowser::HandleItemDataUpdated);

	// We want to be able to search the feature packs in the super search so we need the module loaded 
	IAddContentDialogModule& AddContentDialogModule = FModuleManager::LoadModuleChecked<IAddContentDialogModule>("AddContentDialog");

	// Update the breadcrumb trail path
	OnContentBrowserSettingsChanged(NAME_None);

	RegisterPathViewFiltersMenu();

	// Initialize the search options
	HandleAssetViewSearchOptionsChanged();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SContentBrowser::BindCommands()
{
	Commands = TSharedPtr< FUICommandList >(new FUICommandList);

	Commands->MapAction(FGenericCommands::Get().Rename, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleRenameCommand),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleRenameCommandCanExecute)
	));

	Commands->MapAction(FGenericCommands::Get().Delete, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleDeleteCommandExecute),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleDeleteCommandCanExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().OpenAssetsOrFolders, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleOpenAssetsOrFoldersCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().PreviewAssets, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandlePreviewAssetsCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().CreateNewFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleCreateNewFolderCommandExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().SaveSelectedAsset, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleSaveAssetCommand),
		FCanExecuteAction::CreateSP(this, &SContentBrowser::HandleSaveAssetCommandCanExecute)
	));

	Commands->MapAction(FContentBrowserCommands::Get().SaveAllCurrentFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleSaveAllCurrentFolderCommand)
	));

	Commands->MapAction(FContentBrowserCommands::Get().ResaveAllCurrentFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SContentBrowser::HandleResaveAllCurrentFolderCommand)
	));

	// Allow extenders to add commands
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FContentBrowserCommandExtender> CommmandExtenderDelegates = ContentBrowserModule.GetAllContentBrowserCommandExtenders();

	for (int32 i = 0; i < CommmandExtenderDelegates.Num(); ++i)
	{
		if (CommmandExtenderDelegates[i].IsBound())
		{
			CommmandExtenderDelegates[i].Execute(Commands.ToSharedRef(), FOnContentBrowserGetSelection::CreateSP(this, &SContentBrowser::GetSelectionState));
		}
	}
}

EVisibility SContentBrowser::GetFavoriteFolderVisibility() const
{
	return GetDefault<UContentBrowserSettings>()->GetDisplayFavorites() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SContentBrowser::GetDockedCollectionsVisibility() const
{
	return IsCollectionViewDocked() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SContentBrowser::GetLockButtonVisibility() const
{
	return IsLocked() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SContentBrowser::IsCollectionViewDocked() const
{
	return GetDefault<UContentBrowserSettings>()->GetDockCollections();
}

void SContentBrowser::ToggleFolderFavorite(const TArray<FString>& FolderPaths)
{
	bool bAddedFavorite = false;
	for (FString FolderPath : FolderPaths)
	{
		if (ContentBrowserUtils::IsFavoriteFolder(FolderPath))
		{
			ContentBrowserUtils::RemoveFavoriteFolder(FolderPath, false);
		}
		else
		{
			ContentBrowserUtils::AddFavoriteFolder(FolderPath, false);
			bAddedFavorite = true;
		}
	}
	GConfig->Flush(false, GEditorPerProjectIni);
	FavoritePathViewPtr->Populate();
	if(bAddedFavorite)
	{	
		FavoritePathViewPtr->SetSelectedPaths(FolderPaths);
		if (GetFavoriteFolderVisibility() == EVisibility::Collapsed)
		{
			UContentBrowserSettings* Settings = GetMutableDefault<UContentBrowserSettings>();
			Settings->SetDisplayFavorites(true);
			Settings->SaveConfig();
		}
	}
}

void SContentBrowser::HandleAssetViewSearchOptionsChanged()
{
	TextFilter->SetIncludeClassName(AssetViewPtr->IsIncludingClassNames());
	TextFilter->SetIncludeAssetPath(AssetViewPtr->IsIncludingAssetPaths());
	TextFilter->SetIncludeCollectionNames(AssetViewPtr->IsIncludingCollectionNames());
}

TSharedRef<SWidget> SContentBrowser::CreateToolBar(const FContentBrowserConfig* Config)
{
	RegisterContentBrowserToolBar();

	FToolMenuContext MenuContext;

	UContentBrowserToolbarMenuContext* CommonContextObject = NewObject<UContentBrowserToolbarMenuContext>();
	CommonContextObject->ContentBrowser = SharedThis(this);

	MenuContext.AddObject(CommonContextObject);

	return UToolMenus::Get()->GenerateWidget("ContentBrowser.ToolBar", MenuContext);
}

TSharedRef<SWidget> SContentBrowser::CreateLockButton(const FContentBrowserConfig* Config)
{
	if(Config == nullptr || Config->bCanShowLockButton)
	{
		return
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("LockToggleTooltip", "Toggle lock. If locked, this browser will ignore Find in Content Browser requests."))
			.ContentPadding(FMargin(1, 0))
			.OnClicked(this, &SContentBrowser::ToggleLockClicked)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserLock")))
			.Visibility(this, &SContentBrowser::GetLockButtonVisibility)
			[
				SNew(SImage)
				.Image(this, &SContentBrowser::GetLockIcon)
				.ColorAndOpacity(FSlateColor::UseStyle())
			];
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SContentBrowser::CreateAssetView(const FContentBrowserConfig* Config)
{
	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.0f)
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(FMargin(0.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				// Search
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(5, 0, 0, 0)
				.FillWidth(TAttribute<float>(this, &SContentBrowser::GetSearchBoxFillWidth))
				[
					SAssignNew(SearchBoxPtr, SAssetSearchBox)
					.HintText(this, &SContentBrowser::GetSearchAssetsHintText)
					.OnTextChanged(this, &SContentBrowser::OnSearchBoxChanged)
					.OnTextCommitted(this, &SContentBrowser::OnSearchBoxCommitted)
					.OnKeyDownHandler(this, &SContentBrowser::OnSearchKeyDown)
					.OnAssetSearchBoxSuggestionFilter(this, &SContentBrowser::OnAssetSearchSuggestionFilter)
					.OnAssetSearchBoxSuggestionChosen(this, &SContentBrowser::OnAssetSearchSuggestionChosen)
					.DelayChangeNotificationsWhileTyping(true)
					.Visibility((Config != nullptr ? Config->bCanShowAssetSearch : true) ? EVisibility::Visible : EVisibility::Collapsed)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSearchAssets")))
				]
					
				// Save Search
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("SaveSearchButtonTooltip", "Save the current search as a dynamic collection."))
					.IsEnabled(this, &SContentBrowser::IsSaveSearchButtonEnabled)
					.OnClicked(this, &SContentBrowser::OnSaveSearchButtonClicked)
					.ContentPadding(FMargin(1, 1))
					.Visibility((Config != nullptr ? Config->bCanShowAssetSearch : true) ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
						.Text(FEditorFontGlyphs::Floppy_O)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(5, 0, 0, 0)
				[
					SNew(SComboButton)
					.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
					.ForegroundColor(FSlateColor::UseStyle())
					.ToolTipText(LOCTEXT("AddFilterToolTip", "Add an asset filter."))
					.OnGetMenuContent(this, &SContentBrowser::MakeAddFilterMenu)
					.ContentPadding(FMargin(1, 0))
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserFiltersCombo")))
					.Visibility((Config != nullptr ? Config->bCanShowFilters : true) ? EVisibility::Visible : EVisibility::Collapsed)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Filter"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(5.0f,0.0f,0.0f,0.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(FilterListPtr, SFilterList)
					.OnFilterChanged(this, &SContentBrowser::OnFilterChanged)
					.OnGetContextMenu(this, &SContentBrowser::GetFilterContextMenu)
					.Visibility((Config != nullptr ? Config->bCanShowFilters : true) ? EVisibility::Visible : EVisibility::Collapsed)
					.FrontendFilters(FrontendFilters)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserFilters")))
				]
			]
		]
		// Assets
		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )
		.Padding( 0, 0 )
		[
			AssetViewPtr.ToSharedRef()
		];
}

TSharedRef<SWidget> SContentBrowser::CreateFavoritesView(const FContentBrowserConfig* Config)
{
	return
		SAssignNew(FavoritesArea, SExpandableArea)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(5.0f, 7.0f))
		.Visibility(this, &SContentBrowser::GetFavoriteFolderVisibility)
		.AllowAnimatedTransition(false)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Favorites", "Favorites"))
			.TextStyle(FAppStyle::Get(), "ButtonText")
			.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		]
		.BodyContent()
		[
			SAssignNew(FavoritePathViewPtr, SFavoritePathView)
			.OnItemSelectionChanged(this, &SContentBrowser::OnItemSelectionChanged, EContentBrowserViewContext::FavoriteView)
			.OnGetItemContextMenu(this, &SContentBrowser::GetItemContextMenu, EContentBrowserViewContext::FavoriteView)
			.FocusSearchBoxWhenOpened(false)
			.ShowTreeTitle(false)
			.ShowSeparator(false)
			.AllowClassesFolder(true)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserFavorites")))
			.ExternalSearch(SourcesSearch)
		];
}

TSharedRef<SWidget> SContentBrowser::CreatePathView(const FContentBrowserConfig* Config)
{	
	return
		SAssignNew(PathArea, SExpandableArea)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(5.0f, 3.0f))
		.AllowAnimatedTransition(false)
		.HeaderContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FApp::GetProjectName()))
				.TextStyle(FAppStyle::Get(), "ButtonText")
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(5.0f, 0.0f)
			[
				SAssignNew(PathSearchArea, SExpandableSearchArea, SourcesSearch->GetWidget())
			]
		]
		.BodyContent()
		[
			SAssignNew(PathViewPtr, SPathView)
			.OnItemSelectionChanged(this, &SContentBrowser::OnItemSelectionChanged, EContentBrowserViewContext::PathView)
			.OnGetItemContextMenu(this, &SContentBrowser::GetItemContextMenu, EContentBrowserViewContext::PathView)
			.FocusSearchBoxWhenOpened(false)
			.ShowTreeTitle(false)
			.ShowSeparator(false)
			.AllowClassesFolder(true)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSources")))
			.ExternalSearch(SourcesSearch)
		];
}

TSharedRef<SWidget> SContentBrowser::CreateDockedCollectionsView(const FContentBrowserConfig* Config)
{
	return
		SAssignNew(CollectionArea, SExpandableArea)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
		.BodyBorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.HeaderPadding(FMargin(5.0f, 3.0f))
		.Visibility(this, &SContentBrowser::GetDockedCollectionsVisibility)
		.AllowAnimatedTransition(false)
		.HeaderContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CollectionsTitle", "Collections"))
				.TextStyle(FAppStyle::Get(), "ButtonText")
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(5.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("AddCollectionButtonTooltip", "Add a collection."))
				.OnClicked(this, &SContentBrowser::OnAddCollectionClicked)
				.ContentPadding(FMargin(1, 0))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(5.0f, 0.0f)
			[
				SAssignNew(CollectionSearchArea, SExpandableSearchArea, CollectionSearch->GetWidget())
			]
		]
		.BodyContent()
		[
			CollectionViewPtr.ToSharedRef()
		];
}

TSharedRef<SWidget> SContentBrowser::CreateDrawerDockButton(const FContentBrowserConfig* Config)
{
	if(bIsDrawer)
	{
		return 
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("DockInLayout_Tooltip", "Docks this content browser in the current layout, copying all settings from the drawer.\nThe drawer will still be usable as a temporary browser."))
			.ContentPadding(FMargin(1, 0))
			.OnClicked(this, &SContentBrowser::DockInLayoutClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("EditorViewport.SubMenu.Layouts"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DockInLayout", "Dock in Layout"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	return SNullWidget::NullWidget;
}

void SContentBrowser::ExtendViewOptionsMenu(const FContentBrowserConfig* Config)
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetViewOptions");

	if (Config == nullptr || Config->bCanShowLockButton)
	{
		Menu->AddDynamicSection("ContentBrowserViewOptionsSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				if (UContentBrowserAssetViewContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetViewContextMenuContext>())
				{
					if (TSharedPtr<SContentBrowser> ContentBrowser = Context->OwningContentBrowser.Pin())
					{
						{
							FToolMenuSection& Section = InMenu->AddSection("Locking", LOCTEXT("LockingMenuHeader", "Locking"), FToolMenuInsert("AssetViewType", EToolMenuInsertType::After));
							Section.AddMenuEntry(
								"ToggleLock",
								TAttribute<FText>(ContentBrowser.ToSharedRef(), &SContentBrowser::GetLockMenuText),
								LOCTEXT("LockToggleTooltip", "Toggle lock. If locked, this browser will ignore Find in Content Browser requests."),
								TAttribute<FSlateIcon>(),
								FUIAction(FExecuteAction::CreateLambda([ContentBrowser = Context->OwningContentBrowser]() {ContentBrowser.Pin()->ToggleLockClicked(); })));
						}

						{
							FToolMenuSection& Section = InMenu->FindOrAddSection("View");
							Section.AddMenuEntry(
								"ToggleSources",
								LOCTEXT("ToggleSourcesView", "Show Sources Panel"),
								LOCTEXT("ToggleSourcesView_Tooltip", "Show or hide the sources panel"),
								TAttribute<FSlateIcon>(),
								FUIAction(
									FExecuteAction::CreateLambda([ContentBrowser = Context->OwningContentBrowser]() {ContentBrowser.Pin()->SourcesViewExpandClicked(); }),
									FCanExecuteAction(),
									FIsActionChecked::CreateLambda([ContentBrowser = Context->OwningContentBrowser]() {return ContentBrowser.Pin()->bSourcesViewExpanded; })),
								EUserInterfaceActionType::Check);
						}
					}
				}
			}
		));
	}
}

void SContentBrowser::RegisterContentBrowserToolBar()
{
	static const FName ToolBarName("ContentBrowser.ToolBar");
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(ToolBarName))
	{
		return;
	}

	UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(ToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
	ToolBar->StyleName = "ContentBrowser.ToolBar";

	{
		FToolMenuSection& Section = ToolBar->AddSection("New");
		
		Section.AddDynamicEntry("New", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UContentBrowserToolbarMenuContext* Context = InSection.FindContext<UContentBrowserToolbarMenuContext>();
			TSharedRef<SContentBrowser> ContentBrowser = Context->ContentBrowser.Pin().ToSharedRef();
		
			TSharedRef<SEditorHeaderButton> NewButton = SNew(SEditorHeaderButton)
				.OnGetMenuContent_Lambda([Context] { return Context->ContentBrowser.Pin()->MakeAddNewContextMenu(EContentBrowserDataMenuContext_AddNewMenuDomain::Toolbar, Context); })
				.ToolTipText(ContentBrowser, &SContentBrowser::GetAddNewToolTipText)
				.IsEnabled(ContentBrowser, &SContentBrowser::IsAddNewEnabled)
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserNewAsset")))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.Text(LOCTEXT("AddAssetButton", "Add"));
		
			InSection.AddEntry(
				FToolMenuEntry::InitWidget(
					"NewButton",
					NewButton,
					FText::GetEmpty(),
					true,
					false
				));
		}));
	}

	{
		FToolMenuSection& Section = ToolBar->AddSection("Save");
		Section.AddDynamicEntry("Save", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UContentBrowserToolbarMenuContext* Context = InSection.FindContext<UContentBrowserToolbarMenuContext>();
			TSharedRef<SContentBrowser> ContentBrowser = Context->ContentBrowser.Pin().ToSharedRef();


			TSharedRef<SButton> SaveButton =
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("SaveDirtyPackagesTooltip", "Save all modified assets."))
				.ContentPadding(2)
				.OnClicked(ContentBrowser, &SContentBrowser::OnSaveClicked)
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserSaveDirtyPackages")))
				[
					SNew(SHorizontalBox)
					// Save All Icon
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Save"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]

					// Save All Text
					+ SHorizontalBox::Slot()
					.Padding(FMargin(3, 0, 0, 0))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "NormalText")
						.Text(LOCTEXT("SaveAll", "Save All"))
					]
				];

			InSection.AddEntry(
				FToolMenuEntry::InitWidget(
					"SaveButton",
					SaveButton,
					FText::GetEmpty(),
					true,
					false
				));
		}));
	}
}

SSplitter::ESizeRule SContentBrowser::GetFavoritesAreaSizeRule() const
{
	return FavoritesArea->IsExpanded() ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent;
}

SSplitter::ESizeRule SContentBrowser::GetPathAreaSizeRule() const
{
	return PathArea->IsExpanded() ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent;
}

SSplitter::ESizeRule SContentBrowser::GetCollectionsAreaSizeRule() const
{
	return CollectionArea->IsExpanded() ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent;
}

float SContentBrowser::GetSearchBoxFillWidth() const
{
	// Gives more room to the search box when the  content browser is constrained to a small space
	float LocalWidth = GetTickSpaceGeometry().GetLocalSize().X;
	if (LocalWidth < 1000.f)
	{
		return 1.0f;
	}
	else
	{
		return .35f;
	}
}

FText SContentBrowser::GetHighlightedText() const
{
	return TextFilter->GetRawFilterText();
}

void SContentBrowser::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	AssetViewPtr->CreateNewAsset(DefaultAssetName, PackagePath, AssetClass, Factory);
}

void SContentBrowser::PrepareToSyncItems(TArrayView<const FContentBrowserItem> ItemsToSync, const bool bDisableFiltersThatHideAssets)
{
	bool bRepopulate = false;

	// Check to see if any of the assets require certain folders to be visible
	bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
	bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	bool bDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
	bool bDisplayLocalized = GetDefault<UContentBrowserSettings>()->GetDisplayL10NFolder();
	if ( !bDisplayDev || !bDisplayEngine || !bDisplayPlugins || !bDisplayLocalized )
	{
		for (const FContentBrowserItem& ItemToSync : ItemsToSync)
		{
			if (!bDisplayDev && ContentBrowserUtils::IsItemDeveloperContent(ItemToSync))
			{
				bDisplayDev = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder(true, true);
				bRepopulate = true;
			}

			if (!bDisplayEngine && ContentBrowserUtils::IsItemEngineContent(ItemToSync))
			{
				bDisplayEngine = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder(true, true);
				bRepopulate = true;
			}

			if (!bDisplayPlugins && ContentBrowserUtils::IsItemPluginContent(ItemToSync))
			{
				bDisplayPlugins = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders(true, true);
				bRepopulate = true;
			}

			if (!bDisplayLocalized && ContentBrowserUtils::IsItemLocalizedContent(ItemToSync))
			{
				bDisplayLocalized = true;
				GetMutableDefault<UContentBrowserSettings>()->SetDisplayL10NFolder(true);
				bRepopulate = true;
			}

			if (bDisplayDev && bDisplayEngine && bDisplayPlugins && bDisplayLocalized)
			{
				break;
			}
		}
	}

	// Check to see if any item paths don't exist (this can happen if we haven't ticked since the path was created)
	if (!bRepopulate)
	{
		for (const FContentBrowserItem& ItemToSync : ItemsToSync)
		{
			const FName VirtualPath = *FPaths::GetPath(ItemToSync.GetVirtualPath().ToString());
			TSharedPtr<FTreeItem> Item = PathViewPtr->FindItemRecursive(VirtualPath);
			if (!Item.IsValid())
 			{
				bRepopulate = true;
 				break;
 			}
		}
	}

	// If we have auto-enabled any flags or found a non-existant path, force a refresh
	if (bRepopulate)
	{
		PathViewPtr->Populate();
		FavoritePathViewPtr->Populate();
	}

	if ( bDisableFiltersThatHideAssets )
	{
		// Disable the filter categories
		FilterListPtr->DisableFiltersThatHideItems(ItemsToSync);
	}

	// Disable the filter search (reset the filter, then clear the search text)
	// Note: we have to remove the filter immediately, we can't wait for OnSearchBoxChanged to hit
	SetSearchBoxText(FText::GetEmpty());
	SearchBoxPtr->SetText(FText::GetEmpty());
	SearchBoxPtr->SetError(FText::GetEmpty());
}

void SContentBrowser::PrepareToSyncVirtualPaths(TArrayView<const FName> VirtualPathsToSync, const bool bDisableFiltersThatHideAssets)
{
	// We need to try and resolve these paths back to items in order to query their attributes
	// This will only work for items that have already been discovered
	TArray<FContentBrowserItem> ItemsToSync;
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		for (const FName& VirtualPathToSync : VirtualPathsToSync)
		{
			FContentBrowserItem ItemToSync = ContentBrowserData->GetItemAtPath(VirtualPathToSync, EContentBrowserItemTypeFilter::IncludeAll);
			if (ItemToSync.IsValid())
			{
				ItemsToSync.Add(MoveTemp(ItemToSync));
			}
		}
	}

	PrepareToSyncItems(ItemsToSync, bDisableFiltersThatHideAssets);
}

void SContentBrowser::PrepareToSyncLegacy(TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderPaths, const bool bDisableFiltersThatHideAssets)
{
	TArray<FName> VirtualPathsToSync;
	ContentBrowserUtils::ConvertLegacySelectionToVirtualPaths(AssetDataList, FolderPaths, /*UseFolderPaths*/false, VirtualPathsToSync);

	PrepareToSyncVirtualPaths(VirtualPathsToSync, bDisableFiltersThatHideAssets);
}

void SContentBrowser::SyncToAssets(TArrayView<const FAssetData> AssetDataList, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets)
{
	SyncToLegacy(AssetDataList, TArrayView<const FString>(), bAllowImplicitSync, bDisableFiltersThatHideAssets);
}

void SContentBrowser::SyncToFolders(TArrayView<const FString> FolderList, const bool bAllowImplicitSync)
{
	SyncToLegacy(TArrayView<const FAssetData>(), FolderList, bAllowImplicitSync, /*bDisableFiltersThatHideAssets*/false);
}

void SContentBrowser::SyncToItems(TArrayView<const FContentBrowserItem> ItemsToSync, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets)
{
	PrepareToSyncItems(ItemsToSync, bDisableFiltersThatHideAssets);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncToItems(ItemsToSync, bAllowImplicitSync);
	FavoritePathViewPtr->SyncToItems(ItemsToSync, bAllowImplicitSync);
	AssetViewPtr->SyncToItems(ItemsToSync);
}

void SContentBrowser::SyncToVirtualPaths(TArrayView<const FName> VirtualPathsToSync, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets)
{
	PrepareToSyncVirtualPaths(VirtualPathsToSync, bDisableFiltersThatHideAssets);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncToVirtualPaths(VirtualPathsToSync, bAllowImplicitSync);
	FavoritePathViewPtr->SyncToVirtualPaths(VirtualPathsToSync, bAllowImplicitSync);
	AssetViewPtr->SyncToVirtualPaths(VirtualPathsToSync);
}

void SContentBrowser::SyncToLegacy(TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets)
{
	PrepareToSyncLegacy(AssetDataList, FolderList, bDisableFiltersThatHideAssets);

	// Tell the sources view first so the asset view will be up to date by the time we request the sync
	PathViewPtr->SyncToLegacy(AssetDataList, FolderList, bAllowImplicitSync);
	FavoritePathViewPtr->SyncToLegacy(AssetDataList, FolderList, bAllowImplicitSync);
	AssetViewPtr->SyncToLegacy(AssetDataList, FolderList);
}

void SContentBrowser::SyncTo( const FContentBrowserSelection& ItemSelection, const bool bAllowImplicitSync, const bool bDisableFiltersThatHideAssets )
{
	if (ItemSelection.IsLegacy())
	{
		PrepareToSyncLegacy(ItemSelection.SelectedAssets, ItemSelection.SelectedFolders, bDisableFiltersThatHideAssets);

		// Tell the sources view first so the asset view will be up to date by the time we request the sync
		PathViewPtr->SyncToLegacy(ItemSelection.SelectedAssets, ItemSelection.SelectedFolders, bAllowImplicitSync);
		FavoritePathViewPtr->SyncToLegacy(ItemSelection.SelectedAssets, ItemSelection.SelectedFolders, bAllowImplicitSync);
		AssetViewPtr->SyncToLegacy(ItemSelection.SelectedAssets, ItemSelection.SelectedFolders);
	}
	else
	{
		PrepareToSyncItems(ItemSelection.SelectedItems, bDisableFiltersThatHideAssets);

		// Tell the sources view first so the asset view will be up to date by the time we request the sync
		PathViewPtr->SyncToItems(ItemSelection.SelectedItems, bAllowImplicitSync);
		FavoritePathViewPtr->SyncToItems(ItemSelection.SelectedItems, bAllowImplicitSync);
		AssetViewPtr->SyncToItems(ItemSelection.SelectedItems);
	}
}

void SContentBrowser::SetIsPrimaryContentBrowser(bool NewIsPrimary)
{
	if (!CanSetAsPrimaryContentBrowser()) 
	{
		return;
	}

	bIsPrimaryBrowser = NewIsPrimary;

	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}
	else
	{
		USelection* EditorSelection = GEditor->GetSelectedObjects();
		if ( ensure( EditorSelection != NULL ) )
		{
			EditorSelection->DeselectAll();
		}
	}
}

bool SContentBrowser::CanSetAsPrimaryContentBrowser() const
{
	return bCanSetAsPrimaryBrowser;
}

TSharedPtr<FTabManager> SContentBrowser::GetTabManager() const
{
	if ( ContainingTab.IsValid() )
	{
		return ContainingTab.Pin()->GetTabManager();
	}

	return NULL;
}

void SContentBrowser::LoadSelectedObjectsIfNeeded()
{
	// Get the selected assets in the asset view
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	// Load every asset that isn't already in memory
	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		const FAssetData& AssetData = *AssetIt;
		const bool bShowProgressDialog = (!AssetData.IsAssetLoaded() && FEditorFileUtils::IsMapPackageAsset(AssetData.ObjectPath.ToString()));
		GWarn->BeginSlowTask(LOCTEXT("LoadingObjects", "Loading Objects..."), bShowProgressDialog);

		(*AssetIt).GetAsset();

		GWarn->EndSlowTask();
	}

	// Sync the global selection set if we are the primary browser
	if ( bIsPrimaryBrowser )
	{
		SyncGlobalSelectionSet();
	}
}

void SContentBrowser::GetSelectedAssets(TArray<FAssetData>& SelectedAssets)
{
	SelectedAssets = AssetViewPtr->GetSelectedAssets();
}

void SContentBrowser::GetSelectedFolders(TArray<FString>& SelectedFolders)
{
	SelectedFolders = AssetViewPtr->GetSelectedFolders();
}

TArray<FString> SContentBrowser::GetSelectedPathViewFolders()
{
	check(PathViewPtr.IsValid());
	return PathViewPtr->GetSelectedPaths();
}

void SContentBrowser::SaveSettings() const
{
	const FString& SettingsString = InstanceName.ToString();

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), bSourcesViewExpanded, GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".Locked")), bIsLocked, GEditorPerProjectIni);

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".FavoritesAreaExpanded")), FavoritesArea->IsExpanded(), GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".PathAreaExpanded")), PathArea->IsExpanded(), GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".CollectionAreaExpanded")), CollectionArea->IsExpanded(), GEditorPerProjectIni);

	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".PathSearchAreaExpanded")), PathSearchArea->IsExpanded(), GEditorPerProjectIni);
	GConfig->SetBool(*SettingsIniSection, *(SettingsString + TEXT(".CollectionSearchAreaExpanded")), CollectionSearchArea->IsExpanded(), GEditorPerProjectIni);

	for(int32 SlotIndex = 0; SlotIndex < PathAssetSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->SetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".VerticalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
	}

	for (int32 SlotIndex = 0; SlotIndex < PathFavoriteSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathFavoriteSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->SetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".FavoriteSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
	}


	// Save all our data using the settings string as a key in the user settings ini
	FilterListPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	PathViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	FavoritePathViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString + TEXT(".Favorites"));
	CollectionViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	AssetViewPtr->SaveSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
}

const FName SContentBrowser::GetInstanceName() const
{
	return InstanceName;
}

bool SContentBrowser::IsLocked() const
{
	return bIsLocked;
}

void SContentBrowser::SetKeyboardFocusOnSearch() const
{
	// Focus on the search box
	FSlateApplication::Get().SetKeyboardFocus( SearchBoxPtr, EFocusCause::SetDirectly );
}

void SContentBrowser::CopySettingsFromBrowser(TSharedPtr<SContentBrowser> OtherBrowser)
{
	FName InstanceNameToCopyFrom = OtherBrowser->GetInstanceName();

	// Clear out any existing settings that dont get reset on load
	FilterListPtr->RemoveAllFilters();

	LoadSettings(InstanceNameToCopyFrom);
}

FReply SContentBrowser::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if( Commands->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SContentBrowser::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Clicking in a content browser will shift it to be the primary browser
	FContentBrowserSingleton::Get().SetPrimaryContentBrowser(SharedThis(this));

	return FReply::Unhandled();
}

FReply SContentBrowser::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		HistoryManager.GoBack();
		return FReply::Handled();
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		HistoryManager.GoForward();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SContentBrowser::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		HistoryManager.GoBack();
		return FReply::Handled();
	}
	else if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		HistoryManager.GoForward();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SContentBrowser::OnContainingTabSavingVisualState() const
{
	SaveSettings();
}

void SContentBrowser::OnContainingTabClosed(TSharedRef<SDockTab> DockTab)
{
	FContentBrowserSingleton::Get().ContentBrowserClosed( SharedThis(this) );
}

void SContentBrowser::OnContainingTabActivated(TSharedRef<SDockTab> DockTab, ETabActivationCause InActivationCause)
{
	if(InActivationCause == ETabActivationCause::UserClickedOnTab)
	{
		FContentBrowserSingleton::Get().SetPrimaryContentBrowser(SharedThis(this));
	}
}

void SContentBrowser::LoadSettings(const FName& InInstanceName)
{
	FString SettingsString = InInstanceName.ToString();

	// Now that we have determined the appropriate settings string, actually load the settings
	bSourcesViewExpanded = true;
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".SourcesExpanded")), bSourcesViewExpanded, GEditorPerProjectIni);
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".Locked")), bIsLocked, GEditorPerProjectIni);

	bool bFavoritesAreaExpanded = false;
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".FavoritesAreaExpanded")), bFavoritesAreaExpanded, GEditorPerProjectIni);
	FavoritesArea->SetExpanded(bFavoritesAreaExpanded);

	bool bPathAreaExpanded = true;
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".PathAreaExpanded")), bPathAreaExpanded, GEditorPerProjectIni);
	PathArea->SetExpanded(bPathAreaExpanded);

	bool bCollectionAreaExpanded = false;
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".CollectionAreaExpanded")), bCollectionAreaExpanded, GEditorPerProjectIni);
	CollectionArea->SetExpanded(bCollectionAreaExpanded);

	bool bPathSearchAreaExpanded = false;
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".PathSearchAreaExpanded")), bPathSearchAreaExpanded, GEditorPerProjectIni);
	PathSearchArea->SetExpanded(bPathSearchAreaExpanded);

	bool bCollectionSearchAreaExpanded = false;
	GConfig->GetBool(*SettingsIniSection, *(SettingsString + TEXT(".CollectionSearchAreaExpanded")), bCollectionSearchAreaExpanded, GEditorPerProjectIni);
	CollectionSearchArea->SetExpanded(bCollectionSearchAreaExpanded);

	for(int32 SlotIndex = 0; SlotIndex < PathAssetSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->GetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".VerticalSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
		PathAssetSplitterPtr->SlotAt(SlotIndex).SizeValue = SplitterSize;
	}

	for (int32 SlotIndex = 0; SlotIndex < PathFavoriteSplitterPtr->GetChildren()->Num(); SlotIndex++)
	{
		float SplitterSize = PathFavoriteSplitterPtr->SlotAt(SlotIndex).SizeValue.Get();
		GConfig->GetFloat(*SettingsIniSection, *(SettingsString + FString::Printf(TEXT(".FavoriteSplitter.SlotSize%d"), SlotIndex)), SplitterSize, GEditorPerProjectIni);
		PathFavoriteSplitterPtr->SlotAt(SlotIndex).SizeValue = SplitterSize;
	}

	// Save all our data using the settings string as a key in the user settings ini
	FilterListPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	PathViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	FavoritePathViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString + TEXT(".Favorites"));
	CollectionViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
	AssetViewPtr->LoadSettings(GEditorPerProjectIni, SettingsIniSection, SettingsString);
}

void SContentBrowser::SourcesChanged(const TArray<FString>& SelectedPaths, const TArray<FCollectionNameType>& SelectedCollections)
{
	FString NewSource = SelectedPaths.Num() > 0 ? SelectedPaths[0] : (SelectedCollections.Num() > 0 ? SelectedCollections[0].Name.ToString() : TEXT("None"));
	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("The content browser source was changed by the sources view to '%s'"), *NewSource);

	FSourcesData SourcesData;
	{
		TArray<FName> SelectedPathNames;
		SelectedPathNames.Reserve(SelectedPaths.Num());
		for (const FString& SelectedPath : SelectedPaths)
		{
			SelectedPathNames.Add(FName(*SelectedPath));
		}
		SourcesData = FSourcesData(MoveTemp(SelectedPathNames), SelectedCollections);
	}

	// A dynamic collection should apply its search query to the CB search, so we need to stash the current search so that we can restore it again later
	if (SourcesData.IsDynamicCollection())
	{
		// Only stash the user search term once in case we're switching between dynamic collections
		if (!StashedSearchBoxText.IsSet())
		{
			StashedSearchBoxText = TextFilter->GetRawFilterText();
		}

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		const FCollectionNameType& DynamicCollection = SourcesData.Collections[0];

		FString DynamicQueryString;
		CollectionManagerModule.Get().GetDynamicQueryText(DynamicCollection.Name, DynamicCollection.Type, DynamicQueryString);

		const FText DynamicQueryText = FText::FromString(DynamicQueryString);
		SetSearchBoxText(DynamicQueryText);
		SearchBoxPtr->SetText(DynamicQueryText);
	}
	else if (StashedSearchBoxText.IsSet())
	{
		// Restore the stashed search term
		const FText StashedText = StashedSearchBoxText.GetValue();
		StashedSearchBoxText.Reset();

		SetSearchBoxText(StashedText);
		SearchBoxPtr->SetText(StashedText);
	}

	if (!AssetViewPtr->GetSourcesData().IsEmpty())
	{
		// Update the current history data to preserve selection if there is a valid SourcesData
		HistoryManager.UpdateHistoryData();
	}

	// Change the filter for the asset view
	AssetViewPtr->SetSourcesData(SourcesData);

	// Add a new history data now that the source has changed
	HistoryManager.AddHistoryData();

	// Update the breadcrumb trail path
	UpdatePath();
}

void SContentBrowser::FolderEntered(const FString& FolderPath)
{
	// Have we entered a sub-collection folder?
	FName CollectionName;
	ECollectionShareType::Type CollectionFolderShareType = ECollectionShareType::CST_All;
	if (ContentBrowserUtils::IsCollectionPath(FolderPath, &CollectionName, &CollectionFolderShareType))
	{
		const FCollectionNameType SelectedCollection(CollectionName, CollectionFolderShareType);

		TArray<FCollectionNameType> Collections;
		Collections.Add(SelectedCollection);
		CollectionViewPtr->SetSelectedCollections(Collections);

		CollectionSelected(SelectedCollection);
	}
	else
	{
		// set the path view to the incoming path
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(FolderPath);
		PathViewPtr->SetSelectedPaths(SelectedPaths);

		PathSelected(SelectedPaths[0]);
	}
}

void SContentBrowser::PathSelected(const FString& FolderPath)
{
	// You may not select both collections and paths
	CollectionViewPtr->ClearSelection();

	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();
	// Selecting a folder shows it in the favorite list also
	FavoritePathViewPtr->SetSelectedPaths(SelectedPaths);
	TArray<FCollectionNameType> SelectedCollections;
	SourcesChanged(SelectedPaths, SelectedCollections);

	// Notify 'asset path changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnAssetPathChanged& PathChangedDelegate = ContentBrowserModule.GetOnAssetPathChanged();
	if(PathChangedDelegate.IsBound())
	{
		PathChangedDelegate.Broadcast(FolderPath);
	}

	// Update the context menu's selected paths list
	PathContextMenu->SetSelectedFolders(PathViewPtr->GetSelectedFolderItems());
}

void SContentBrowser::FavoritePathSelected(const FString& FolderPath)
{
	// You may not select both collections and paths
	CollectionViewPtr->ClearSelection();

	TArray<FString> SelectedPaths = FavoritePathViewPtr->GetSelectedPaths();
	// Selecting a favorite shows it in the main list also
	PathViewPtr->SetSelectedPaths(SelectedPaths);
	TArray<FCollectionNameType> SelectedCollections;
	SourcesChanged(SelectedPaths, SelectedCollections);

	// Notify 'asset path changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	FContentBrowserModule::FOnAssetPathChanged& PathChangedDelegate = ContentBrowserModule.GetOnAssetPathChanged();
	if (PathChangedDelegate.IsBound())
	{
		PathChangedDelegate.Broadcast(FolderPath);
	}

	// Update the context menu's selected paths list
	PathContextMenu->SetSelectedFolders(FavoritePathViewPtr->GetSelectedFolderItems());
}

TSharedRef<FExtender> SContentBrowser::GetPathContextMenuExtender(const TArray<FString>& InSelectedPaths) const
{
	return PathContextMenu->MakePathViewContextMenuExtender(InSelectedPaths);
}

void SContentBrowser::CollectionSelected(const FCollectionNameType& SelectedCollection)
{
	// You may not select both collections and paths
	PathViewPtr->ClearSelection();
	FavoritePathViewPtr->ClearSelection();

	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();
	TArray<FString> SelectedPaths;

	if (SelectedCollections.Num() == 0)
	{
		// Select a dummy "None" collection to avoid the sources view switching to the paths view
		SelectedCollections.Add(FCollectionNameType(NAME_None, ECollectionShareType::CST_System));
	}
	
	SourcesChanged(SelectedPaths, SelectedCollections);
}

void SContentBrowser::PathPickerPathSelected(const FString& FolderPath)
{
	PathPickerButton->SetIsOpen(false);

	if ( !FolderPath.IsEmpty() )
	{
		TArray<FString> Paths;
		Paths.Add(FolderPath);
		PathViewPtr->SetSelectedPaths(Paths);
		FavoritePathViewPtr->SetSelectedPaths(Paths);
	}

	PathSelected(FolderPath);
}

void SContentBrowser::SetSelectedPaths(const TArray<FString>& FolderPaths, bool bNeedsRefresh/* = false */)
{
	if (FolderPaths.Num() > 0)
	{
		if (bNeedsRefresh)
		{
			PathViewPtr->Populate();
			FavoritePathViewPtr->Populate();
		}

		PathViewPtr->SetSelectedPaths(FolderPaths);
		FavoritePathViewPtr->SetSelectedPaths(FolderPaths);
		PathSelected(FolderPaths[0]);
	}
}

void SContentBrowser::ForceShowPluginContent(bool bEnginePlugin)
{
	if (AssetViewPtr.IsValid())
	{
		AssetViewPtr->ForceShowPluginFolder(bEnginePlugin);
	}
}

void SContentBrowser::PathPickerCollectionSelected(const FCollectionNameType& SelectedCollection)
{
	PathPickerButton->SetIsOpen(false);

	TArray<FCollectionNameType> Collections;
	Collections.Add(SelectedCollection);
	CollectionViewPtr->SetSelectedCollections(Collections);

	CollectionSelected(SelectedCollection);
}

void SContentBrowser::OnApplyHistoryData( const FHistoryData& History )
{
	PathViewPtr->ApplyHistoryData(History);
	FavoritePathViewPtr->ApplyHistoryData(History);
	CollectionViewPtr->ApplyHistoryData(History);
	AssetViewPtr->ApplyHistoryData(History);

	// Update the breadcrumb trail path
	UpdatePath();

	if (History.SourcesData.HasVirtualPaths())
	{
		// Notify 'asset path changed' delegate
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		FContentBrowserModule::FOnAssetPathChanged& PathChangedDelegate = ContentBrowserModule.GetOnAssetPathChanged();
		if (PathChangedDelegate.IsBound())
		{
			PathChangedDelegate.Broadcast(History.SourcesData.VirtualPaths[0].ToString());
		}
	}
}

void SContentBrowser::OnUpdateHistoryData(FHistoryData& HistoryData) const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	const TArray<FContentBrowserItem> SelectedItems = AssetViewPtr->GetSelectedItems();

	const FText NewSource = SourcesData.HasVirtualPaths() ? FText::FromName(SourcesData.VirtualPaths[0]) : (SourcesData.HasCollections() ? FText::FromName(SourcesData.Collections[0].Name) : LOCTEXT("AllAssets", "All Assets"));

	HistoryData.HistoryDesc = NewSource;
	HistoryData.SourcesData = SourcesData;

	HistoryData.SelectionData.Reset();
	for (const FContentBrowserItem& SelectedItem : SelectedItems)
	{
		HistoryData.SelectionData.SelectedVirtualPaths.Add(SelectedItem.GetVirtualPath());
	}
}

void SContentBrowser::NewFolderRequested(const FString& SelectedPath)
{
	if( ensure(SelectedPath.Len() > 0) && AssetViewPtr.IsValid() )
	{
		CreateNewFolder(SelectedPath, FOnCreateNewFolder::CreateSP(AssetViewPtr.Get(), &SAssetView::NewFolderItemRequested));
	}
}

void SContentBrowser::NewFileItemRequested(const FContentBrowserItemDataTemporaryContext& NewItemContext)
{
	if (AssetViewPtr)
	{
		AssetViewPtr->NewFileItemRequested(NewItemContext);
	}
}

void SContentBrowser::SetSearchBoxText(const FText& InSearchText)
{
	// Has anything changed? (need to test case as the operators are case-sensitive)
	if (!InSearchText.ToString().Equals(TextFilter->GetRawFilterText().ToString(), ESearchCase::CaseSensitive))
	{
		TextFilter->SetRawFilterText( InSearchText );
		SearchBoxPtr->SetError( TextFilter->GetFilterErrorText() );
		if(InSearchText.IsEmpty())
		{
			FrontendFilters->Remove(TextFilter);
			AssetViewPtr->SetUserSearching(false);
		}
		else
		{
			FrontendFilters->Add(TextFilter);
			AssetViewPtr->SetUserSearching(true);
		}
	}
}

void SContentBrowser::OnSearchBoxChanged(const FText& InSearchText)
{
	SetSearchBoxText(InSearchText);

	// Broadcast 'search box changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	ContentBrowserModule.GetOnSearchBoxChanged().Broadcast(InSearchText, bIsPrimaryBrowser);	
}

void SContentBrowser::OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo)
{
	SetSearchBoxText(InSearchText);
}

FReply SContentBrowser::OnSearchKeyDown(const FGeometry& Geometry, const FKeyEvent& InKeyEvent)
{
	FInputChord CheckChord(InKeyEvent.GetKey(), EModifierKey::FromBools(InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown()));

	// Clear focus if the content browser drawer key is clicked so it will close the opened content browser
	if (FGlobalEditorCommonCommands::Get().OpenContentBrowserDrawer->HasActiveChord(CheckChord))
	{
		return FReply::Handled().ClearUserFocus(EFocusCause::SetDirectly);
	}

	return FReply::Unhandled();
}

bool SContentBrowser::IsSaveSearchButtonEnabled() const
{
	return !TextFilter->GetRawFilterText().IsEmptyOrWhitespace();
}

FReply SContentBrowser::OnSaveSearchButtonClicked()
{
	// Need to make sure we can see the collections view
	if (!bSourcesViewExpanded)
	{
		SourcesViewExpandClicked();
	}
	if (!GetDefault<UContentBrowserSettings>()->GetDockCollections() && ActiveSourcesWidgetIndex != ContentBrowserSourcesWidgetSwitcherIndex::CollectionsView)
	{
		ActiveSourcesWidgetIndex = ContentBrowserSourcesWidgetSwitcherIndex::CollectionsView;
		SourcesWidgetSwitcher->SetActiveWidgetIndex(ActiveSourcesWidgetIndex);
	}

	// We want to add any currently selected paths to the final saved query so that you get back roughly the same list of objects as what you're currently seeing
	FString SelectedPathsQuery;
	{
		const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
		for (int32 SelectedPathIndex = 0; SelectedPathIndex < SourcesData.VirtualPaths.Num(); ++SelectedPathIndex)
		{
			SelectedPathsQuery.Append(TEXT("Path:'"));
			SelectedPathsQuery.Append(SourcesData.VirtualPaths[SelectedPathIndex].ToString());
			SelectedPathsQuery.Append(TEXT("'..."));

			if (SelectedPathIndex + 1 < SourcesData.VirtualPaths.Num())
			{
				SelectedPathsQuery.Append(TEXT(" OR "));
			}
		}
	}

	// todo: should we automatically append any type filters too?

	// Produce the final query
	FText FinalQueryText;
	if (SelectedPathsQuery.IsEmpty())
	{
		FinalQueryText = TextFilter->GetRawFilterText();
	}
	else
	{
		FinalQueryText = FText::FromString(FString::Printf(TEXT("(%s) AND (%s)"), *TextFilter->GetRawFilterText().ToString(), *SelectedPathsQuery));
	}

	CollectionViewPtr->MakeSaveDynamicCollectionMenu(FinalQueryText);
	return FReply::Handled();
}

void SContentBrowser::OnPathClicked( const FString& CrumbData )
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	if ( SourcesData.HasCollections() )
	{
		// Collection crumb was clicked. See if we've clicked on a different collection in the hierarchy, and change the path if required.
		TOptional<FCollectionNameType> CollectionClicked;
		{
			FString CollectionName;
			FString CollectionTypeString;
			if (CrumbData.Split(TEXT("?"), &CollectionName, &CollectionTypeString))
			{
				const int32 CollectionType = FCString::Atoi(*CollectionTypeString);
				if (CollectionType >= 0 && CollectionType < ECollectionShareType::CST_All)
				{
					CollectionClicked = FCollectionNameType(FName(*CollectionName), ECollectionShareType::Type(CollectionType));
				}
			}
		}

		if ( CollectionClicked.IsSet() && SourcesData.Collections[0] != CollectionClicked.GetValue() )
		{
			TArray<FCollectionNameType> Collections;
			Collections.Add(CollectionClicked.GetValue());
			CollectionViewPtr->SetSelectedCollections(Collections);

			CollectionSelected(CollectionClicked.GetValue());
		}
	}
	else if ( !SourcesData.HasVirtualPaths() )
	{
		// No collections or paths are selected. This is "All Assets". Don't change the path when this is clicked.
	}
	else if ( SourcesData.VirtualPaths.Num() > 1 || SourcesData.VirtualPaths[0].ToString() != CrumbData )
	{
		// More than one path is selected or the crumb that was clicked is not the same path as the current one. Change the path.
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(CrumbData);
		PathViewPtr->SetSelectedPaths(SelectedPaths);
		FavoritePathViewPtr->SetSelectedPaths(SelectedPaths);
		PathSelected(SelectedPaths[0]);
	}
}

void SContentBrowser::OnPathMenuItemClicked(FString ClickedPath)
{
	OnPathClicked( ClickedPath );
}

bool SContentBrowser::OnHasCrumbDelimiterContent(const FString& CrumbData) const
{
	const FSourcesData SourcesData = AssetViewPtr->GetSourcesData();
	if (SourcesData.HasCollections())
	{
		TOptional<FCollectionNameType> CollectionClicked;
		{
			FString CollectionName;
			FString CollectionTypeString;
			if (CrumbData.Split(TEXT("?"), &CollectionName, &CollectionTypeString))
			{
				const int32 CollectionType = FCString::Atoi(*CollectionTypeString);
				if (CollectionType >= 0 && CollectionType < ECollectionShareType::CST_All)
				{
					CollectionClicked = FCollectionNameType(FName(*CollectionName), ECollectionShareType::Type(CollectionType));
				}
			}
		}

		TArray<FCollectionNameType> ChildCollections;
		if (CollectionClicked.IsSet())
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			CollectionManagerModule.Get().GetChildCollections(CollectionClicked->Name, CollectionClicked->Type, ChildCollections);
		}

		return (ChildCollections.Num() > 0);
	}
	else if (SourcesData.HasVirtualPaths())
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		FContentBrowserDataFilter SubItemsFilter;
		SubItemsFilter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFolders;
		SubItemsFilter.bRecursivePaths = false;

		bool bHasSubItems = false;
		ContentBrowserData->EnumerateItemsUnderPath(*CrumbData, SubItemsFilter, [&bHasSubItems](FContentBrowserItemData&& InSubItem)
		{
			bHasSubItems = true;
			return false;
		});

		return bHasSubItems;
	}

	return false;
}

TSharedRef<SWidget> SContentBrowser::OnGetCrumbDelimiterContent(const FString& CrumbData) const
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
	TSharedPtr<SWidget> MenuWidget;

	if( SourcesData.HasCollections() )
	{
		TOptional<FCollectionNameType> CollectionClicked;
		{
			FString CollectionName;
			FString CollectionTypeString;
			if (CrumbData.Split(TEXT("?"), &CollectionName, &CollectionTypeString))
			{
				const int32 CollectionType = FCString::Atoi(*CollectionTypeString);
				if (CollectionType >= 0 && CollectionType < ECollectionShareType::CST_All)
				{
					CollectionClicked = FCollectionNameType(FName(*CollectionName), ECollectionShareType::Type(CollectionType));
				}
			}
		}

		if( CollectionClicked.IsSet() )
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			TArray<FCollectionNameType> ChildCollections;
			CollectionManagerModule.Get().GetChildCollections(CollectionClicked->Name, CollectionClicked->Type, ChildCollections);

			if( ChildCollections.Num() > 0 )
			{
				FMenuBuilder MenuBuilder( true, nullptr );

				for( const FCollectionNameType& ChildCollection : ChildCollections )
				{
					const FString ChildCollectionCrumbData = FString::Printf(TEXT("%s?%s"), *ChildCollection.Name.ToString(), *FString::FromInt(ChildCollection.Type));

					MenuBuilder.AddMenuEntry(
						FText::FromName(ChildCollection.Name),
						FText::GetEmpty(),
						FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ChildCollection.Type)),
						FUIAction(FExecuteAction::CreateSP(const_cast<SContentBrowser*>(this), &SContentBrowser::OnPathMenuItemClicked, ChildCollectionCrumbData))
						);
				}

				MenuWidget = MenuBuilder.MakeWidget();
			}
		}
	}
	else if( SourcesData.HasVirtualPaths() )
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		FContentBrowserDataFilter SubItemsFilter;
		SubItemsFilter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFolders;
		SubItemsFilter.bRecursivePaths = false;

		TArray<FContentBrowserItem> SubItems = ContentBrowserData->GetItemsUnderPath(*CrumbData, SubItemsFilter);
		SubItems.Sort([](const FContentBrowserItem& ItemOne, const FContentBrowserItem& ItemTwo)
		{
			return ItemOne.GetDisplayName().CompareTo(ItemTwo.GetDisplayName()) < 0;
		});

		if(SubItems.Num() > 0)
		{
			FMenuBuilder MenuBuilder( true, nullptr );

			for (const FContentBrowserItem& SubItem : SubItems)
			{
				MenuBuilder.AddMenuEntry(
					SubItem.GetDisplayName(),
					FText::GetEmpty(),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.BreadcrumbPathPickerFolder"),
					FUIAction(FExecuteAction::CreateSP(const_cast<SContentBrowser*>(this), &SContentBrowser::OnPathMenuItemClicked, SubItem.GetVirtualPath().ToString()))
					);
			}

			MenuWidget = MenuBuilder.MakeWidget();
		}
	}

	if( MenuWidget.IsValid() )
	{
		// Do not allow the menu to become too large if there are many directories
		Widget =
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.MaxHeight( 400.0f )
			[
				MenuWidget.ToSharedRef()
			];
	}

	return Widget.ToSharedRef();
}

TSharedRef<SWidget> SContentBrowser::GetPathPickerContent()
{
	FPathPickerConfig PathPickerConfig;

	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();
	if ( SourcesData.HasVirtualPaths() )
	{
		PathPickerConfig.DefaultPath = SourcesData.VirtualPaths[0].ToString();
	}
	
	// TODO: This needs to be able to pick any content folder, so needs to use the new item-based API
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SContentBrowser::PathPickerPathSelected);
	PathPickerConfig.bAllowContextMenu = false;
	PathPickerConfig.bAllowClassesFolder = true;

	return SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(500)
		.Padding(4)
		[
			SNew(SVerticalBox)

			// Path Picker
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				FContentBrowserSingleton::Get().CreatePathPicker(PathPickerConfig)
			]

			// Collection View
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 6, 0, 0)
			[
				SNew(SCollectionView)
				.AllowCollectionButtons(false)
				.OnCollectionSelected(this, &SContentBrowser::PathPickerCollectionSelected)
				.AllowContextMenu(false)
			]
		];
}

FString SContentBrowser::GetCurrentPath() const
{
	FString CurrentPath;
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	if ( SourcesData.HasVirtualPaths() && SourcesData.VirtualPaths[0] != NAME_None )
	{
		CurrentPath = SourcesData.VirtualPaths[0].ToString();
	}

	return CurrentPath;
}

void SContentBrowser::AppendNewMenuContextObjects(const EContentBrowserDataMenuContext_AddNewMenuDomain InDomain, const TArray<FName>& InSelectedPaths, FToolMenuContext& InOutMenuContext, UContentBrowserToolbarMenuContext* CommonContext)
{
	if (!UToolMenus::Get()->IsMenuRegistered("ContentBrowser.AddNewContextMenu"))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("ContentBrowser.AddNewContextMenu");
		Menu->AddDynamicSection("DynamicSection_Common", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			TSharedPtr<SContentBrowser> ContentBrowser;
			const UContentBrowserMenuContext* MenuContext = InMenu->FindContext<UContentBrowserMenuContext>();
			if (MenuContext)
			{
				ContentBrowser = MenuContext->ContentBrowser.Pin();
			}
			else
			{
				const UContentBrowserToolbarMenuContext* ToolbarContext  = InMenu->FindContext<UContentBrowserToolbarMenuContext>();
				if (ToolbarContext)
				{
					ContentBrowser = ToolbarContext->ContentBrowser.Pin();
				}
			}

			if (ContentBrowser)
			{
				ContentBrowser->PopulateAddNewContextMenu(InMenu);
			}
		}));
	}

	if(!CommonContext)
	{
		UContentBrowserMenuContext* CommonContextObject = NewObject<UContentBrowserMenuContext>();
		CommonContextObject->ContentBrowser = SharedThis(this);
		InOutMenuContext.AddObject(CommonContextObject);
	}
	else
	{
		InOutMenuContext.AddObject(CommonContext);
	}

	{
		UContentBrowserDataMenuContext_AddNewMenu* DataContextObject = NewObject<UContentBrowserDataMenuContext_AddNewMenu>();
		DataContextObject->SelectedPaths = InSelectedPaths;
		DataContextObject->OwnerDomain = InDomain;
		DataContextObject->OnBeginItemCreation = UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation::CreateSP(this, &SContentBrowser::NewFileItemRequested);
		InOutMenuContext.AddObject(DataContextObject);
	}
}

TSharedRef<SWidget> SContentBrowser::MakeAddNewContextMenu(const EContentBrowserDataMenuContext_AddNewMenuDomain InDomain, UContentBrowserToolbarMenuContext* CommonContext)
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	// Get all menu extenders for this context menu from the content browser module
	TSharedPtr<FExtender> MenuExtender;
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedPaths> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetContextMenuExtenders();

		// Delegate wants paths as FStrings
		TArray<FString> SelectedPackagePaths;
		{
			// We need to try and resolve these paths back to items in order to query their attributes
			// This will only work for items that have already been discovered
			UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

			for (const FName& VirtualPathToSync : SourcesData.VirtualPaths)
			{
				const FContentBrowserItem ItemToSync = ContentBrowserData->GetItemAtPath(VirtualPathToSync, EContentBrowserItemTypeFilter::IncludeFolders);
				if (ItemToSync.IsValid())
				{
					FName PackagePath;
					if (ItemToSync.Legacy_TryGetPackagePath(PackagePath))
					{
						SelectedPackagePaths.Add(PackagePath.ToString());
					}
				}
			}
		}

		if (SelectedPackagePaths.Num() > 0)
		{
			TArray<TSharedPtr<FExtender>> Extenders;
			for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
			{
				if (MenuExtenderDelegates[i].IsBound())
				{
					Extenders.Add(MenuExtenderDelegates[i].Execute(SelectedPackagePaths));
				}
			}
			MenuExtender = FExtender::Combine(Extenders);
		}
	}

	FToolMenuContext ToolMenuContext(nullptr, MenuExtender, nullptr);
	AppendNewMenuContextObjects(InDomain, SourcesData.VirtualPaths, ToolMenuContext, CommonContext);

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetCachedDisplayMetrics( DisplayMetrics );

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	return 
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.MaxHeight(DisplaySize.Y * 0.9)
		[
			UToolMenus::Get()->GenerateWidget("ContentBrowser.AddNewContextMenu", ToolMenuContext)
		];
}

void SContentBrowser::PopulateAddNewContextMenu(class UToolMenu* Menu)
{
	const UContentBrowserDataMenuContext_AddNewMenu* ContextObject = Menu->FindContext<UContentBrowserDataMenuContext_AddNewMenu>();
	checkf(ContextObject, TEXT("Required context UContentBrowserDataMenuContext_AddNewMenu was missing!"));

	// Only add "New Folder" item if we do not have a collection selected
	FNewAssetOrClassContextMenu::FOnNewFolderRequested OnNewFolderRequested;
	if (ContextObject->OwnerDomain != EContentBrowserDataMenuContext_AddNewMenuDomain::PathView && CollectionViewPtr->GetSelectedCollections().Num() == 0)
	{
		OnNewFolderRequested = FNewAssetOrClassContextMenu::FOnNewFolderRequested::CreateSP(this, &SContentBrowser::NewFolderRequested);
	}


	// New feature packs don't depend on the current paths, so we always add this item if it was requested
	FNewAssetOrClassContextMenu::FOnGetContentRequested OnGetContentRequested;
	if (ContextObject->OwnerDomain == EContentBrowserDataMenuContext_AddNewMenuDomain::Toolbar)
	{
		OnGetContentRequested = FNewAssetOrClassContextMenu::FOnGetContentRequested::CreateSP(this, &SContentBrowser::OnAddContentRequested);
	}

	FNewAssetOrClassContextMenu::MakeContextMenu(
		Menu,
		ContextObject->SelectedPaths,
		OnNewFolderRequested,
		OnGetContentRequested
		);
}

bool SContentBrowser::IsAddNewEnabled() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	return SourcesData.VirtualPaths.Num() == 1;
}

FText SContentBrowser::GetAddNewToolTipText() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	if ( SourcesData.VirtualPaths.Num() == 1 )
	{
		const FString CurrentPath = SourcesData.VirtualPaths[0].ToString();
		return FText::Format( LOCTEXT("AddNewToolTip_AddNewContent", "Create a new content in {0}..."), FText::FromString(CurrentPath) );
	}
	else if ( SourcesData.VirtualPaths.Num() > 1 )
	{
		return LOCTEXT( "AddNewToolTip_MultiplePaths", "Cannot add content to multiple paths." );
	}
	
	return LOCTEXT( "AddNewToolTip_NoPath", "No path is selected as an add target." );
}

TSharedRef<SWidget> SContentBrowser::MakeAddFilterMenu()
{
	return FilterListPtr->ExternalMakeAddFilterMenu();
}

TSharedPtr<SWidget> SContentBrowser::GetFilterContextMenu()
{
	return FilterListPtr->ExternalMakeAddFilterMenu();
}

void SContentBrowser::RegisterPathViewFiltersMenu()
{
	static const FName PathViewFiltersMenuName = TEXT("ContentBrowser.AssetViewOptions.PathViewFilters");
	if (!UToolMenus::Get()->IsMenuRegistered(PathViewFiltersMenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(PathViewFiltersMenuName);
		Menu->AddDynamicSection("DynamicContent", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			if (const UContentBrowserMenuContext* ContextObject = InMenu->FindContext<UContentBrowserMenuContext>())
			{
				if (TSharedPtr<SContentBrowser> ContentBrowser = ContextObject->ContentBrowser.Pin())
				{
					ContentBrowser->PopulatePathViewFiltersMenu(InMenu);
				}
			}
		}));
	}
}

void SContentBrowser::PopulatePathViewFiltersMenu(UToolMenu* Menu)
{
	if (PathViewPtr.IsValid())
	{
		PathViewPtr->PopulatePathViewFiltersMenu(Menu);
	}
}

void SContentBrowser::ExtendAssetViewButtonMenuContext(FToolMenuContext& InMenuContext)
{
	UContentBrowserMenuContext* ContextObject = NewObject<UContentBrowserMenuContext>();
	ContextObject->ContentBrowser = SharedThis(this);
	InMenuContext.AddObject(ContextObject);
}

FReply SContentBrowser::OnSaveClicked()
{
	ContentBrowserUtils::SaveDirtyPackages();
	return FReply::Handled();
}

void SContentBrowser::OnAddContentRequested()
{
	IAddContentDialogModule& AddContentDialogModule = FModuleManager::LoadModuleChecked<IAddContentDialogModule>("AddContentDialog");
	FWidgetPath WidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetChecked(AsShared(), WidgetPath);
	AddContentDialogModule.ShowDialog(WidgetPath.GetWindow());
}

void SContentBrowser::OnNewItemRequested(const FContentBrowserItem& NewItem)
{
	// Make sure we are showing the location of the new file (we may have created it in a folder)
	TArray<FString> SelectedPaths;
	SelectedPaths.Add(FPaths::GetPath(NewItem.GetVirtualPath().ToString()));
	PathViewPtr->SetSelectedPaths(SelectedPaths);
	PathSelected(SelectedPaths[0]);
}

void SContentBrowser::OnItemSelectionChanged(const FContentBrowserItem& SelectedItem, ESelectInfo::Type SelectInfo, EContentBrowserViewContext ViewContext)
{
	if (ViewContext == EContentBrowserViewContext::AssetView)
	{		
		if (bIsPrimaryBrowser)
		{
			SyncGlobalSelectionSet();
		}

		// Notify 'asset selection changed' delegate
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		FContentBrowserModule::FOnAssetSelectionChanged& AssetSelectionChangedDelegate = ContentBrowserModule.GetOnAssetSelectionChanged();

		const TArray<FContentBrowserItem> SelectedItems = AssetViewPtr->GetSelectedItems();
		AssetContextMenu->SetSelectedItems(SelectedItems);

		{
			TArray<FName> SelectedCollectionItems;
			for (const FContentBrowserItem& SelectedAssetItem : SelectedItems)
			{
				FName CollectionItemId;
				if (SelectedAssetItem.TryGetCollectionId(CollectionItemId))
				{
					SelectedCollectionItems.Add(CollectionItemId);
				}
			}

			CollectionViewPtr->SetSelectedAssetPaths(SelectedCollectionItems);
		}

		if (AssetSelectionChangedDelegate.IsBound())
		{
			TArray<FAssetData> SelectedAssets;
			for (const FContentBrowserItem& SelectedAssetItem : SelectedItems)
			{
				FAssetData ItemAssetData;
				if (SelectedAssetItem.Legacy_TryGetAssetData(ItemAssetData))
				{
					SelectedAssets.Add(MoveTemp(ItemAssetData));
				}
			}

			AssetSelectionChangedDelegate.Broadcast(SelectedAssets, bIsPrimaryBrowser);
		}
	}
	else if (ViewContext == EContentBrowserViewContext::FavoriteView)
	{
		checkf(!SelectedItem.IsValid() || SelectedItem.IsFolder(), TEXT("File item passed to path view selection!"));
		FavoritePathSelected(SelectedItem.IsValid() ? SelectedItem.GetVirtualPath().ToString() : FString());
	}
	else
	{
		checkf(!SelectedItem.IsValid() || SelectedItem.IsFolder(), TEXT("File item passed to path view selection!"));
		PathSelected(SelectedItem.IsValid() ? SelectedItem.GetVirtualPath().ToString() : FString());
	}
}

void SContentBrowser::OnItemsActivated(TArrayView<const FContentBrowserItem> ActivatedItems, EAssetTypeActivationMethod::Type ActivationMethod)
{
	FContentBrowserItem FirstActivatedFolder;

	// Batch these by their data sources
	TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
	for (const FContentBrowserItem& ActivatedItem : ActivatedItems)
	{
		if (ActivatedItem.IsFile())
		{
			FContentBrowserItem::FItemDataArrayView ItemDataArray = ActivatedItem.GetInternalItems();
			for (const FContentBrowserItemData& ItemData : ItemDataArray)
			{
				if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
				{
					TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
					ItemsForSource.Add(ItemData);
				}
			}
		}

		if (ActivatedItem.IsFolder() && !FirstActivatedFolder.IsValid())
		{
			FirstActivatedFolder = ActivatedItem;
		}
	}

	if (SourcesAndItems.Num() == 0 && FirstActivatedFolder.IsValid())
	{
		// Activate the selected folder
		FolderEntered(FirstActivatedFolder.GetVirtualPath().ToString());
		return;
	}

	// Execute the operation now
	for (const auto& SourceAndItemsPair : SourcesAndItems)
	{
		if (ActivationMethod == EAssetTypeActivationMethod::Previewed)
		{
			SourceAndItemsPair.Key->BulkPreviewItems(SourceAndItemsPair.Value);
		}
		else
		{
			for (const FContentBrowserItemData& ItemToEdit : SourceAndItemsPair.Value)
			{
				FText EditErrorMsg;
				if (!SourceAndItemsPair.Key->CanEditItem(ItemToEdit, &EditErrorMsg))
				{
					AssetViewUtils::ShowErrorNotifcation(EditErrorMsg);
				}
			}

			SourceAndItemsPair.Key->BulkEditItems(SourceAndItemsPair.Value);
		}
	}
}

FReply SContentBrowser::ToggleLockClicked()
{
	bIsLocked = !bIsLocked;

	return FReply::Handled();
}

FReply SContentBrowser::DockInLayoutClicked()
{
	FContentBrowserSingleton::Get().DockContentBrowserDrawer();

	return FReply::Handled();
}

FText SContentBrowser::GetLockMenuText() const
{
	return IsLocked() ? LOCTEXT("ContentBrowserLockMenu_Unlock", "Unlock Content Browser") : LOCTEXT("ContentBrowserLockMenu_Lock", "Lock Content Browser");
}

const FSlateBrush* SContentBrowser::GetLockIcon() const
{
	static const FName Unlock = "Icons.Unlock";
	static const FName Lock = "Icons.Lock";

	return FAppStyle::Get().GetBrush(IsLocked() ? Lock : Unlock);
}


EVisibility SContentBrowser::GetSourcesViewVisibility() const
{
	return bSourcesViewExpanded ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SContentBrowser::SourcesViewExpandClicked()
{
	bSourcesViewExpanded = !bSourcesViewExpanded;

	// Notify 'Sources View Expanded' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	FContentBrowserModule::FOnSourcesViewChanged& SourcesViewChangedDelegate = ContentBrowserModule.GetOnSourcesViewChanged();
	if(SourcesViewChangedDelegate.IsBound())
	{
		SourcesViewChangedDelegate.Broadcast(bSourcesViewExpanded);
	}

	return FReply::Handled();
}

EVisibility SContentBrowser::GetSourcesSwitcherVisibility() const
{
	return GetDefault<UContentBrowserSettings>()->GetDockCollections() ? EVisibility::Collapsed : EVisibility::Visible;
}


FReply SContentBrowser::OnSourcesSwitcherClicked()
{
	// This only works because we only have two switcher types
	ActiveSourcesWidgetIndex = !ActiveSourcesWidgetIndex;
	SourcesWidgetSwitcher->SetActiveWidgetIndex(ActiveSourcesWidgetIndex);

	return FReply::Handled();
}



void SContentBrowser::OnContentBrowserSettingsChanged(FName PropertyName)
{
	if (PropertyName.IsNone())
	{
		// Ensure the path is set to the correct view mode
		UpdatePath();
	}
}

FReply SContentBrowser::BackClicked()
{
	HistoryManager.GoBack();

	return FReply::Handled();
}

FReply SContentBrowser::ForwardClicked()
{
	HistoryManager.GoForward();

	return FReply::Handled();
}

FReply SContentBrowser::OnAddCollectionClicked()
{
	CollectionArea->SetExpanded(true);

	CollectionViewPtr->MakeAddCollectionMenu(AsShared());

	return FReply::Handled();
}

bool SContentBrowser::HandleRenameCommandCanExecute() const
{
	// The order of these conditions are carefully crafted to match the logic of the context menu summoning, as this callback 
	// is shared between the path and asset views, and is given zero context as to which one is making the request
	// Change this logic at your peril, lest the the dominoes fall like a house of cards (checkmate)
	if (PathViewPtr->HasFocusedDescendants())
	{
		// Prefer the path view if it has focus, which may be the case when using the keyboard to invoke the action,
		// but will be false when using the context menu (which isn't an issue, as the path view clears the asset view 
		// selection when invoking its context menu to avoid the selection ambiguity present when using the keyboard)
		if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
		{
			return PathContextMenu->CanExecuteRename();
		}
	}
	else if (AssetViewPtr->HasFocusedDescendants())
	{
		// Prefer the asset menu if the asset view has focus (which may be the case when using the keyboard to invoke
		// the action), as it is the only thing that is updated with the correct selection context when no context menu 
		// has been invoked, and can work for both folders and files
		if (AssetViewPtr->GetSelectedItems().Num() > 0)
		{
			return AssetContextMenu->CanExecuteRename();
		}
	}
	else if (AssetViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		// Folder selection takes precedence over file selection for the context menu used...
		return PathContextMenu->CanExecuteRename();
	}
	else if (AssetViewPtr->GetSelectedFileItems().Num() > 0)
	{
		// ... but the asset view still takes precedence over an unfocused path view unless it has no selection
		return AssetContextMenu->CanExecuteRename();
	}
	else if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		return PathContextMenu->CanExecuteRename();
	}

	return false;
}

void SContentBrowser::HandleRenameCommand()
{
	// The order of these conditions are carefully crafted to match the logic of the context menu summoning, as this callback 
	// is shared between the path and asset views, and is given zero context as to which one is making the request
	// Change this logic at your peril, lest the the dominoes fall like a house of cards (checkmate)
	if (PathViewPtr->HasFocusedDescendants())
	{
		// Prefer the path view if it has focus, which may be the case when using the keyboard to invoke the action,
		// but will be false when using the context menu (which isn't an issue, as the path view clears the asset view 
		// selection when invoking its context menu to avoid the selection ambiguity present when using the keyboard)
		if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
		{
			PathContextMenu->ExecuteRename(EContentBrowserViewContext::PathView);
		}
	}
	else if (AssetViewPtr->HasFocusedDescendants())
	{
		// Prefer the asset menu if the asset view has focus (which may be the case when using the keyboard to invoke
		// the action), as it is the only thing that is updated with the correct selection context when no context menu 
		// has been invoked, and can work for both folders and files
		if (AssetViewPtr->GetSelectedItems().Num() > 0)
		{
			AssetContextMenu->ExecuteRename(EContentBrowserViewContext::AssetView);
		}
	}
	else if (AssetViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		// Folder selection takes precedence over file selection for the context menu used...
		PathContextMenu->ExecuteRename(EContentBrowserViewContext::AssetView);
	}
	else if (AssetViewPtr->GetSelectedFileItems().Num() > 0)
	{
		// ... but the asset view still takes precedence over an unfocused path view unless it has no selection
		AssetContextMenu->ExecuteRename(EContentBrowserViewContext::AssetView);
	}
	else if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		PathContextMenu->ExecuteRename(EContentBrowserViewContext::PathView);
	}
}

bool SContentBrowser::HandleSaveAssetCommandCanExecute() const
{
	if (AssetViewPtr->GetSelectedFileItems().Num() > 0 && !AssetViewPtr->IsRenamingAsset())
	{
		return AssetContextMenu->CanExecuteSaveAsset();
	}

	return false;
}

void SContentBrowser::HandleSaveAssetCommand()
{
	if (AssetViewPtr->GetSelectedFileItems().Num() > 0)
	{
		AssetContextMenu->ExecuteSaveAsset();
	}
}

void SContentBrowser::HandleSaveAllCurrentFolderCommand() const
{
	PathContextMenu->ExecuteSaveFolder();
}

void SContentBrowser::HandleResaveAllCurrentFolderCommand() const
{
	PathContextMenu->ExecuteResaveFolder();
}

bool SContentBrowser::HandleDeleteCommandCanExecute() const
{
	// The order of these conditions are carefully crafted to match the logic of the context menu summoning, as this callback 
	// is shared between the path and asset views, and is given zero context as to which one is making the request
	// Change this logic at your peril, lest the the dominoes fall like a house of cards (checkmate)
	if (PathViewPtr->HasFocusedDescendants())
	{
		// Prefer the path view if it has focus, which may be the case when using the keyboard to invoke the action,
		// but will be false when using the context menu (which isn't an issue, as the path view clears the asset view 
		// selection when invoking its context menu to avoid the selection ambiguity present when using the keyboard)
		if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
		{
			return PathContextMenu->CanExecuteDelete();
		}
	}
	else if (AssetViewPtr->HasFocusedDescendants())
	{
		// Prefer the asset menu if the asset view has focus (which may be the case when using the keyboard to invoke
		// the action), as it is the only thing that is updated with the correct selection context when no context menu 
		// has been invoked, and can work for both folders and files
		if (AssetViewPtr->GetSelectedItems().Num() > 0)
		{
			return AssetContextMenu->CanExecuteDelete();
		}
	}
	else if (AssetViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		// Folder selection takes precedence over file selection for the context menu used...
		return PathContextMenu->CanExecuteDelete();
	}
	else if (AssetViewPtr->GetSelectedFileItems().Num() > 0)
	{
		// ... but the asset view still takes precedence over an unfocused path view unless it has no selection
		return AssetContextMenu->CanExecuteDelete();
	}
	else if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		return PathContextMenu->CanExecuteDelete();
	}

	return false;
}

void SContentBrowser::HandleDeleteCommandExecute()
{
	// The order of these conditions are carefully crafted to match the logic of the context menu summoning, as this callback 
	// is shared between the path and asset views, and is given zero context as to which one is making the request
	// Change this logic at your peril, lest the the dominoes fall like a house of cards (checkmate)
	if (PathViewPtr->HasFocusedDescendants())
	{
		// Prefer the path view if it has focus, which may be the case when using the keyboard to invoke the action,
		// but will be false when using the context menu (which isn't an issue, as the path view clears the asset view 
		// selection when invoking its context menu to avoid the selection ambiguity present when using the keyboard)
		if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
		{
			PathContextMenu->ExecuteDelete();
		}
	}
	else if (AssetViewPtr->HasFocusedDescendants())
	{
		// Prefer the asset menu if the asset view has focus (which may be the case when using the keyboard to invoke
		// the action), as it is the only thing that is updated with the correct selection context when no context menu 
		// has been invoked, and can work for both folders and files
		if (AssetViewPtr->GetSelectedItems().Num() > 0)
		{
			AssetContextMenu->ExecuteDelete();
		}
	}
	else if (AssetViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		// Folder selection takes precedence over file selection for the context menu used...
		PathContextMenu->ExecuteDelete();
	}
	else if (AssetViewPtr->GetSelectedFileItems().Num() > 0)
	{
		// ... but the asset view still takes precedence over an unfocused path view unless it has no selection
		AssetContextMenu->ExecuteDelete();
	}
	else if (PathViewPtr->GetSelectedFolderItems().Num() > 0)
	{
		PathContextMenu->ExecuteDelete();
	}
}

void SContentBrowser::HandleOpenAssetsOrFoldersCommandExecute()
{
	AssetViewPtr->OnOpenAssetsOrFolders();
}

void SContentBrowser::HandlePreviewAssetsCommandExecute()
{
	AssetViewPtr->OnPreviewAssets();
}

void SContentBrowser::HandleCreateNewFolderCommandExecute()
{
	TArray<FString> SelectedPaths = PathViewPtr->GetSelectedPaths();

	// only create folders when a single path is selected
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	const bool bCanCreateNewFolder = SelectedPaths.Num() == 1 && ContentBrowserData->CanCreateFolder(*SelectedPaths[0], nullptr);;

	if (bCanCreateNewFolder)
	{
		CreateNewFolder(
			SelectedPaths.Num() > 0
			? SelectedPaths[0]
			: FString(),
			FOnCreateNewFolder::CreateSP(AssetViewPtr.Get(), &SAssetView::NewFolderItemRequested));
	}
}

void SContentBrowser::GetSelectionState(TArray<FAssetData>& SelectedAssets, TArray<FString>& SelectedPaths)
{
	SelectedAssets.Reset();
	SelectedPaths.Reset();
	if (AssetViewPtr->HasAnyUserFocusOrFocusedDescendants())
	{
		SelectedAssets = AssetViewPtr->GetSelectedAssets();
		SelectedPaths = AssetViewPtr->GetSelectedFolders();
	}
	else if (PathViewPtr->HasAnyUserFocusOrFocusedDescendants())
	{
		SelectedPaths = PathViewPtr->GetSelectedPaths();
	}
}

bool SContentBrowser::IsBackEnabled() const
{
	return HistoryManager.CanGoBack();
}

bool SContentBrowser::IsForwardEnabled() const
{
	return HistoryManager.CanGoForward();
}

FText SContentBrowser::GetHistoryBackTooltip() const
{
	if ( HistoryManager.CanGoBack() )
	{
		return FText::Format( LOCTEXT("HistoryBackTooltipFmt", "Back to {0}"), HistoryManager.GetBackDesc() );
	}
	return FText::GetEmpty();
}

FText SContentBrowser::GetHistoryForwardTooltip() const
{
	if ( HistoryManager.CanGoForward() )
	{
		return FText::Format( LOCTEXT("HistoryForwardTooltipFmt", "Forward to {0}"), HistoryManager.GetForwardDesc() );
	}
	return FText::GetEmpty();
}

void SContentBrowser::SyncGlobalSelectionSet()
{
	USelection* EditorSelection = GEditor->GetSelectedObjects();
	if ( !ensure( EditorSelection != NULL ) )
	{
		return;
	}

	// Get the selected assets in the asset view
	const TArray<FAssetData>& SelectedAssets = AssetViewPtr->GetSelectedAssets();

	EditorSelection->BeginBatchSelectOperation();
	{
		TSet< UObject* > SelectedObjects;
		// Lets see what the user has selected and add any new selected objects to the global selection set
		for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			// Grab the object if it is loaded
			if ( (*AssetIt).IsAssetLoaded() )
			{
				UObject* FoundObject = (*AssetIt).GetAsset();
				if( FoundObject != NULL && FoundObject->GetClass() != UObjectRedirector::StaticClass() )
				{
					SelectedObjects.Add( FoundObject );

					// Select this object!
					EditorSelection->Select( FoundObject );
				}
			}
		}


		// Now we'll build a list of objects that need to be removed from the global selection set
		for( int32 CurEditorObjectIndex = 0; CurEditorObjectIndex < EditorSelection->Num(); ++CurEditorObjectIndex )
		{
			UObject* CurEditorObject = EditorSelection->GetSelectedObject( CurEditorObjectIndex );
			if( CurEditorObject != NULL ) 
			{
				if( !SelectedObjects.Contains( CurEditorObject ) )
				{
					EditorSelection->Deselect( CurEditorObject );
				}
			}
		}
	}
	EditorSelection->EndBatchSelectOperation();
}

void SContentBrowser::UpdatePath()
{
	FSourcesData SourcesData = AssetViewPtr->GetSourcesData();

	PathBreadcrumbTrail->ClearCrumbs();

	int32 NewSourcesWidgetIndex = ActiveSourcesWidgetIndex;

	if ( SourcesData.HasVirtualPaths() )
	{
		NewSourcesWidgetIndex = ContentBrowserSourcesWidgetSwitcherIndex::PathView;

		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		TArray<FString> Crumbs;
		SourcesData.VirtualPaths[0].ToString().ParseIntoArray(Crumbs, TEXT("/"), true);

		FString CrumbPath = TEXT("/");
		for (const FString& Crumb : Crumbs)
		{
			CrumbPath += Crumb;

			const FContentBrowserItem CrumbFolderItem = ContentBrowserData->GetItemAtPath(*CrumbPath, EContentBrowserItemTypeFilter::IncludeFolders);
			PathBreadcrumbTrail->PushCrumb(CrumbFolderItem.IsValid() ? CrumbFolderItem.GetDisplayName() : FText::FromString(Crumb), CrumbPath);

			CrumbPath += TEXT("/");
		}
	}
	else if ( SourcesData.HasCollections() )
	{
		NewSourcesWidgetIndex = GetDefault<UContentBrowserSettings>()->GetDockCollections() ? ContentBrowserSourcesWidgetSwitcherIndex::PathView : ContentBrowserSourcesWidgetSwitcherIndex::CollectionsView;

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		TArray<FCollectionNameType> CollectionPathItems;

		// Walk up the parents of this collection so that we can generate a complete path (this loop also adds the child collection to the array)
		for (TOptional<FCollectionNameType> CurrentCollection = SourcesData.Collections[0]; 
			CurrentCollection.IsSet(); 
			CurrentCollection = CollectionManagerModule.Get().GetParentCollection(CurrentCollection->Name, CurrentCollection->Type)
			)
		{
			CollectionPathItems.Insert(CurrentCollection.GetValue(), 0);
		}

		// Now add each part of the path to the breadcrumb trail
		for (const FCollectionNameType& CollectionPathItem : CollectionPathItems)
		{
			const FString CrumbData = FString::Printf(TEXT("%s?%s"), *CollectionPathItem.Name.ToString(), *FString::FromInt(CollectionPathItem.Type));

			FFormatNamedArguments Args;
			Args.Add(TEXT("CollectionName"), FText::FromName(CollectionPathItem.Name));
			const FText DisplayName = FText::Format(LOCTEXT("CollectionPathIndicator", "{CollectionName} (Collection)"), Args);

			PathBreadcrumbTrail->PushCrumb(DisplayName, CrumbData);
		}
	}
	else
	{
		PathBreadcrumbTrail->PushCrumb(LOCTEXT("AllAssets", "All Assets"), TEXT(""));
	}

	if (ActiveSourcesWidgetIndex != NewSourcesWidgetIndex)
	{
		ActiveSourcesWidgetIndex = NewSourcesWidgetIndex;
		SourcesWidgetSwitcher->SetActiveWidgetIndex(ActiveSourcesWidgetIndex);
	}
}

void SContentBrowser::OnFilterChanged()
{
	FARFilter Filter = FilterListPtr->GetCombinedBackendFilter();
	AssetViewPtr->SetBackendFilter( Filter );

	// Notify 'filter changed' delegate
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	ContentBrowserModule.GetOnFilterChanged().Broadcast(Filter, bIsPrimaryBrowser);
}

FText SContentBrowser::GetPathText() const
{
	FText PathLabelText;

	if ( IsFilteredBySource() )
	{
		const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

		// At least one source is selected
		const int32 NumSources = SourcesData.VirtualPaths.Num() + SourcesData.Collections.Num();

		if (NumSources > 0)
		{
			PathLabelText = FText::FromName(SourcesData.HasVirtualPaths() ? SourcesData.VirtualPaths[0] : SourcesData.Collections[0].Name);

			if (NumSources > 1)
			{
				PathLabelText = FText::Format(LOCTEXT("PathTextFmt", "{0} and {1} {1}|plural(one=other,other=others)..."), PathLabelText, NumSources - 1);
			}
		}
	}
	else
	{
		PathLabelText = LOCTEXT("AllAssets", "All Assets");
	}

	return PathLabelText;
}

bool SContentBrowser::IsFilteredBySource() const
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();
	return !SourcesData.IsEmpty();
}

void SContentBrowser::OnItemRenameCommitted(TArrayView<const FContentBrowserItem> Items)
{
	// After a rename is committed we allow an implicit sync so as not to
	// disorientate the user if they are looking at a parent folder

	const bool bAllowImplicitSync = true;
	const bool bDisableFiltersThatHideAssets = false;
	SyncToItems(Items, bAllowImplicitSync, bDisableFiltersThatHideAssets);
}

void SContentBrowser::OnShowInPathsViewRequested(TArrayView<const FContentBrowserItem> ItemsToFind)
{
	SyncToItems(ItemsToFind);
}

void SContentBrowser::OnRenameRequested(const FContentBrowserItem& Item, EContentBrowserViewContext ViewContext)
{
	FText RenameErrorMsg;
	if (Item.CanRename(nullptr, &RenameErrorMsg))
	{
		if (ViewContext == EContentBrowserViewContext::AssetView)
		{
			AssetViewPtr->RenameItem(Item);
		}
		else
		{
			PathViewPtr->RenameFolderItem(Item);
		}
	}
	else
	{
		AssetViewUtils::ShowErrorNotifcation(RenameErrorMsg);
	}
}

void SContentBrowser::OnOpenedFolderDeleted()
{
	// Since the contents of the asset view have just been deleted, set the selected path to the default "/Game"
	TArray<FString> DefaultSelectedPaths;
	DefaultSelectedPaths.Add(TEXT("/Game"));
	PathViewPtr->SetSelectedPaths(DefaultSelectedPaths);
	PathSelected(TEXT("/Game"));
}

void SContentBrowser::OnDuplicateRequested(TArrayView<const FContentBrowserItem> OriginalItems)
{
	if (OriginalItems.Num() == 1)
	{
		// Asynchronous duplication of a single item
		const FContentBrowserItem& OriginalItem = OriginalItems[0];
		if (ensureAlwaysMsgf(OriginalItem.IsFile(), TEXT("Can only duplicate files!")))
		{
			FText DuplicateErrorMsg;
			if (OriginalItem.CanDuplicate(&DuplicateErrorMsg))
			{
				const FContentBrowserItemDataTemporaryContext NewItemContext = OriginalItem.Duplicate();
				if (NewItemContext.IsValid())
				{
					AssetViewPtr->NewFileItemRequested(NewItemContext);
				}
			}
			else
			{
				AssetViewUtils::ShowErrorNotifcation(DuplicateErrorMsg);
			}
		}
	}
	else if (OriginalItems.Num() > 1)
	{
		// Batch these by their data sources
		TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
		for (const FContentBrowserItem& OriginalItem : OriginalItems)
		{
			FContentBrowserItem::FItemDataArrayView ItemDataArray = OriginalItem.GetInternalItems();
			for (const FContentBrowserItemData& ItemData : ItemDataArray)
			{
				if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
				{
					FText DuplicateErrorMsg;
					if (ItemDataSource->CanDuplicateItem(ItemData, &DuplicateErrorMsg))
					{
						TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
						ItemsForSource.Add(ItemData);
					}
					else
					{
						AssetViewUtils::ShowErrorNotifcation(DuplicateErrorMsg);
					}
				}
			}
		}

		// Execute the operation now
		TArray<FContentBrowserItemData> NewItems;
		for (const auto& SourceAndItemsPair : SourcesAndItems)
		{
			SourceAndItemsPair.Key->BulkDuplicateItems(SourceAndItemsPair.Value, NewItems);
		}

		// Sync the view to the new items
		if (NewItems.Num() > 0)
		{
			TArray<FContentBrowserItem> ItemsToSync;
			for (const FContentBrowserItemData& NewItem : NewItems)
			{
				ItemsToSync.Emplace(NewItem);
			}

			SyncToItems(ItemsToSync);
		}
	}
}

void SContentBrowser::OnEditRequested(TArrayView<const FContentBrowserItem> Items)
{
	OnItemsActivated(Items, EAssetTypeActivationMethod::Opened);
}

void SContentBrowser::OnAssetViewRefreshRequested()
{
	AssetViewPtr->RequestSlowFullListRefresh();
}

void SContentBrowser::HandleCollectionRemoved(const FCollectionNameType& Collection)
{
	AssetViewPtr->SetSourcesData(FSourcesData());

	auto RemoveHistoryDelegate = [&](const FHistoryData& HistoryData)
	{
		return (HistoryData.SourcesData.Collections.Num() == 1 &&
				HistoryData.SourcesData.VirtualPaths.Num() == 0 &&
				HistoryData.SourcesData.Collections.Contains(Collection));
	};

	HistoryManager.RemoveHistoryData(RemoveHistoryDelegate);
}

void SContentBrowser::HandleCollectionRenamed(const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection)
{
	return HandleCollectionRemoved(OriginalCollection);
}

void SContentBrowser::HandleCollectionUpdated(const FCollectionNameType& Collection)
{
	const FSourcesData& SourcesData = AssetViewPtr->GetSourcesData();

	// If we're currently viewing the dynamic collection that was updated, make sure our active filter text is up-to-date
	if (SourcesData.IsDynamicCollection() && SourcesData.Collections[0] == Collection)
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		const FCollectionNameType& DynamicCollection = SourcesData.Collections[0];

		FString DynamicQueryString;
		CollectionManagerModule.Get().GetDynamicQueryText(DynamicCollection.Name, DynamicCollection.Type, DynamicQueryString);

		const FText DynamicQueryText = FText::FromString(DynamicQueryString);
		SetSearchBoxText(DynamicQueryText);
		SearchBoxPtr->SetText(DynamicQueryText);
	}
}

void SContentBrowser::HandlePathRemoved(const FName Path)
{
	auto RemoveHistoryDelegate = [&](const FHistoryData& HistoryData)
	{
		return (HistoryData.SourcesData.VirtualPaths.Num() == 1 &&
				HistoryData.SourcesData.Collections.Num() == 0 &&
				HistoryData.SourcesData.VirtualPaths.Contains(Path));
	};

	HistoryManager.RemoveHistoryData(RemoveHistoryDelegate);
}

void SContentBrowser::HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems)
{
	for (const FContentBrowserItemDataUpdate& ItemDataUpdate : InUpdatedItems)
	{
		if (!ItemDataUpdate.GetItemData().IsFolder())
		{
			continue;
		}

		switch (ItemDataUpdate.GetUpdateType())
		{
		case EContentBrowserItemUpdateType::Moved:
			HandlePathRemoved(ItemDataUpdate.GetPreviousVirtualPath());
			break;

		case EContentBrowserItemUpdateType::Removed:
			HandlePathRemoved(ItemDataUpdate.GetItemData().GetVirtualPath());
			break;

		default:
			break;
		}
	}
}

FText SContentBrowser::GetSearchAssetsHintText() const
{
	if (PathViewPtr.IsValid())
	{
		TArray<FContentBrowserItem> Paths = PathViewPtr->GetSelectedFolderItems();
		if (Paths.Num() > 0)
		{
			FString SearchHint = NSLOCTEXT( "ContentBrowser", "SearchBoxPartialHint", "Search" ).ToString();
			SearchHint += TEXT(" ");
			for(int32 i = 0; i < Paths.Num(); i++)
			{
				SearchHint += Paths[i].GetDisplayName().ToString();

				if (i + 1 < Paths.Num())
				{
					SearchHint += ", ";
				}
			}

			return FText::FromString(SearchHint);
		}
	}
	
	return NSLOCTEXT( "ContentBrowser", "SearchBoxHint", "Search Assets" );
}

void ExtractAssetSearchFilterTerms(const FText& SearchText, FString* OutFilterKey, FString* OutFilterValue, int32* OutSuggestionInsertionIndex)
{
	const FString SearchString = SearchText.ToString();

	if (OutFilterKey)
	{
		OutFilterKey->Reset();
	}
	if (OutFilterValue)
	{
		OutFilterValue->Reset();
	}
	if (OutSuggestionInsertionIndex)
	{
		*OutSuggestionInsertionIndex = SearchString.Len();
	}

	// Build the search filter terms so that we can inspect the tokens
	FTextFilterExpressionEvaluator LocalFilter(ETextFilterExpressionEvaluatorMode::Complex);
	LocalFilter.SetFilterText(SearchText);

	// Inspect the tokens to see what the last part of the search term was
	// If it was a key->value pair then we'll use that to control what kinds of results we show
	// For anything else we just use the text from the last token as our filter term to allow incremental auto-complete
	const TArray<FExpressionToken>& FilterTokens = LocalFilter.GetFilterExpressionTokens();
	if (FilterTokens.Num() > 0)
	{
		const FExpressionToken& LastToken = FilterTokens.Last();

		// If the last token is a text token, then consider it as a value and walk back to see if we also have a key
		if (LastToken.Node.Cast<TextFilterExpressionParser::FTextToken>())
		{
			if (OutFilterValue)
			{
				*OutFilterValue = LastToken.Context.GetString();
			}
			if (OutSuggestionInsertionIndex)
			{
				*OutSuggestionInsertionIndex = FMath::Min(*OutSuggestionInsertionIndex, LastToken.Context.GetCharacterIndex());
			}

			if (FilterTokens.IsValidIndex(FilterTokens.Num() - 2))
			{
				const FExpressionToken& ComparisonToken = FilterTokens[FilterTokens.Num() - 2];
				if (ComparisonToken.Node.Cast<TextFilterExpressionParser::FEqual>())
				{
					if (FilterTokens.IsValidIndex(FilterTokens.Num() - 3))
					{
						const FExpressionToken& KeyToken = FilterTokens[FilterTokens.Num() - 3];
						if (KeyToken.Node.Cast<TextFilterExpressionParser::FTextToken>())
						{
							if (OutFilterKey)
							{
								*OutFilterKey = KeyToken.Context.GetString();
							}
							if (OutSuggestionInsertionIndex)
							{
								*OutSuggestionInsertionIndex = FMath::Min(*OutSuggestionInsertionIndex, KeyToken.Context.GetCharacterIndex());
							}
						}
					}
				}
			}
		}

		// If the last token is a comparison operator, then walk back and see if we have a key
		else if (LastToken.Node.Cast<TextFilterExpressionParser::FEqual>())
		{
			if (FilterTokens.IsValidIndex(FilterTokens.Num() - 2))
			{
				const FExpressionToken& KeyToken = FilterTokens[FilterTokens.Num() - 2];
				if (KeyToken.Node.Cast<TextFilterExpressionParser::FTextToken>())
				{
					if (OutFilterKey)
					{
						*OutFilterKey = KeyToken.Context.GetString();
					}
					if (OutSuggestionInsertionIndex)
					{
						*OutSuggestionInsertionIndex = FMath::Min(*OutSuggestionInsertionIndex, KeyToken.Context.GetCharacterIndex());
					}
				}
			}
		}
	}
}

void SContentBrowser::OnAssetSearchSuggestionFilter(const FText& SearchText, TArray<FAssetSearchBoxSuggestion>& PossibleSuggestions, FText& SuggestionHighlightText) const
{
	// We don't bind the suggestion list, so this list should be empty as we populate it here based on the search term
	check(PossibleSuggestions.Num() == 0);

	FString FilterKey;
	FString FilterValue;
	ExtractAssetSearchFilterTerms(SearchText, &FilterKey, &FilterValue, nullptr);

	auto PassesValueFilter = [&FilterValue](const FString& InOther)
	{
		return FilterValue.IsEmpty() || InOther.Contains(FilterValue);
	};

	if (FilterKey.IsEmpty() || (FilterKey == TEXT("Type") || FilterKey == TEXT("Class")))
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		TArray< TWeakPtr<IAssetTypeActions> > AssetTypeActionsList;
		AssetToolsModule.Get().GetAssetTypeActionsList(AssetTypeActionsList);

		const FText TypesCategoryName = NSLOCTEXT("ContentBrowser", "TypesCategoryName", "Types");
		for (auto TypeActionsIt = AssetTypeActionsList.CreateConstIterator(); TypeActionsIt; ++TypeActionsIt)
		{
			if ((*TypeActionsIt).IsValid())
			{
				const TSharedPtr<IAssetTypeActions> TypeActions = (*TypeActionsIt).Pin();
				if (TypeActions->GetSupportedClass())
				{
					const FString TypeName = TypeActions->GetSupportedClass()->GetName();
					const FText TypeDisplayName = TypeActions->GetSupportedClass()->GetDisplayNameText();
					FString TypeSuggestion = FString::Printf(TEXT("Type=%s"), *TypeName);
					if (PassesValueFilter(TypeSuggestion))
					{
						PossibleSuggestions.Add(FAssetSearchBoxSuggestion{ MoveTemp(TypeSuggestion), TypeDisplayName, TypesCategoryName });
					}
				}
			}
		}
	}

	if (FilterKey.IsEmpty() || (FilterKey == TEXT("Collection") || FilterKey == TEXT("Tag")))
	{
		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

		TArray<FCollectionNameType> AllCollections;
		CollectionManager.GetCollections(AllCollections);

		const FText CollectionsCategoryName = NSLOCTEXT("ContentBrowser", "CollectionsCategoryName", "Collections");
		for (const FCollectionNameType& Collection : AllCollections)
		{
			FString CollectionName = Collection.Name.ToString();
			FString CollectionSuggestion = FString::Printf(TEXT("Collection=%s"), *CollectionName);
			if (PassesValueFilter(CollectionSuggestion))
			{
				PossibleSuggestions.Add(FAssetSearchBoxSuggestion{ MoveTemp(CollectionSuggestion), FText::FromString(MoveTemp(CollectionName)), CollectionsCategoryName });
			}
		}
	}

	if (FilterKey.IsEmpty())
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();

		if (const FAssetRegistryState* StatePtr = AssetRegistry.GetAssetRegistryState())
		{
			const FText MetaDataCategoryName = NSLOCTEXT("ContentBrowser", "MetaDataCategoryName", "Meta-Data");
			for (const auto& TagAndArrayPair : StatePtr->GetTagToAssetDatasMap())
			{
				const FString TagNameStr = TagAndArrayPair.Key.ToString();
				if (PassesValueFilter(TagNameStr))
				{
					PossibleSuggestions.Add(FAssetSearchBoxSuggestion{ TagNameStr, FText::FromString(TagNameStr), MetaDataCategoryName });
				}
			}
		}
	}

	SuggestionHighlightText = FText::FromString(FilterValue);
}

FText SContentBrowser::OnAssetSearchSuggestionChosen(const FText& SearchText, const FString& Suggestion) const
{
	int32 SuggestionInsertionIndex = 0;
	ExtractAssetSearchFilterTerms(SearchText, nullptr, nullptr, &SuggestionInsertionIndex);

	FString SearchString = SearchText.ToString();
	SearchString.RemoveAt(SuggestionInsertionIndex, SearchString.Len() - SuggestionInsertionIndex, false);
	SearchString.Append(Suggestion);

	return FText::FromString(SearchString);
}

TSharedPtr<SWidget> SContentBrowser::GetItemContextMenu(TArrayView<const FContentBrowserItem> SelectedItems, EContentBrowserViewContext ViewContext)
{
	// We may only open the file or folder context menu (folder takes priority), so see whether we have any folders selected
	TArray<FContentBrowserItem> SelectedFolders;
	for (const FContentBrowserItem& SelectedItem : SelectedItems)
	{
		if (SelectedItem.IsFolder())
		{
			SelectedFolders.Add(SelectedItem);
		}
	}

	if (SelectedFolders.Num() > 0)
	{
		// Folders selected - show the folder menu

		// Clear any selection in the asset view, as it'll conflict with other view info
		// This is important for determining which context menu may be open based on the asset selection for rename/delete operations
		if (ViewContext != EContentBrowserViewContext::AssetView)
		{
			AssetViewPtr->ClearSelection();
		}

		// Ensure the path context menu has the up-to-date list of paths being worked on
		PathContextMenu->SetSelectedFolders(SelectedFolders);

		if (!UToolMenus::Get()->IsMenuRegistered("ContentBrowser.FolderContextMenu"))
		{
			UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("ContentBrowser.FolderContextMenu");
			Menu->bCloseSelfOnly = true;
			Menu->AddDynamicSection("Section", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				UContentBrowserFolderContext* Context = InMenu->FindContext<UContentBrowserFolderContext>();
				if (Context && Context->ContentBrowser.IsValid())
				{
					Context->ContentBrowser.Pin()->PopulateFolderContextMenu(InMenu);
				}
			}));
		}

		TArray<FString> SelectedPackagePaths;
		for (const FContentBrowserItem& SelectedFolder : SelectedFolders)
		{
			FName PackagePath;
			if (SelectedFolder.Legacy_TryGetPackagePath(PackagePath))
			{
				SelectedPackagePaths.Add(PackagePath.ToString());
			}
		}

		TSharedPtr<FExtender> Extender;
		if (SelectedPackagePaths.Num() > 0)
		{
			Extender = GetPathContextMenuExtender(SelectedPackagePaths);
		}

		UContentBrowserFolderContext* Context = NewObject<UContentBrowserFolderContext>();
		Context->ContentBrowser = SharedThis(this);
		// Note: This always uses the path view to manage the temporary folder item, even if the context menu came from the favorites view, as the favorites view can't make folders correctly
		Context->OnCreateNewFolder = ViewContext == EContentBrowserViewContext::AssetView ? FOnCreateNewFolder::CreateSP(AssetViewPtr.Get(), &SAssetView::NewFolderItemRequested) : FOnCreateNewFolder::CreateSP(PathViewPtr.Get(), &SPathView::NewFolderItemRequested);
		ContentBrowserUtils::CountPathTypes(SelectedPackagePaths, Context->NumAssetPaths, Context->NumClassPaths);

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		Context->bCanBeModified = AssetToolsModule.Get().AllPassWritableFolderFilter(SelectedPackagePaths);

		if (SelectedPackagePaths.Num() == 0)
		{
			Context->bNoFolderOnDisk = true;
			Context->bCanBeModified = false;
		}

		FToolMenuContext MenuContext(Commands, Extender, Context);

		{
			UContentBrowserDataMenuContext_FolderMenu* DataContextObject = NewObject<UContentBrowserDataMenuContext_FolderMenu>();
			DataContextObject->SelectedItems = PathContextMenu->GetSelectedFolders();
			DataContextObject->bCanBeModified = Context->bCanBeModified;
			DataContextObject->ParentWidget = ViewContext == EContentBrowserViewContext::AssetView ? TSharedPtr<SWidget>(AssetViewPtr) : ViewContext == EContentBrowserViewContext::FavoriteView ? TSharedPtr<SWidget>(FavoritePathViewPtr) : TSharedPtr<SWidget>(PathViewPtr);
			MenuContext.AddObject(DataContextObject);
		}

		{
			TArray<FName> SelectedVirtualPaths;
			for (const FContentBrowserItem& SelectedFolder : SelectedFolders)
			{
				SelectedVirtualPaths.Add(SelectedFolder.GetVirtualPath());
			}
			AppendNewMenuContextObjects(EContentBrowserDataMenuContext_AddNewMenuDomain::PathView, SelectedVirtualPaths, MenuContext, nullptr);
		}

		return UToolMenus::Get()->GenerateWidget("ContentBrowser.FolderContextMenu", MenuContext);
	}
	else if (SelectedItems.Num() > 0)
	{
		// Files selected - show the file menu
		checkf(ViewContext == EContentBrowserViewContext::AssetView, TEXT("File items were passed from a path view!"));
		return AssetContextMenu->MakeContextMenu(SelectedItems, AssetViewPtr->GetSourcesData(), Commands);
	}
	else if (ViewContext == EContentBrowserViewContext::AssetView)
	{
		// Nothing selected - show the new asset menu
		return MakeAddNewContextMenu(EContentBrowserDataMenuContext_AddNewMenuDomain::AssetView, nullptr);
	}

	return nullptr;
}

void SContentBrowser::PopulateFolderContextMenu(UToolMenu* Menu)
{
	UContentBrowserFolderContext* Context = Menu->FindContext<UContentBrowserFolderContext>();
	check(Context);

	const TArray<FContentBrowserItem>& SelectedFolders = PathContextMenu->GetSelectedFolders();

	// We can only create folders when we have a single path selected
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	const bool bCanCreateNewFolder = SelectedFolders.Num() == 1 && ContentBrowserData->CanCreateFolder(SelectedFolders[0].GetVirtualPath(), nullptr);

	FText NewFolderToolTip;
	if(SelectedFolders.Num() == 1)
	{
		if(bCanCreateNewFolder)
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_CreateIn", "Create a new folder in {0}."), FText::FromName(SelectedFolders[0].GetVirtualPath()));
		}
		else
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_InvalidPath", "Cannot create new folders in {0}."), FText::FromName(SelectedFolders[0].GetVirtualPath()));
		}
	}
	else
	{
		NewFolderToolTip = LOCTEXT("NewFolderTooltip_InvalidNumberOfPaths", "Can only create folders when there is a single path selected.");
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Section");

		if (Context->bCanBeModified)
		{
			// New Folder
			Section.AddMenuEntry(
				"NewFolder",
				LOCTEXT("NewFolder", "New Folder"),
				NewFolderToolTip,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"),
				FUIAction(
					FExecuteAction::CreateSP(this, &SContentBrowser::CreateNewFolder, SelectedFolders.Num() > 0 ? SelectedFolders[0].GetVirtualPath().ToString() : FString(), Context->OnCreateNewFolder),
					FCanExecuteAction::CreateLambda([bCanCreateNewFolder] { return bCanCreateNewFolder; })
				)
			);
		}

		Section.AddMenuEntry(
			"FolderContext",
			LOCTEXT("ShowInNewContentBrowser", "Show in New Content Browser"),
			LOCTEXT("ShowInNewContentBrowserTooltip", "Opens a new Content Browser at this folder location (at least 1 Content Browser window needs to be locked)"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SContentBrowser::OpenNewContentBrowser))
		);
	}

	PathContextMenu->MakePathViewContextMenu(Menu);
}

void SContentBrowser::CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder)
{
	const FText DefaultFolderBaseName = LOCTEXT("DefaultFolderName", "NewFolder");
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	// Create a valid base name for this folder
	FString DefaultFolderName = DefaultFolderBaseName.ToString();
	int32 NewFolderPostfix = 0;
	FName CombinedPathName;
	for (;;)
	{
		FString CombinedPathNameStr = FolderPath / DefaultFolderName;
		if (NewFolderPostfix > 0)
		{
			CombinedPathNameStr.AppendInt(NewFolderPostfix);
		}
		++NewFolderPostfix;

		CombinedPathName = *CombinedPathNameStr;

		const FContentBrowserItem ExistingFolder = ContentBrowserData->GetItemAtPath(CombinedPathName, EContentBrowserItemTypeFilter::IncludeFolders);
		if (!ExistingFolder.IsValid())
		{
			break;
		}
	}

	const FContentBrowserItemTemporaryContext NewFolderItem = ContentBrowserData->CreateFolder(CombinedPathName);
	if (NewFolderItem.IsValid())
	{
		InOnCreateNewFolder.ExecuteIfBound(NewFolderItem);
	}
}

void SContentBrowser::OpenNewContentBrowser()
{
	const TArray<FContentBrowserItem> SelectedFolders = PathContextMenu->GetSelectedFolders();
	FContentBrowserSingleton::Get().SyncBrowserToItems(SelectedFolders, false, true, NAME_None, true);
}

#undef LOCTEXT_NAMESPACE
