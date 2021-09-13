// Copyright Epic Games, Inc. All Rights Reserved.

#include "Menus/MainMenu.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IMainFrameModule.h"
#include "DesktopPlatformModule.h"
#include "ISourceControlModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "SourceCodeNavigation.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "EditorStyleSet.h"
#include "Classes/EditorStyleSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorExperimentalSettings.h"
#include "UnrealEdGlobals.h"
#include "Frame/MainFrameActions.h"
#include "Menus/LayoutsMenu.h"
#include "Menus/RecentProjectsMenu.h"
#include "Menus/SettingsMenu.h"
#include "Menus/MainFrameTranslationEditorMenu.h"

#include "ToolMenus.h"

#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Features/EditorFeatures.h"
#include "Features/IModularFeatures.h"
#include "UndoHistoryModule.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "MainFileMenu"


void FMainMenu::RegisterFileMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* FileMenu = ToolMenus->RegisterMenu("MainFrame.MainMenu.File");

	FToolMenuSection& FileLoadAndSaveSection = FileMenu->AddSection("FileOpen", LOCTEXT("FileOpenHeading", "Open"), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));
	{
		// Open Asset...
		FileLoadAndSaveSection.AddMenuEntry(FGlobalEditorCommonCommands::Get().SummonOpenAssetDialog);
	}


	FToolMenuSection& FileSaveSection = FileMenu->AddSection("FileSave", LOCTEXT("FileSaveHeading", "Save"), FToolMenuInsert("FileOpen", EToolMenuInsertType::After));
	{

		// Save All
		FileSaveSection.AddMenuEntry(FMainFrameCommands::Get().SaveAll);

		// Choose specific files to save
		FileSaveSection.AddMenuEntry(FMainFrameCommands::Get().ChooseFilesToSave);
	}

	RegisterFileProjectMenu();
	RegisterExitMenuItems();
}

#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "MainEditMenu"

void FMainMenu::RegisterEditMenu()
{
	UToolMenu* EditMenu = UToolMenus::Get()->RegisterMenu("MainFrame.MainMenu.Edit");
	{
		FToolMenuSection& Section = EditMenu->AddSection("EditHistory", LOCTEXT("HistoryHeading", "History"));
		struct Local
		{
			/** @return Returns a dynamic text string for Undo that contains the name of the action */
			static FText GetUndoLabelText()
			{
				return FText::Format(LOCTEXT("DynamicUndoLabel", "Undo {0}"), GUnrealEd->Trans->GetUndoContext().Title);
			}

			/** @return Returns a dynamic text string for Redo that contains the name of the action */
			static FText GetRedoLabelText()
			{
				return FText::Format(LOCTEXT("DynamicRedoLabel", "Redo {0}"), GUnrealEd->Trans->GetRedoContext().Title);
			}
		};

		// Undo
		TAttribute<FText> DynamicUndoLabel;
		DynamicUndoLabel.BindStatic(&Local::GetUndoLabelText);
		Section.AddMenuEntry(FGenericCommands::Get().Undo, DynamicUndoLabel); // TAttribute< FString >::Create( &Local::GetUndoLabelText ) );

		// Redo
		TAttribute< FText > DynamicRedoLabel;
		DynamicRedoLabel.BindStatic( &Local::GetRedoLabelText );
		Section.AddMenuEntry(FGenericCommands::Get().Redo, DynamicRedoLabel); // TAttribute< FString >::Create( &Local::GetRedoLabelText ) );

		// Show undo history
		Section.AddMenuEntry(
			"UndoHistory",
			LOCTEXT("UndoHistoryTabTitle", "Undo History"),
			LOCTEXT("UndoHistoryTooltipText", "View the entire undo history."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "UndoHistory.TabIcon"),
			FUIAction(FExecuteAction::CreateStatic(&FUndoHistoryModule::ExecuteOpenUndoHistory))
			);
	}

	{
		FToolMenuSection& Section = EditMenu->AddSection("Configuration", LOCTEXT("ConfigurationHeading", "Configuration"));
		if (GetDefault<UEditorStyleSettings>()->bExpandConfigurationMenus)
		{
			Section.AddSubMenu(
				"EditorPreferencesSubMenu",
				LOCTEXT("EditorPreferencesSubMenuLabel", "Editor Preferences"),
				LOCTEXT("EditorPreferencesSubMenuToolTip", "Configure the behavior and features of this Editor"),
				FNewToolMenuDelegate::CreateStatic(&FSettingsMenu::MakeMenu, FName("Editor")),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorPreferences.TabIcon")
			);

			Section.AddSubMenu(
				"ProjectSettingsSubMenu",
				LOCTEXT("ProjectSettingsSubMenuLabel", "Project Settings"),
				LOCTEXT("ProjectSettingsSubMenuToolTip", "Change the settings of the currently loaded project"),
				FNewToolMenuDelegate::CreateStatic(&FSettingsMenu::MakeMenu, FName("Project")),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon")
			);
		}
		else
		{
#if !PLATFORM_MAC // Handled by app's menu in menu bar
			Section.AddMenuEntry(
				"EditorPreferencesMenu",
				LOCTEXT("EditorPreferencesMenuLabel", "Editor Preferences..."),
				LOCTEXT("EditorPreferencesMenuToolTip", "Configure the behavior and features of the Unreal Editor."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorPreferences.TabIcon"),
				FUIAction(FExecuteAction::CreateStatic(&FSettingsMenu::OpenSettings, FName("Editor"), FName("General"), FName("Appearance")))
			);
#endif

			Section.AddMenuEntry(
				"ProjectSettingsMenu",
				LOCTEXT("ProjectSettingsMenuLabel", "Project Settings..."),
				LOCTEXT("ProjectSettingsMenuToolTip", "Change the settings of the currently loaded project."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ProjectSettings.TabIcon"),
				FUIAction(FExecuteAction::CreateStatic(&FSettingsMenu::OpenSettings, FName("Project"), FName("Project"), FName("General")))
			);
		}

		Section.AddDynamicEntry("PluginsEditor", FNewToolMenuDelegateLegacy::CreateLambda([](FMenuBuilder& InBuilder, UToolMenu* InData)
		{
			//@todo The tab system needs to be able to be extendable by plugins [9/3/2013 Justin.Sargent]
			if (IModularFeatures::Get().IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
			{
				FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(InBuilder, "PluginsEditor");
			}
		}));

		Section.AddMenuEntry(FMainFrameCommands::Get().ConnectToSourceControl);
	}
}

#undef LOCTEXT_NAMESPACE

#define LOCTEXT_NAMESPACE "MainWindowMenu"

void FMainMenu::RegisterWindowMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("MainFrame.MainMenu.Window");

	// Level Editor, General, and Testing sections
	// Automatically populate tab spawners from TabManager
	Menu->AddDynamicSection("TabManagerSection", FNewToolMenuDelegateLegacy::CreateLambda([](FMenuBuilder& InBuilder, UToolMenu* InData)
	{
		if (USlateTabManagerContext* TabManagerContext = InData->FindContext<USlateTabManagerContext>())
		{
			TSharedPtr<FTabManager> TabManager = TabManagerContext->TabManager.Pin();
			if (TabManager.IsValid())
			{
				// Local editor tabs
				TabManager->PopulateLocalTabSpawnerMenu(InBuilder);

				// General tabs
				const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
				TabManager->PopulateTabSpawnerMenu(InBuilder, MenuStructure.GetStructureRoot());
			}
		}
	}));

	

	// Experimental section
	{
		// This is a temporary home for the spawners of experimental features that must be explicitly enabled.
		// When the feature becomes permanent and need not check a flag, register a nomad spawner for it in the proper WorkspaceMenu category
		const bool bLocalizationDashboard = GetDefault<UEditorExperimentalSettings>()->bEnableLocalizationDashboard;
		const bool bTranslationPicker = GetDefault<UEditorExperimentalSettings>()->bEnableTranslationPicker;

		// Make sure at least one is enabled before creating the section
		if (bLocalizationDashboard || bTranslationPicker)
		{
			FToolMenuSection& Section = Menu->AddSection("ExperimentalTabSpawners", LOCTEXT("ExperimentalTabSpawnersHeading", "Experimental"), FToolMenuInsert("WindowLayout", EToolMenuInsertType::Before));
			{
				// Localization Dashboard
				if (bLocalizationDashboard)
				{
					Section.AddMenuEntry(
						"LocalizationDashboard",
						LOCTEXT("LocalizationDashboardLabel", "Localization Dashboard"),
						LOCTEXT("LocalizationDashboardToolTip", "Open the Localization Dashboard for this Project."),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "LocalizationDashboard.MenuIcon"), 
						FUIAction(FExecuteAction::CreateStatic(&FMainMenu::OpenLocalizationDashboard))
						);
				}

				// Translation Picker
				if (bTranslationPicker)
				{
					Section.AddMenuEntry(
						"TranslationPicker",
						LOCTEXT("TranslationPickerMenuItem", "Translation Picker"),
						LOCTEXT("TranslationPickerMenuItemToolTip", "Launch the Translation Picker to Modify Editor Translations"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateStatic(&FMainFrameTranslationEditorMenu::HandleOpenTranslationPicker))
						);
				}
			}
		}
	}

	// Layout section
	{
		FToolMenuSection& Section = Menu->AddSection("WindowLayout", NSLOCTEXT("MainAppMenu", "LayoutManagementHeader", "Layout"));
		// Load Layout
		Section.AddEntry(FToolMenuEntry::InitSubMenu(
			"LoadLayout",
			NSLOCTEXT("LayoutMenu", "LayoutLoadHeader", "Load Layout"),
			NSLOCTEXT("LayoutMenu", "LoadLayoutsSubMenu_ToolTip", "Load a layout configuration from disk"),
			FNewToolMenuDelegate::CreateStatic(&FLayoutsMenuLoad::MakeLoadLayoutsMenu),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.LoadLayout") 
		));
		// Save and Remove Layout
		// Opposite to "Load Layout", Save and Remove are dynamic, i.e., they can be enabled/removed depending on the value of
		// the variable GetDefault<UEditorStyleSettings>()->bEnableUserEditorLayoutManagement
		Section.AddDynamicEntry("OverrideAndRemoveLayout", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			if (GetDefault<UEditorStyleSettings>()->bEnableUserEditorLayoutManagement)
			{
				// Save Layout
				InSection.AddEntry(FToolMenuEntry::InitSubMenu(
					"OverrideLayout",
					NSLOCTEXT("LayoutMenu", "OverrideLayoutsSubMenu", "Save Layout"),
					NSLOCTEXT("LayoutMenu", "OverrideLayoutsSubMenu_ToolTip", "Save your current layout configuration on disk"),
					FNewToolMenuDelegate::CreateStatic(&FLayoutsMenuSave::MakeSaveLayoutsMenu),
					false,
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.SaveLayout") 
				));
				// Remove Layout
				InSection.AddEntry(FToolMenuEntry::InitSubMenu(
					"RemoveLayout",
					NSLOCTEXT("LayoutMenu", "RemoveLayoutsSubMenu", "Remove Layout"),
					NSLOCTEXT("LayoutMenu", "RemoveLayoutsSubMenu_ToolTip", "Remove a layout configuration from disk"),
					FNewToolMenuDelegate::CreateStatic(&FLayoutsMenuRemove::MakeRemoveLayoutsMenu),
					false,
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.RemoveLayout")
				));
			}
		}));

		// Enable Fullscreen section
#if !PLATFORM_MAC && !PLATFORM_LINUX // On Mac/Linux windowed fullscreen mode in the editor is currently unavailable
		// Separator
		Section.AddSeparator("FullscreenSeparator");
		// Fullscreen
		Section.AddMenuEntry(FMainFrameCommands::Get().ToggleFullscreen);
#endif
	}
}

#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "MainHelpMenu"

void FMainMenu::RegisterHelpMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("MainFrame.MainMenu.Help");
	FToolMenuSection& BugReportingSection = Menu->AddSection("BugReporting", NSLOCTEXT("MainHelpMenu", "BugsReporting", "Bugs"));
	{
		BugReportingSection.AddMenuEntry(FMainFrameCommands::Get().ReportABug);
		BugReportingSection.AddMenuEntry(FMainFrameCommands::Get().OpenIssueTracker);
	}

	FToolMenuSection& HelpOnlineSection = Menu->AddSection("HelpOnline", NSLOCTEXT("MainHelpMenu", "Online", "Help Online"));
	{
		HelpOnlineSection.AddMenuEntry(FMainFrameCommands::Get().VisitSupportWebSite);
		HelpOnlineSection.AddMenuEntry(FMainFrameCommands::Get().VisitForums);
		HelpOnlineSection.AddMenuEntry(FMainFrameCommands::Get().VisitSearchForAnswersPage);
		HelpOnlineSection.AddMenuEntry(FMainFrameCommands::Get().VisitOnlineLearning);



		const FText SupportWebSiteLabel = NSLOCTEXT("MainHelpMenu", "VisitUnrealEngineSupportWebSite", "Unreal Engine Support Web Site...");

		HelpOnlineSection.AddSeparator("EpicGamesHelp");
		HelpOnlineSection.AddMenuEntry(FMainFrameCommands::Get().VisitEpicGamesDotCom);

		HelpOnlineSection.AddSeparator("Credits");
		HelpOnlineSection.AddMenuEntry(FMainFrameCommands::Get().CreditsUnrealEd);
	}

#if !PLATFORM_MAC // Handled by app's menu in menu bar
	FToolMenuSection& HelpApplicationSection = Menu->AddSection("HelpApplication", NSLOCTEXT("MainHelpMenu", "Application", "Application"));
	{
		const FText AboutWindowTitle = NSLOCTEXT("MainHelpMenu", "AboutUnrealEditor", "About Unreal Editor...");

		HelpApplicationSection.AddMenuEntry(FMainFrameCommands::Get().AboutUnrealEd, AboutWindowTitle);
	}
#endif
}

#undef LOCTEXT_NAMESPACE

TSharedRef<SWidget> FMainMenu::MakeMainMenu(const TSharedPtr<FTabManager>& TabManager, const FName MenuName, FToolMenuContext& ToolMenuContext)
{
	// Cache all project names once
	FMainFrameActionCallbacks::CacheProjectNames();

	FMainMenu::RegisterMainMenu();

	ToolMenuContext.AppendCommandList(FMainFrameCommands::ActionList);

	USlateTabManagerContext* ContextObject = NewObject<USlateTabManagerContext>();
	ContextObject->TabManager = TabManager;
	ToolMenuContext.AddObject(ContextObject);

	// Create the menu bar!
	TSharedRef<SWidget> MenuBarWidget = UToolMenus::Get()->GenerateWidget(MenuName, ToolMenuContext);
	if (MenuBarWidget != SNullWidget::NullWidget)
	{
		// Tell tab-manager about the multi-box for platforms with a global menu bar
		TSharedRef<SMultiBoxWidget> MultiBoxWidget = StaticCastSharedRef<SMultiBoxWidget>(MenuBarWidget);
		TabManager->SetMenuMultiBox(ConstCastSharedRef<FMultiBox>(MultiBoxWidget->GetMultiBox()), MultiBoxWidget);
	}

	return MenuBarWidget;
}

void FMainMenu::RegisterMainMenu()
{
#define LOCTEXT_NAMESPACE "MainMenu"

	static const FName MainMenuName("MainFrame.MainMenu");
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(MainMenuName))
	{
		return;
	}

	RegisterFileMenu();
	RegisterEditMenu();
	RegisterWindowMenu();
	RegisterToolsMenu();
	RegisterHelpMenu();

	UToolMenu* MenuBar = ToolMenus->RegisterMenu(MainMenuName, NAME_None, EMultiBoxType::MenuBar);

	static const FName MainMenuStyleName("WindowMenuBar");

	MenuBar->StyleName = MainMenuStyleName;

	MenuBar->AddSubMenu(
		"MainMenu",
		NAME_None,
		"File",
		LOCTEXT("FileMenu", "File"),
		LOCTEXT("FileMenu_ToolTip", "Open the file menu")
	);

	MenuBar->AddSubMenu(
		"MainMenu",
		NAME_None,
		"Edit",
		LOCTEXT("EditMenu", "Edit"),
		LOCTEXT("EditMenu_ToolTip", "Open the edit menu")
		);

	MenuBar->AddSubMenu(
		"MainMenu",
		NAME_None,
		"Window",
		LOCTEXT("WindowMenu", "Window"),
		LOCTEXT("WindowMenu_ToolTip", "Open new windows or tabs.")
		);

	MenuBar->AddSubMenu(
		"MainMenu",
		NAME_None,
		"Tools",
		LOCTEXT("ToolsMenu", "Tools"),
		LOCTEXT("ToolsMenu_ToolTip", "Level Tools")
		);


	MenuBar->AddSubMenu(
		"MainMenu",
		NAME_None,
		"Help",
		LOCTEXT("HelpMenu", "Help"),
		LOCTEXT("HelpMenu_ToolTip", "Open the help menu")
	);
}

#undef LOCTEXT_NAMESPACE


#define LOCTEXT_NAMESPACE "MainTabMenu"

void FMainMenu::RegisterFileProjectMenu()
{
	if (!GetDefault<UEditorStyleSettings>()->bShowProjectMenus)
	{
		return;
	}

	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* MainTabFileMenu = ToolMenus->ExtendMenu("MainFrame.MainTabMenu.File");
	FToolMenuSection& Section = MainTabFileMenu->AddSection("FileProject", LOCTEXT("ProjectHeading", "Project"));

	Section.AddMenuEntry( FMainFrameCommands::Get().NewProject );
	Section.AddMenuEntry( FMainFrameCommands::Get().OpenProject );
	/*
	MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().LocalizeProject,
		NAME_None,
		TAttribute<FText>(),
		LOCTEXT("LocalizeProjectToolTip", "Gather text from your project and import/export translations.")
		);
		*/
	/*
	MenuBuilder.AddSubMenu(
		LOCTEXT("CookProjectSubMenuLabel", "Cook Project"),
		LOCTEXT("CookProjectSubMenuToolTip", "Cook your project content for debugging"),
		FNewMenuDelegate::CreateStatic( &FCookContentMenu::MakeMenu ), false, FSlateIcon()
	);
	*/


	Section.AddMenuEntry(FMainFrameCommands::Get().ZipUpProject);

	if (GetDefault<UEditorStyleSettings>()->bShowProjectMenus && FMainFrameActionCallbacks::RecentProjects.Num() > 0)
	{
		Section.AddSubMenu(
			"RecentProjects",
			LOCTEXT("SwitchProjectSubMenu", "Recent Projects"),
			LOCTEXT("SwitchProjectSubMenu_ToolTip", "Select a project to switch to"),
			FNewToolMenuDelegate::CreateStatic(&FRecentProjectsMenu::MakeMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.RecentProjects")
		);
	}

}

void FMainMenu::RegisterToolsMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("MainFrame.MainMenu.Tools");
	FToolMenuSection& Section = Menu->AddSection("Programming", LOCTEXT("ProgrammingHeading", "Programming"));


	FText ShortIDEName = FSourceCodeNavigation::GetSelectedSourceCodeIDE();
	FSlateIcon OpenIDEIcon = FSourceCodeNavigation::GetOpenSourceCodeIDEIcon();
	FSlateIcon RefreshIDEIcon = FSourceCodeNavigation::GetRefreshSourceCodeIDEIcon(); 

	Section.AddMenuEntry( FMainFrameCommands::Get().AddCodeToProject,
		TAttribute<FText>(),
		FText::Format(LOCTEXT("AddCodeToProjectTooltip", "Adds C++ code to the project. The code can only be compiled if you have {0} installed."), ShortIDEName)
	);

	Section.AddDynamicEntry("CodeProject", FNewToolMenuSectionDelegate::CreateLambda([ShortIDEName, RefreshIDEIcon](FToolMenuSection& InSection)
	{
		if (FSourceCodeNavigation::DoesModuleSolutionExist())
		{
			InSection.AddMenuEntry( FMainFrameCommands::Get().RefreshCodeProject,
				FText::Format(LOCTEXT("RefreshCodeProjectLabel", "Refresh {0} Project"), ShortIDEName),
				FText::Format(LOCTEXT("RefreshCodeProjectTooltip", "Refreshes your C++ code project in {0}."), ShortIDEName),
				RefreshIDEIcon	
			);
		}
		else
		{
			InSection.AddMenuEntry( FMainFrameCommands::Get().RefreshCodeProject,
				FText::Format(LOCTEXT("GenerateCodeProjectLabel", "Generate {0} Project"), ShortIDEName),
				FText::Format(LOCTEXT("GenerateCodeProjectTooltip", "Generates your C++ code project in {0}."), ShortIDEName),
				RefreshIDEIcon	
			);
		}
	}));

	Section.AddMenuEntry( FMainFrameCommands::Get().OpenIDE,
		FText::Format(LOCTEXT("OpenIDELabel", "Open {0}"), ShortIDEName),
		FText::Format(LOCTEXT("OpenIDETooltip", "Opens your C++ code in {0}."), ShortIDEName),
		OpenIDEIcon
	);


	// Level Editor, General, and Testing sections
	// Automatically populate tab spawners from TabManager
	Menu->AddDynamicSection("TabManagerSection", FNewToolMenuDelegateLegacy::CreateLambda([](FMenuBuilder& InBuilder, UToolMenu* InData)
	{
		if (USlateTabManagerContext* TabManagerContext = InData->FindContext<USlateTabManagerContext>())
		{
			TSharedPtr<FTabManager> TabManager = TabManagerContext->TabManager.Pin();
			if (TabManager.IsValid())
			{
				// Local editor tabs
				TabManager->PopulateLocalTabSpawnerMenu(InBuilder);

				// General tabs
				const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
				TabManager->PopulateTabSpawnerMenu(InBuilder, MenuStructure.GetToolsStructureRoot());
			}
		}
	}));

}

void FMainMenu::RegisterExitMenuItems()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	UToolMenu* MainTabFileMenu = ToolMenus->RegisterMenu("MainFrame.MainTabMenu.File", "MainFrame.MainMenu.File");

	

#if !PLATFORM_MAC // Handled by app's menu in menu bar
	{
		FToolMenuSection& Section = MainTabFileMenu->AddSection("Exit", LOCTEXT("Exit", "Exit"), FToolMenuInsert("FileProject", EToolMenuInsertType::After));
		Section.AddSeparator("Exit");
		Section.AddMenuEntry( FMainFrameCommands::Get().Exit );
	}
#endif
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedRef< SWidget > FMainMenu::MakeMainTabMenu( const TSharedPtr<FTabManager>& TabManager, const FName MenuName, FToolMenuContext& ToolMenuContext )
{
	return FMainMenu::MakeMainMenu( TabManager, MenuName, ToolMenuContext );
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
