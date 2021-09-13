// Copyright Epic Games, Inc. All Rights Reserved.

#include "StarshipStyle.h"
#include "Misc/CommandLine.h"
#include "Styling/StarshipCoreStyle.h"

#include "Classes/EditorStyleSettings.h"

#include "SlateOptMacros.h"
#include "Styling/SlateStyleMacros.h"

#if (WITH_EDITOR || (IS_PROGRAM && PLATFORM_DESKTOP))
	#include "PlatformInfo.h"
#endif
#include "Styling/ToolBarStyle.h"
#include "Styling/SegmentedControlStyle.h"
#include "Styling/StyleColors.h"
#include "Framework/Application/SlateApplication.h"


#define ICON_FONT(...) FSlateFontInfo(RootToContentDir("Fonts/FontAwesome", TEXT(".ttf")), __VA_ARGS__)

#define LOCTEXT_NAMESPACE "EditorStyle"

void FStarshipEditorStyle::Initialize()
{
	Settings = NULL;

	// The core style must be initialized before the editor style
	FSlateApplication::InitializeCoreStyle();

#if WITH_EDITOR
	Settings = GetMutableDefault<UEditorStyleSettings>();
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Editor", "General", "Appearance",
			LOCTEXT("Appearance_UserSettingsName", "Appearance"),
			LOCTEXT("Appearance_UserSettingsDescription", "Customize the look of the editor."),
			Settings
		);
	}


	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout("EditorStyleSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FEditorStyleSettingsCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout("StyleColorList", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStyleColorListCustomization::MakeInstance));

	// Background
	USlateThemeManager::Get().SetDefaultColor(EStyleColor::User1, USlateThemeManager::Get().GetColor(EStyleColor::Input));
	USlateThemeManager::Get().SetColorDisplayName(EStyleColor::User1, LOCTEXT("UserColor_OutputLogBackground", "Log Background"));
	// Selection highlight
	USlateThemeManager::Get().SetDefaultColor(EStyleColor::User2, USlateThemeManager::Get().GetColor(EStyleColor::Highlight));
	USlateThemeManager::Get().SetColorDisplayName(EStyleColor::User2, LOCTEXT("UserColor_OutputLogHighlight", "Log Highlight"));
	// Normal
	USlateThemeManager::Get().SetDefaultColor(EStyleColor::User3, USlateThemeManager::Get().GetColor(EStyleColor::Foreground));
	USlateThemeManager::Get().SetColorDisplayName(EStyleColor::User3, LOCTEXT("UserColor_OutputLogText", "Log Text"));
	// Command
	USlateThemeManager::Get().SetDefaultColor(EStyleColor::User4, USlateThemeManager::Get().GetColor(EStyleColor::AccentGreen));
	USlateThemeManager::Get().SetColorDisplayName(EStyleColor::User4, LOCTEXT("UserColor_OutputLogCommand", "Log Command"));
	// Warning
	USlateThemeManager::Get().SetDefaultColor(EStyleColor::User5, USlateThemeManager::Get().GetColor(EStyleColor::AccentYellow));
	USlateThemeManager::Get().SetColorDisplayName(EStyleColor::User5, LOCTEXT("UserColor_OutputLogWarning", "Log Warning"));
	// Error
	USlateThemeManager::Get().SetDefaultColor(EStyleColor::User6, USlateThemeManager::Get().GetColor(EStyleColor::AccentRed));
	USlateThemeManager::Get().SetColorDisplayName(EStyleColor::User6, LOCTEXT("UserColor_OutputLogError", "Log Error"));
#endif

	const FString ThemesSubDir = TEXT("Slate/Themes");

#if ALLOW_THEMES
	USlateThemeManager::Get().ApplyTheme(USlateThemeManager::Get().GetCurrentTheme().Id);
	//UStyleColorTable::Get().SaveCurrentThemeAs(UStyleColorTable::Get().GetCurrentTheme().Filename);
#endif
	StyleInstance = Create(Settings);
	SetStyle(StyleInstance.ToSharedRef());
}

void FStarshipEditorStyle::Shutdown()
{
#if WITH_EDITOR
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "General", "Appearance");
	}

	FPropertyEditorModule* PropertyEditorModule = FModuleManager::Get().GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		PropertyEditorModule->UnregisterCustomClassLayout("EditorStyleSettings");
	}
#endif
	//FEditorStyle::SetStyle()
	StyleInstance.Reset();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

/* FStarshipEditorStyle static initialization
 *****************************************************************************/

TSharedPtr< FStarshipEditorStyle::FStyle > FStarshipEditorStyle::StyleInstance = NULL;
TWeakObjectPtr< UEditorStyleSettings > FStarshipEditorStyle::Settings = NULL;

void FStarshipEditorStyle::FStyle::SetColor(const TSharedRef< FLinearColor >& Source, const FLinearColor& Value)
{
	Source->R = Value.R;
	Source->G = Value.G;
	Source->B = Value.B;
	Source->A = Value.A;
}

bool FStarshipEditorStyle::FStyle::IncludeEditorSpecificStyles()
{
#if IS_PROGRAM
	return true;
#else
	return GIsEditor;
#endif
}

/* FStarshipEditorStyle interface
 *****************************************************************************/

FStarshipEditorStyle::FStyle::FStyle( const TWeakObjectPtr< UEditorStyleSettings >& InSettings )
	: FSlateStyleSet("EditorStyle")

	// Note, these sizes are in Slate Units.
	// Slate Units do NOT have to map to pixels.
	, Icon7x16(7.0f, 16.0f)
	, Icon8x4(8.0f, 4.0f)
	, Icon16x4(16.0f, 4.0f)
	, Icon8x8(8.0f, 8.0f)
	, Icon10x10(10.0f, 10.0f)
	, Icon12x12(12.0f, 12.0f)
	, Icon12x16(12.0f, 16.0f)
	, Icon14x14(14.0f, 14.0f)
	, Icon16x16(16.0f, 16.0f)
	, Icon16x20(16.0f, 20.0f)
	, Icon20x20(20.0f, 20.0f)
	, Icon22x22(22.0f, 22.0f)
	, Icon24x24(24.0f, 24.0f)
	, Icon25x25(25.0f, 25.0f)
	, Icon32x32(32.0f, 32.0f)
	, Icon40x40(40.0f, 40.0f)
	, Icon48x48(48.0f, 48.0f)
	, Icon64x64(64.0f, 64.0f)
	, Icon36x24(36.0f, 24.0f)
	, Icon128x128(128.0f, 128.0f)

	// These are the colors that are updated by the user style customizations
	, SelectionColor_Subdued_LinearRef(MakeShareable(new FLinearColor(0.807f, 0.596f, 0.388f)))
	, HighlightColor_LinearRef( MakeShareable( new FLinearColor(0.068f, 0.068f, 0.068f) ) ) 
	, WindowHighlightColor_LinearRef(MakeShareable(new FLinearColor(0,0,0,0)))



	// These are the Slate colors which reference those above; these are the colors to put into the style
	, SelectionColor_Subdued( SelectionColor_Subdued_LinearRef )
	, HighlightColor( HighlightColor_LinearRef )
	, WindowHighlightColor(WindowHighlightColor_LinearRef)

	, LogColor_Background(EStyleColor::User1)
	, LogColor_SelectionBackground(EStyleColor::User2)
	, LogColor_Normal(EStyleColor::User3)
	, LogColor_Command(EStyleColor::User4)
	, LogColor_Warning(EStyleColor::User5)
	, LogColor_Error(EStyleColor::User6)

	, InheritedFromBlueprintTextColor(FLinearColor(0.25f, 0.5f, 1.0f))

	, Settings( InSettings )
{
}


void FStarshipEditorStyle::FStyle::SettingsChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	if ( ChangedObject == Settings.Get() )
	{
		SyncSettings();
	}
}

void FStarshipEditorStyle::FStyle::SyncSettings()
{
	if (Settings.IsValid())
	{
		// The subdued selection color is derived from the selection color
		auto SubduedSelectionColor = Settings->GetSubduedSelectionColor();
		SetColor(SelectionColor_Subdued_LinearRef, SubduedSelectionColor);

		// Also sync the colors used by FStarshipCoreStyle, as FEditorStyle isn't yet being used as an override everywhere
	/*	FStarshipCoreStyle::SetSelectorColor(Settings->KeyboardFocusColor);
		FStarshipCoreStyle::SetSelectionColor(Settings->SelectionColor);
		FStarshipCoreStyle::SetInactiveSelectionColor(Settings->InactiveSelectionColor);
		FStarshipCoreStyle::SetPressedSelectionColor(Settings->PressedSelectionColor);
*/

		// Sync the window background settings
		FWindowStyle& WindowStyle = const_cast<FWindowStyle&>(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FWindowStyle>("Window"));

		if (Settings->bEnableEditorWindowBackgroundColor)
		{
			SetColor(WindowHighlightColor_LinearRef, Settings->EditorWindowBackgroundColor);
		}
		else
		{
			SetColor(WindowHighlightColor_LinearRef, FLinearColor(0, 0, 0, 0));
		}
	}
}

void FStarshipEditorStyle::FStyle::SyncParentStyles()
{
	const ISlateStyle* ParentStyle = GetParentStyle();

	// Get the scrollbar style from the core style as it is referenced by the editor style
	ScrollBar = ParentStyle->GetWidgetStyle<FScrollBarStyle>("ScrollBar");
	NoBorder = ParentStyle->GetWidgetStyle<FButtonStyle>("NoBorder");
	NormalFont = ParentStyle->GetFontStyle("NormalFont");
	NormalText = ParentStyle->GetWidgetStyle<FTextBlockStyle>("NormalText");
	Button = ParentStyle->GetWidgetStyle<FButtonStyle>("Button");
	NormalEditableTextBoxStyle = ParentStyle->GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	NormalTableRowStyle = ParentStyle->GetWidgetStyle<FTableRowStyle>("TableView.Row");

	DefaultForeground = ParentStyle->GetSlateColor("DefaultForeground");
	InvertedForeground = ParentStyle->GetSlateColor("InvertedForeground");

	SelectorColor = ParentStyle->GetSlateColor("SelectorColor");
	SelectionColor = ParentStyle->GetSlateColor("SelectionColor");
	SelectionColor_Inactive = ParentStyle->GetSlateColor("SelectionColor_Inactive");
	SelectionColor_Pressed = ParentStyle->GetSlateColor("SelectionColor_Pressed");
}

static void AuditDuplicatedCoreStyles(const ISlateStyle& EditorStyle)
{
	const ISlateStyle& CoreStyle = FStarshipCoreStyle::GetCoreStyle();
	TSet<FName> CoreStyleKeys = CoreStyle.GetStyleKeys();

	TSet<FName> EditorStyleKeys = EditorStyle.GetStyleKeys();

	TSet<FName> DuplicatedNames = CoreStyleKeys.Intersect(EditorStyleKeys);

	DuplicatedNames.Sort(FNameLexicalLess());
	for (FName& Name : DuplicatedNames)
	{
		UE_LOG(LogSlate, Log, TEXT("%s"), *Name.ToString());
	}
}

void FStarshipEditorStyle::FStyle::Initialize()
{
	SetParentStyleName("CoreStyle");

	// Sync styles from the parent style that will be used as templates for styles defined here
	SyncParentStyles();


	SetContentRoot( FPaths::EngineContentDir() / TEXT("Editor/Slate") );
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	SetupGeneralStyles();
	SetupLevelGeneralStyles();
	SetupWorldBrowserStyles();
	SetupWorldPartitionStyles();
	SetupSequencerStyles();
	SetupViewportStyles();
	SetupMenuBarStyles();
	SetupGeneralIcons();
	SetupWindowStyles();
	SetupPropertyEditorStyles();

	// Avoid polluting the game texture atlas with non-core editor style items when not the editor (or a standalone application, like UFE)
	if (!IncludeEditorSpecificStyles())
	{
		return;
	}

	SetupProjectBadgeStyle();
	SetupDockingStyles();
	SetupTutorialStyles();
	SetupProfilerStyle();
	SetupGraphEditorStyles();
	SetupLevelEditorStyle();
	SetupPersonaStyle();
	SetupClassThumbnailOverlays();
	SetupClassIconsAndThumbnails();
	SetupContentBrowserStyle();
	SetupLandscapeEditorStyle();
	SetupToolkitStyles();
	SetupTranslationEditorStyles();
	SetupLocalizationDashboardStyles();
	SetupMatineeStyle();
	SetupSourceControlStyles();
	SetupAutomationStyles();
	SetupUMGEditorStyles();
	SetupMyBlueprintStyles();
	SetupStatusBarStyle();

//	LogUnusedBrushResources();

	AuditDuplicatedCoreStyles(*this);
	
	SyncSettings();
}

void FStarshipEditorStyle::FStyle::SetupGeneralStyles()
{
	
	// Normal Text
	{
		Set( "RichTextBlock.TextHighlight", FTextBlockStyle(NormalText)
			.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f ) ) );
		Set( "RichTextBlock.Bold", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize )) );
		Set( "RichTextBlock.BoldHighlight", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize ))
			.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f ) ) );
		Set("RichTextBlock.Italic", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Italic", FStarshipCoreStyle::RegularTextSize)));
		Set("RichTextBlock.ItalicHighlight", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Italic", FStarshipCoreStyle::RegularTextSize))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f)));

		Set( "TextBlock.HighlightShape",  new BOX_BRUSH( "Common/TextBlockHighlightShape", FMargin(3.f/8.f) ));
		Set( "TextBlock.HighlighColor", FLinearColor( 0.02f, 0.3f, 0.0f ) );

		Set("TextBlock.ShadowedText", FTextBlockStyle(NormalText)
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f)));

		Set("TextBlock.ShadowedTextWarning", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FStyleColors::Warning)
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f)));

		Set("NormalText.Subdued", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground()));

		Set("NormalText.Important", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		Set("SmallText.Subdued", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", FStarshipCoreStyle::SmallTextSize))
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground()));

		Set("TinyText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", FStarshipCoreStyle::SmallTextSize)));

		Set("TinyText.Subdued", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", FStarshipCoreStyle::SmallTextSize))
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground()));

		Set("LargeText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 11))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));
	}

	// Rendering resources that never change
	{
		Set( "None", new FSlateNoResource() );
	}


	Set( "PlainBorder", new BORDER_BRUSH( "Common/PlainBorder", 2.f/8.f) );

	Set( "WideDash.Horizontal", new IMAGE_BRUSH("Common/WideDash_Horizontal", FVector2D(22, 4), FLinearColor::White, ESlateBrushTileType::Horizontal));
	Set( "WideDash.Vertical", new IMAGE_BRUSH("Common/WideDash_Vertical", FVector2D(4, 22), FLinearColor::White, ESlateBrushTileType::Vertical));

	Set("ThinLine.Horizontal", new IMAGE_BRUSH("Common/ThinLine_Horizontal", FVector2D(11, 2), FLinearColor::White, ESlateBrushTileType::Horizontal));


	// Buttons that only provide a hover hint.
	HoverHintOnly = FButtonStyle()
			.SetNormal( FSlateNoResource() )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,0.15f) ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,0.25f) ) )
			.SetNormalPadding( FMargin(0,0,0,1) )
			.SetPressedPadding( FMargin(0,1,0,0) );
	Set( "HoverHintOnly", HoverHintOnly );


	FButtonStyle SimpleSharpButton = FButtonStyle()
		.SetNormal(BOX_BRUSH("Common/Button/simple_sharp_normal", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 1)))
		.SetHovered(BOX_BRUSH("Common/Button/simple_sharp_hovered", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 1)))
		.SetPressed(BOX_BRUSH("Common/Button/simple_sharp_hovered", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 1)))
		.SetNormalPadding(FMargin(0, 0, 0, 1))
		.SetPressedPadding(FMargin(0, 1, 0, 0));
	Set("SimpleSharpButton", SimpleSharpButton);

	FButtonStyle SimpleRoundButton = FButtonStyle()
		.SetNormal(BOX_BRUSH("Common/Button/simple_round_normal", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 1)))
		.SetHovered(BOX_BRUSH("Common/Button/simple_round_hovered", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 1)))
		.SetPressed(BOX_BRUSH("Common/Button/simple_round_hovered", FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 1)))
		.SetNormalPadding(FMargin(0, 0, 0, 1))
		.SetPressedPadding(FMargin(0, 1, 0, 0));
	Set("SimpleRoundButton", SimpleRoundButton);

	// Common glyphs
	{
		Set( "Symbols.SearchGlass", new IMAGE_BRUSH( "Common/SearchGlass", Icon16x16 ) );
		Set( "Symbols.X", new IMAGE_BRUSH( "Common/X", Icon16x16 ) );
		Set( "Symbols.VerticalPipe", new BOX_BRUSH( "Common/VerticalPipe", FMargin(0) ) );
		Set( "Symbols.UpArrow", new IMAGE_BRUSH( "Common/UpArrow", Icon8x8 ) );
		Set( "Symbols.DoubleUpArrow", new IMAGE_BRUSH( "Common/UpArrow2", Icon8x8 ) );
		Set( "Symbols.DownArrow", new IMAGE_BRUSH( "Common/DownArrow", Icon8x8 ) );
		Set( "Symbols.DoubleDownArrow", new IMAGE_BRUSH( "Common/DownArrow2", Icon8x8 ) );
		Set( "Symbols.RightArrow", new IMAGE_BRUSH("Common/SubmenuArrow", Icon8x8));
		Set( "Symbols.Check", new IMAGE_BRUSH( "Common/Check", Icon16x16 ) );
	}

	// Common icons
	{
		Set( "Icons.Contact", new IMAGE_BRUSH( "Icons/icon_mail_16x", Icon16x16 ) );

		Set( "Icons.DirtyBadge", new IMAGE_BRUSH_SVG("Starship/Common/DirtyBadge", Icon12x12));
	}

	Set( "WarningStripe", new IMAGE_BRUSH( "Common/WarningStripe", FVector2D(20,6), FLinearColor::White, ESlateBrushTileType::Horizontal ) );
	
	Set("RoundedWarning", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Warning, 1.0f));
	Set("RoundedError", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Error, 1.0f));

	Set( "Button.Disabled", new BOX_BRUSH( "Common/Button_Disabled", 8.0f/32.0f ) );
	

	// Toggle button
	{
		Set( "ToggleButton", FButtonStyle(Button)
			.SetNormal(FSlateNoResource())
			.SetHovered(BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor ))
			.SetPressed(BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor_Pressed ))
		);

		//FSlateColorBrush(FLinearColor::White)

		Set("RoundButton", FButtonStyle(Button)
			.SetNormal(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, FLinearColor(1, 1, 1, 0.1f)))
			.SetHovered(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor))
			.SetPressed(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor_Pressed))
			);

		Set("FlatButton", FButtonStyle(Button)
			.SetNormal(FSlateNoResource())
			.SetHovered(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, SelectionColor))
			.SetPressed(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, SelectionColor_Pressed))
			);

		Set("FlatButton.Dark", FButtonStyle(Button)
			.SetNormal(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0.125f, 0.125f, 0.125f, 0.8f)))
			.SetHovered(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, SelectionColor))
			.SetPressed(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, SelectionColor_Pressed))
			);

		Set("FlatButton.DarkGrey", FButtonStyle(Button)
			.SetNormal(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0.05f, 0.05f, 0.05f, 0.8f)))
			.SetHovered(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, SelectionColor))
			.SetPressed(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, SelectionColor_Pressed))
		);


		Set("FlatButton.Default", GetWidgetStyle<FButtonStyle>("FlatButton.Dark"));

		Set("FlatButton.DefaultTextStyle", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		struct ButtonColor
		{
		public:
			FName Name;
			FLinearColor Normal;
			FLinearColor Hovered;
			FLinearColor Pressed;

			ButtonColor(const FName& InName, const FLinearColor& Color) : Name(InName)
			{
				Normal = Color * 0.8f;
				Normal.A = Color.A;
				Hovered = Color * 1.0f;
				Hovered.A = Color.A;
				Pressed = Color * 0.6f;
				Pressed.A = Color.A;
			}
		};

		TArray< ButtonColor > FlatButtons;
		FlatButtons.Add(ButtonColor("FlatButton.Primary", FLinearColor(0.02899, 0.19752, 0.48195)));
		FlatButtons.Add(ButtonColor("FlatButton.Success", FLinearColor(0.10616, 0.48777, 0.10616)));
		FlatButtons.Add(ButtonColor("FlatButton.Info", FLinearColor(0.10363, 0.53564, 0.7372)));
		FlatButtons.Add(ButtonColor("FlatButton.Warning", FLinearColor(0.87514, 0.42591, 0.07383)));
		FlatButtons.Add(ButtonColor("FlatButton.Danger", FLinearColor(0.70117, 0.08464, 0.07593)));

		for ( const ButtonColor& Entry : FlatButtons )
		{
			Set(Entry.Name, FButtonStyle(Button)
				.SetNormal(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, Entry.Normal))
				.SetHovered(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, Entry.Hovered))
				.SetPressed(BOX_BRUSH("Common/FlatButton", 2.0f / 8.0f, Entry.Pressed))
				);
		}

		Set("FontAwesome.7", ICON_FONT(7));
		Set("FontAwesome.8", ICON_FONT(8));
		Set("FontAwesome.9", ICON_FONT(9));
		Set("FontAwesome.10", ICON_FONT(10));
		Set("FontAwesome.11", ICON_FONT(11));
		Set("FontAwesome.12", ICON_FONT(12));
		Set("FontAwesome.14", ICON_FONT(14));
		Set("FontAwesome.16", ICON_FONT(16));
		Set("FontAwesome.18", ICON_FONT(18));

		/* Create a checkbox style for "ToggleButton" but with the images used by a normal checkbox (see "Checkbox" below) ... */
		const FCheckBoxStyle CheckboxLookingToggleButtonStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage( IMAGE_BRUSH( "Common/CheckBox", Icon16x16 ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/CheckBox", Icon16x16 ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/CheckBox_Hovered", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/CheckBox_Checked_Hovered", Icon16x16 ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/CheckBox_Checked_Hovered", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/CheckBox_Checked", Icon16x16 ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined", Icon16x16 ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined_Hovered", Icon16x16 ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined_Hovered", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetPadding(1.0f);
		/* ... and set new style */
		Set( "CheckboxLookToggleButtonCheckbox", CheckboxLookingToggleButtonStyle );


		Set( "ToggleButton.LabelFont", DEFAULT_FONT( "Regular", 9 ) );
		Set( "ToggleButtonCheckbox.LabelFont", DEFAULT_FONT( "Regular", 9 ) );
	}

	// Combo Button, Combo Box
	{
		// Legacy style; still being used by some editor widgets
		Set( "ComboButton.Arrow", new IMAGE_BRUSH("Common/ComboArrow", Icon8x8 ) );


		FComboButtonStyle ToolbarComboButton = FComboButtonStyle()
			.SetButtonStyle( GetWidgetStyle<FButtonStyle>( "ToggleButton" ) )
			.SetDownArrowImage( IMAGE_BRUSH( "Common/ShadowComboArrow", Icon8x8 ) )
			.SetMenuBorderBrush(FSlateNoResource())
			.SetMenuBorderPadding( FMargin( 0.0f ) );
		Set( "ToolbarComboButton", ToolbarComboButton );

		Set("GenericFilters.ComboButtonStyle", ToolbarComboButton);

		Set("GenericFilters.TextStyle", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 9))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

	}

	// Help button
	Set( "HelpButton", FButtonStyle(Button)
		.SetNormal( FSlateNoResource() )
		.SetHovered(FSlateNoResource() )
		.SetPressed(FSlateNoResource() )
	);

	Set( "HelpIcon", new IMAGE_BRUSH( "Common/icon_Help_Default_16x", Icon16x16 ) );
	Set( "HelpIcon.Hovered", new IMAGE_BRUSH( "Common/icon_Help_Hover_16x", Icon16x16 ) );
	Set( "HelpIcon.Pressed", new IMAGE_BRUSH( "Common/icon_Help_Pressed_16x", Icon16x16 ) );

	// Error Reporting
	{
		Set( "InfoReporting.BackgroundColor", FLinearColor(0.1f, 0.33f, 1.0f));

	}

	// EditableTextBox
	{
		Set( "EditableTextBox.Background.Normal", new BOX_BRUSH( "Common/TextBox", FMargin(4.0f/16.0f) ) );
		Set( "EditableTextBox.Background.Hovered", new BOX_BRUSH( "Common/TextBox_Hovered", FMargin(4.0f/16.0f) ) );
		Set( "EditableTextBox.Background.Focused", new BOX_BRUSH( "Common/TextBox_Hovered", FMargin(4.0f/16.0f) ) );
		Set( "EditableTextBox.Background.ReadOnly", new BOX_BRUSH( "Common/TextBox_ReadOnly", FMargin(4.0f/16.0f) ) );
		Set( "EditableTextBox.BorderPadding", FMargin(4.0f, 2.0f) );
	}

	// EditableTextBox Special
	{
		FSlateBrush* SpecialEditableTextImageNormal = new BOX_BRUSH( "Common/TextBox_Special", FMargin(8.0f/32.0f) );
		Set( "SpecialEditableTextImageNormal", SpecialEditableTextImageNormal );

		const FEditableTextBoxStyle SpecialEditableTextBoxStyle = FEditableTextBoxStyle()
			.SetBackgroundImageNormal( *SpecialEditableTextImageNormal )
			.SetBackgroundImageHovered( BOX_BRUSH( "Common/TextBox_Special_Hovered", FMargin(8.0f/32.0f) ) )
			.SetBackgroundImageFocused( BOX_BRUSH( "Common/TextBox_Special_Hovered", FMargin(8.0f/32.0f) ) )
			.SetBackgroundImageReadOnly( BOX_BRUSH( "Common/TextBox_ReadOnly", FMargin(4.0f/16.0f) ) )
			.SetScrollBarStyle( ScrollBar );
		Set( "SpecialEditableTextBox", SpecialEditableTextBoxStyle );

		Set( "SearchBox.ActiveBorder", new BOX_BRUSH( "Common/TextBox_Special_Active", FMargin(8.0f/32.0f) ) );
	}

	// Filtering/Searching feedback
	{
		const FLinearColor ActiveFilterColor = FLinearColor(1.0f,0.55f,0.0f,1.0f);
		Set("Searching.SearchActiveTab",    new FSlateNoResource());
		Set("Searching.SearchActiveBorder", new FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0, FStyleColors::Primary, 1.f));
	}



	// Images sizes are specified in Slate Screen Units. These do not necessarily correspond to pixels!
	// An IMAGE_BRUSH( "SomeImage", FVector2D(32,32)) will have a desired size of 16x16 Slate Screen Units.
	// This allows the original resource to be scaled up or down as needed.

	Set( "WhiteTexture", new IMAGE_BRUSH( "Old/White", Icon16x16 ) );

	Set( "BoldFont", DEFAULT_FONT( "Bold", FStarshipCoreStyle::RegularTextSize ) );

	Set( "Editor.AppIcon", new IMAGE_BRUSH( "Icons/EditorAppIcon", Icon24x24) );

	Set( "MarqueeSelection", new BORDER_BRUSH( "Old/DashedBorder", FMargin(6.0f/32.0f) ) );

	Set( "GenericPlay", new IMAGE_BRUSH( "Icons/generic_play_16x", Icon16x16 ) );
	Set( "GenericPause", new IMAGE_BRUSH( "Icons/generic_pause_16x", Icon16x16 ) );
	Set( "GenericStop", new IMAGE_BRUSH( "Icons/generic_stop_16x", Icon16x16 ) );

	Set( "SoftwareCursor_Grab", new IMAGE_BRUSH( "Icons/cursor_grab", Icon16x16 ) );
	Set( "SoftwareCursor_CardinalCross", new IMAGE_BRUSH( "Icons/cursor_cardinal_cross", Icon24x24 ) );
	Set( "SoftwareCursor_UpDown", new IMAGE_BRUSH( "Icons/cursor_updown", Icon16x20 ) );

	Set( "Border", new BOX_BRUSH( "Old/Border", 4.0f/16.0f ) );
	
	Set( "NoteBorder", new BOX_BRUSH( "Old/NoteBorder", FMargin(15.0f/40.0f, 15.0f/40.0f) ) );
	
	Set( "FilledBorder", new BOX_BRUSH( "Old/FilledBorder", 4.0f/16.0f ) );

	Set( "GenericViewButton", new IMAGE_BRUSH( "Icons/view_button", Icon20x20 ) );

	Set("GenericLink", new IMAGE_BRUSH("Common/link", Icon16x16));

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		// Dark Hyperlink - for use on light backgrounds
		FButtonStyle DarkHyperlinkButton = FButtonStyle()
			.SetNormal ( BORDER_BRUSH( "Old/HyperlinkDotted", FMargin(0,0,0,3/16.0f), FLinearColor::Black ) )
			.SetPressed( FSlateNoResource() )
			.SetHovered( BORDER_BRUSH( "Old/HyperlinkUnderline", FMargin(0,0,0,3/16.0f), FLinearColor::Black ) );
		FHyperlinkStyle DarkHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(DarkHyperlinkButton)
			.SetTextStyle(NormalText)
			.SetPadding(FMargin(0.0f));
		Set("DarkHyperlink", DarkHyperlink);

		// Visible on hover hyper link
		FButtonStyle HoverOnlyHyperlinkButton = FButtonStyle()
			.SetNormal(FSlateNoResource() )
			.SetPressed(FSlateNoResource() )
			.SetHovered(BORDER_BRUSH( "Old/HyperlinkUnderline", FMargin(0,0,0,3/16.0f) ) );
		Set("HoverOnlyHyperlinkButton", HoverOnlyHyperlinkButton);

		FHyperlinkStyle HoverOnlyHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(HoverOnlyHyperlinkButton)
			.SetTextStyle(NormalText)
			.SetPadding(FMargin(0.0f));
		Set("HoverOnlyHyperlink", HoverOnlyHyperlink);
	}

	FButtonStyle RoundedButtonStyle = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FStyleColors::Dropdown, 4))
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Hover, 4))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Hover, 4))
		.SetNormalForeground(FStyleColors::Foreground)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::White25)
		.SetNormalPadding(FMargin(6, 4))
		.SetPressedPadding(FMargin(6, 4));

	Set("RoundedButton", RoundedButtonStyle);

	FComboButtonStyle AddNewComboButtonStyle = FComboButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
		.SetButtonStyle(RoundedButtonStyle) 
		.SetDownArrowImage(FSlateNoResource());

	Set("RoundedComboButton", AddNewComboButtonStyle);

#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Expandable button
	{
		Set( "ExpandableButton.Collapsed", new IMAGE_BRUSH( "Old/ExpansionButton_Collapsed", Icon32x32) );
		Set( "ExpandableButton.Expanded_Left", new IMAGE_BRUSH( "Old/ExpansionButton_ExpandedLeft", Icon32x32) );
		Set( "ExpandableButton.Expanded_Center", new IMAGE_BRUSH( "Old/ExpansionButton_ExpandedMiddle", Icon32x32) );
		Set( "ExpandableButton.Expanded_Right", new IMAGE_BRUSH( "Old/ExpansionButton_ExpandedRight", Icon32x32) );
	}

	// Content reference
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set( "ContentReference.Background.Normal", new BOX_BRUSH( "Common/TextBox", FMargin(4.0f/16.0f) ) );
		Set( "ContentReference.Background.Hovered", new BOX_BRUSH( "Common/TextBox_Hovered", FMargin(4.0f/16.0f) ) );
		Set( "ContentReference.BorderPadding", FMargin(4.0f, 2.0f) );
		Set( "ContentReference.FindInContentBrowser", new IMAGE_BRUSH( "Icons/lens_12x", Icon12x12 ) );
		Set( "ContentReference.UseSelectionFromContentBrowser", new IMAGE_BRUSH( "Icons/assign_12x", Icon12x12 ) );
		Set( "ContentReference.PickAsset", new IMAGE_BRUSH( "Icons/pillarray_16x", Icon12x12 ) );
		Set( "ContentReference.Clear", new IMAGE_BRUSH( "Icons/Cross_12x", Icon12x12 ) );
		Set( "ContentReference.Tools", new IMAGE_BRUSH( "Icons/wrench_16x", Icon12x12 ) );
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	{
		Set("SystemWideCommands.FindInContentBrowser", new IMAGE_BRUSH_SVG("Starship/Common/ContentBrowser", Icon20x20));
		Set("SystemWideCommands.FindInContentBrowser.Small", new IMAGE_BRUSH_SVG("Starship/Common/ContentBrowser", Icon16x16));
	}
	

	// PList Editor
	{
		Set( "PListEditor.HeaderRow.Background",				new BOX_BRUSH( "Common/TableViewHeader", 4.f/32.f ) );

		Set( "PListEditor.FilteredColor",						new FSlateColorBrush( FColor( 0, 255, 0, 80 ) ) );
		Set( "PListEditor.NoOverlayColor",						new FSlateNoResource() );

		Set( "PListEditor.Button_AddToArray",					new IMAGE_BRUSH( "Icons/PlusSymbol_12x", Icon12x12 ) );
	}
	
	// Material List
	{
		Set( "MaterialList.DragDropBorder", new BOX_BRUSH( "Old/Window/ViewportDebugBorder", 0.8f ) );
		Set( "MaterialList.HyperlinkStyle", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 8 ) ) );
		Set( "MaterialList.HyperlinkStyle.ShadowOffset", FVector2D::ZeroVector );
	}

	// Dialogue Wave Details
	{
		Set( "DialogueWaveDetails.SpeakerToTarget", new IMAGE_BRUSH( "PropertyView/SpeakerToTarget", FVector2D(30.0f, 30.0f) ) );
		Set( "DialogueWaveDetails.HeaderBorder", new BOX_BRUSH( "Common/MenuBarBorder", FMargin(4.0f/16.0f) ) );
		Set( "DialogueWaveDetails.PropertyEditorMenu", new BOX_BRUSH( "Old/Menu_Background", FMargin(8.0f/64.0f) ) );
	}

	// Dialogue Wave Parameter Border
	{
		Set( "DialogueWaveParameter.DropDownBorder", new BOX_BRUSH( "Old/Border", 4.0f/16.0f, FLinearColor::Black) );
	}

#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	Set( "DashedBorder", new BORDER_BRUSH( "Old/DashedBorder", FMargin(6.0f/32.0f) ) );

	Set( "UniformShadow", new BORDER_BRUSH( "Common/UniformShadow", FMargin( 16.0f / 64.0f ) ) );
	Set( "UniformShadow_Tint", new BORDER_BRUSH( "Common/UniformShadow_Tint", FMargin( 16.0f / 64.0f ) ) );

	// Splitter
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set ("SplitterDark", FSplitterStyle()
			.SetHandleNormalBrush( FSlateColorBrush( FLinearColor(FColor( 32, 32, 32) ) ) )
			.SetHandleHighlightBrush( FSlateColorBrush( FLinearColor(FColor( 96, 96, 96) ) ) ) 
			);
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)


	// Lists, Trees
	{

		const FTableViewStyle DefaultTreeViewStyle = FTableViewStyle()
			.SetBackgroundBrush(FSlateColorBrush(FStyleColors::Recessed));
		Set("ListView", DefaultTreeViewStyle);

		const FTableViewStyle DefaultTableViewStyle = FTableViewStyle()
			.SetBackgroundBrush(FSlateColorBrush(FStyleColors::Recessed));
		Set("TreeView", DefaultTableViewStyle);

		Set( "TableView.Row", FTableRowStyle( NormalTableRowStyle) );
		Set( "TableView.DarkRow",FTableRowStyle( NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle", FVector2D(16, 16)))
			.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle_Hovered", FVector2D(16, 16)))
			.SetOddRowBackgroundBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle", FVector2D(16, 16)))
			.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle_Hovered", FVector2D(16, 16)))
			.SetSelectorFocusedBrush(BORDER_BRUSH("Common/Selector", FMargin(4.f / 16.f), SelectorColor))
			.SetActiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
			.SetActiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
			.SetInactiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
			.SetInactiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
		);
		Set("TableView.NoHoverTableRow", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())
			.SetOddRowBackgroundHoveredBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())
			.SetInactiveHoveredBrush(FSlateNoResource())
			);
	}
	
	// Spinboxes
	{

		// Legacy styles; used by other editor widgets
		Set( "SpinBox.Background", new BOX_BRUSH( "Common/Spinbox", FMargin(4.0f/16.0f) ) );
		Set( "SpinBox.Background.Hovered", new BOX_BRUSH( "Common/Spinbox_Hovered", FMargin(4.0f/16.0f) ) );
		Set( "SpinBox.Fill", new BOX_BRUSH( "Common/Spinbox_Fill", FMargin(4.0f/16.0f, 4.0f/16.0f, 8.0f/16.0f, 4.0f/16.0f) ) );
		Set( "SpinBox.Fill.Hovered", new BOX_BRUSH( "Common/Spinbox_Fill_Hovered", FMargin(4.0f/16.0f) ) );
		Set( "SpinBox.Arrows", new IMAGE_BRUSH( "Common/SpinArrows", Icon12x12 ) );
		Set( "SpinBox.TextMargin", FMargin(1.0f,2.0f) );
	}

	// Throbber
	{
		Set( "SmallThrobber.Chunk", new IMAGE_BRUSH( "Common/ThrobberPiece_Small", FVector2D(8,16) ) );
	}

	{
		Set("CurveEd.TimelineArea", new IMAGE_BRUSH("Old/White", Icon16x16, FLinearColor(1, 1, 1, 0.25f)));
		Set("CurveEd.FitHorizontal", new IMAGE_BRUSH("Icons/FitHorz_16x", Icon16x16));
		Set("CurveEd.FitVertical", new IMAGE_BRUSH("Icons/FitVert_16x", Icon16x16));
		Set("CurveEd.CurveKey", new IMAGE_BRUSH("Common/Key", FVector2D(11.0f, 11.0f)));
		Set("CurveEd.CurveKeySelected", new IMAGE_BRUSH("Common/Key", FVector2D(11.0f, 11.0f), SelectionColor));
		Set("CurveEd.InfoFont", DEFAULT_FONT("Regular", 8));
		Set("CurveEd.LabelFont", DEFAULT_FONT("Bold", 10));
		Set("CurveEd.Tangent", new IMAGE_BRUSH("Common/Tangent", FVector2D(7.0f, 7.0f), FLinearColor(0.0f, 0.66f, 0.7f)));
		Set("CurveEd.TangentSelected", new IMAGE_BRUSH("Common/Tangent", FVector2D(7.0f, 7.0f), FLinearColor(1.0f, 1.0f, 0.0f)));
		Set("CurveEd.TangentColor", FLinearColor(0.0f, 0.66f, 0.7f));
		Set("CurveEd.TangentColorSelected", FLinearColor(1.0f, 1.0f, 0.0f));
	}
	
	// Scrub control buttons
	{
		Set( "Animation.Pause", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Pause_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Pause_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Pause_24x", Icon24x24 ))
		);

		Set( "Animation.Forward", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Play_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Play_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Play_24x", Icon24x24 ))
		);

		Set( "Animation.Forward_Step", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Step_Forward_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Step_Forward_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Step_Forward_24x", Icon24x24 ))
		);

		Set( "Animation.Forward_End", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Go_To_End_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Go_To_End_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Go_To_End_24x", Icon24x24 ))
		);

		Set( "Animation.Backward", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Backwards_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Backwards_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Backwards_24x", Icon24x24 ))
		);

		Set( "Animation.Backward_Step", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Step_Backwards_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Step_Backwards_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Step_Backwards_24x", Icon24x24 ))
		);

		Set( "Animation.Backward_End", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Go_To_Front_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Go_To_Front_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Go_To_Front_24x", Icon24x24 ))
		);

		Set( "Animation.Loop.Enabled", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_24x", Icon24x24 ))
		);

		Set( "Animation.Loop.Disabled", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_Toggle_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_Toggle_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_Toggle_24x", Icon24x24 ))
		);

		Set( "Animation.Loop.SelectionRange", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_SelectionRange_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_SelectionRange_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Loop_SelectionRange_24x", Icon24x24 ))
		);

		Set( "Animation.Record", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Record_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Record_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Record_24x", Icon24x24 ))
		);

		Set( "Animation.Recording", FButtonStyle( Button )
			.SetNormal(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Recording_24x_OFF", Icon24x24 ))
			.SetHovered(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Recording_24x_OFF", Icon24x24 ))
			.SetPressed(IMAGE_BRUSH( "/Sequencer/Transport_Bar/Recording_24x", Icon24x24 ))
		);
	}

	// Message Log
	{
		
		Set( "MessageLog.Action", new IMAGE_BRUSH( "Icons/icon_file_choosepackages_16px", Icon16x16) );
		Set( "MessageLog.Docs", new IMAGE_BRUSH( "Icons/icon_Docs_16x", Icon16x16) );
		Set( "MessageLog.Tutorial", new IMAGE_BRUSH( "Icons/icon_Blueprint_Enum_16x", Icon16x16 ) );
		Set( "MessageLog.Url", new IMAGE_BRUSH( "Icons/icon_world_16x", Icon16x16 ) );

		Set( "MessageLog.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/MessageLog", Icon16x16 ) );
		Set( "MessageLog.ListBorder", new BOX_BRUSH( "/Docking/AppTabContentArea", FMargin(4/16.0f) ) );
	}

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Animation tools
	if (IncludeEditorSpecificStyles())
	{
		Set( "AnimEditor.RefreshButton", new IMAGE_BRUSH( "Old/AnimEditor/RefreshButton", Icon16x16 ) );
		Set( "AnimEditor.VisibleEye", new IMAGE_BRUSH( "Old/AnimEditor/RefreshButton", Icon16x16 ) );
		Set( "AnimEditor.InvisibleEye", new IMAGE_BRUSH( "Old/AnimEditor/RefreshButton", Icon16x16 ) );
		Set( "AnimEditor.FilterSearch", new IMAGE_BRUSH( "Old/FilterSearch", Icon16x16 ) );
		Set( "AnimEditor.FilterCancel", new IMAGE_BRUSH( "Old/FilterCancel", Icon16x16 ) );

		Set( "AnimEditor.NotifyGraphBackground", new IMAGE_BRUSH( "Old/AnimEditor/NotifyTrackBackground", FVector2D(64, 64), FLinearColor::White, ESlateBrushTileType::Both) );

		Set( "BlendSpace.SamplePoint", new IMAGE_BRUSH( "Old/AnimEditor/BlendSpace_Sample", Icon16x16 ) );
		Set( "BlendSpace.SamplePoint_Highlight", new IMAGE_BRUSH( "Old/AnimEditor/BlendSpace_Sample_Highlight", Icon16x16 ) );
		Set( "BlendSpace.SamplePoint_Invalid", new IMAGE_BRUSH( "Old/AnimEditor/BlendSpace_Sample_Invalid", Icon16x16 ) );
		Set( "BlendSpace.Graph", new IMAGE_BRUSH_SVG("Starship/Animation/BlendSpace", Icon16x16));
		Set( "BlendSpace.SampleGraph", new IMAGE_BRUSH_SVG("Starship/Animation/BlendSpace", Icon16x16));

		Set( "AnimEditor.EditPreviewParameters", new IMAGE_BRUSH( "Icons/icon_adjust_parameters_40x", Icon40x40) );		
		Set( "AnimEditor.EditPreviewParameters.Small", new IMAGE_BRUSH( "Icons/icon_adjust_parameters_40x", Icon20x20) );		
	}

#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Output Log Window
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		const int32 LogFontSize = Settings.IsValid() ? Settings->LogFontSize : 9;

		const FTextBlockStyle NormalLogText = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Mono", LogFontSize))
			.SetColorAndOpacity(LogColor_Normal)
			.SetSelectedBackgroundColor(LogColor_SelectionBackground)
			.SetHighlightColor(FStyleColors::Black);

		Set("Log.Normal", NormalLogText );

		Set("Log.Command", FTextBlockStyle(NormalLogText)
			.SetColorAndOpacity( LogColor_Command )
			);

		Set("Log.Warning", FTextBlockStyle(NormalLogText)
			.SetColorAndOpacity( LogColor_Warning )
			);

		Set("Log.Error", FTextBlockStyle(NormalLogText)
			.SetColorAndOpacity( LogColor_Error )
			);

		Set("Log.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/OutputLog", Icon16x16 ) );

		Set("Log.TextBox", FEditableTextBoxStyle(NormalEditableTextBoxStyle)
			.SetBackgroundImageNormal( BOX_BRUSH( "Common/WhiteGroupBorder", FMargin(4.0f/16.0f) ) )
			.SetBackgroundImageHovered( BOX_BRUSH( "Common/WhiteGroupBorder", FMargin(4.0f/16.0f) ) )
			.SetBackgroundImageFocused( BOX_BRUSH( "Common/WhiteGroupBorder", FMargin(4.0f/16.0f) ) )
			.SetBackgroundImageReadOnly( BOX_BRUSH( "Common/WhiteGroupBorder", FMargin(4.0f/16.0f) ) )
			.SetBackgroundColor( LogColor_Background )
			);

		Set("DebugConsole.Background", new FSlateNoResource());

		const FButtonStyle DebugConsoleButton = FButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.SetNormalForeground(FStyleColors::Foreground)
			.SetNormalPadding(FMargin(2, 2, 2, 2))
			.SetPressedPadding(FMargin(2, 3, 2, 1));

		const FComboButtonStyle DebugConsoleComboButton = FComboButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
			.SetDownArrowImage(FSlateNoResource())
			.SetButtonStyle(DebugConsoleButton);

		Set("DebugConsole.ComboButton", DebugConsoleComboButton);

		Set("DebugConsole.Icon", new IMAGE_BRUSH_SVG("Starship/Common/Console", Icon16x16));

		Set("OutputLog.OpenSourceLocation", new IMAGE_BRUSH("Icons/icon_Asset_Open_Source_Location_16x", Icon16x16));
		Set("OutputLog.OpenInExternalEditor", new IMAGE_BRUSH("Icons/icon_Asset_Open_In_External_Editor_16x", Icon16x16));

	}
	// Debugging tools 
	{
		Set("PerfTools.TabIcon", new IMAGE_BRUSH( "Icons/icon_tab_PerfTools_16x", Icon16x16 ) );
		Set("ClassViewer.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/Class", Icon16x16 ) );
		Set("StructViewer.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/UserDefinedStruct", Icon16x16 ) );
		Set("BlueprintDebugger.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/BlueprintDebugger", Icon16x16 ) );
		Set("CollisionAnalyzer.TabIcon", new IMAGE_BRUSH_SVG("Starship/Common/Collision", Icon16x16));
		Set("ObjectBrowser.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/ObjectsBrowser", Icon16x16 ) );
		Set("PixelInspector.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/PixelInspector", Icon16x16 ) );
		Set("StylusInputDebug.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/StylusInputBug", Icon16x16 ) );
	}

	{
		Set("DeveloperTools.MenuIcon", new IMAGE_BRUSH_SVG( "Starship/Common/DeveloperTools", Icon16x16 ) );
	}

	// Automation Tools Menu
	{
		Set("AutomationTools.MenuIcon", new IMAGE_BRUSH_SVG("Starship/Common/AutomationTools", Icon16x16));
		Set("AutomationTools.TestAutomation", new IMAGE_BRUSH_SVG("Starship/Common/TestAutomation", Icon16x16));
	}

	// Session Browser tab
	{
		Set("SessionBrowser.SessionLocked", new IMAGE_BRUSH( "Icons/icon_locked_highlight_16px", Icon16x16 ) );
		Set("SessionBrowser.StatusRunning", new IMAGE_BRUSH( "Icons/icon_status_green_16x", Icon16x16 ) );
		Set("SessionBrowser.StatusTimedOut", new IMAGE_BRUSH( "Icons/icon_status_grey_16x", Icon16x16 ) );
		Set("SessionBrowser.Terminate", new IMAGE_BRUSH( "Icons/icon_DevicePowerOff_40x", Icon20x20 ) );

		Set("SessionBrowser.Terminate.Font", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT( "Bold", 12))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f)));
	}

	// Session Console tab
	{
		Set( "SessionConsole.SessionCopy", new IMAGE_BRUSH( "Icons/icon_file_open_40x", Icon40x40 ) );
		Set( "SessionConsole.SessionCopy.Small", new IMAGE_BRUSH( "Icons/icon_file_open_16px", Icon20x20 ) );
		Set( "SessionConsole.Clear", new IMAGE_BRUSH( "Icons/icon_file_new_40x", Icon40x40 ) );
		Set( "SessionConsole.Clear.Small", new IMAGE_BRUSH( "Icons/icon_file_new_16px", Icon20x20 ) );
		Set( "SessionConsole.SessionSave", new IMAGE_BRUSH( "Icons/icon_file_savelevels_40x", Icon40x40 ) );
		Set( "SessionConsole.SessionSave.Small", new IMAGE_BRUSH( "Icons/icon_file_savelevels_16px", Icon20x20 ) );
	}

	// Session Frontend Window
	{
		Set("SessionFrontEnd.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/SessionFrontend", Icon16x16 ) );
		Set("SessionFrontEnd.Tabs.Tools", new IMAGE_BRUSH( "/Icons/icon_tab_Tools_16x", Icon16x16 ) );
	}

	// Launcher Window
	{
		Set("Launcher.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/ProjectLauncher", Icon16x16 ) );
		Set("Launcher.Tabs.Tools", new IMAGE_BRUSH( "/Icons/icon_tab_Tools_16x", Icon16x16 ) );
	}

	// Undo History Window
	{
		Set("UndoHistory.TabIcon", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/UndoHistory", Icon16x16 ) );
	}

	// InputBinding editor
	{
		Set( "InputBindingEditor.ContextFont", DEFAULT_FONT( "Bold", 9 ) );
		Set( "InputBindingEditor.ContextBorder", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, FLinearColor(0.5,0.5,0.5,1.0) ) );
		Set( "InputBindingEditor.SmallFont", DEFAULT_FONT( "Regular", 8 ) );

		Set( "InputBindingEditor.HeaderButton", FButtonStyle(NoBorder)
			.SetNormalPadding(FMargin( 1,1,2,2 ))
			.SetPressedPadding(FMargin( 2,2,2,2 )) );

		Set( "InputBindingEditor.HeaderButton.Disabled", new FSlateNoResource() );


		Set( "InputBindingEditor.Tab",  new IMAGE_BRUSH( "Icons/icon_tab_KeyBindings_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.AssetEditor",  new IMAGE_BRUSH( "Icons/icon_keyb_AssetEditor_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.AssetEditor",  new IMAGE_BRUSH( "Icons/icon_keyb_AssetEditor_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.GenericCommands",  new IMAGE_BRUSH( "Icons/icon_keyb_CommonCommands_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.FoliageEditMode",  new IMAGE_BRUSH( "Icons/icon_keyb_FoliageEditMode_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.LandscapeEditor",  new IMAGE_BRUSH( "Icons/icon_keyb_LandscapeEditor_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.LayersView",  new IMAGE_BRUSH( "Icons/icon_keyb_Layers_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.LevelEditor",  new IMAGE_BRUSH( "Icons/icon_keyb_LevelEditor_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.LevelViewport",  new IMAGE_BRUSH( "Icons/icon_keyb_LevelViewports_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.MainFrame",  new IMAGE_BRUSH( "Icons/icon_keyb_MainFrame_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.OutputLog",  new IMAGE_BRUSH( "Icons/icon_keyb_OutputLog_16px", FVector2D( 16, 16 ) ) );
		Set( "InputBindingEditor.PlayWorld",  new IMAGE_BRUSH( "Icons/icon_keyb_PlayWorld_16px", FVector2D( 16, 16 ) ) );		
	}

	// Package restore
	{
		Set( "PackageRestore.FolderOpen", new IMAGE_BRUSH( "Icons/FolderOpen", FVector2D(18, 16) ) );
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Package Dialog
	{
		Set( "PackageDialog.ListHeader", new BOX_BRUSH( "Old/SavePackages/ListHeader", 4.0f/32.0f ) );
		Set( "SavePackages.SCC_DlgCheckedOutOther", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_DlgCheckedOutOther", Icon16x16));
		Set( "SavePackages.SCC_DlgNotCurrent", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_DlgNotCurrent", Icon16x16));
		Set( "SavePackages.SCC_DlgReadOnly", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_DlgReadOnly", Icon16x16));

	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	// Layers General
	{
		Set( "Layer.Icon16x", new IMAGE_BRUSH( "Icons/layer_16x", Icon16x16 ) );
		Set( "Layer.VisibleIcon16x", new IMAGE_BRUSH( "Icons/icon_layer_visible", Icon16x16 ) );
		Set( "Layer.NotVisibleIcon16x", new IMAGE_BRUSH( "Icons/icon_layer_not_visible", Icon16x16 ) );
	}

	// Layer Stats
	{
		Set( "LayerStats.Item.ClearButton", new IMAGE_BRUSH( "Icons/Cross_12x", Icon12x12 ) );
	}

	// Layer Cloud
	{
		Set( "LayerCloud.Item.BorderImage", new BOX_BRUSH( "Common/RoundedSelection_16x", FMargin(4.0f/16.0f) ) );
		Set( "LayerCloud.Item.ClearButton", new IMAGE_BRUSH( "Icons/Cross_12x", Icon12x12 ) );
		Set( "LayerCloud.Item.LabelFont", DEFAULT_FONT( "Bold", 9 ) );
	}

	// Layer Browser
	{
		Set( "LayerBrowser.LayerContentsQuickbarBackground",  new CORE_BOX_BRUSH( "Common/DarkGroupBorder", 4.f/16.f ) );
		Set( "LayerBrowser.ExploreLayerContents",  new IMAGE_BRUSH( "Icons/ExploreLayerContents", Icon16x16 ) );
		Set( "LayerBrowser.ReturnToLayersList",  new IMAGE_BRUSH( "Icons/ReturnToLayersList", Icon16x16) );
		Set( "LayerBrowser.Actor.RemoveFromLayer", new IMAGE_BRUSH( "Icons/Cross_12x", Icon12x12 ) );

		Set( "LayerBrowserButton", FButtonStyle( Button )
			.SetNormal(FSlateNoResource())
			.SetHovered(BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor ))
			.SetPressed(BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor_Pressed ))
		);

		Set( "LayerBrowserButton.LabelFont", DEFAULT_FONT( "Regular", 8 ) );
	}

	// DataLayer
	{
		Set("DataLayer.Icon16x", new IMAGE_BRUSH("Icons/datalayer_16x", Icon16x16));
		Set("DataLayer.DynamicallyLoaded", new IMAGE_BRUSH("/Icons/datalayer_dynamicallyloaded_16x", Icon16x16));
		Set("DataLayer.NotDynamicallyLoaded", new IMAGE_BRUSH("/Icons/datalayer_not_dynamicallyloaded_16x", Icon16x16));
		Set("DataLayerBrowser.DataLayerContentsQuickbarBackground", new CORE_BOX_BRUSH("Common/DarkGroupBorder", 4.f / 16.f));
		Set("DataLayerBrowser.ExploreDataLayerContents", new IMAGE_BRUSH("Icons/datalayer_explore_contents", Icon16x16));
		Set("DataLayerBrowser.ReturnToDataLayersList", new IMAGE_BRUSH("Icons/datalayer_exit_contents", Icon16x16));
		Set("DataLayerBrowser.Actor.RemoveFromDataLayer", new IMAGE_BRUSH("Icons/Cross_12x", Icon12x12));
		Set("DataLayerBrowserButton", FButtonStyle(Button)
			.SetNormal(FSlateNoResource())
			.SetHovered(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor))
			.SetPressed(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor_Pressed))
		);
	}

	// Scene Outliner
	{
		Set( "SceneOutliner.NewFolderIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-plus", Icon16x16 ) );
		Set( "SceneOutliner.FolderClosed",  new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-closed", Icon16x16, FStyleColors::AccentFolder ) );
		Set( "SceneOutliner.FolderOpen",    new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-open", Icon16x16, FStyleColors::AccentFolder ) );
		Set( "SceneOutliner.World", 		new CORE_IMAGE_BRUSH_SVG("Starship/Common/world", Icon16x16 ) );
		Set( "SceneOutliner.ChangedItemHighlight", new FSlateRoundedBoxBrush( FStyleColors::White, 1.0) );

		Set( "SceneOutliner.TableViewRow", FTableRowStyle(NormalTableRowStyle));
	}

	// Socket chooser
	{
		Set( "SocketChooser.TitleFont", DEFAULT_FONT( "Regular", 8 ) );
		Set( "SocketIcon.Bone", new IMAGE_BRUSH( "Old/bone", Icon16x16 ) );
		Set( "SocketIcon.Socket", new IMAGE_BRUSH( "Old/socket", Icon16x16 ) );
		Set( "SocketIcon.None", new IMAGE_BRUSH( "Old/Favorites_Disabled", Icon16x16 ) );
	}
	
	// Matinee Recorder
	{
		Set( "MatineeRecorder.Record", new IMAGE_BRUSH( "Icons/Record_16x", Icon16x16 ) );
		Set( "MatineeRecorder.Stop", new IMAGE_BRUSH( "Icons/Stop_16x", Icon16x16 ) );
	}

	// Graph breadcrumb button
	{
		Set( "GraphBreadcrumbButton", FButtonStyle()
			.SetNormal        ( FSlateNoResource() )
			.SetPressed       ( BOX_BRUSH( "Common/Button_Pressed", 8.0f/32.0f, SelectionColor_Pressed ) )
			.SetHovered       ( BOX_BRUSH( "Common/Button_Hovered", 8.0f/32.0f, SelectionColor ) )
			.SetNormalPadding ( FMargin( 2,2,4,4 ) )
			.SetPressedPadding( FMargin( 3,3,3,3 ) )
		);

		Set( "GraphBreadcrumbButtonText", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 14 ) )
			.SetColorAndOpacity( FLinearColor(1,1,1,0.5) )
			.SetShadowOffset( FVector2D::ZeroVector )
		);

		Set("GraphBreadcrumb.BrowseBack", new IMAGE_BRUSH_SVG( "Starship/GraphEditors/PreviousArrow", Icon20x20));
		Set("GraphBreadcrumb.BrowseForward", new IMAGE_BRUSH_SVG( "Starship/GraphEditors/NextArrow", Icon20x20));

		const FComboButtonStyle FastJumpComboBoxComboButton = FComboButtonStyle()
			.SetButtonStyle(GetWidgetStyle<FButtonStyle>("GraphBreadcrumbButton"));
		Set("GraphBreadcrumbFastJumpComboBoxStyle", FComboBoxStyle()
			.SetComboButtonStyle(FastJumpComboBoxComboButton));
	}

	// Graph bookmark button
	{

		Set("GraphBookmarkMenuImage.Button_Add", new IMAGE_BRUSH("Icons/PlusSymbol_12x", Icon12x12));
		Set("GraphBookmarkMenuText.EmptyListItem", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Fonts/Roboto-Italic", 9))
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground()));
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	// Breadcrumb Trail
	{
		Set( "BreadcrumbButton", FButtonStyle()
			.SetNormal ( FSlateNoResource() )
			.SetPressed( BOX_BRUSH( "Common/Button_Pressed", 8.0f/32.0f, SelectionColor_Pressed ) )
			.SetHovered( BOX_BRUSH( "Common/Button_Pressed", 8.0f/32.0f, SelectionColor ) )
			);
		
	}

	// Notification List
	{
		Set( "NotificationList.Glow", new FSlateColorBrush( FColor(255, 255, 255, 255) ) );
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	// Asset editors (common)
	{
		Set( "AssetEditor.SaveAsset", new IMAGE_BRUSH_SVG("Starship/Common/SaveCurrent", Icon16x16));
		Set( "AssetEditor.SaveAssetAs", new IMAGE_BRUSH_SVG("Starship/Common/SaveCurrentAs", Icon16x16));

		Set( "AssetEditor.ReimportAsset", new IMAGE_BRUSH( "Icons/icon_TextureEd_Reimport_40x", Icon40x40 ) );
		Set( "AssetEditor.ReimportAsset.Small", new IMAGE_BRUSH( "Icons/icon_TextureEd_Reimport_40x", Icon20x20 ) );
	}
		
	// Asset Thumbnail
	{
		Set( "AssetThumbnail.AssetBackground", new FSlateColorBrush(FStyleColors::Recessed));
		Set( "AssetThumbnail.ClassBackground", new IMAGE_BRUSH( "Common/ClassBackground_64x", FVector2D(64.f, 64.f), FLinearColor(0.75f, 0.75f, 0.75f, 1.0f) ) );
		Set( "AssetThumbnail.Font", DEFAULT_FONT( "Regular", 10 ) );
		Set( "AssetThumbnail.FontSmall", DEFAULT_FONT( "Regular", 7 ) );
		Set( "AssetThumbnail.ColorAndOpacity", FLinearColor(1.75f, 1.75f, 1.75f, 1) );
		Set( "AssetThumbnail.HintFont", DEFAULT_FONT( "Regular", 8 ) );
		Set( "AssetThumbnail.HintFontSmall", DEFAULT_FONT( "Regular", 6 ) );
		Set( "AssetThumbnail.HintColorAndOpacity", FLinearColor(0.75f, 0.75f, 0.75f, 1) );
		Set( "AssetThumbnail.HintShadowOffset", FVector2D(1,1) );
		Set( "AssetThumbnail.HintShadowColorAndOpacity", FLinearColor(0, 0, 0, 0.5) );
		Set( "AssetThumbnail.HintBackground", new BOX_BRUSH( "Common/TableViewHeader", FMargin(8.0f/32.0f) ) );
	
	}

	// Open any asset dialog
	{
		Set( "SystemWideCommands.SummonOpenAssetDialog", new IMAGE_BRUSH_SVG( "Starship/Common/OpenAsset", Icon16x16 ) );
	
		Set( "GlobalAssetPicker.Background", new BOX_BRUSH( "Old/Menu_Background", FMargin(8.0f/64.0f) ) );
		Set( "GlobalAssetPicker.OutermostMargin", FMargin(4, 4, 4, 4) );

		Set( "GlobalAssetPicker.TitleFont", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Regular", 9 ) )
			.SetColorAndOpacity( FLinearColor::White )
				.SetShadowOffset( FVector2D( 1,1 ) )
				.SetShadowColorAndOpacity( FLinearColor::Black )
			);
	}


	// Main frame
	{
		Set( "MainFrame.AutoSaveImage", new IMAGE_BRUSH( "Icons/icon_Autosave", Icon24x24 ) );

		Set( "MainFrame.SaveAll",              new IMAGE_BRUSH_SVG( "Starship/Common/SaveAll", Icon16x16 ) );
		Set( "MainFrame.ChoosePackagesToSave", new IMAGE_BRUSH_SVG( "Starship/Common/icon_file_choosepackages_16px", Icon16x16 ) );
		Set( "MainFrame.NewProject",           new IMAGE_BRUSH_SVG( "Starship/Common/ProjectNew", Icon16x16 ) );
		Set( "MainFrame.OpenProject",          new IMAGE_BRUSH_SVG( "Starship/Common/ProjectOpen", Icon16x16 ) );
		Set( "MainFrame.AddCodeToProject",     new IMAGE_BRUSH_SVG( "Starship/Common/ProjectC++", Icon16x16 ) );
		Set( "MainFrame.Exit",                 new IMAGE_BRUSH_SVG( "Starship/Common/Exit", Icon16x16 ) );
		Set( "MainFrame.CookContent",          new IMAGE_BRUSH_SVG( "Starship/Common/CookContent", Icon16x16 ) );
		Set( "MainFrame.OpenVisualStudio",     new IMAGE_BRUSH_SVG( "Starship/Common/VisualStudio", Icon16x16 ) );
		Set( "MainFrame.RefreshVisualStudio",  new IMAGE_BRUSH_SVG( "Starship/Common/RefreshVisualStudio", Icon16x16 ) );
		Set( "MainFrame.PackageProject",       new IMAGE_BRUSH_SVG( "Starship/Common/ProjectPackage", Icon16x16 ) );
		Set( "MainFrame.RecentProjects",       new IMAGE_BRUSH_SVG( "Starship/Common/ProjectsRecent", Icon16x16 ) );
		Set( "MainFrame.RecentLevels",         new IMAGE_BRUSH_SVG( "Starship/Common/LevelRecent", Icon16x16 ) );
		Set( "MainFrame.FavoriteLevels",       new IMAGE_BRUSH_SVG( "Starship/Common/LevelFavorite", Icon16x16 ) );
		Set( "MainFrame.ZipUpProject", 		   new IMAGE_BRUSH_SVG( "Starship/Common/ZipProject", Icon16x16 ) );


		Set( "MainFrame.ChooseFilesToSave",       new IMAGE_BRUSH_SVG( "Starship/Common/SaveChoose", Icon16x16 ) );
		Set( "MainFrame.ConnectToSourceControl",  new IMAGE_BRUSH_SVG( "Starship/Common/SourceControl", Icon16x16 ) );

		Set( "MainFrame.DebugTools.SmallFont", DEFAULT_FONT( "Regular", 8 ) );
		Set( "MainFrame.DebugTools.NormalFont", DEFAULT_FONT( "Regular", 9 ) );
		Set( "MainFrame.DebugTools.LabelFont", DEFAULT_FONT( "Regular", 8 ) );
	}

	// Editor preferences
	{
		Set("EditorPreferences.TabIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Preferences", Icon16x16));
	}

	// Project settings
	{
		Set("ProjectSettings.TabIcon", new IMAGE_BRUSH_SVG("Starship/Common/ProjectSettings", Icon16x16));
	}

	// Main frame
	{
		Set("MainFrame.StatusInfoButton", FButtonStyle(Button)
			.SetNormal( IMAGE_BRUSH( "Icons/StatusInfo_16x", Icon16x16 ) )
			.SetHovered( IMAGE_BRUSH( "Icons/StatusInfo_16x", Icon16x16 ) )
			.SetPressed( IMAGE_BRUSH( "Icons/StatusInfo_16x", Icon16x16 ) )
			.SetNormalPadding(0)
			.SetPressedPadding(0)
		);
	}

	// CodeView selection detail view section
	{
		Set( "CodeView.ClassIcon", new IMAGE_BRUSH( "Icons/icon_class_16x", Icon16x16 ) );
		Set( "CodeView.FunctionIcon", new IMAGE_BRUSH( "Icons/icon_codeview_16x", Icon16x16 ) );
	}

	Set( "Editor.SearchBoxFont", DEFAULT_FONT( "Regular", 12) );
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Console
	{
		Set( "DebugConsole.Background", new BOX_BRUSH( "Old/Menu_Background", FMargin(8.0f/64.0f) ) );
	}

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	// About screen
	if (IncludeEditorSpecificStyles())
	{
		Set( "AboutScreen.Background", new IMAGE_BRUSH( "About/Background", FVector2D(688,317) ) );
		Set( "AboutScreen.UnrealLogo", new IMAGE_BRUSH_SVG( "About/UnrealLogo", FVector2D(40,40) ) );
		Set( "AboutScreen.EpicGamesLogo", new IMAGE_BRUSH_SVG( "About/EpicGamesLogo", FVector2D(40,40) ) );
		Set( "AboutScreen.TitleFont", DEFAULT_FONT( "Bold", 13) );
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR
	// Credits screen
	if (IncludeEditorSpecificStyles())
	{
		Set("Credits.Button", FButtonStyle(NoBorder)
			.SetNormal(FSlateNoResource())
			.SetPressed(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor_Pressed))
			.SetHovered(BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor))
			);

		Set("Credits.Pause", new IMAGE_BRUSH("Icons/PauseCredits", Icon20x20));
		Set("Credits.Play", new IMAGE_BRUSH("Icons/PlayCredits", Icon20x20));

		FLinearColor EditorOrange = FLinearColor(0.728f, 0.364f, 0.003f);

		FTextBlockStyle CreditsNormal = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 16))
			.SetShadowOffset(FVector2D::UnitVector);

		Set("Credits.Normal", CreditsNormal);

		Set("Credits.Strong", FTextBlockStyle(CreditsNormal)
			.SetFont(DEFAULT_FONT("Bold", 16))
			.SetShadowOffset(FVector2D::UnitVector));

		Set("Credits.H1", FTextBlockStyle(CreditsNormal)
			.SetColorAndOpacity(EditorOrange)
			.SetFont(DEFAULT_FONT("Bold", 36))
			.SetShadowOffset(FVector2D::UnitVector));

		Set("Credits.H2", FTextBlockStyle(CreditsNormal)
			.SetColorAndOpacity(EditorOrange)
			.SetFont(DEFAULT_FONT("Bold", 30))
			.SetShadowOffset(FVector2D::UnitVector));

		Set("Credits.H3", FTextBlockStyle(CreditsNormal)
			.SetFont(DEFAULT_FONT("Bold", 24))
			.SetShadowOffset(FVector2D::UnitVector));

		Set("Credits.H4", FTextBlockStyle(CreditsNormal)
			.SetFont(DEFAULT_FONT("Bold", 18))
			.SetShadowOffset(FVector2D::UnitVector));

		Set("Credits.H5", FTextBlockStyle(CreditsNormal)
			.SetFont(DEFAULT_FONT("Bold", 12))
			.SetShadowOffset(FVector2D::UnitVector));

		Set("Credits.H6", FTextBlockStyle(CreditsNormal)
			.SetFont(DEFAULT_FONT("Bold", 6))
			.SetShadowOffset(FVector2D::UnitVector));

		FTextBlockStyle LinkText = FTextBlockStyle(NormalText)
			.SetColorAndOpacity(EditorOrange)
			.SetShadowOffset(FVector2D::UnitVector);
		FButtonStyle HoverOnlyHyperlinkButton = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetPressed(FSlateNoResource())
			.SetHovered(BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0, 0, 0, 3 / 16.0f)));
		FHyperlinkStyle HoverOnlyHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(HoverOnlyHyperlinkButton)
			.SetTextStyle(LinkText)
			.SetPadding(FMargin(0.0f));

		Set("Credits.Hyperlink", HoverOnlyHyperlink);
	}
#endif // WITH_EDITOR

	// Hardware target settings
#if WITH_EDITOR
	{
		FLinearColor EditorOrange = FLinearColor(0.728f, 0.364f, 0.003f);

		FTextBlockStyle TargetSettingsNormal = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 8));

		Set("HardwareTargets.Normal", TargetSettingsNormal);

		Set("HardwareTargets.Strong", FTextBlockStyle(TargetSettingsNormal)
			.SetFont(DEFAULT_FONT("Bold", 8))
			.SetColorAndOpacity(EditorOrange)
			.SetShadowOffset(FVector2D::UnitVector));
	}
#endif

	// New Level Dialog
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set( "NewLevelDialog.BlackBorder", new FSlateColorBrush( FColor(0, 0, 0, 100) ) );
		Set( "NewLevelDialog.Blank", new IMAGE_BRUSH( "NewLevels/NewLevelBlank", FVector2D(256,256) ) );
		Set( "NewLevelDialog.Default", new IMAGE_BRUSH( "NewLevels/NewLevelDefault", FVector2D(256,256) ) );
	}

	// Build and Submit
	{
		Set( "BuildAndSubmit.NormalFont", DEFAULT_FONT( "Regular", 8 ) );
		Set( "BuildAndSubmit.SmallFont", DEFAULT_FONT( "Regular", 7 ) );
	}

	// Foliage Edit Mode
	if (IncludeEditorSpecificStyles())
	{	
		FLinearColor DimBackground = FLinearColor(FColor(64, 64, 64));
		FLinearColor DimBackgroundHover = FLinearColor(FColor(50, 50, 50));
		FLinearColor DarkBackground = FLinearColor(FColor(42, 42, 42));

		FToolBarStyle FoliageEditToolBar = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("ToolBar");

		FoliageEditToolBar.SetButtonPadding(FMargin(0.f));
		FoliageEditToolBar.SetComboButtonPadding(FMargin(4.0f));
		FoliageEditToolBar.SetCheckBoxPadding(FMargin(10.0f, 6.f));
		FoliageEditToolBar.SetSeparatorPadding(1.0f);
		FoliageEditToolBar.SetToggleButtonStyle(
			FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH("Common/Selection", 8.0f / 32.0f, DimBackground))
			.SetUncheckedPressedImage(BOX_BRUSH("PlacementMode/TabActive", 8.0f / 32.0f))
			.SetUncheckedHoveredImage(BOX_BRUSH("Common/Selection", 8.0f / 32.0f, DimBackgroundHover))
			.SetCheckedImage(BOX_BRUSH("PlacementMode/TabActive", 8.0f / 32.0f))
			.SetCheckedHoveredImage(BOX_BRUSH("PlacementMode/TabActive", 8.0f / 32.0f))
			.SetCheckedPressedImage(BOX_BRUSH("PlacementMode/TabActive", 8.0f / 32.0f))
			.SetPadding(0));

		Set("FoliageEditToolBar", FoliageEditToolBar);

		Set("FoliageEditMode.SetSelect",                 new IMAGE_BRUSH("Icons/GeneralTools/Select_40x", Icon20x20));
		Set("FoliageEditMode.SetSelect.Small",           new IMAGE_BRUSH("Icons/GeneralTools/Select_40x", Icon20x20));
		Set("FoliageEditMode.SelectAll",                 new IMAGE_BRUSH("Icons/GeneralTools/SelectAll_40x", Icon20x20));
		Set("FoliageEditMode.SelectAll.Small",           new IMAGE_BRUSH("Icons/GeneralTools/SelectAll_40x", Icon20x20));
		Set("FoliageEditMode.DeselectAll",               new IMAGE_BRUSH("Icons/GeneralTools/Deselect_40x", Icon20x20));
		Set("FoliageEditMode.DeselectAll.Small",         new IMAGE_BRUSH("Icons/GeneralTools/Deselect_40x", Icon20x20));
		Set("FoliageEditMode.SelectInvalid",             new IMAGE_BRUSH("Icons/GeneralTools/SelectInvalid_40x", Icon20x20));
		Set("FoliageEditMode.SelectInvalid.Small",       new IMAGE_BRUSH("Icons/GeneralTools/SelectInvalid_40x", Icon20x20));
		Set("FoliageEditMode.SetLassoSelect",            new IMAGE_BRUSH("Icons/GeneralTools/Lasso_40x", Icon20x20));
		Set("FoliageEditMode.SetLassoSelect.Small",      new IMAGE_BRUSH("Icons/GeneralTools/Lasso_40x", Icon20x20));
		Set("FoliageEditMode.Foliage",                   new IMAGE_BRUSH("Icons/GeneralTools/Foliage_40x", Icon20x20));
		Set("FoliageEditMode.Foliage.Small",             new IMAGE_BRUSH("Icons/GeneralTools/Foliage_40x", Icon20x20));
		Set("FoliageEditMode.SetPaint",                  new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("FoliageEditMode.SetPaint.Small",            new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("FoliageEditMode.SetReapplySettings",        new IMAGE_BRUSH("Icons/GeneralTools/Repaint_40x", Icon20x20));
		Set("FoliageEditMode.SetReapplySettings.Small",  new IMAGE_BRUSH("Icons/GeneralTools/Repaint_40x", Icon20x20));
		Set("FoliageEditMode.SetPaintBucket",            new IMAGE_BRUSH("Icons/GeneralTools/PaintBucket_40x", Icon20x20));
		Set("FoliageEditMode.SetPaintBucket.Small",      new IMAGE_BRUSH("Icons/GeneralTools/PaintBucket_40x", Icon20x20));
		Set("FoliageEditMode.Remove",                    new IMAGE_BRUSH("Icons/GeneralTools/Delete_40x", Icon20x20));
		Set("FoliageEditMode.Remove.Small",              new IMAGE_BRUSH("Icons/GeneralTools/Delete_40x", Icon20x20));
		Set("FoliageEditMode.Erase",                     new IMAGE_BRUSH("Icons/GeneralTools/Erase_40x", Icon20x20));
		Set("FoliageEditMode.Erase.Small",               new IMAGE_BRUSH("Icons/GeneralTools/Erase_40x", Icon20x20));
		Set("FoliageEditMode.Filter",                    new IMAGE_BRUSH("Icons/GeneralTools/Filter_40x", Icon20x20));
		Set("FoliageEditMode.Filter.Small",              new IMAGE_BRUSH("Icons/GeneralTools/Filter_40x", Icon20x20));
		Set("FoliageEditMode.Settings",                  new IMAGE_BRUSH("Icons/GeneralTools/Settings_40x", Icon20x20));
		Set("FoliageEditMode.Settings.Small",            new IMAGE_BRUSH("Icons/GeneralTools/Settings_40x", Icon20x20));
		Set("FoliageEditMode.MoveToCurrentLevel",        new IMAGE_BRUSH("Icons/GeneralTools/MoveToLevel_40x", Icon20x20));
		Set("FoliageEditMode.MoveToCurrentLevel.Small",  new IMAGE_BRUSH("Icons/GeneralTools/MoveToLevel_40x", Icon20x20));

		Set( "FoliageEditMode.SetNoSettings", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEdMode_NoSettings_20x", Icon20x20 ) );
		Set( "FoliageEditMode.SetPaintSettings", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEdMode_PaintingSettings_20x", Icon20x20 ) );
		Set( "FoliageEditMode.SetClusterSettings", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEdMode_ClusterSettings_20x", Icon20x20 ) );
		Set( "FoliageEditMode.SetNoSettings.Small", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEdMode_NoSettings_20x", Icon20x20 ) );
		Set( "FoliageEditMode.SetPaintSettings.Small", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEdMode_PaintingSettings_20x", Icon20x20 ) );
		Set( "FoliageEditMode.SetClusterSettings.Small", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEdMode_ClusterSettings_20x", Icon20x20 ) );

		Set( "FoliageEditMode.OpenSettings", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEditMode_LoadSettings_20px", Icon20x20 ) );
		Set( "FoliageEditMode.SaveSettings", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEditMode_SaveSettings_20px", Icon20x20 ) );
		Set( "FoliageEditMode.DeleteItem", new IMAGE_BRUSH( "Icons/FoliageEditMode/icon_FoliageEditMode_RemoveSettings_20x", Icon20x20 ) );
		Set( "FoliageEditMode.SelectionBackground", new IMAGE_BRUSH( "Icons/FoliageEditMode/FoliageEditMode_SelectionBackground", Icon32x32 ) );
		Set( "FoliageEditMode.ItemBackground", new IMAGE_BRUSH( "Icons/FoliageEditMode/FoliageEditMode_Background", Icon64x64 ) );
		Set( "FoliageEditMode.BubbleBorder", new BOX_BRUSH( "Icons/FoliageEditMode/FoliageEditMode_BubbleBorder", FMargin(8/32.0f) ) );

		Set( "FoliageEditMode.TreeView.ScrollBorder", FScrollBorderStyle()
			.SetTopShadowBrush(FSlateNoResource())
			.SetBottomShadowBrush(BOX_BRUSH("Common/ScrollBorderShadowBottom", FVector2D(16, 8), FMargin(0.5, 0, 0.5, 1)))
			);

		Set("FoliageEditMode.Splitter", FSplitterStyle()
			.SetHandleNormalBrush(IMAGE_BRUSH("Common/SplitterHandleHighlight", Icon8x8, FLinearColor(.2f, .2f, .2f, 1.f)))
			.SetHandleHighlightBrush(IMAGE_BRUSH("Common/SplitterHandleHighlight", Icon8x8, FLinearColor::White))
			);

		Set("FoliageEditMode.ActiveToolName.Text", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 11))
			.SetShadowOffset(FVector2D(1, 1))
			);

		Set("FoliageEditMode.AddFoliageType.Text", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));


		FLinearColor InputA = FStyleColors::Input.GetSpecifiedColor();
		InputA .A = .60;

		FLinearColor InputB = FStyleColors::Input.GetSpecifiedColor();
		InputA .A = .70;

		Set("FoliageEditMode.FloatingButton", FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(InputA, 2))
			.SetHovered(FSlateRoundedBoxBrush(InputB, 2))
			.SetPressed(FSlateRoundedBoxBrush(InputB, 2))
			.SetNormalForeground(FStyleColors::Foreground)
			.SetHoveredForeground(FStyleColors::ForegroundHover)
			.SetPressedForeground(FStyleColors::ForegroundHover)
			.SetDisabledForeground(FStyleColors::White25)
			.SetNormalPadding(FMargin(4))
			.SetPressedPadding(FMargin(4))
		 );

	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)


#if WITH_EDITOR
	// Surface Props
	{
		Set( "SurfaceDetails.PanUPositive", new IMAGE_BRUSH( "Icons/icon_PanRight", Icon16x16 ) );
		Set( "SurfaceDetails.PanUNegative", new IMAGE_BRUSH( "Icons/icon_PanLeft", Icon16x16 ) );

		Set( "SurfaceDetails.PanVPositive", new IMAGE_BRUSH( "Icons/icon_PanUp", Icon16x16 ) );
		Set( "SurfaceDetails.PanVNegative", new IMAGE_BRUSH( "Icons/icon_PanDown", Icon16x16 ) );

		
		Set( "SurfaceDetails.ClockwiseRotation", new IMAGE_BRUSH( "Icons/icon_ClockwiseRotation_16x", Icon16x16 ) );
		Set( "SurfaceDetails.AntiClockwiseRotation", new IMAGE_BRUSH( "Icons/icon_AntiClockwiseRotation_16x", Icon16x16 ) );
	}

	// GameProjectDialog
	if (IncludeEditorSpecificStyles())
	{
		Set( "GameProjectDialog.BlankProjectThumbnail", new IMAGE_BRUSH( "GameProjectDialog/blank_project_thumbnail", Icon128x128 ) );
		Set( "GameProjectDialog.BlankProjectPreview", new IMAGE_BRUSH( "GameProjectDialog/blank_project_preview", FVector2D(400, 200) ) );

	}

	// NewClassDialog
	if (IncludeEditorSpecificStyles())
	{
		Set( "NewClassDialog.ErrorLabelCloseButton", new IMAGE_BRUSH( "Icons/Cross_12x", Icon12x12 ) );

		Set( "NewClassDialog.ParentClassListView.TableRow", FTableRowStyle()
			.SetEvenRowBackgroundBrush( FSlateNoResource() )
			.SetEvenRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(FStyleColors::Panel, 4.0f))
			.SetOddRowBackgroundBrush( FSlateNoResource() )
			.SetOddRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(FStyleColors::Panel, 4.0f))
			.SetSelectorFocusedBrush(FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Select, 1.0f))
			.SetActiveBrush(FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Select, 1.0f))
			.SetActiveHoveredBrush(FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Select, 1.0f))
			.SetInactiveBrush(FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::SelectInactive, 1.0f))
			.SetInactiveHoveredBrush(FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::SelectInactive, 1.0f))
			.SetTextColor( FStyleColors::Foreground )
			.SetSelectedTextColor(FStyleColors::Foreground)
			);

	}

	// Package Migration
	{
		Set( "PackageMigration.DialogTitle", FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Regular", 12 ) )
		);
	}

	// Hardware Targeting
	{
		Set( "HardwareTargeting.MobilePlatform", new IMAGE_BRUSH( "/Icons/HardwareTargeting/Mobile", FVector2D(64, 64) ) );
		Set( "HardwareTargeting.DesktopPlatform", new IMAGE_BRUSH( "/Icons/HardwareTargeting/Desktop", FVector2D(64, 64) ) );
		Set( "HardwareTargeting.HardwareUnspecified", new IMAGE_BRUSH( "/Icons/HardwareTargeting/HardwareUnspecified", FVector2D(64, 64) ) );

		Set( "HardwareTargeting.MaximumQuality", new IMAGE_BRUSH( "/Icons/HardwareTargeting/MaximumQuality", FVector2D(64, 64) ) );
		Set( "HardwareTargeting.ScalableQuality", new IMAGE_BRUSH( "/Icons/HardwareTargeting/ScalableQuality", FVector2D(64, 64) ) );
		Set( "HardwareTargeting.GraphicsUnspecified", new IMAGE_BRUSH( "/Icons/HardwareTargeting/GraphicsUnspecified", FVector2D(64, 64) ) );
	}

#endif // WITH_EDITOR

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	// Mode ToolPalette 
	{

		FToolBarStyle PaletteToolBarStyle = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		PaletteToolBarStyle.SetLabelStyle( GetParentStyle()->GetWidgetStyle<FTextBlockStyle>("SmallText") );
		
		PaletteToolBarStyle.SetBackground(FSlateColorBrush(FStyleColors::Recessed));

		PaletteToolBarStyle.SetLabelPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));

		PaletteToolBarStyle.SetButtonPadding(       FMargin(0.0f, 0.0f));
		PaletteToolBarStyle.SetCheckBoxPadding(     FMargin(0.0f, 0.0f));
		PaletteToolBarStyle.SetComboButtonPadding(  FMargin(0.0f, 0.0f));
		PaletteToolBarStyle.SetIndentedBlockPadding(FMargin(0.0f, 0.0f));
		PaletteToolBarStyle.SetBlockPadding(        FMargin(0.0f, 0.0f));

		Set( "PaletteToolBar.Tab",  FCheckBoxStyle()
			.SetCheckBoxType(            ESlateCheckBoxType::ToggleButton)

			.SetCheckedImage(            FSlateRoundedBoxBrush(FStyleColors::Input, 2.0f))
			.SetCheckedHoveredImage(     FSlateRoundedBoxBrush(FStyleColors::Input, 2.0f))
			.SetCheckedPressedImage(     FSlateRoundedBoxBrush(FStyleColors::Input, 2.0f))

			.SetUncheckedImage(          FSlateRoundedBoxBrush(FStyleColors::Secondary, 2.0f))
			.SetUncheckedHoveredImage(   FSlateRoundedBoxBrush(FStyleColors::Hover, 2.0f))
			.SetUncheckedPressedImage(   FSlateRoundedBoxBrush(FStyleColors::Secondary, 2.0f))

			.SetForegroundColor(         FStyleColors::Foreground)
			.SetHoveredForegroundColor(  FStyleColors::ForegroundHover)
			.SetPressedForegroundColor(  FStyleColors::ForegroundHover)
			.SetCheckedForegroundColor(  FStyleColors::Primary)
			.SetCheckedHoveredForegroundColor(FStyleColors::PrimaryHover)
			.SetPadding(FMargin(2.f, 6.f))
		);

		Set("PaletteToolBar.MaxUniformToolbarSize", 48.f);
		Set("PaletteToolBar.MinUniformToolbarSize", 48.f);

		Set("PaletteToolBar.ExpandableAreaHeader", new FSlateRoundedBoxBrush(FStyleColors::Dropdown, FVector4(4.0, 4.0, 0.0, 0.0)));
		Set("PaletteToolBar.ExpandableAreaBody", new FSlateRoundedBoxBrush(FStyleColors::Recessed, FVector4(0.0, 0.0, 4.0, 4.0)));
		
		FLinearColor ButtonHoverColor(FColor(81, 81, 81));

			/* Create style for "ToolBar.ToggleButton" widget ... */
		const FCheckBoxStyle ToolBarToggleButtonCheckBoxStyle = FCheckBoxStyle()
				.SetCheckBoxType( ESlateCheckBoxType::ToggleButton )
				.SetUncheckedImage(        FSlateNoResource() )
				.SetUncheckedPressedImage( FSlateNoResource() )
				.SetUncheckedHoveredImage( FSlateNoResource() )
				.SetCheckedImage(          FSlateNoResource() )
				.SetCheckedHoveredImage(   FSlateNoResource() )
				.SetCheckedPressedImage(   FSlateNoResource() )
				.SetForegroundColor(       FStyleColors::Foreground)
				.SetHoveredForegroundColor(FStyleColors::ForegroundHover)
				.SetPressedForegroundColor(FStyleColors::ForegroundHover)
				.SetCheckedForegroundColor(FStyleColors::Primary)
				.SetCheckedHoveredForegroundColor(FStyleColors::PrimaryHover)
				.SetPadding(FMargin(2.f, 6.f));
		
		PaletteToolBarStyle.SetToggleButtonStyle(ToolBarToggleButtonCheckBoxStyle);
		PaletteToolBarStyle.SetButtonStyle(FButtonStyle(Button)
			.SetNormal(  FSlateNoResource() )
			.SetPressed( FSlateNoResource() )
			.SetHovered( FSlateNoResource() )
			.SetNormalForeground(         FStyleColors::Foreground)
			.SetHoveredForeground(        FStyleColors::ForegroundHover)
			.SetPressedForeground(        FStyleColors::ForegroundHover)
			.SetNormalPadding(FMargin(2.0f, 6.0f))
			.SetPressedPadding(FMargin(2.0f, 6.0f))
		);

		Set("PaletteToolBar", PaletteToolBarStyle);

		Set("EditorModesPanel.CategoryFontStyle", DEFAULT_FONT( "Bold", 10 ));
		Set("EditorModesPanel.ToolDescriptionFont", DEFAULT_FONT("Italic", 10));

	}
	
	// Ctrl+Tab menu
	{
		Set("ControlTabMenu.Background", new BOX_BRUSH("Old/Menu_Background", FMargin(8.0f / 64.0f)));

		Set("ControlTabMenu.HeadingStyle",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);

		Set("ControlTabMenu.AssetTypeStyle",
			FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FLinearColor::White)
			);

		Set("ControlTabMenu.AssetPathStyle",
			FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FLinearColor::White)
			);

		Set("ControlTabMenu.AssetNameStyle",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 14))
			.SetColorAndOpacity(FLinearColor::White)
			);
	}

	

	// ViewportLayoutToolbar
	{
		FToolBarStyle ViewportLayoutToolbar = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
		ViewportLayoutToolbar.SetIconSize(Icon40x40);

		Set("ViewportLayoutToolbar", ViewportLayoutToolbar);
	}

	
	// Highres Screenshot
	{
		Set("HighresScreenshot.WarningStrip", new IMAGE_BRUSH( "Common/WarningStripe", FVector2D(20,6), FLinearColor::White, ESlateBrushTileType::Horizontal ) );
		Set("HighresScreenshot.SpecifyCaptureRectangle", new IMAGE_BRUSH( "Icons/icon_CaptureRegion_24x", Icon24x24 ) );
		Set("HighresScreenshot.FullViewportCaptureRegion", new IMAGE_BRUSH( "Icons/icon_CaptureRegion_FullViewport_24x", Icon24x24 ) );
		Set("HighresScreenshot.CameraSafeAreaCaptureRegion", new IMAGE_BRUSH("Icons/icon_CaptureRegion_Camera_Safe_24x", Icon24x24));
		Set("HighresScreenshot.Capture", new IMAGE_BRUSH( "Icons/icon_HighResScreenshotCapture_24px", Icon24x24 ) );
		Set("HighresScreenshot.AcceptCaptureRegion", new IMAGE_BRUSH( "Icons/icon_CaptureRegionAccept_24x", Icon24x24 ) );
		Set("HighresScreenshot.DiscardCaptureRegion", new IMAGE_BRUSH( "Icons/icon_CaptureRegionDiscard_24x", Icon24x24 ) );
	}

	// Scalability 
	{
		const float Tint = 0.65f;
		Set("Scalability.RowBackground", new BOX_BRUSH( "Common/GroupBorder", FMargin(4.0f/16.0f), FLinearColor(Tint, Tint, Tint) ) );
		Set("Scalability.TitleFont", DEFAULT_FONT( "Bold", 12 ) );
		Set("Scalability.GroupFont", DEFAULT_FONT( "Bold", 10 ) );
	}

	// Common styles for blueprint/code references that also need to be exposed to external tools
	{
		FTextBlockStyle InheritedFromNativeTextStyle = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10));

		Set("Common.InheritedFromNativeTextStyle", InheritedFromNativeTextStyle);

		// Go to native class hyperlink
		FButtonStyle EditNativeHyperlinkButton = FButtonStyle()
			.SetNormal(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0, 0, 0, 3 / 16.0f)))
			.SetPressed(FSlateNoResource())
			.SetHovered(BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0, 0, 0, 3 / 16.0f)));
		FHyperlinkStyle EditNativeHyperlinkStyle = FHyperlinkStyle()
			.SetUnderlineStyle(EditNativeHyperlinkButton)
			.SetTextStyle(InheritedFromNativeTextStyle)
			.SetPadding(FMargin(0.0f));

		Set("Common.GotoNativeCodeHyperlink", EditNativeHyperlinkStyle);
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Gameplay Tags
	{
		Set("GameplayTagTreeView", FTableRowStyle()
			.SetEvenRowBackgroundBrush(FSlateNoResource())
			.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
			.SetOddRowBackgroundBrush(FSlateNoResource())
			.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
			.SetSelectorFocusedBrush(FSlateNoResource())
			.SetActiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
			.SetActiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor))
			.SetInactiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
			.SetInactiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, SelectionColor_Inactive))
			);
	}


	// Common styles for blueprint/code references
	{
		// Inherited from blueprint
		Set("Common.InheritedFromBlueprintTextColor", InheritedFromBlueprintTextColor);

		FTextBlockStyle InheritedFromBlueprintTextStyle = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(InheritedFromBlueprintTextColor);

		Set("Common.InheritedFromBlueprintTextStyle", InheritedFromBlueprintTextStyle);

		// Go to blueprint hyperlink
		FButtonStyle EditBPHyperlinkButton = FButtonStyle()
			.SetNormal(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0, 0, 0, 3 / 16.0f), InheritedFromBlueprintTextColor))
			.SetPressed(FSlateNoResource())
			.SetHovered(BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0, 0, 0, 3 / 16.0f), InheritedFromBlueprintTextColor));
		FHyperlinkStyle EditBPHyperlinkStyle = FHyperlinkStyle()
			.SetUnderlineStyle(EditBPHyperlinkButton)
			.SetTextStyle(InheritedFromBlueprintTextStyle)
			.SetPadding(FMargin(0.0f));

		Set("Common.GotoBlueprintHyperlink", EditBPHyperlinkStyle);
	}

	// Timecode Provider
	{
		Set("TimecodeProvider.TabIcon", new IMAGE_BRUSH("Icons/icon_tab_TimecodeProvider_16x", Icon16x16));
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
}

void FStarshipEditorStyle::FStyle::SetupLevelGeneralStyles()
{
// Levels General
	{
		Set("Level.VisibleIcon16x", new CORE_IMAGE_BRUSH_SVG("Starship/Common/visible", Icon16x16));
		Set("Level.VisibleHighlightIcon16x", new CORE_IMAGE_BRUSH_SVG("Starship/Common/visible", Icon16x16));
		Set("Level.NotVisibleIcon16x", new CORE_IMAGE_BRUSH_SVG("Starship/Common/hidden", Icon16x16));
		Set("Level.NotVisibleHighlightIcon16x", new CORE_IMAGE_BRUSH_SVG("Starship/Common/hidden", Icon16x16));

		Set("Level.LightingScenarioIcon16x", new IMAGE_BRUSH_SVG("Starship/AssetIcons/PointLight_16", Icon16x16));
		Set("Level.LightingScenarioNotIcon16x", new IMAGE_BRUSH_SVG("Starship/Common/LightBulbOff", Icon16x16));
		Set("Level.LockedIcon16x", new IMAGE_BRUSH("Icons/icon_locked_16px", Icon16x16));
		Set("Level.LockedHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_locked_highlight_16px", Icon16x16));
		Set("Level.UnlockedIcon16x", new IMAGE_BRUSH("Icons/icon_levels_unlocked_16px", Icon16x16));
		Set("Level.UnlockedHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_levels_unlocked_hi_16px", Icon16x16));
		Set("Level.ReadOnlyLockedIcon16x", new IMAGE_BRUSH("Icons/icon_levels_LockedReadOnly_16px", Icon16x16));
		Set("Level.ReadOnlyLockedHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_levels_LockedReadOnly_hi_16px", Icon16x16));
		Set("Level.SaveIcon16x", new IMAGE_BRUSH("Icons/icon_levels_Save_16px", Icon16x16));
		Set("Level.SaveHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_levels_Save_hi_16px", Icon16x16));
		Set("Level.SaveModifiedIcon16x", new IMAGE_BRUSH("Icons/icon_levels_SaveModified_16px", Icon16x16));
		Set("Level.SaveModifiedHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_levels_SaveModified_hi_16px", Icon16x16));
		Set("Level.SaveDisabledIcon16x", new IMAGE_BRUSH("Icons/icon_levels_SaveDisabled_16px", Icon16x16));
		Set("Level.SaveDisabledHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_levels_SaveDisabled_hi_16px", Icon16x16));
		Set("Level.ScriptIcon16x", new IMAGE_BRUSH("Icons/icon_levels_Blueprint_16px", Icon16x16));
		Set("Level.ScriptHighlightIcon16x", new IMAGE_BRUSH("Icons/icon_levels_Blueprint_hi_16px", Icon16x16));
		Set("Level.EmptyIcon16x", new IMAGE_BRUSH("Icons/Empty_16x", Icon16x16));
		Set("Level.ColorIcon", new FSlateRoundedBoxBrush(FStyleColors::White, 4.0f, Icon16x16));
	}

	// Spline component controls
	{
		Set("SplineComponentDetails.SelectFirst", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectFirst", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectFirst", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectFirst", Icon20x20, SelectionColor_Pressed))
		);

		Set("SplineComponentDetails.AddPrev", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_AddPrevious", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_AddPrevious", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_AddPrevious", Icon20x20, SelectionColor_Pressed))
		);

		Set("SplineComponentDetails.SelectPrev", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectPrevious", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectPrevious", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectPrevious", Icon20x20, SelectionColor_Pressed))
		);

		Set("SplineComponentDetails.SelectAll", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectAll", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectAll", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectAll", Icon20x20, SelectionColor_Pressed))
		);

		Set("SplineComponentDetails.SelectNext", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectNext", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectNext", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectNext", Icon20x20, SelectionColor_Pressed))
		);

		Set("SplineComponentDetails.AddNext", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_AddNext", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_AddNext", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_AddNext", Icon20x20, SelectionColor_Pressed))
		);

		Set("SplineComponentDetails.SelectLast", FButtonStyle(Button)
			.SetNormal(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectLast", Icon20x20))
			.SetHovered(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectLast", Icon20x20, SelectionColor))
			.SetPressed(IMAGE_BRUSH_SVG("Starship/Splines/Spline_SelectLast", Icon20x20, SelectionColor_Pressed))
		);

	}
}

void FStarshipEditorStyle::FStyle::SetupWorldBrowserStyles()
{

	// World Browser
	{
		Set("WorldBrowser.AddLayer", new IMAGE_BRUSH("Icons/icon_levels_addlayer_16x", Icon16x16));
		Set("WorldBrowser.SimulationViewPosition", new IMAGE_BRUSH("Icons/icon_levels_simulationviewpos_16x", Icon16x16));
		Set("WorldBrowser.MouseLocation", new IMAGE_BRUSH("Icons/icon_levels_mouselocation_16x", Icon16x16));
		Set("WorldBrowser.MarqueeRectSize", new IMAGE_BRUSH("Icons/icon_levels_marqueerectsize_16x", Icon16x16));
		Set("WorldBrowser.WorldSize", new IMAGE_BRUSH("Icons/icon_levels_worldsize_16x", Icon16x16));
		Set("WorldBrowser.WorldOrigin", new IMAGE_BRUSH("Icons/icon_levels_worldorigin_16x", Icon16x16));
		Set("WorldBrowser.DirectionXPositive", new IMAGE_BRUSH("Icons/icon_PanRight", Icon16x16));
		Set("WorldBrowser.DirectionXNegative", new IMAGE_BRUSH("Icons/icon_PanLeft", Icon16x16));
		Set("WorldBrowser.DirectionYPositive", new IMAGE_BRUSH("Icons/icon_PanUp", Icon16x16));
		Set("WorldBrowser.DirectionYNegative", new IMAGE_BRUSH("Icons/icon_PanDown", Icon16x16));
		Set("WorldBrowser.LevelStreamingAlwaysLoaded", new FSlateNoResource());
		Set("WorldBrowser.LevelStreamingBlueprint", new IMAGE_BRUSH("Icons/icon_levels_blueprinttype_7x16", Icon7x16));
		Set("WorldBrowser.LevelsMenuBrush", new IMAGE_BRUSH("Icons/icon_levels_levelsmenu_40x", Icon25x25));
		Set("WorldBrowser.HierarchyButtonBrush", new IMAGE_BRUSH("Icons/icon_levels_hierarchybutton_16x", Icon16x16));
		Set("WorldBrowser.DetailsButtonBrush", new IMAGE_BRUSH("Icons/icon_levels_detailsbutton_40x", Icon16x16));
		Set("WorldBrowser.CompositionButtonBrush", new IMAGE_BRUSH("Icons/icon_levels_compositionbutton_16x", Icon16x16));

		Set("WorldBrowser.NewFolderIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-plus", Icon16x16));

		Set("WorldBrowser.StatusBarText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("BoldCondensed", 12))
			.SetColorAndOpacity(FLinearColor(0.9, 0.9f, 0.9f, 0.5f))
			.SetShadowOffset(FVector2D::ZeroVector)
		);

		Set("WorldBrowser.LabelFont", DEFAULT_FONT("Regular", 9));
		Set("WorldBrowser.LabelFontBold", DEFAULT_FONT("Bold", 10));
	}
}

void FStarshipEditorStyle::FStyle::SetupWorldPartitionStyles()
{
	// World Partition
	Set("WorldPartition.PartiallyLoadedCell", new IMAGE_BRUSH("WorldPartition/PartiallyLoadedCell", Icon32x32));
	Set("WorldPartition.GridBackground", new BOX_BRUSH("WorldPartition/GridBackground", FMargin(0.f)));
	Set("WorldPartition.SimulationViewPosition", new IMAGE_BRUSH("Icons/icon_levels_simulationviewpos_16x", Icon16x16));
}

void FStarshipEditorStyle::FStyle::SetupSequencerStyles()
{
	// Sequencer
	if (IncludeEditorSpecificStyles())
	{
		Set("Sequencer.IconKeyAuto", new IMAGE_BRUSH("Sequencer/IconKeyAuto", Icon12x12));
		Set("Sequencer.IconKeyBreak", new IMAGE_BRUSH("Sequencer/IconKeyBreak", Icon12x12));
		Set("Sequencer.IconKeyConstant", new IMAGE_BRUSH("Sequencer/IconKeyConstant", Icon12x12));
		Set("Sequencer.IconKeyLinear", new IMAGE_BRUSH("Sequencer/IconKeyLinear", Icon12x12));
		Set("Sequencer.IconKeyUser", new IMAGE_BRUSH("Sequencer/IconKeyUser", Icon12x12));

		Set("Sequencer.KeyCircle", new IMAGE_BRUSH("Sequencer/KeyCircle", Icon12x12));
		Set("Sequencer.KeyDiamond", new IMAGE_BRUSH("Sequencer/KeyDiamond", Icon12x12));
		Set("Sequencer.KeyDiamondBorder", new IMAGE_BRUSH("Sequencer/KeyDiamondBorder", Icon12x12));
		Set("Sequencer.KeySquare", new IMAGE_BRUSH("Sequencer/KeySquare", Icon12x12));
		Set("Sequencer.KeyTriangle", new IMAGE_BRUSH("Sequencer/KeyTriangle", Icon12x12));
		Set("Sequencer.KeyLeft", new IMAGE_BRUSH("Sequencer/KeyLeft", Icon12x12));
		Set("Sequencer.KeyRight", new IMAGE_BRUSH("Sequencer/KeyRight", Icon12x12));
		Set("Sequencer.PartialKey", new IMAGE_BRUSH("Sequencer/PartialKey", FVector2D(11.f, 11.f)));
		Set("Sequencer.Star", new IMAGE_BRUSH("Sequencer/Star", Icon12x12));
		Set("Sequencer.Empty", new IMAGE_BRUSH("Sequencer/Empty", Icon12x12));
		Set("Sequencer.TangentHandle", new IMAGE_BRUSH("Sequencer/TangentHandle", FVector2D(7, 7)));
		Set("Sequencer.GenericDivider", new IMAGE_BRUSH("Sequencer/GenericDivider", FVector2D(2.f, 2.f), FLinearColor::White, ESlateBrushTileType::Vertical));

		Set("Sequencer.Timeline.ScrubHandleDown", new BOX_BRUSH("Sequencer/ScrubHandleDown", FMargin(6.f / 13.f, 5 / 12.f, 6 / 13.f, 8 / 12.f)));
		Set("Sequencer.Timeline.ScrubHandleUp", new BOX_BRUSH("Sequencer/ScrubHandleUp", FMargin(6.f / 13.f, 8 / 12.f, 6 / 13.f, 5 / 12.f)));
		Set("Sequencer.Timeline.ScrubFill", new BOX_BRUSH("Sequencer/ScrubFill", FMargin(2.f / 4.f, 0.f)));
		Set("Sequencer.Timeline.FrameBlockScrubHandleDown", new BOX_BRUSH("Sequencer/ScrubHandleDown", FMargin(6.f / 13.f, 5 / 12.f, 6 / 13.f, 8 / 12.f)));
		Set("Sequencer.Timeline.FrameBlockScrubHandleUp", new BOX_BRUSH("Sequencer/ScrubHandleUp", FMargin(6.f / 13.f, 8 / 12.f, 6 / 13.f, 5 / 12.f)));
		Set("Sequencer.Timeline.VanillaScrubHandleDown", new BOX_BRUSH("Sequencer/ScrubHandleDown_Clamped", FMargin(6.f / 13.f, 3.f / 12.f, 6.f / 13.f, 7.f / 12.f)));
		Set("Sequencer.Timeline.VanillaScrubHandleUp", new BOX_BRUSH("Sequencer/ScrubHandleUp_Clamped", FMargin(6.f / 13.f, 8 / 12.f, 6 / 13.f, 5 / 12.f)));
		Set("Sequencer.Timeline.ScrubHandleWhole", new BOX_BRUSH("Sequencer/ScrubHandleWhole", FMargin(6.f / 13.f, 10 / 24.f, 6 / 13.f, 10 / 24.f)));
		Set("Sequencer.Timeline.RangeHandleLeft", new BOX_BRUSH("Sequencer/GenericGripLeft", FMargin(5.f / 16.f)));
		Set("Sequencer.Timeline.RangeHandleRight", new BOX_BRUSH("Sequencer/GenericGripRight", FMargin(5.f / 16.f)));
		Set("Sequencer.Timeline.RangeHandle", new BOX_BRUSH("Sequencer/GenericSectionBackground", FMargin(5.f / 16.f)));
		Set("Sequencer.Timeline.NotifyAlignmentMarker", new IMAGE_BRUSH("Sequencer/NotifyAlignmentMarker", FVector2D(10, 19)));
		Set("Sequencer.Timeline.PlayRange_Top_L", new BOX_BRUSH("Sequencer/PlayRange_Top_L", FMargin(1.f, 0.5f, 0.f, 0.5f)));
		Set("Sequencer.Timeline.PlayRange_Top_R", new BOX_BRUSH("Sequencer/PlayRange_Top_R", FMargin(0.f, 0.5f, 1.f, 0.5f)));
		Set("Sequencer.Timeline.PlayRange_L", new BOX_BRUSH("Sequencer/PlayRange_L", FMargin(1.f, 0.5f, 0.f, 0.5f)));
		Set("Sequencer.Timeline.PlayRange_R", new BOX_BRUSH("Sequencer/PlayRange_R", FMargin(0.f, 0.5f, 1.f, 0.5f)));
		Set("Sequencer.Timeline.PlayRange_Bottom_L", new BOX_BRUSH("Sequencer/PlayRange_Bottom_L", FMargin(1.f, 0.5f, 0.f, 0.5f)));
		Set("Sequencer.Timeline.PlayRange_Bottom_R", new BOX_BRUSH("Sequencer/PlayRange_Bottom_R", FMargin(0.f, 0.5f, 1.f, 0.5f)));

		Set("Sequencer.Timeline.SubSequenceRangeHashL", new BORDER_BRUSH("Sequencer/SubSequenceRangeHashL", FMargin(1.f, 0.f, 0.f, 0.f)));
		Set("Sequencer.Timeline.SubSequenceRangeHashR", new BORDER_BRUSH("Sequencer/SubSequenceRangeHashR", FMargin(1.f, 0.f, 0.f, 0.f)));
		Set("Sequencer.Timeline.EaseInOut", new IMAGE_BRUSH("Sequencer/EaseInOut", FVector2D(128, 128)));
		Set("Sequencer.InterpLine", new BOX_BRUSH("Sequencer/InterpLine", FMargin(5.f / 7.f, 0.f, 0.f, 0.f)));

		Set("Sequencer.Transport.JumpToPreviousKey", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("/Sequencer/Transport_Bar/Previous_Frame_OFF", Icon24x24))
			.SetPressed(IMAGE_BRUSH("/Sequencer/Transport_Bar/Previous_Frame", Icon24x24))
			.SetHovered(IMAGE_BRUSH("/Sequencer/Transport_Bar/Previous_Frame_OFF", Icon24x24)));
		Set("Sequencer.Transport.JumpToNextKey", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("/Sequencer/Transport_Bar/Next_Frame_24x_OFF", Icon24x24))
			.SetPressed(IMAGE_BRUSH("/Sequencer/Transport_Bar/Next_Frame_24x", Icon24x24))
			.SetHovered(IMAGE_BRUSH("/Sequencer/Transport_Bar/Next_Frame_24x_OFF", Icon24x24)));
		Set("Sequencer.Transport.SetPlayStart", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("/Sequencer/Transport_Bar/Bracket_In_16x24_OFF", FVector2D(16, 24)))
			.SetPressed(IMAGE_BRUSH("/Sequencer/Transport_Bar/Bracket_In_16x24", FVector2D(16, 24)))
			.SetHovered(IMAGE_BRUSH("/Sequencer/Transport_Bar/Bracket_In_16x24_OFF", FVector2D(16, 24))));
		Set("Sequencer.Transport.SetPlayEnd", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("/Sequencer/Transport_Bar/Bracket_Out_16x24_OFF", FVector2D(16, 24)))
			.SetPressed(IMAGE_BRUSH("/Sequencer/Transport_Bar/Bracket_Out_16x24", FVector2D(16, 24)))
			.SetHovered(IMAGE_BRUSH("/Sequencer/Transport_Bar/Bracket_Out_16x24_OFF", FVector2D(16, 24))));

		Set("Sequencer.Transport.CloseButton", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("/Docking/CloseApp_Normal", Icon16x16))
			.SetPressed(IMAGE_BRUSH("/Docking/CloseApp_Pressed", Icon16x16))
			.SetHovered(IMAGE_BRUSH("/Docking/CloseApp_Hovered", Icon16x16)));

		Set("Sequencer.NotificationImage_AddedPlayMovieSceneEvent", new IMAGE_BRUSH("Old/Checkbox_checked", Icon16x16));

		Set("Sequencer.Save", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Save_48x", Icon48x48));
		Set("Sequencer.Save.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Save_48x", Icon24x24));
		Set("Sequencer.SaveAsterisk", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_SaveAsterisk_48x", Icon48x48));
		Set("Sequencer.SaveAsterisk.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_SaveAsterisk_48x", Icon24x24));
		Set("Sequencer.SaveAs", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_SaveAs_48x", Icon48x48));
		Set("Sequencer.SaveAs.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_SaveAs_48x", Icon24x24));
		Set("Sequencer.ImportFBX", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_ImportFBX_48x", Icon48x48));
		Set("Sequencer.ImportFBX.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_ImportFBX_48x", Icon24x24));
		Set("Sequencer.ExportFBX", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_ExportFBX_48x", Icon48x48));
		Set("Sequencer.ExportFBX.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_ExportFBX_48x", Icon24x24));
		Set("Sequencer.DiscardChanges", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Revert_24x", Icon48x48));
		Set("Sequencer.DiscardChanges.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Revert_24x", Icon24x24));
		Set("Sequencer.RestoreAnimatedState", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_RestoreAnimatedState_24x", Icon48x48));
		Set("Sequencer.RestoreAnimatedState.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_RestoreAnimatedState_24x", Icon24x24));
		Set("Sequencer.GenericGripLeft", new BOX_BRUSH("Sequencer/GenericGripLeft", FMargin(5.f / 16.f)));
		Set("Sequencer.GenericGripRight", new BOX_BRUSH("Sequencer/GenericGripRight", FMargin(5.f / 16.f)));
		Set("Sequencer.SectionArea.Background", new FSlateColorBrush(FStyleColors::White));

		Set("Sequencer.Section.Background", new BORDER_BRUSH(TEXT("Sequencer/SectionBackground"), FMargin(4.f / 16.f)));
		Set("Sequencer.Section.BackgroundTint", new BOX_BRUSH(TEXT("Sequencer/SectionBackgroundTint"), FMargin(4 / 16.f)));
		Set("Sequencer.Section.SelectedSectionOverlay", new IMAGE_BRUSH(TEXT("Sequencer/SelectedSectionOverlay"), Icon16x16, FLinearColor::White, ESlateBrushTileType::Both));
		Set("Sequencer.Section.SelectedTrackTint", new BOX_BRUSH(TEXT("Sequencer/SelectedTrackTint"), FMargin(0.f, 0.5f)));
		Set("Sequencer.Section.SelectionBorder", new BORDER_BRUSH(TEXT("Sequencer/SectionHighlight"), FMargin(7.f / 16.f)));
		Set("Sequencer.Section.LockedBorder", new BORDER_BRUSH(TEXT("Sequencer/SectionLocked"), FMargin(7.f / 16.f)));
		Set("Sequencer.Section.SelectedSectionOverlay", new IMAGE_BRUSH(TEXT("Sequencer/SelectedSectionOverlay"), Icon16x16, FLinearColor::White, ESlateBrushTileType::Both));
		Set("Sequencer.Section.FilmBorder", new IMAGE_BRUSH(TEXT("Sequencer/SectionFilmBorder"), FVector2D(10, 7), FLinearColor::White, ESlateBrushTileType::Horizontal));
		Set("Sequencer.Section.GripLeft", new BOX_BRUSH("Sequencer/SectionGripLeft", FMargin(5.f / 16.f)));
		Set("Sequencer.Section.GripRight", new BOX_BRUSH("Sequencer/SectionGripRight", FMargin(5.f / 16.f)));
		Set("Sequencer.Section.EasingHandle", new IMAGE_BRUSH("Sequencer/EasingHandle", FVector2D(10.f, 10.f)));

		Set("Sequencer.Section.PreRoll", new BORDER_BRUSH(TEXT("Sequencer/PreRoll"), FMargin(0.f, .5f, 0.f, .5f)));

		Set("Sequencer.Section.PinCusion", new IMAGE_BRUSH(TEXT("Sequencer/PinCusion"), Icon16x16, FLinearColor::White, ESlateBrushTileType::Both));
		Set("Sequencer.Section.OverlapBorder", new BORDER_BRUSH(TEXT("Sequencer/OverlapBorder"), FMargin(1.f / 4.f, 0.f)));
		Set("Sequencer.Section.StripeOverlay", new BOX_BRUSH("Sequencer/SectionStripeOverlay", FMargin(0.f, .5f)));
		Set("Sequencer.Section.BackgroundText", DEFAULT_FONT("Bold", 24));
		Set("Sequencer.Section.EmptySpace", new BOX_BRUSH(TEXT("Sequencer/EmptySpace"), FMargin(0.f, 7.f / 14.f)));

		Set("Sequencer.ExposedNamePill_BG", new BOX_BRUSH(TEXT("Sequencer/ExposedNamePill_BG"), FMargin(14.f / 30.f), FLinearColor(1.f, 1.f, 1.f, .8f)));
		Set("Sequencer.ExposedNamePill", FButtonStyle()
			.SetNormal(  BOX_BRUSH(TEXT("Sequencer/ExposedNamePill_BG"), FMargin(14.f / 30.f), FLinearColor(1.f, 1.f, 1.f, .8f)) )
			.SetHovered( BOX_BRUSH(TEXT("Sequencer/ExposedNamePill_BG_Hovered"), FMargin(14.f / 30.f), FLinearColor::White) )
			.SetPressed( BOX_BRUSH(TEXT("Sequencer/ExposedNamePill_BG_Pressed"), FMargin(14.f / 30.f), FLinearColor::White) )
			.SetNormalPadding( FMargin(0,0,0,0) )
			.SetPressedPadding( FMargin(0,0,0,0) )
		);
		Set("Sequencer.ExposedNamePill.Input", FEditableTextBoxStyle()
			.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor::Transparent))
			.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor::Transparent))
			.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor::Transparent))
			.SetBackgroundImageReadOnly(FSlateColorBrush(FLinearColor::Transparent))
		);


		Set("Sequencer.AnimationOutliner.ColorStrip", FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateNoResource())
			.SetPressed(FSlateNoResource())
			.SetNormalPadding(FMargin(0, 0, 0, 0))
			.SetPressedPadding(FMargin(0, 0, 0, 0))
		);

		Set("Sequencer.AnimationOutliner.TopLevelBorder_Expanded", new BOX_BRUSH("Sequencer/TopLevelNodeBorder_Expanded", FMargin(4.0f / 16.0f)));
		Set("Sequencer.AnimationOutliner.TopLevelBorder_Collapsed", new BOX_BRUSH("Sequencer/TopLevelNodeBorder_Collapsed", FMargin(4.0f / 16.0f)));
		Set("Sequencer.AnimationOutliner.DefaultBorder", new FSlateColorBrush(FLinearColor::White));
		Set("Sequencer.AnimationOutliner.TransparentBorder", new FSlateColorBrush(FLinearColor::Transparent));
		Set("Sequencer.AnimationOutliner.BoldFont", DEFAULT_FONT("Bold", 11));
		Set("Sequencer.AnimationOutliner.RegularFont", DEFAULT_FONT("Regular", 9));
		Set("Sequencer.AnimationOutliner.ItalicFont", DEFAULT_FONT("Italic", 10));
		Set("Sequencer.ShotFilter", new IMAGE_BRUSH("Sequencer/FilteredArea", FVector2D(74, 74), FLinearColor::White, ESlateBrushTileType::Both));
		Set("Sequencer.KeyMark", new IMAGE_BRUSH("Sequencer/KeyMark", FVector2D(3, 21), FLinearColor::White, ESlateBrushTileType::NoTile));
		Set("Sequencer.ToggleAutoKeyEnabled", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Key_24x", Icon48x48));
		Set("Sequencer.ToggleAutoKeyEnabled.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Key_24x", Icon24x24));
		Set("Sequencer.SetAutoKey", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Key_24x", Icon48x48));
		Set("Sequencer.SetAutoKey.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Key_24x", Icon24x24));
		Set("Sequencer.SetAutoTrack", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Track_24x", Icon48x48));
		Set("Sequencer.SetAutoTrack.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Track_24x", Icon24x24));
		Set("Sequencer.SetAutoChangeAll", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Key_All_24x", Icon48x48));
		Set("Sequencer.SetAutoChangeAll.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Auto_Key_All_24x", Icon24x24));
		Set("Sequencer.SetAutoChangeNone", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Disable_Auto_Key_24x", Icon48x48));
		Set("Sequencer.SetAutoChangeNone.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Disable_Auto_Key_24x", Icon24x24));
		Set("Sequencer.AllowAllEdits", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Allow_All_Edits_24x", Icon48x48));
		Set("Sequencer.AllowAllEdits.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Allow_All_Edits_24x", Icon24x24));
		Set("Sequencer.AllowSequencerEditsOnly", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Allow_Sequencer_Edits_Only_24x", Icon48x48));
		Set("Sequencer.AllowSequencerEditsOnly.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Allow_Sequencer_Edits_Only_24x", Icon24x24));
		Set("Sequencer.AllowLevelEditsOnly", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Allow_Level_Edits_Only_24x", Icon48x48));
		Set("Sequencer.AllowLevelEditsOnly.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Allow_Level_Edits_Only_24x", Icon24x24));
		Set("Sequencer.SetKeyAll", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Key_All_24x", Icon48x48));
		Set("Sequencer.SetKeyAll.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Key_All_24x", Icon24x24));
		Set("Sequencer.SetKeyGroup", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Key_Group_24x", Icon48x48));
		Set("Sequencer.SetKeyGroup.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Key_Group_24x", Icon24x24));
		Set("Sequencer.SetKeyChanged", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Key_Part_24x", Icon48x48));
		Set("Sequencer.SetKeyChanged.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Key_Part_24x", Icon24x24));
		Set("Sequencer.ToggleIsSnapEnabled", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Snap_24x", Icon48x48));
		Set("Sequencer.ToggleIsSnapEnabled.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Snap_24x", Icon24x24));
		Set("Sequencer.ToggleShowCurveEditor", new IMAGE_BRUSH("GenericCurveEditor/Icons/GenericCurveEditor_48x", Icon48x48));
		Set("Sequencer.ToggleShowCurveEditor.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/GenericCurveEditor_48x", Icon24x24));
		Set("Sequencer.OpenDirectorBlueprint", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_OpenDirectorBlueprint_24x", Icon48x48));
		Set("Sequencer.OpenDirectorBlueprint.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_OpenDirectorBlueprint_24x", Icon24x24));
		Set("Sequencer.OpenTaggedBindingManager", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_OpenTaggedBindingManager_16x", Icon48x48));
		Set("Sequencer.OpenTaggedBindingManager.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_OpenTaggedBindingManager_16x", Icon24x24));
		Set("Sequencer.OpenNodeGroupsManager", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_OpenGroupManager_16x", Icon48x48));
		Set("Sequencer.OpenNodeGroupsManager.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_OpenGroupManager_16x", Icon24x24));

		Set("Sequencer.ToggleAutoScroll", new IMAGE_BRUSH("Icons/icon_Sequencer_ToggleAutoScroll_40x", Icon48x48));
		Set("Sequencer.ToggleAutoScroll.Small", new IMAGE_BRUSH("Icons/icon_Sequencer_ToggleAutoScroll_16x", Icon16x16));
		Set("Sequencer.MoveTool.Small", new IMAGE_BRUSH("Icons/SequencerIcons/icon_Sequencer_Move_24x", Icon16x16));
		Set("Sequencer.MarqueeTool.Small", new IMAGE_BRUSH("Icons/SequencerIcons/icon_Sequencer_Marquee_24x", Icon16x16));
		Set("Sequencer.RenderMovie.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Create_Movie_24x", Icon24x24));
		Set("Sequencer.CreateCamera.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Create_Camera_24x", Icon24x24));
		Set("Sequencer.FindInContentBrowser.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Find_In_Content_Browser_24x", Icon24x24));
		Set("Sequencer.LockCamera", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Look_Thru_24x", Icon16x16));
		Set("Sequencer.UnlockCamera", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Look_Thru_24x", Icon16x16, FLinearColor(1.f, 1.f, 1.f, 0.5f)));
		Set("Sequencer.Thumbnail.SectionHandle", new IMAGE_BRUSH("Old/White", Icon16x16, FLinearColor::Black));
		Set("Sequencer.TrackHoverHighlight_Top", new IMAGE_BRUSH(TEXT("Sequencer/TrackHoverHighlight_Top"), FVector2D(4, 4)));
		Set("Sequencer.TrackHoverHighlight_Bottom", new IMAGE_BRUSH(TEXT("Sequencer/TrackHoverHighlight_Bottom"), FVector2D(4, 4)));
		Set("Sequencer.SpawnableIconOverlay", new IMAGE_BRUSH(TEXT("Sequencer/SpawnableIconOverlay"), FVector2D(13, 13)));
		Set("Sequencer.MultipleIconOverlay", new IMAGE_BRUSH(TEXT("Sequencer/MultipleIconOverlay"), FVector2D(13, 13)));
		Set("Sequencer.LockSequence", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Locked_16x", Icon16x16));
		Set("Sequencer.UnlockSequence", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Unlocked_16x", Icon16x16));

		Set("Sequencer.Actions", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Actions_24x", Icon48x48));
		Set("Sequencer.Actions.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Actions_24x", Icon24x24));
		Set("Sequencer.ViewOptions", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_View_Options_24x", Icon48x48));
		Set("Sequencer.ViewOptions.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_View_Options_24x", Icon24x24));
		Set("Sequencer.PlaybackOptions", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Playback_Options_24x", Icon48x48));
		Set("Sequencer.PlaybackOptions.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Playback_Options_24x", Icon24x24));
		Set("Sequencer.SelectEditOptions", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_SelectEdit_Options_24x", Icon48x48));
		Set("Sequencer.SelectEditOptions.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_SelectEdit_Options_24x", Icon24x24));
		Set("Sequencer.Time", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Time_24x", Icon48x48));
		Set("Sequencer.Time.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Time_24x", Icon24x24));
		Set("Sequencer.Value", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Value_24x", Icon48x48));
		Set("Sequencer.Value.Small", new IMAGE_BRUSH("Sequencer/Main_Icons/Icon_Sequencer_Value_24x", Icon24x24));

		Set("Sequencer.OverlayPanel.Background", new BOX_BRUSH("Sequencer/OverlayPanelBackground", FMargin(26.f / 54.f)));

		Set("Sequencer.TrackArea.LaneColor", FLinearColor(0.3f, 0.3f, 0.3f, 0.3f));

		Set("Sequencer.Tracks.Media", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Media_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.Audio", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Audio_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.Event", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Event_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.Fade", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Fade_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.CameraCut", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Camera_Cut_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.CinematicShot", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Shot_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.Slomo", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Play_Rate_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.Sub", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Sub_Track_16x", Icon16x16));
		Set("Sequencer.Tracks.LevelVisibility", new IMAGE_BRUSH("Sequencer/Dropdown_Icons/Icon_Level_Visibility_Track_16x", Icon16x16));

		Set("Sequencer.CursorDecorator_MarqueeAdd", new IMAGE_BRUSH("Sequencer/CursorDecorator_MarqueeAdd", Icon16x16));
		Set("Sequencer.CursorDecorator_MarqueeSubtract", new IMAGE_BRUSH("Sequencer/CursorDecorator_MarqueeSubtract", Icon16x16));

		Set("Sequencer.BreadcrumbText", FTextBlockStyle(NormalText).SetFont(FStyleFonts::Get().NormalBold));
		Set("Sequencer.BreadcrumbIcon", new IMAGE_BRUSH("Common/SmallArrowRight", Icon10x10));

		Set("Sequencer.AddKey.Details", new IMAGE_BRUSH("Sequencer/AddKey_Details", FVector2D(11, 11)));

		const FSplitterStyle OutlinerSplitterStyle = FSplitterStyle()
		.SetHandleNormalBrush(FSlateNoResource())
		.SetHandleHighlightBrush(FSlateNoResource());
		Set("Sequencer.AnimationOutliner.Splitter", OutlinerSplitterStyle);

		Set("Sequencer.HyperlinkSpinBox", FSpinBoxStyle(GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
			.SetTextPadding(FMargin(0))
			.SetBackgroundBrush(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0, 0, 0, 3 / 16.0f), FSlateColor::UseSubduedForeground()))
			.SetHoveredBackgroundBrush(FSlateNoResource())
			.SetInactiveFillBrush(FSlateNoResource())
			.SetActiveFillBrush(FSlateNoResource())
			.SetForegroundColor(FSlateColor::UseSubduedForeground())
			.SetArrowsImage(FSlateNoResource())
		);

		Set("Sequencer.PlayTimeSpinBox", FSpinBoxStyle(GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
			.SetTextPadding(FMargin(0))
			.SetBackgroundBrush(FSlateNoResource())
			.SetHoveredBackgroundBrush(FSlateNoResource())
			.SetInactiveFillBrush(FSlateNoResource())
			.SetActiveFillBrush(FSlateNoResource())
			.SetForegroundColor(SelectionColor_Pressed)
			.SetArrowsImage(FSlateNoResource())
		);

		Set("Sequencer.HyperlinkTextBox", FEditableTextBoxStyle()
			.SetFont(DEFAULT_FONT("Regular", 9))
			.SetBackgroundImageNormal(FSlateNoResource())
			.SetBackgroundImageHovered(FSlateNoResource())
			.SetBackgroundImageFocused(FSlateNoResource())
			.SetBackgroundImageReadOnly(FSlateNoResource())
			.SetBackgroundColor(FLinearColor::Transparent)
			.SetForegroundColor(FSlateColor::UseSubduedForeground())
		);
		Set("Sequencer.FixedFont", DEFAULT_FONT("Mono", 9));

		Set("Sequencer.RecordSelectedActors", new IMAGE_BRUSH("SequenceRecorder/icon_tab_SequenceRecorder_16x", Icon16x16));

		FComboButtonStyle SequencerSectionComboButton = FComboButtonStyle()
		.SetButtonStyle(
			FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateNoResource())
			.SetPressed(FSlateNoResource())
			.SetNormalPadding(FMargin(0, 0, 0, 0))
			.SetPressedPadding(FMargin(0, 1, 0, 0))
		)
		.SetDownArrowImage(IMAGE_BRUSH("Common/ComboArrow", Icon8x8));
		Set("Sequencer.SectionComboButton", SequencerSectionComboButton);

		Set("Sequencer.CreateEventBinding", new IMAGE_BRUSH("Icons/icon_Blueprint_AddFunction_16px", Icon16x16));
		Set("Sequencer.CreateQuickBinding", new IMAGE_BRUSH("Icons/icon_Blueprint_Node_16x", Icon16x16));
		Set("Sequencer.ClearEventBinding", new IMAGE_BRUSH("Icons/Edit/icon_Edit_Delete_40x", Icon16x16));
		Set("Sequencer.MultipleEvents", new IMAGE_BRUSH("Sequencer/MultipleEvents", Icon16x16));
		Set("Sequencer.UnboundEvent", new IMAGE_BRUSH("Sequencer/UnboundEvent", Icon16x16));

		// Sequencer Blending Iconography
		Set("EMovieSceneBlendType::Absolute", new IMAGE_BRUSH("Sequencer/EMovieSceneBlendType_Absolute", FVector2D(32, 16)));
		Set("EMovieSceneBlendType::Relative", new IMAGE_BRUSH("Sequencer/EMovieSceneBlendType_Relative", FVector2D(32, 16)));
		Set("EMovieSceneBlendType::Additive", new IMAGE_BRUSH("Sequencer/EMovieSceneBlendType_Additive", FVector2D(32, 16)));
	}


	// Sequence recorder standalone UI
	if (IncludeEditorSpecificStyles())
	{
		Set("SequenceRecorder.TabIcon", new IMAGE_BRUSH_SVG("Starship/Sequencer/SequenceRecorder", Icon16x16));
		Set("SequenceRecorder.Common.RecordAll.Small", new IMAGE_BRUSH("SequenceRecorder/icon_RecordAll_40x", Icon20x20));
		Set("SequenceRecorder.Common.RecordAll", new IMAGE_BRUSH("SequenceRecorder/icon_RecordAll_40x", Icon40x40));
		Set("SequenceRecorder.Common.StopAll.Small", new IMAGE_BRUSH("SequenceRecorder/icon_StopAll_40x", Icon20x20));
		Set("SequenceRecorder.Common.StopAll", new IMAGE_BRUSH("SequenceRecorder/icon_StopAll_40x", Icon40x40));
		Set("SequenceRecorder.Common.AddRecording.Small", new IMAGE_BRUSH("SequenceRecorder/icon_AddRecording_40x", Icon20x20));
		Set("SequenceRecorder.Common.AddRecording", new IMAGE_BRUSH("SequenceRecorder/icon_AddRecording_40x", Icon40x40));
		Set("SequenceRecorder.Common.AddCurrentPlayerRecording.Small", new IMAGE_BRUSH("SequenceRecorder/icon_AddCurrentPlayerRecording_40x", Icon20x20));
		Set("SequenceRecorder.Common.AddCurrentPlayerRecording", new IMAGE_BRUSH("SequenceRecorder/icon_AddCurrentPlayerRecording_40x", Icon40x40));
		Set("SequenceRecorder.Common.RemoveRecording.Small", new IMAGE_BRUSH("SequenceRecorder/icon_RemoveRecording_40x", Icon20x20));
		Set("SequenceRecorder.Common.RemoveRecording", new IMAGE_BRUSH("SequenceRecorder/icon_RemoveRecording_40x", Icon40x40));
		Set("SequenceRecorder.Common.RemoveAllRecordings.Small", new IMAGE_BRUSH("SequenceRecorder/icon_RemoveRecording_40x", Icon20x20));
		Set("SequenceRecorder.Common.RemoveAllRecordings", new IMAGE_BRUSH("SequenceRecorder/icon_RemoveRecording_40x", Icon40x40));
		Set("SequenceRecorder.Common.RecordingActive", new IMAGE_BRUSH("Common/SmallCheckBox_Checked", Icon14x14));
		Set("SequenceRecorder.Common.RecordingInactive", new IMAGE_BRUSH("Common/SmallCheckBox", Icon14x14));
	}

}

void FStarshipEditorStyle::FStyle::SetupViewportStyles()
{
	{
		FToolBarStyle ViewportToolbarStyle = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		FLinearColor TransInput = FStyleColors::Input.GetSpecifiedColor();
		TransInput.A = .71;

		FMargin ViewportMargin(4.f, 4.f);

		FSlateRoundedBoxBrush* ViewportGroupBrush = new FSlateRoundedBoxBrush(TransInput, 12.f, FStyleColors::Dropdown, 1.0);
		Set("EditorViewportToolBar.Group", ViewportGroupBrush);

		FButtonStyle ViewportMenuButton = FButtonStyle()
			.SetNormal(*ViewportGroupBrush)
			.SetHovered(*ViewportGroupBrush)
			.SetPressed(*ViewportGroupBrush)
			.SetNormalForeground(FStyleColors::Foreground)
			.SetHoveredForeground(FStyleColors::ForegroundHover)
			.SetPressedForeground(FStyleColors::ForegroundHover)
			.SetDisabledForeground(FStyleColors::Foreground)
			.SetNormalPadding(ViewportMargin)
			.SetPressedPadding(ViewportMargin);
		Set("EditorViewportToolBar.Button", ViewportMenuButton);

		const FCheckBoxStyle ViewportMenuToggleLeftButtonStyle = FCheckBoxStyle(ViewportToolbarStyle.ToggleButton)
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(		  BOX_BRUSH("Starship/EditorViewport/ToolBarLeftGroup", 12.f/25.f))
			.SetUncheckedPressedImage(BOX_BRUSH("Starship/EditorViewport/ToolBarLeftGroup", 12.f/25.f))
			.SetUncheckedHoveredImage(BOX_BRUSH("Starship/EditorViewport/ToolBarLeftGroup", 12.f/25.f))
			.SetCheckedHoveredImage(  BOX_BRUSH("Starship/EditorViewport/ToolBarLeftGroup", 12.f/25.f))
			.SetCheckedPressedImage(  BOX_BRUSH("Starship/EditorViewport/ToolBarLeftGroup", 12.f/25.f))
			.SetCheckedImage(         BOX_BRUSH("Starship/EditorViewport/ToolBarLeftGroup", 12.f/25.f))
			.SetPadding(ViewportMargin);
		Set("EditorViewportToolBar.ToggleButton.Start", ViewportMenuToggleLeftButtonStyle);

		const FCheckBoxStyle ViewportMenuToggleMiddleButtonStyle = FCheckBoxStyle(ViewportToolbarStyle.ToggleButton)
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(		  BOX_BRUSH("Starship/EditorViewport/ToolBarMiddleGroup", 12.f/25.f))
			.SetUncheckedPressedImage(BOX_BRUSH("Starship/EditorViewport/ToolBarMiddleGroup", 12.f/25.f))
			.SetUncheckedHoveredImage(BOX_BRUSH("Starship/EditorViewport/ToolBarMiddleGroup", 12.f/25.f))
			.SetCheckedHoveredImage(  BOX_BRUSH("Starship/EditorViewport/ToolBarMiddleGroup", 12.f/25.f))
			.SetCheckedPressedImage(  BOX_BRUSH("Starship/EditorViewport/ToolBarMiddleGroup", 12.f/25.f))
			.SetCheckedImage(         BOX_BRUSH("Starship/EditorViewport/ToolBarMiddleGroup", 12.f/25.f))
			.SetPadding(ViewportMargin);
		Set("EditorViewportToolBar.ToggleButton.Middle", ViewportMenuToggleMiddleButtonStyle);

		const FCheckBoxStyle ViewportMenuToggleRightButtonStyle = FCheckBoxStyle(ViewportToolbarStyle.ToggleButton)
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(		  BOX_BRUSH("Starship/EditorViewport/ToolBarRightGroup", 12.f/25.f))
			.SetUncheckedPressedImage(BOX_BRUSH("Starship/EditorViewport/ToolBarRightGroup", 12.f/25.f))
			.SetUncheckedHoveredImage(BOX_BRUSH("Starship/EditorViewport/ToolBarRightGroup", 12.f/25.f))
			.SetCheckedHoveredImage(  BOX_BRUSH("Starship/EditorViewport/ToolBarRightGroup", 12.f/25.f))
			.SetCheckedPressedImage(  BOX_BRUSH("Starship/EditorViewport/ToolBarRightGroup", 12.f/25.f))
			.SetCheckedImage(         BOX_BRUSH("Starship/EditorViewport/ToolBarRightGroup", 12.f/25.f))
			.SetPadding(ViewportMargin);
		Set("EditorViewportToolBar.ToggleButton.End", ViewportMenuToggleRightButtonStyle);

		// We want a background-less version as the ComboMenu has its own unified background
		const FToolBarStyle& SlimCoreToolBarStyle = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
		Set("EditorViewportToolBar.ComboMenu.ButtonStyle", FButtonStyle(SlimCoreToolBarStyle.ButtonStyle).SetNormalPadding(0.0).SetPressedPadding(0.0));
		Set("EditorViewportToolBar.ComboMenu.ToggleButton", FCheckBoxStyle(SlimCoreToolBarStyle.ToggleButton).SetPadding(0.0));
		Set("EditorViewportToolBar.ComboMenu.LabelStyle", SlimCoreToolBarStyle.LabelStyle);

		FCheckBoxStyle MaximizeRestoreButton = FCheckBoxStyle(ViewportToolbarStyle.ToggleButton)
			.SetUncheckedImage(*ViewportGroupBrush)
			.SetUncheckedPressedImage(*ViewportGroupBrush)
			.SetUncheckedHoveredImage(*ViewportGroupBrush)
			.SetCheckedImage(*ViewportGroupBrush)
			.SetCheckedHoveredImage(*ViewportGroupBrush)
			.SetCheckedPressedImage(*ViewportGroupBrush)
			.SetForegroundColor(FStyleColors::Foreground)
			.SetPressedForegroundColor(FStyleColors::ForegroundHover)
			.SetHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetCheckedForegroundColor(FStyleColors::Foreground)
			.SetCheckedPressedForegroundColor(FStyleColors::ForegroundHover)
			.SetCheckedHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetPadding(ViewportMargin);
		Set("EditorViewportToolBar.MaximizeRestoreButton", MaximizeRestoreButton);

		Set("EditorViewportToolBar.Heading.Padding", FMargin(4.f));


		// SComboBox 
		FComboButtonStyle ViewportComboButton = FComboButtonStyle()
			.SetButtonStyle(ViewportMenuButton)
			.SetContentPadding(ViewportMargin);

		ViewportToolbarStyle
			.SetBackground(FSlateNoResource())
			.SetIconSize(Icon16x16)
			.SetBackgroundPadding(FMargin(0))
			.SetLabelPadding(FMargin(0))
			.SetComboButtonPadding(FMargin(4.f, 0.0f))
			.SetBlockPadding(FMargin(0.0f,0.0f))
			.SetIndentedBlockPadding(FMargin(0))
			.SetButtonPadding(FMargin(0))
			.SetCheckBoxPadding(FMargin(4.0f, 0.0f))
			.SetComboButtonStyle(ViewportComboButton)
			.SetButtonStyle(ViewportMenuButton)
			.SetSeparatorBrush(FSlateNoResource())
			.SetSeparatorPadding(FMargin(2.0f, 0.0f))
			.SetExpandBrush(IMAGE_BRUSH("Icons/toolbar_expand_16x", Icon8x8));
		Set("EditorViewportToolBar", ViewportToolbarStyle);

		FButtonStyle ViewportMenuWarningButton = FButtonStyle(ViewportMenuButton)
			.SetNormalForeground(FStyleColors::AccentYellow)
			.SetHoveredForeground(FStyleColors::ForegroundHover)
			.SetPressedForeground(FStyleColors::ForegroundHover)
			.SetDisabledForeground(FStyleColors::AccentYellow);
		Set("EditorViewportToolBar.WarningButton", ViewportMenuWarningButton);

		FLinearColor ToolbarBackgroundColor = FStyleColors::Foldout.GetSpecifiedColor();
		ToolbarBackgroundColor.A = .75f;

		Set("EditorViewportToolBar.Background", new FSlateNoResource());
		Set("EditorViewportToolBar.OptionsDropdown", new IMAGE_BRUSH_SVG("Starship/EditorViewport/menu", Icon16x16));

		Set("EditorViewportToolBar.Font", FStyleFonts::Get().Normal);

		Set("EditorViewportToolBar.MenuButton", FButtonStyle(Button)
			.SetNormal(BOX_BRUSH("Common/SmallRoundedButton", FMargin(7.f / 16.f), FLinearColor(1, 1, 1, 0.75f)))
			.SetHovered(BOX_BRUSH("Common/SmallRoundedButton", FMargin(7.f / 16.f), FLinearColor(1, 1, 1, 1.0f)))
			.SetPressed(BOX_BRUSH("Common/SmallRoundedButton", FMargin(7.f / 16.f)))
		);

		
		Set("EditorViewportToolBar.MenuDropdown", new IMAGE_BRUSH("Common/ComboArrow", Icon8x8));
		Set("EditorViewportToolBar.Maximize.Normal", new IMAGE_BRUSH_SVG("Starship/EditorViewport/square", Icon16x16));
		Set("EditorViewportToolBar.Maximize.Checked", new IMAGE_BRUSH_SVG("Starship/EditorViewport/quad", Icon16x16));
		Set("EditorViewportToolBar.RestoreFromImmersive.Normal", new IMAGE_BRUSH("Icons/icon_RestoreFromImmersive_16px", Icon16x16));

		FLinearColor ViewportOverlayColor = FStyleColors::Input.GetSpecifiedColor();
		ViewportOverlayColor.A = 0.75f;

		Set("EditorViewport.OverlayBrush", new FSlateRoundedBoxBrush(ViewportOverlayColor, 8.0, FStyleColors::Dropdown, 1.0));

	}

	// Legacy Viewport ToolbarBar
	{
		FToolBarStyle ViewportToolbarStyle =
			FToolBarStyle()
			.SetBackground(BOX_BRUSH("Old/Menu_Background", FMargin(8.0f / 64.0f), FLinearColor::Transparent))
			.SetExpandBrush(IMAGE_BRUSH("Icons/toolbar_expand_16x", Icon8x8))
			.SetComboButtonPadding(FMargin(0))
			.SetButtonPadding(FMargin(0))
			.SetCheckBoxPadding(FMargin(4))
			.SetSeparatorBrush(BOX_BRUSH("Old/Button", 8.0f / 32.0f, FLinearColor::Transparent))
			.SetSeparatorPadding(FMargin(1.f, 0.f, 0.f, 0.f))
			.SetIconSize(Icon16x16)
			.SetLabelPadding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
			.SetEditableTextStyle(FEditableTextBoxStyle(NormalEditableTextBoxStyle).SetFont(DEFAULT_FONT("Regular", 9)))
			.SetIndentedBlockPadding(FMargin(0))
			.SetBlockPadding(FMargin(0))
			.SetLabelStyle(
				FTextBlockStyle(NormalText)
				.SetFont(DEFAULT_FONT("Bold", 9))
				.SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
			);
		
		const FString SmallRoundedButton(TEXT("Common/SmallRoundedButton"));
		const FString SmallRoundedButtonStart(TEXT("Common/SmallRoundedButtonLeft"));
		const FString SmallRoundedButtonMiddle(TEXT("Common/SmallRoundedButtonCentre"));
		const FString SmallRoundedButtonEnd(TEXT("Common/SmallRoundedButtonRight"));

		const FLinearColor NormalColor(1, 1, 1, 0.75f);
		const FLinearColor PressedColor(1, 1, 1, 1.f);

		/* Create style for "LegacyViewportMenu.ToggleButton" ... */
		const FCheckBoxStyle ViewportMenuToggleButtonStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), PressedColor))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), PressedColor))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor_Pressed));
		/* ... and add new style */
		ViewportToolbarStyle.SetToggleButtonStyle(ViewportMenuToggleButtonStyle);

		/* Create style for "LegacyViewportMenu.ToggleButton.Start" ... */
		const FCheckBoxStyle ViewportMenuToggleStartButtonStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), PressedColor))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), PressedColor))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor_Pressed));
		/* ... and add new style */
		Set("LegacyViewportMenu.ToggleButton.Start", ViewportMenuToggleStartButtonStyle);

		/* Create style for "LegacyViewportMenu.ToggleButton.Middle" ... */
		const FCheckBoxStyle ViewportMenuToggleMiddleButtonStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), PressedColor))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), PressedColor))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor_Pressed));
		/* ... and add new style */
		Set("LegacyViewportMenu.ToggleButton.Middle", ViewportMenuToggleMiddleButtonStyle);

		/* Create style for "LegacyViewportMenu.ToggleButton.End" ... */
		const FCheckBoxStyle ViewportMenuToggleEndButtonStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), PressedColor))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), PressedColor))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor_Pressed));
		/* ... and add new style */
		Set("LegacyViewportMenu.ToggleButton.End", ViewportMenuToggleEndButtonStyle);

		const FMargin NormalPadding = FMargin(4.0f, 4.0f, 4.0f, 4.0f);
		const FMargin PressedPadding = FMargin(4.0f, 4.0f, 4.0f, 4.0f);

		const FButtonStyle ViewportMenuButton = FButtonStyle(Button)
		.SetNormal(BOX_BRUSH(*SmallRoundedButton, 7.0f / 16.0f, NormalColor))
		.SetPressed(BOX_BRUSH(*SmallRoundedButton, 7.0f / 16.0f, PressedColor))
		.SetHovered(BOX_BRUSH(*SmallRoundedButton, 7.0f / 16.0f, PressedColor))
		.SetPressedPadding(PressedPadding)
		.SetNormalPadding(NormalPadding);

		ViewportToolbarStyle.SetButtonStyle(ViewportMenuButton);

		Set("LegacyViewportMenu.Button.Start", FButtonStyle(ViewportMenuButton)
			.SetNormal(BOX_BRUSH(*SmallRoundedButtonStart, 7.0f / 16.0f, NormalColor))
			.SetPressed(BOX_BRUSH(*SmallRoundedButtonStart, 7.0f / 16.0f, PressedColor))
			.SetHovered(BOX_BRUSH(*SmallRoundedButtonStart, 7.0f / 16.0f, PressedColor))
		);

		Set("LegacyViewportMenu.Button.Middle", FButtonStyle(ViewportMenuButton)
			.SetNormal(BOX_BRUSH(*SmallRoundedButtonMiddle, 7.0f / 16.0f, NormalColor))
			.SetPressed(BOX_BRUSH(*SmallRoundedButtonMiddle, 7.0f / 16.0f, PressedColor))
			.SetHovered(BOX_BRUSH(*SmallRoundedButtonMiddle, 7.0f / 16.0f, PressedColor))
		);

		Set("LegacyViewportMenu.Button.End", FButtonStyle(ViewportMenuButton)
			.SetNormal(BOX_BRUSH(*SmallRoundedButtonEnd, 7.0f / 16.0f, NormalColor))
			.SetPressed(BOX_BRUSH(*SmallRoundedButtonEnd, 7.0f / 16.0f, PressedColor))
			.SetHovered(BOX_BRUSH(*SmallRoundedButtonEnd, 7.0f / 16.0f, PressedColor))
		);

		Set("LegacyViewportMenu", ViewportToolbarStyle);
	}

	// Viewport actor preview's pin/unpin and attach/detach buttons
	{
		Set("ViewportActorPreview.Pinned", new IMAGE_BRUSH("Common/PushPin_Down", Icon16x16));
		Set("ViewportActorPreview.Unpinned", new IMAGE_BRUSH("Common/PushPin_Up", Icon16x16));
		Set("VRViewportActorPreview.Pinned", new IMAGE_BRUSH("Common/PushPin_Down_VR", Icon64x64));
		Set("VRViewportActorPreview.Unpinned", new IMAGE_BRUSH("Common/PushPin_Up_VR", Icon64x64));
		Set("VRViewportActorPreview.Attached", new IMAGE_BRUSH("Common/ScreenAttach_VR", Icon64x64));
		Set("VRViewportActorPreview.Detached", new IMAGE_BRUSH("Common/ScreenDetach_VR", Icon64x64));
	}
}

void FStarshipEditorStyle::FStyle::SetupMenuBarStyles()
{
	// MenuBar
	{
		Set("Menu.Label.Padding", FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		Set("Menu.Label.ContentPadding", FMargin(10.0f, 2.0f));
	}
}

void FStarshipEditorStyle::FStyle::SetupGeneralIcons()
{
	Set("Plus", new IMAGE_BRUSH("Icons/PlusSymbol_12x", Icon12x12));
	Set("Cross", new IMAGE_BRUSH("Icons/Cross_12x", Icon12x12));
	Set("ArrowUp", new IMAGE_BRUSH("Icons/ArrowUp_12x", Icon12x12));
	Set("ArrowDown", new IMAGE_BRUSH("Icons/ArrowDown_12x", Icon12x12));
	Set("AssetEditor.SaveThumbnail", new IMAGE_BRUSH_SVG("Starship/AssetEditors/SaveThumbnail", Icon20x20));
	Set("AssetEditor.ToggleShowBounds", new IMAGE_BRUSH_SVG("Starship/Common/SetShowBounds", Icon20x20));
	Set("AssetEditor.Apply", new IMAGE_BRUSH_SVG("Starship/Common/Apply", Icon20x20));
	Set("AssetEditor.Simulate", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));
	Set("AssetEditor.ToggleStats", new IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon20x20));
	Set("AssetEditor.CompileStatus.Background", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Background", Icon20x20));
	Set("AssetEditor.CompileStatus.Overlay.Unknown", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Unknown_Badge", Icon20x20, FStyleColors::AccentYellow));
	Set("AssetEditor.CompileStatus.Overlay.Warning", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Warning_Badge", Icon20x20, FStyleColors::Warning));
	Set("AssetEditor.CompileStatus.Overlay.Good", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Good_Badge", Icon20x20, FStyleColors::AccentGreen));
	Set("AssetEditor.CompileStatus.Overlay.Error", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Fail_Badge", Icon20x20, FStyleColors::Error));

	Set("Debug", new IMAGE_BRUSH_SVG( "Starship/Common/Debug", Icon16x16 ) );
	Set("Modules", new IMAGE_BRUSH_SVG( "Starship/Common/Modules", Icon16x16 ) );
}

void FStarshipEditorStyle::FStyle::SetupWindowStyles()
{
	// Window styling
	{
		EditorWindowHighlightBrush = CORE_IMAGE_BRUSH("Common/Window/WindowTitle", FVector2D(74, 74), FLinearColor::White, ESlateBrushTileType::Horizontal);
	}
}

void FStarshipEditorStyle::FStyle::SetupProjectBadgeStyle()
{
	Set("SProjectBadge.Text", FTextBlockStyle(NormalText));
	Set("SProjectBadge.BadgeShape", new BOX_BRUSH("ProjectBadge/Badge", Icon16x16, FMargin(6.0f/16)));
	Set("SProjectBadge.BadgePadding", FMargin(32.0f, 6.0f, 32.0f, 7.0f));
}

void FStarshipEditorStyle::FStyle::SetupDockingStyles()
{
	// Use the Docking Styles defined in StarshipCoreStyle
}

void FStarshipEditorStyle::FStyle::SetupTutorialStyles()
{
	// Documentation tooltip defaults
	const FSlateColor HyperlinkColor( FLinearColor( 0.1f, 0.1f, 0.5f ) );
	{
		const FTextBlockStyle DocumentationTooltipText = FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Regular", 9 ) )
			.SetColorAndOpacity( FLinearColor::Black );
		Set("Documentation.SDocumentationTooltip", FTextBlockStyle(DocumentationTooltipText));

		const FTextBlockStyle DocumentationTooltipTextSubdued = FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Regular", 8 ) )
			.SetColorAndOpacity( FLinearColor( 0.1f, 0.1f, 0.1f ) );
		Set("Documentation.SDocumentationTooltipSubdued", FTextBlockStyle(DocumentationTooltipTextSubdued));

		const FTextBlockStyle DocumentationTooltipHyperlinkText = FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Regular", 8 ) )
			.SetColorAndOpacity( HyperlinkColor );
		Set("Documentation.SDocumentationTooltipHyperlinkText", FTextBlockStyle(DocumentationTooltipHyperlinkText));

		const FButtonStyle DocumentationTooltipHyperlinkButton = FButtonStyle()
				.SetNormal(BORDER_BRUSH( "Old/HyperlinkDotted", FMargin(0,0,0,3/16.0f), HyperlinkColor ) )
				.SetPressed(FSlateNoResource())
				.SetHovered(BORDER_BRUSH( "Old/HyperlinkUnderline", FMargin(0,0,0,3/16.0f), HyperlinkColor ) );
		Set("Documentation.SDocumentationTooltipHyperlinkButton", FButtonStyle(DocumentationTooltipHyperlinkButton));
	}


	// Documentation defaults
	const FTextBlockStyle DocumentationText = FTextBlockStyle(NormalText)
		.SetColorAndOpacity( FLinearColor::Black )
		.SetFont(DEFAULT_FONT( "Regular", 11 ));

	const FTextBlockStyle DocumentationHyperlinkText = FTextBlockStyle(DocumentationText)
		.SetColorAndOpacity( HyperlinkColor );

	const FTextBlockStyle DocumentationHeaderText = FTextBlockStyle(NormalText)
		.SetColorAndOpacity( FLinearColor::Black )
		.SetFont(DEFAULT_FONT("Black", 32));

	const FButtonStyle DocumentationHyperlinkButton = FButtonStyle()
			.SetNormal(BORDER_BRUSH( "Old/HyperlinkDotted", FMargin(0,0,0,3/16.0f), HyperlinkColor ) )
			.SetPressed(FSlateNoResource())
			.SetHovered(BORDER_BRUSH( "Old/HyperlinkUnderline", FMargin(0,0,0,3/16.0f), HyperlinkColor ) );

	// Documentation
	{
		Set( "Documentation.Content", FTextBlockStyle(DocumentationText) );

		const FHyperlinkStyle DocumentationHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(DocumentationHyperlinkButton)
			.SetTextStyle(DocumentationText)
			.SetPadding(FMargin(0.0f));
		Set("Documentation.Hyperlink", DocumentationHyperlink);

		Set("Documentation.Hyperlink.Button", FButtonStyle(DocumentationHyperlinkButton));
		Set("Documentation.Hyperlink.Text",   FTextBlockStyle(DocumentationHyperlinkText));
		Set("Documentation.NumberedContent",  FTextBlockStyle(DocumentationText));
		Set( "Documentation.BoldContent", FTextBlockStyle(DocumentationText)
			.SetTypefaceFontName(TEXT("Bold")));

		Set("Documentation.Header1", FTextBlockStyle(DocumentationHeaderText)
			.SetFontSize(32));

		Set("Documentation.Header2", FTextBlockStyle(DocumentationHeaderText)
			.SetFontSize(24));

		Set( "Documentation.Separator", new BOX_BRUSH( "Common/Separator", 1/4.0f, FLinearColor(1,1,1,0.5f) ) );
	}

	{
		Set("Documentation.ToolTip.Background", new BOX_BRUSH("Tutorials/TutorialContentBackground", FMargin(4 / 16.0f)));
	}

	// Tutorials
	{
		const FLinearColor TutorialButtonColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
		const FLinearColor TutorialSelectionColor = FLinearColor(0.19f, 0.33f, 0.72f);
		const FLinearColor TutorialNavigationButtonColor = FLinearColor(0.0f, 0.59f, 0.14f, 1.0f);
		const FLinearColor TutorialNavigationButtonHoverColor = FLinearColor(0.2f, 0.79f, 0.34f, 1.0f);
		const FLinearColor TutorialNavigationBackButtonColor = TutorialNavigationButtonColor;
		const FLinearColor TutorialNavigationBackButtonHoverColor = TutorialNavigationButtonHoverColor;

		const FTextBlockStyle TutorialText = FTextBlockStyle(DocumentationText)
			.SetColorAndOpacity(FLinearColor::Black)
			.SetHighlightColor(TutorialSelectionColor);

		const FTextBlockStyle TutorialHeaderText = FTextBlockStyle(DocumentationHeaderText)
			.SetColorAndOpacity(FLinearColor::Black)
			.SetHighlightColor(TutorialSelectionColor);

		Set("Tutorials.Border", new BOX_BRUSH( "Tutorials/OverlayFrame", FMargin(18.0f/64.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) );
		Set("Tutorials.Shadow", new BOX_BRUSH("Tutorials/TutorialShadow", FVector2D(256.0f, 256.0f), FMargin(114.0f / 256.0f)));

		const FTextBlockStyle TutorialBrowserText = FTextBlockStyle(TutorialText)
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetHighlightColor(TutorialSelectionColor);

		Set( "Tutorials.Browser.Text", TutorialBrowserText );

		Set( "Tutorials.Browser.WelcomeHeader", FTextBlockStyle(TutorialBrowserText)
			.SetFontSize(20));

		Set( "Tutorials.Browser.SummaryHeader", FTextBlockStyle(TutorialBrowserText)
			.SetFontSize(16));

		Set( "Tutorials.Browser.SummaryText", FTextBlockStyle(TutorialBrowserText)
			.SetFontSize(10));

		Set( "Tutorials.Browser.HighlightTextColor", TutorialSelectionColor );

		Set( "Tutorials.Browser.Button", FButtonStyle()
			.SetNormal( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0.05f,0.05f,0.05f,1) ) )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0.07f,0.07f,0.07f,1) ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0.08f,0.08f,0.08f,1) ) )
			.SetNormalPadding( FMargin(0,0,0,1))
			.SetPressedPadding( FMargin(0,1,0,0)));

		Set( "Tutorials.Browser.BackButton", FButtonStyle()
			.SetNormal( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1.0f,1.0f,1.0f,0.0f) ) )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1.0f,1.0f,1.0f,0.05f) ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1.0f,1.0f,1.0f,0.05f) ) )
			.SetNormalPadding( FMargin(0,0,0,1))
			.SetPressedPadding( FMargin(0,1,0,0)));

		Set( "Tutorials.Content.Button", FButtonStyle()
			.SetNormal( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0,0,0,0) ) )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,1) ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,1) ) )
			.SetNormalPadding( FMargin(0,0,0,1))
			.SetPressedPadding( FMargin(0,1,0,0)));

		Set( "Tutorials.Content.NavigationButtonWrapper", FButtonStyle()
			.SetNormal( FSlateNoResource() )
			.SetHovered( FSlateNoResource() )
			.SetPressed( FSlateNoResource() )
			.SetNormalPadding( FMargin(0,0,0,1))
			.SetPressedPadding( FMargin(0,1,0,0)));

		Set( "Tutorials.Content.NavigationButton", FButtonStyle()
			.SetNormal( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), TutorialNavigationButtonColor ) )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), TutorialNavigationButtonHoverColor ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), TutorialNavigationButtonHoverColor ) )
			.SetNormalPadding( FMargin(0,0,0,1))
			.SetPressedPadding( FMargin(0,1,0,0)));

		Set("Tutorials.Content.NavigationBackButton", FButtonStyle()
			.SetNormal(BOX_BRUSH("Common/ButtonHoverHint", FMargin(4 / 16.0f), TutorialNavigationBackButtonColor))
			.SetHovered(BOX_BRUSH("Common/ButtonHoverHint", FMargin(4 / 16.0f), TutorialNavigationBackButtonHoverColor))
			.SetPressed(BOX_BRUSH("Common/ButtonHoverHint", FMargin(4 / 16.0f), TutorialNavigationBackButtonHoverColor))
			.SetNormalPadding(FMargin(0, 0, 0, 1))
			.SetPressedPadding(FMargin(0, 1, 0, 0)));

		Set( "Tutorials.Content.NavigationText", FTextBlockStyle(TutorialText));

		Set( "Tutorials.Content.Color", FLinearColor(1.0f,1.0f,1.0f,0.9f) );
		Set( "Tutorials.Content.Color.Hovered", FLinearColor(1.0f,1.0f,1.0f,1.0f) );

		Set( "Tutorials.Browser.CategoryArrow", new IMAGE_BRUSH( "Tutorials/BrowserCategoryArrow", FVector2D(24.0f, 24.0f), FSlateColor::UseForeground() ) );
		Set( "Tutorials.Browser.DefaultTutorialIcon", new IMAGE_BRUSH( "Tutorials/DefaultTutorialIcon_40x", FVector2D(40.0f, 40.0f), FLinearColor::White ) );
		Set( "Tutorials.Browser.DefaultCategoryIcon", new IMAGE_BRUSH( "Tutorials/DefaultCategoryIcon_40x", FVector2D(40.0f, 40.0f), FLinearColor::White ) );

		Set( "Tutorials.Browser.BackButton.Image", new IMAGE_BRUSH( "Tutorials/BrowserBack", FVector2D(32.0f, 32.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) );
		Set( "Tutorials.Browser.PlayButton.Image", new IMAGE_BRUSH( "Tutorials/BrowserPlay", FVector2D(32.0f, 32.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) );
		Set( "Tutorials.Browser.RestartButton", new IMAGE_BRUSH( "Tutorials/BrowserRestart", FVector2D(16.0f, 16.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) ) );

		Set( "Tutorials.Browser.Completed", new IMAGE_BRUSH( "Tutorials/TutorialCompleted", Icon32x32 ) );

		Set( "Tutorials.Browser.Breadcrumb", new IMAGE_BRUSH( "Tutorials/Breadcrumb", Icon8x8, FLinearColor::White ) );
		Set( "Tutorials.Browser.PathText", FTextBlockStyle(TutorialBrowserText)
			.SetFontSize(9));

		Set( "Tutorials.Navigation.Button", FButtonStyle()
			.SetNormal( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0,0,0,0) ) )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0,0,0,0) ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(0,0,0,0) ) )
			.SetNormalPadding( FMargin(0,0,0,1))
			.SetPressedPadding( FMargin(0,1,0,0)));

		Set( "Tutorials.Navigation.NextButton", new IMAGE_BRUSH( "Tutorials/NavigationNext", Icon32x32 ) );
		Set( "Tutorials.Navigation.HomeButton", new IMAGE_BRUSH( "Tutorials/NavigationHome", Icon32x32 ) );
		Set( "Tutorials.Navigation.BackButton", new IMAGE_BRUSH( "Tutorials/NavigationBack", Icon32x32 ) );

		Set( "Tutorials.WidgetContent", FTextBlockStyle(TutorialText)
			.SetFontSize(10));

		Set( "Tutorials.ButtonColor", TutorialButtonColor );
		Set( "Tutorials.ButtonHighlightColor", TutorialSelectionColor );
		Set( "Tutorials.ButtonDisabledColor", SelectionColor_Inactive );
		Set( "Tutorials.ContentAreaBackground", new BOX_BRUSH( "Tutorials/TutorialContentBackground", FMargin(4/16.0f) ) );
		Set( "Tutorials.HomeContentAreaBackground", new BOX_BRUSH( "Tutorials/TutorialHomeContentBackground", FMargin(4/16.0f) ) );
		Set( "Tutorials.ContentAreaFrame", new BOX_BRUSH( "Tutorials/ContentAreaFrame", FMargin(26.0f/64.0f) ) );
		Set( "Tutorials.CurrentExcerpt", new IMAGE_BRUSH( "Tutorials/CurrentExcerpt", FVector2D(24.0f, 24.0f), TutorialSelectionColor ) );
		Set( "Tutorials.Home", new IMAGE_BRUSH( "Tutorials/HomeButton", FVector2D(32.0f, 32.0f) ) );
		Set( "Tutorials.Back", new IMAGE_BRUSH("Tutorials/BackButton", FVector2D(24.0f, 24.0f) ) );
		Set( "Tutorials.Next", new IMAGE_BRUSH("Tutorials/NextButton", FVector2D(24.0f, 24.0f) ) );

		Set("Tutorials.PageHeader", FTextBlockStyle(TutorialHeaderText)
			.SetFontSize(22));

		Set("Tutorials.CurrentExcerpt", FTextBlockStyle(TutorialHeaderText)
			.SetFontSize(16));
 
		Set("Tutorials.NavigationButtons", FTextBlockStyle(TutorialHeaderText)
			.SetFontSize(16));

		// UDN documentation styles
		Set("Tutorials.Content", FTextBlockStyle(TutorialText)
			.SetColorAndOpacity(FSlateColor::UseForeground()));
		Set("Tutorials.Hyperlink.Text",  FTextBlockStyle(DocumentationHyperlinkText));
		Set("Tutorials.NumberedContent", FTextBlockStyle(TutorialText));
		Set("Tutorials.BoldContent",     FTextBlockStyle(TutorialText)
			.SetTypefaceFontName(TEXT("Bold")));

		Set("Tutorials.Header1", FTextBlockStyle(TutorialHeaderText)
			.SetFontSize(32));

		Set("Tutorials.Header2", FTextBlockStyle(TutorialHeaderText)
			.SetFontSize(24));

		Set( "Tutorials.Hyperlink.Button", FButtonStyle(DocumentationHyperlinkButton)
			.SetNormal(BORDER_BRUSH( "Old/HyperlinkDotted", FMargin(0,0,0,3/16.0f), FLinearColor::Black))
			.SetHovered(BORDER_BRUSH( "Old/HyperlinkUnderline", FMargin(0,0,0,3/16.0f), FLinearColor::Black)));

		Set( "Tutorials.Separator", new BOX_BRUSH( "Common/Separator", 1/4.0f, FLinearColor::Black));

		Set( "Tutorials.ProgressBar", FProgressBarStyle()
			.SetBackgroundImage( BOX_BRUSH( "Common/ProgressBar_Background", FMargin(5.f/12.f) ) )
			.SetFillImage( BOX_BRUSH( "Common/ProgressBar_NeutralFill", FMargin(5.f/12.f) ) )
			.SetMarqueeImage( IMAGE_BRUSH( "Common/ProgressBar_Marquee", FVector2D(20,12), FLinearColor::White, ESlateBrushTileType::Horizontal ) )
			);

		// Default text styles
		{
			const FTextBlockStyle RichTextNormal = FTextBlockStyle()
				.SetFont(DEFAULT_FONT("Regular", 11))
				.SetColorAndOpacity(FSlateColor::UseForeground())
				.SetShadowOffset(FVector2D::ZeroVector)
				.SetShadowColorAndOpacity(FLinearColor::Black)
				.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
				.SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f /8.f)));
			Set( "Tutorials.Content.Text", RichTextNormal );

			Set( "Tutorials.Content.TextBold", FTextBlockStyle(RichTextNormal)
				.SetFont(DEFAULT_FONT("Bold", 11)));

			Set( "Tutorials.Content.HeaderText1", FTextBlockStyle(RichTextNormal)
				.SetFontSize(20));

			Set( "Tutorials.Content.HeaderText2", FTextBlockStyle(RichTextNormal)
				.SetFontSize(16));

			{
				const FButtonStyle RichTextHyperlinkButton = FButtonStyle()
					.SetNormal(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0,0,0,3/16.0f), FLinearColor::Blue ) )
					.SetPressed(FSlateNoResource() )
					.SetHovered(BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0,0,0,3/16.0f), FLinearColor::Blue ) );

				const FTextBlockStyle RichTextHyperlinkText = FTextBlockStyle(RichTextNormal)
					.SetColorAndOpacity(FLinearColor::Blue);

				Set( "Tutorials.Content.HyperlinkText", RichTextHyperlinkText );

				// legacy style
				Set( "TutorialEditableText.Editor.HyperlinkText", RichTextHyperlinkText );

				const FHyperlinkStyle RichTextHyperlink = FHyperlinkStyle()
					.SetUnderlineStyle(RichTextHyperlinkButton)
					.SetTextStyle(RichTextHyperlinkText)
					.SetPadding(FMargin(0.0f));
				Set( "Tutorials.Content.Hyperlink", RichTextHyperlink );

				Set( "Tutorials.Content.ExternalLink", new IMAGE_BRUSH("Tutorials/ExternalLink", Icon16x16, FLinearColor::Blue));

				// legacy style
				Set( "TutorialEditableText.Editor.Hyperlink", RichTextHyperlink );
			}
		}

		// Toolbar
		{
			const FLinearColor NormalColor(FColor(0xffeff3f3));
			const FLinearColor SelectedColor(FColor(0xffdbe4d5));
			const FLinearColor HoverColor(FColor(0xffdbe4e4));
			const FLinearColor DisabledColor(FColor(0xaaaaaa));
			const FLinearColor TextColor(FColor(0xff2c3e50));

			Set("TutorialEditableText.RoundedBackground", new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, FLinearColor(FColor(0xffeff3f3))));

			Set("TutorialEditableText.Toolbar.HyperlinkImage", new IMAGE_BRUSH("Tutorials/hyperlink", Icon16x16, TextColor));
			Set("TutorialEditableText.Toolbar.ImageImage", new IMAGE_BRUSH("Tutorials/Image", Icon16x16, TextColor));

			Set("TutorialEditableText.Toolbar.TextColor", TextColor);

			Set("TutorialEditableText.Toolbar.Text", FTextBlockStyle(NormalText)
				.SetFont(DEFAULT_FONT("Regular", 10))
				.SetColorAndOpacity(TextColor)
				);

			Set("TutorialEditableText.Toolbar.BoldText", FTextBlockStyle(NormalText)
				.SetFont(DEFAULT_FONT("Bold", 10))
				.SetColorAndOpacity(TextColor)
				);

			Set("TutorialEditableText.Toolbar.ItalicText", FTextBlockStyle(NormalText)
				.SetFont(DEFAULT_FONT("Italic", 10))
				.SetColorAndOpacity(TextColor)
				);

			Set("TutorialEditableText.Toolbar.Checkbox", FCheckBoxStyle()
				.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
				.SetUncheckedImage(IMAGE_BRUSH("Common/CheckBox", Icon16x16, FLinearColor::White))
				.SetUncheckedHoveredImage(IMAGE_BRUSH("Common/CheckBox", Icon16x16, HoverColor))
				.SetUncheckedPressedImage(IMAGE_BRUSH("Common/CheckBox_Hovered", Icon16x16, HoverColor))
				.SetCheckedImage(IMAGE_BRUSH("Common/CheckBox_Checked_Hovered", Icon16x16, FLinearColor::White))
				.SetCheckedHoveredImage(IMAGE_BRUSH("Common/CheckBox_Checked_Hovered", Icon16x16, HoverColor))
				.SetCheckedPressedImage(IMAGE_BRUSH("Common/CheckBox_Checked", Icon16x16, HoverColor))
				.SetUndeterminedImage(IMAGE_BRUSH("Common/CheckBox_Undetermined", Icon16x16, FLinearColor::White))
				.SetUndeterminedHoveredImage(IMAGE_BRUSH("Common/CheckBox_Undetermined_Hovered", Icon16x16, HoverColor))
				.SetUndeterminedPressedImage(IMAGE_BRUSH("Common/CheckBox_Undetermined_Hovered", Icon16x16, FLinearColor::White))
				);

			Set("TutorialEditableText.Toolbar.ToggleButtonCheckbox", FCheckBoxStyle()
				.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
				.SetUncheckedImage(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), NormalColor))
				.SetUncheckedHoveredImage(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), HoverColor))
				.SetUncheckedPressedImage(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), HoverColor))
				.SetCheckedImage(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), SelectedColor))
				.SetCheckedHoveredImage(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), HoverColor))
				.SetCheckedPressedImage(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), HoverColor))
				);

			const FButtonStyle TutorialButton = FButtonStyle()
				.SetNormal(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), NormalColor))
				.SetHovered(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), HoverColor))
				.SetPressed(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), SelectedColor))
				.SetNormalPadding(FMargin(2, 2, 2, 2))
				.SetPressedPadding(FMargin(2, 3, 2, 1));
			Set("TutorialEditableText.Toolbar.Button", TutorialButton);

			const FComboButtonStyle ComboButton = FComboButtonStyle()
				.SetButtonStyle(Button)
				.SetDownArrowImage(IMAGE_BRUSH("Common/ComboArrow", Icon8x8))
				.SetMenuBorderBrush(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), NormalColor))
				.SetMenuBorderPadding(FMargin(0.0f));
			Set("TutorialEditableText.Toolbar.ComboButton", ComboButton);

			{
				const FButtonStyle ComboBoxButton = FButtonStyle()
					.SetNormal(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), FLinearColor::White))
					.SetHovered(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), FLinearColor::White))
					.SetPressed(BOX_BRUSH("Tutorials/FlatColorSquare", FVector2D(1.0f, 1.0f), FMargin(1), FLinearColor::White))
					.SetNormalPadding(FMargin(2, 2, 2, 2))
					.SetPressedPadding(FMargin(2, 3, 2, 1));

				const FComboButtonStyle ComboBoxComboButton = FComboButtonStyle(ComboButton)
					.SetButtonStyle(ComboBoxButton)
					.SetMenuBorderPadding(FMargin(1.0));

				Set("TutorialEditableText.Toolbar.ComboBox", FComboBoxStyle()
					.SetComboButtonStyle(ComboBoxComboButton)
					);
			}
		}

		// In-editor tutorial launch button
		{
			Set("TutorialLaunch.Button", FButtonStyle()
				.SetNormalPadding(0)
				.SetPressedPadding(0)
				.SetNormal(IMAGE_BRUSH("Tutorials/TutorialButton_Default_16x", Icon16x16))
				.SetHovered(IMAGE_BRUSH("Tutorials/TutorialButton_Hovered_16x", Icon16x16))
				.SetPressed(IMAGE_BRUSH("Tutorials/TutorialButton_Pressed_16x", Icon16x16))
				);

			Set("TutorialLaunch.Circle", new IMAGE_BRUSH("Tutorials/Circle_128x", Icon128x128, FLinearColor::White));
			Set("TutorialLaunch.Circle.Color", FLinearColor::Green);
		}
	}
}

void FStarshipEditorStyle::FStyle::SetupPropertyEditorStyles()
	{
	// Property / details Window / PropertyTable 
	{
		Set( "PropertyEditor.Grid.TabIcon", new IMAGE_BRUSH( "Icons/icon_PropertyMatrix_16px", Icon16x16 ) );
		Set( "PropertyEditor.Properties.TabIcon", new IMAGE_BRUSH( "Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );

		Set( "PropertyEditor.RemoveColumn", new IMAGE_BRUSH( "Common/PushPin_Down", Icon16x16, FColor( 96, 194, 253, 255 ).ReinterpretAsLinear() ) );
		Set( "PropertyEditor.AddColumn", new IMAGE_BRUSH( "Common/PushPin_Up", Icon16x16, FColor( 96, 194, 253, 255 ).ReinterpretAsLinear() ) );

		Set( "PropertyEditor.AddColumnOverlay",	new IMAGE_BRUSH( "Common/TinyChalkArrow", FVector2D( 71, 20 ), FColor( 96, 194, 253, 255 ).ReinterpretAsLinear() ) );
		Set( "PropertyEditor.AddColumnMessage", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "BoldCondensedItalic", 10 ) )
			.SetColorAndOpacity(FColor( 96, 194, 253, 255 ).ReinterpretAsLinear())
		);
	

		Set( "PropertyEditor.AssetName.ColorAndOpacity", FLinearColor::White );

		Set("PropertyEditor.AssetThumbnailBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::InputOutline, 1.0f));
		Set("PropertyEditor.AssetThumbnailBorderHovered", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Hover2, 1.0f));
		Set("PropertyEditor.AssetTileItem.DropShadow", new BOX_BRUSH("Starship/ContentBrowser/drop-shadow", FMargin(4.0f / 64.0f)));

		Set( "PropertyEditor.AssetClass", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 10 ) )
			.SetColorAndOpacity( FLinearColor::White )
			.SetShadowOffset( FVector2D(1,1) )
			.SetShadowColorAndOpacity( FLinearColor::Black )
		);

		const FButtonStyle AssetComboStyle = FButtonStyle()
			.SetNormal( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,0.15f) ) )
			.SetHovered( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,0.25f) ) )
			.SetPressed( BOX_BRUSH( "Common/ButtonHoverHint", FMargin(4/16.0f), FLinearColor(1,1,1,0.30f) ) )
			.SetNormalPadding( FMargin(0,0,0,1) )
			.SetPressedPadding( FMargin(0,1,0,0) );
		Set( "PropertyEditor.AssetComboStyle", AssetComboStyle );

		Set( "PropertyEditor.HorizontalDottedLine",		new IMAGE_BRUSH( "Common/HorizontalDottedLine_16x1px", FVector2D(16.0f, 1.0f), FLinearColor::White, ESlateBrushTileType::Horizontal ) );
		Set( "PropertyEditor.VerticalDottedLine",		new IMAGE_BRUSH( "Common/VerticalDottedLine_1x16px", FVector2D(1.0f, 16.0f), FLinearColor::White, ESlateBrushTileType::Vertical ) );
		Set( "PropertyEditor.SlateBrushPreview",		new BOX_BRUSH( "PropertyView/SlateBrushPreview_32px", Icon32x32, FMargin(3.f/32.f, 3.f/32.f, 15.f/32.f, 13.f/32.f) ) );

		Set( "PropertyTable.TableRow", FTableRowStyle()
			.SetEvenRowBackgroundBrush( FSlateColorBrush( FLinearColor( 0.70f, 0.70f, 0.70f, 255 ) ) )
			.SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
			.SetOddRowBackgroundBrush( FSlateColorBrush( FLinearColor( 0.80f, 0.80f, 0.80f, 255 ) ) )
			.SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
			.SetSelectorFocusedBrush( BORDER_BRUSH( "Common/Selector", FMargin(4.f/16.f), SelectorColor ) )
			.SetActiveBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) )
			.SetActiveHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) )
			.SetInactiveBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
			.SetInactiveHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
			.SetTextColor( DefaultForeground )
			.SetSelectedTextColor( InvertedForeground )
			);

		const FTableColumnHeaderStyle PropertyTableColumnHeaderStyle = FTableColumnHeaderStyle()
			.SetSortPrimaryAscendingImage(IMAGE_BRUSH("Common/SortUpArrow", Icon8x4))
			.SetSortPrimaryDescendingImage(IMAGE_BRUSH("Common/SortDownArrow", Icon8x4))
			.SetSortSecondaryAscendingImage(IMAGE_BRUSH("Common/SortUpArrows", Icon16x4))
			.SetSortSecondaryDescendingImage(IMAGE_BRUSH("Common/SortDownArrows", Icon16x4))
			.SetNormalBrush( BOX_BRUSH( "Common/ColumnHeader", 4.f/32.f ) )
			.SetHoveredBrush( BOX_BRUSH( "Common/ColumnHeader_Hovered", 4.f/32.f ) )
			.SetMenuDropdownImage( IMAGE_BRUSH( "Common/ColumnHeader_Arrow", Icon8x8 ) )
			.SetMenuDropdownNormalBorderBrush( BOX_BRUSH( "Common/ColumnHeaderMenuButton_Normal", 4.f/32.f ) )
			.SetMenuDropdownHoveredBorderBrush( BOX_BRUSH( "Common/ColumnHeaderMenuButton_Hovered", 4.f/32.f ) );

		const FTableColumnHeaderStyle PropertyTableLastColumnHeaderStyle = FTableColumnHeaderStyle()
			.SetSortPrimaryAscendingImage(IMAGE_BRUSH("Common/SortUpArrow", Icon8x4))
			.SetSortPrimaryDescendingImage(IMAGE_BRUSH("Common/SortDownArrow", Icon8x4))
			.SetSortSecondaryAscendingImage(IMAGE_BRUSH("Common/SortUpArrows", Icon16x4))
			.SetSortSecondaryDescendingImage(IMAGE_BRUSH("Common/SortDownArrows", Icon16x4))
			.SetNormalBrush( FSlateNoResource() )
			.SetHoveredBrush( BOX_BRUSH( "Common/LastColumnHeader_Hovered", 4.f/32.f ) )
			.SetMenuDropdownImage( IMAGE_BRUSH( "Common/ColumnHeader_Arrow", Icon8x8 ) )
			.SetMenuDropdownNormalBorderBrush( BOX_BRUSH( "Common/ColumnHeaderMenuButton_Normal", 4.f/32.f ) )
			.SetMenuDropdownHoveredBorderBrush( BOX_BRUSH( "Common/ColumnHeaderMenuButton_Hovered", 4.f/32.f ) );

		const FSplitterStyle PropertyTableHeaderSplitterStyle = FSplitterStyle()
			.SetHandleNormalBrush( FSlateNoResource() )
			.SetHandleHighlightBrush( IMAGE_BRUSH( "Common/HeaderSplitterGrip", Icon8x8 ) );

		Set( "PropertyTable.HeaderRow", FHeaderRowStyle()
			.SetColumnStyle( PropertyTableColumnHeaderStyle )
			.SetLastColumnStyle( PropertyTableLastColumnHeaderStyle )
			.SetColumnSplitterStyle( PropertyTableHeaderSplitterStyle )
			.SetBackgroundBrush( BOX_BRUSH( "Common/TableViewHeader", 4.f/32.f ) )
			.SetForegroundColor( DefaultForeground )
			);

	
		
		FLinearColor TransparentHeader = FStyleColors::Recessed.GetSpecifiedColor();
		TransparentHeader.A = 0.95f;
		FLinearColor TransparentToolbar = FStyleColors::Header.GetSpecifiedColor();
		TransparentToolbar.A = 0.95f;
		FLinearColor TransparentBackground = FStyleColors::Background.GetSpecifiedColor();
		TransparentBackground.A = 0.75f;
		Set("PropertyTable.InViewport.Background", new FSlateColorBrush(FSlateColor(TransparentBackground)));
		Set("PropertyTable.InViewport.Header", new FSlateColorBrush(FSlateColor(TransparentHeader)));
		// InViewportToolbar
		{
			FToolBarStyle InViewportToolbar = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
			InViewportToolbar.SetBackground(FSlateColorBrush(FSlateColor(TransparentToolbar)));
			Set("InViewportToolbar", InViewportToolbar);
		}

		Set( "PropertyTable.Selection.Active",						new IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) );

		Set( "PropertyTable.HeaderRow.Column.PathDelimiter",		new IMAGE_BRUSH( "Common/SmallArrowRight", Icon10x10 ) );

		Set( "PropertyTable.RowHeader.Background",					new BOX_BRUSH( "Old/Menu_Background", FMargin(4.f/64.f) ) );
		Set( "PropertyTable.RowHeader.BackgroundActive",			new BOX_BRUSH( "Old/Menu_Background", FMargin(4.f/64.f), SelectionColor_Inactive ) );
		Set( "PropertyTable.ColumnBorder",							new BOX_BRUSH( "Common/ColumnBorder", FMargin(4.f/16.f), FLinearColor(0.1f, 0.1f, 0.1f, 0.5f) ) );
		Set( "PropertyTable.CellBorder",							new BOX_BRUSH( "Common/CellBorder", FMargin(4.f/16.f), FLinearColor(0.1f, 0.1f, 0.1f, 0.5f) ) );
		Set( "PropertyTable.ReadOnlyEditModeCellBorder",			new BORDER_BRUSH( "Common/ReadOnlyEditModeCellBorder", FMargin(6.f/32.f), SelectionColor ) );
		Set( "PropertyTable.ReadOnlyCellBorder",					new BOX_BRUSH( "Common/ReadOnlyCellBorder", FMargin(4.f/16.f), FLinearColor(0.1f, 0.1f, 0.1f, 0.5f) ) );
		Set( "PropertyTable.CurrentCellBorder",						new BOX_BRUSH( "Common/CurrentCellBorder", FMargin(4.f/16.f), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f) ) );
		Set( "PropertyTable.ReadOnlySelectedCellBorder",			new BOX_BRUSH( "Common/ReadOnlySelectedCellBorder", FMargin(4.f/16.f), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f) ) );
		Set( "PropertyTable.ReadOnlyCurrentCellBorder",				new BOX_BRUSH( "Common/ReadOnlyCurrentCellBorder", FMargin(4.f/16.f), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f) ) );
		Set( "PropertyTable.Cell.DropDown.Background",				new BOX_BRUSH( "Common/GroupBorder", FMargin(4.f/16.f) ) );
		Set( "PropertyTable.ContentBorder",							new BOX_BRUSH( "Common/GroupBorder", FMargin(4.0f/16.0f) ) );	
		Set( "PropertyTable.NormalFont",							DEFAULT_FONT( "Regular", 9 ) );
		Set( "PropertyTable.BoldFont",								DEFAULT_FONT( "Bold", 9 ) );
		Set( "PropertyTable.FilterFont",							DEFAULT_FONT( "Regular", 10 ) );

		Set( "PropertyWindow.FilterSearch", new IMAGE_BRUSH( "Old/FilterSearch", Icon16x16 ) );
		Set( "PropertyWindow.FilterCancel", new IMAGE_BRUSH( "Old/FilterCancel", Icon16x16 ) );
		Set( "PropertyWindow.Favorites_Disabled", new IMAGE_BRUSH( "Icons/EmptyStar_16x", Icon16x16 ) );
		Set( "PropertyWindow.Locked", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/lock", Icon16x16 ) );
		Set( "PropertyWindow.Unlocked", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/lock-unlocked", Icon16x16 ) );
		Set( "PropertyWindow.DiffersFromDefault", new IMAGE_BRUSH_SVG( "Starship/Common/ResetToDefault", Icon16x16) ) ;
		
		Set( "PropertyWindow.NormalFont", FStyleFonts::Get().Small);
		Set( "PropertyWindow.BoldFont",FStyleFonts::Get().SmallBold);
		Set( "PropertyWindow.ItalicFont", DEFAULT_FONT( "Italic", 8 ) );
		Set( "PropertyWindow.FilterFont", DEFAULT_FONT( "Regular", 10 ) );

		FSlateFontInfo MobilityFont = FStyleFonts::Get().Small;
		MobilityFont.LetterSpacing = 100;

		Set("PropertyWindow.MobilityFont", MobilityFont );
		Set("PropertyWindow.MobilityStatic", new IMAGE_BRUSH_SVG("Starship/Common/MobilityStatic", Icon16x16));
		Set("PropertyWindow.MobilityStationary", new IMAGE_BRUSH_SVG("Starship/Common/MobilityStationary", Icon16x16));
		Set("PropertyWindow.MobilityMoveable", new IMAGE_BRUSH_SVG("Starship/Common/MobilityMoveable", Icon16x16));

		Set( "PropertyWindow.NoOverlayColor", new FSlateNoResource() );
		Set( "PropertyWindow.EditConstColor", new FSlateColorBrush( FColor( 152, 152, 152, 80 ) ) );
		Set( "PropertyWindow.FilteredColor", new FSlateColorBrush( FColor( 0, 255, 0, 80 ) ) );
		Set( "PropertyWindow.FilteredEditConstColor", new FSlateColorBrush( FColor( 152, 152, 152, 80 ).ReinterpretAsLinear() * FColor(0,255,0,255).ReinterpretAsLinear() ) );
		Set( "PropertyWindow.CategoryBackground", new BOX_BRUSH( "/PropertyView/CategoryBackground", FMargin(4.f/16.f) ) );
		Set( "PropertyWindow.CategoryForeground", FLinearColor::Black );
		Set( "PropertyWindow.Button_Clear", new IMAGE_BRUSH( "Icons/Cross_12x", Icon12x12 ) );
		Set( "PropertyWindow.Button_Ellipsis", new IMAGE_BRUSH( "Icons/ellipsis_12x", Icon12x12 ) );
		Set( "PropertyWindow.Button_PickAsset", new IMAGE_BRUSH( "Icons/pillarray_12x", Icon12x12 ) );
		Set( "PropertyWindow.Button_PickActor", new IMAGE_BRUSH( "Icons/levels_16x", Icon12x12 ) );
		Set( "PropertyWindow.Button_PickActorInteractive", new IMAGE_BRUSH( "Icons/eyedropper_16px", Icon12x12 ) );

		Set( "PropertyWindow.WindowBorder", new BOX_BRUSH( "Common/GroupBorder", FMargin(4.0f/16.0f) ) );

		FInlineEditableTextBlockStyle NameStyle(FCoreStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("InlineEditableTextBlockStyle"));
		NameStyle.EditableTextBoxStyle.SetFont(DEFAULT_FONT("Regular", 11))
			.SetForegroundColor(FSlateColor(EStyleColor::White));
		NameStyle.TextStyle.SetFont(DEFAULT_FONT("Regular", 11))
			.SetColorAndOpacity(FSlateColor(EStyleColor::White));
		Set( "DetailsView.ConstantTextBlockStyle", NameStyle.TextStyle);
		Set( "DetailsView.NameTextBlockStyle",  NameStyle );

		Set( "DetailsView.NameChangeCommitted", new BOX_BRUSH( "Common/EditableTextSelectionBackground", FMargin(4.f/16.f) ) );
		Set( "DetailsView.HyperlinkStyle", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 8 ) ) );

		FTextBlockStyle BPWarningMessageTextStyle = FTextBlockStyle(NormalText) .SetFont(DEFAULT_FONT("Regular", 8));
		FTextBlockStyle BPWarningMessageHyperlinkTextStyle = FTextBlockStyle(BPWarningMessageTextStyle).SetColorAndOpacity(FLinearColor(0.25f, 0.5f, 1.0f));

		FButtonStyle EditBPHyperlinkButton = FButtonStyle()
			.SetNormal(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0, 0, 0, 3 / 16.0f), FLinearColor(0.25f, 0.5f, 1.0f)))
			.SetPressed(FSlateNoResource())
			.SetHovered(BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0, 0, 0, 3 / 16.0f), FLinearColor(0.25f, 0.5f, 1.0f)));

		FHyperlinkStyle BPWarningMessageHyperlinkStyle = FHyperlinkStyle()
			.SetUnderlineStyle(EditBPHyperlinkButton)
			.SetTextStyle(BPWarningMessageHyperlinkTextStyle)
			.SetPadding(FMargin(0.0f));

		Set( "DetailsView.BPMessageHyperlinkStyle", BPWarningMessageHyperlinkStyle );
		Set( "DetailsView.BPMessageTextStyle", BPWarningMessageTextStyle );

		Set( "DetailsView.GroupSection",              new FSlateNoResource());

		Set( "DetailsView.PulldownArrow.Down",        new CORE_IMAGE_BRUSH_SVG("Starship/Common/chevron-down", Icon16x16, FStyleColors::Foreground)); 
		Set( "DetailsView.PulldownArrow.Down.Hovered",new CORE_IMAGE_BRUSH_SVG("Starship/Common/chevron-down", Icon16x16, FStyleColors::ForegroundHover)); 
		Set( "DetailsView.PulldownArrow.Up",          new CORE_IMAGE_BRUSH_SVG("Starship/Common/chevron-up",   Icon16x16, FStyleColors::Foreground)); 
		Set( "DetailsView.PulldownArrow.Up.Hovered",  new CORE_IMAGE_BRUSH_SVG("Starship/Common/chevron-up",   Icon16x16, FStyleColors::ForegroundHover)); 

		Set( "DetailsView.EditRawProperties",         new CORE_IMAGE_BRUSH_SVG("Starship/Common/layout-spreadsheet",  Icon16x16, FLinearColor::White) );
		Set( "DetailsView.ViewOptions",         	  new CORE_IMAGE_BRUSH_SVG("Starship/Common/settings",  Icon16x16, FLinearColor::White) );
		Set( "DetailsView.EditConfigProperties",      new IMAGE_BRUSH("Icons/icon_PropertyMatrix_16px",  Icon16x16, FLinearColor::White ) );

		Set( "DetailsView.CollapsedCategory",         new FSlateColorBrush(FStyleColors::Header));
		Set( "DetailsView.CollapsedCategory_Hovered", new FSlateColorBrush(FStyleColors::Header));
		Set( "DetailsView.CategoryTop",               new FSlateColorBrush(FStyleColors::Header));
		Set( "DetailsView.CategoryTop_Hovered",       new FSlateColorBrush(FStyleColors::Header));
		Set( "DetailsView.CategoryBottom",            new FSlateColorBrush(FStyleColors::Recessed));
		
		// these are not actually displayed as white, see PropertyEditorConstants::GetRowBackgroundColor
		Set( "DetailsView.CategoryMiddle",            new FSlateColorBrush(FStyleColors::White));

		Set( "DetailsView.PropertyIsFavorite", new IMAGE_BRUSH("PropertyView/Favorites_Enabled", Icon12x12));
		Set( "DetailsView.PropertyIsNotFavorite", new IMAGE_BRUSH("PropertyView/Favorites_Disabled", Icon12x12));
		Set( "DetailsView.NoFavoritesSystem", new IMAGE_BRUSH("PropertyView/NoFavoritesSystem", Icon12x12));

		Set( "DetailsView.Splitter", FSplitterStyle()
			.SetHandleNormalBrush(FSlateColorBrush(FStyleColors::Recessed))                   
			.SetHandleHighlightBrush(FSlateColorBrush(FStyleColors::Recessed))
		);

		Set( "DetailsView.GridLine", new FSlateColorBrush(FStyleColors::Recessed) );

		Set( "DetailsView.AdvancedDropdownBorder",      new FSlateColorBrush(FStyleColors::Panel));
		Set( "DetailsView.AdvancedDropdownBorder.Open", new IMAGE_BRUSH("Common/ScrollBoxShadowTop", FVector2D(64,8)));

		Set( "DetailsView.CategoryFontStyle", FStyleFonts::Get().NormalBold);
		Set( "DetailsView.CategoryTextStyle", FTextBlockStyle(NormalText)
			.SetFont(GetFontStyle("DetailsView.CategoryFontStyle"))
			.SetColorAndOpacity(FStyleColors::ForegroundHeader)
		);

		Set("DetailsView.CategoryTextStyleUpdate", FTextBlockStyle(NormalText)
			.SetFont(FStyleFonts::Get().Small)
			.SetColorAndOpacity(FStyleColors::ForegroundHeader)
			.SetTransformPolicy(ETextTransformPolicy::ToUpper)
		);

		FButtonStyle DetailsExtensionMenuButton = FButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.SetNormalForeground(FStyleColors::Foreground)
			.SetHoveredForeground(FStyleColors::ForegroundHover)
			.SetPressedForeground(FStyleColors::ForegroundHover)
			.SetDisabledForeground(FStyleColors::Foreground)
			.SetNormalPadding(FMargin(0, 2, 0, 2))
			.SetPressedPadding(FMargin(0, 3, 0, 1));

		Set("DetailsView.ExtensionToolBar.Button", DetailsExtensionMenuButton);

		FToolBarStyle DetailsExtensionToolBarStyle = FToolBarStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar"))
			.SetBackground(FSlateNoResource())
			.SetIconSize(Icon16x16)
			.SetLabelPadding(FMargin(0))
			.SetComboButtonPadding(FMargin(0))
			.SetBlockPadding(FMargin(2, 0))
			.SetIndentedBlockPadding(FMargin(0))
			.SetIndentedBlockPadding(FMargin(0))
			.SetBackgroundPadding(FMargin(0))
			.SetButtonPadding(FMargin(2, 0))
			.SetCheckBoxPadding(FMargin(3, 0))
			.SetButtonStyle(DetailsExtensionMenuButton)
			.SetSeparatorBrush(FSlateNoResource())
			.SetSeparatorPadding(FMargin(4, 0))
			.SetExpandBrush(CORE_IMAGE_BRUSH_SVG("Starship/Common/ellipsis-vertical-narrow", FVector2D(6, 24)));

		Set("DetailsView.ExtensionToolBar", DetailsExtensionToolBarStyle);

		Set("DetailsView.ArrayDropShadow", new IMAGE_BRUSH("Common/ArrayDropShadow", FVector2D(32,2)));

		Set( "DetailsView.TreeView.TableRow", FTableRowStyle()
			.SetEvenRowBackgroundBrush( FSlateNoResource() )
			.SetEvenRowBackgroundHoveredBrush( FSlateNoResource() )
			.SetOddRowBackgroundBrush( FSlateNoResource() )
			.SetOddRowBackgroundHoveredBrush( FSlateNoResource() )
			.SetSelectorFocusedBrush( FSlateNoResource() )
			.SetActiveBrush( FSlateNoResource() )
			.SetActiveHoveredBrush( FSlateNoResource() )
			.SetInactiveBrush( FSlateNoResource() )
			.SetInactiveHoveredBrush( FSlateNoResource() )
			.SetTextColor( DefaultForeground )
			.SetSelectedTextColor( InvertedForeground )
			);

		Set("DetailsView.DropZone.Below", new BOX_BRUSH("Common/VerticalBoxDropZoneIndicator_Below", FMargin(10.0f / 16.0f, 0, 0, 10.0f / 16.0f), SelectionColor_Subdued));

	}
	}

void FStarshipEditorStyle::FStyle::SetupProfilerStyle()
{
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	// Profiler
	{
		// Profiler group brushes
		Set( "Profiler.Group.16", new BOX_BRUSH( "Icons/Profiler/GroupBorder-16Gray", FMargin(4.0f/16.0f) ) );

		// Profiler toolbar icons
		Set( "Profiler.Tab", new IMAGE_BRUSH_SVG( "Starship/Common/Visualizer", Icon16x16 ) );
		Set( "Profiler.Tab.GraphView", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Graph_View_Tab_16x", Icon16x16 ) );
		Set( "Profiler.Tab.EventGraph", new IMAGE_BRUSH( "Icons/Profiler/profiler_OpenEventGraph_32x", Icon16x16 ) );
		Set( "Profiler.Tab.FiltersAndPresets", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Filter_Presets_Tab_16x", Icon16x16 ) );

		Set( "ProfilerCommand.ProfilerManager_Load", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Load_Profiler_40x", Icon40x40 ) );
		Set( "ProfilerCommand.ProfilerManager_Load.Small", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Load_Profiler_40x", Icon20x20 ) );

		Set("ProfilerCommand.ProfilerManager_LoadMultiple", new IMAGE_BRUSH("Icons/Profiler/Profiler_LoadMultiple_Profiler_40x", Icon40x40));
		Set("ProfilerCommand.ProfilerManager_LoadMultiple.Small", new IMAGE_BRUSH("Icons/Profiler/Profiler_LoadMultiple_Profiler_40x", Icon20x20));

		Set( "ProfilerCommand.ProfilerManager_Save", new IMAGE_BRUSH( "Icons/LV_Save", Icon40x40 ) );
		Set( "ProfilerCommand.ProfilerManager_Save.Small", new IMAGE_BRUSH( "Icons/LV_Save", Icon20x20 ) );
		
		Set( "ProfilerCommand.ProfilerManager_ToggleLivePreview", new IMAGE_BRUSH( "Automation/RefreshTests", Icon40x40) );
		Set( "ProfilerCommand.ProfilerManager_ToggleLivePreview.Small", new IMAGE_BRUSH( "Automation/RefreshTests", Icon20x20) );

		Set( "ProfilerCommand.StatsProfiler", new IMAGE_BRUSH( "Icons/Profiler/profiler_stats_40x", Icon40x40 ) );
		Set( "ProfilerCommand.StatsProfiler.Small", new IMAGE_BRUSH( "Icons/Profiler/profiler_stats_40x", Icon20x20 ) );

		Set( "ProfilerCommand.MemoryProfiler", new IMAGE_BRUSH( "Icons/Profiler/profiler_mem_40x", Icon40x40 ) );
		Set( "ProfilerCommand.MemoryProfiler.Small", new IMAGE_BRUSH( "Icons/Profiler/profiler_mem_40x", Icon20x20 ) );

		Set( "ProfilerCommand.FPSChart", new IMAGE_BRUSH( "Icons/Profiler/Profiler_FPS_Chart_40x", Icon40x40 ) );
		Set( "ProfilerCommand.FPSChart.Small", new IMAGE_BRUSH( "Icons/Profiler/Profiler_FPS_Chart_40x", Icon20x20 ) );

		Set( "ProfilerCommand.OpenSettings", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Settings_40x", Icon40x40 ) );
		Set( "ProfilerCommand.OpenSettings.Small", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Settings_40x", Icon20x20 ) );

		Set( "ProfilerCommand.ToggleDataPreview", new IMAGE_BRUSH( "Icons/Profiler/profiler_sync_40x", Icon40x40 ) );
		Set( "ProfilerCommand.ToggleDataPreview.Small", new IMAGE_BRUSH( "Icons/Profiler/profiler_sync_40x", Icon20x20 ) );

		Set( "ProfilerCommand.ToggleDataCapture", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Data_Capture_40x", Icon40x40 ) );
		Set( "ProfilerCommand.ToggleDataCapture.Small", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Data_Capture_40x", Icon20x20 ) );

		Set( "ProfilerCommand.ToggleDataCapture.Checked", new IMAGE_BRUSH( "Icons/icon_stop_40x", Icon40x40 ) );
		Set( "ProfilerCommand.ToggleDataCapture.Checked.Small", new IMAGE_BRUSH( "Icons/icon_stop_40x", Icon20x20 ) );

		Set( "ProfilerCommand.ToggleShowDataGraph", new IMAGE_BRUSH( "Icons/Profiler/profiler_ShowGraphData_32x", Icon32x32 ) );
		Set( "ProfilerCommand.OpenEventGraph", new IMAGE_BRUSH( "Icons/Profiler/profiler_OpenEventGraph_32x", Icon16x16 ) );

		// Generic
		Set( "Profiler.LineGraphArea", new IMAGE_BRUSH( "Old/White", Icon16x16, FLinearColor(1.0f,1.0f,1.0f,0.25f) ) );
		
		// Tooltip hint icon
		Set( "Profiler.Tooltip.HintIcon10", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Custom_Tooltip_12x", Icon12x12 ) );

		// Text styles
		Set( "Profiler.CaptionBold", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Bold", 10 ) )
			.SetColorAndOpacity( FLinearColor::White )
			.SetShadowOffset( FVector2D(1.0f, 1.0f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f,0.8f) )
		);

		Set( "Profiler.Caption", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 10 ) )
			.SetColorAndOpacity( FLinearColor::White )
			.SetShadowOffset( FVector2D(1.0f, 1.0f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f,0.8f) )
		);

		Set( "Profiler.TooltipBold", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Bold", 8 ) )
			.SetColorAndOpacity( FLinearColor(0.5f,0.5f,0.5f,1.0f) )
			.SetShadowOffset( FVector2D(1.0f, 1.0f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f,0.8f) )
		);

		Set( "Profiler.Tooltip", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 8 ) )
			.SetColorAndOpacity( FLinearColor::White )
			.SetShadowOffset( FVector2D(1.0f, 1.0f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f,0.8f) )
		);

		// Event graph icons
		Set( "Profiler.EventGraph.SetRoot", new IMAGE_BRUSH( "Icons/Profiler/profiler_SetRoot_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.CullEvents", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Cull_Events_16x", Icon16x16) );
		Set( "Profiler.EventGraph.FilterEvents", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Filter_Events_16x", Icon16x16) );

		Set( "Profiler.EventGraph.SelectStack", new IMAGE_BRUSH( "Icons/Profiler/profiler_SelectStack_32x", Icon32x32 ) );

		Set( "Profiler.EventGraph.ExpandAll", new IMAGE_BRUSH( "Icons/Profiler/profiler_ExpandAll_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.CollapseAll", new IMAGE_BRUSH( "Icons/Profiler/profiler_CollapseAll_32x", Icon32x32 ) );
		
		Set( "Profiler.EventGraph.ExpandSelection", new IMAGE_BRUSH( "Icons/Profiler/profiler_ExpandSelection_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.CollapseSelection", new IMAGE_BRUSH( "Icons/Profiler/profiler_CollapseSelection_32x", Icon32x32 ) );

		Set( "Profiler.EventGraph.ExpandThread", new IMAGE_BRUSH( "Icons/Profiler/profiler_ExpandThread_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.CollapseThread", new IMAGE_BRUSH( "Icons/Profiler/profiler_CollapseThread_32x", Icon32x32 ) );

		Set( "Profiler.EventGraph.ExpandHotPath", new IMAGE_BRUSH( "Icons/Profiler/profiler_ExpandHotPath_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.HotPathSmall", new IMAGE_BRUSH( "Icons/Profiler/profiler_HotPath_32x", Icon12x12 ) );

		Set( "Profiler.EventGraph.ExpandHotPath16", new IMAGE_BRUSH( "Icons/Profiler/profiler_HotPath_32x", Icon16x16 ) );

		Set( "Profiler.EventGraph.GameThread", new IMAGE_BRUSH( "Icons/Profiler/profiler_GameThread_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.RenderThread", new IMAGE_BRUSH( "Icons/Profiler/profiler_RenderThread_32x", Icon32x32 ) );
	
		Set( "Profiler.EventGraph.ViewColumn", new IMAGE_BRUSH( "Icons/Profiler/profiler_ViewColumn_32x", Icon32x32 ) );
		Set( "Profiler.EventGraph.ResetColumn", new IMAGE_BRUSH( "Icons/Profiler/profiler_ResetColumn_32x", Icon32x32 ) );

		Set( "Profiler.EventGraph.HistoryBack", new IMAGE_BRUSH( "Icons/Profiler/Profiler_History_Back_16x", Icon16x16) );
		Set( "Profiler.EventGraph.HistoryForward", new IMAGE_BRUSH( "Icons/Profiler/Profiler_History_Fwd_16x", Icon16x16) );

		Set( "Profiler.EventGraph.MaximumIcon", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Max_Event_Graph_16x", Icon16x16) );
		Set( "Profiler.EventGraph.AverageIcon", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Average_Event_Graph_16x", Icon16x16) );

		Set( "Profiler.EventGraph.FlatIcon", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Events_Flat_16x", Icon16x16) );
		Set( "Profiler.EventGraph.FlatCoalescedIcon", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Events_Flat_Coalesced_16x", Icon16x16) );
		Set( "Profiler.EventGraph.HierarchicalIcon", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Events_Hierarchial_16x", Icon16x16) );

		Set( "Profiler.EventGraph.HasCulledEventsSmall", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Has_Culled_Children_12x", Icon12x12) );
		Set( "Profiler.EventGraph.CulledEvent", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Culled_12x", Icon12x12) );
		Set( "Profiler.EventGraph.FilteredEvent", new IMAGE_BRUSH( "Icons/Profiler/Profiler_Filtered_12x", Icon12x12) );

		Set( "Profiler.EventGraph.DarkText", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 8 ) )
			.SetColorAndOpacity( FLinearColor::Black )
			.SetShadowOffset( FVector2D(0.0f, 0.0f) )
			);

		// Thread-view
		Set( "Profiler.ThreadView.SampleBorder", new BOX_BRUSH( "Icons/Profiler/Profiler_ThreadView_SampleBorder_16x", FMargin( 2.0f / 16.0f ) ) );

		// Event graph selected event border
		Set( "Profiler.EventGraph.Border.TB", new BOX_BRUSH( "Icons/Profiler/Profiler_Border_TB_16x", FMargin(4.0f/16.0f) ) );
		Set( "Profiler.EventGraph.Border.L", new BOX_BRUSH( "Icons/Profiler/Profiler_Border_L_16x",   FMargin(4.0f/16.0f) ) );
		Set( "Profiler.EventGraph.Border.R", new BOX_BRUSH( "Icons/Profiler/Profiler_Border_R_16x",   FMargin(4.0f/16.0f) ) );

		// Misc
		Set( "Profiler.Misc.WarningSmall", new IMAGE_BRUSH( "ContentBrowser/SCC_NotAtHeadRevision", Icon12x12 ) );

		Set( "Profiler.Misc.SortBy", new IMAGE_BRUSH( "Icons/Profiler/profiler_SortBy_32x", Icon32x32 ) );
		Set( "Profiler.Misc.SortAscending", new IMAGE_BRUSH( "Icons/Profiler/profiler_SortAscending_32x", Icon32x32 ) );
		Set( "Profiler.Misc.SortDescending", new IMAGE_BRUSH( "Icons/Profiler/profiler_SortDescending_32x", Icon32x32 ) );

		Set( "Profiler.Misc.ResetToDefault", new IMAGE_BRUSH( "Icons/Profiler/profiler_ResetToDefault_32x", Icon32x32 ) );

		Set( "Profiler.Misc.Save16", new IMAGE_BRUSH( "Icons/LV_Save", Icon16x16 ) );
		Set( "Profiler.Misc.Reset16", new IMAGE_BRUSH( "Icons/Profiler/profiler_ResetToDefault_32x", Icon16x16 ) );

		Set( "Profiler.Type.Calls", new IMAGE_BRUSH( "Icons/Profiler/profiler_Calls_32x", Icon16x16 ) );
		Set( "Profiler.Type.Event", new IMAGE_BRUSH( "Icons/Profiler/profiler_Event_32x", Icon16x16 ) );
		Set( "Profiler.Type.Memory", new IMAGE_BRUSH( "Icons/Profiler/profiler_Memory_32x", Icon16x16 ) );
		Set( "Profiler.Type.Number", new IMAGE_BRUSH( "Icons/Profiler/profiler_Number_32x", Icon16x16 ) );

		// NumberInt, NumberFloat, Memory, Hierarchical
		Set( "Profiler.Type.NumberInt", new IMAGE_BRUSH( "Icons/Profiler/profiler_Number_32x", Icon16x16 ) );
		Set( "Profiler.Type.NumberFloat", new IMAGE_BRUSH( "Icons/Profiler/profiler_Number_32x", Icon16x16 ) );
		Set( "Profiler.Type.Memory", new IMAGE_BRUSH( "Icons/Profiler/profiler_Memory_32x", Icon16x16 ) );
		Set( "Profiler.Type.Hierarchical", new IMAGE_BRUSH( "Icons/Profiler/profiler_Event_32x", Icon16x16 ) );

		Set( "Profiler.Misc.GenericFilter", new IMAGE_BRUSH( "Icons/Profiler/profiler_GenericFilter_32x", Icon16x16 ) );
		Set( "Profiler.Misc.GenericGroup", new IMAGE_BRUSH( "Icons/Profiler/profiler_GenericGroup_32x", Icon16x16 ) );
		Set( "Profiler.Misc.CopyToClipboard", new IMAGE_BRUSH( "Icons/Profiler/profiler_CopyToClipboard_32x", Icon32x32 ) );
	
		Set( "Profiler.Misc.Disconnect", new IMAGE_BRUSH( "Icons/Profiler/profiler_Disconnect_32x", Icon32x32 ) );

		//Set( "Profiler.Type.Calls", new IMAGE_BRUSH( "Icons/Profiler/profiler_Calls_32x", Icon40x40) );
		//Set( "Profiler.Type.Calls.Small", new IMAGE_BRUSH( "Icons/Profiler/profiler_Calls_32x", Icon20x20) );
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
}
	
void FStarshipEditorStyle::FStyle::SetupGraphEditorStyles()
{
	// Graph Editor
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set( "Graph.ForegroundColor", FLinearColor(218.0f/255.0f, 218.0f/255.0f, 218.0f/255.0f, 1.0f) );

		Set( "Graph.TitleBackground", new BOX_BRUSH( "Old/Graph/GraphTitleBackground", FMargin(0) ) );
		Set( "Graph.Shadow", new BOX_BRUSH( "Old/Window/WindowBorder", 0.48f ) );
		Set( "Graph.Arrow", new IMAGE_BRUSH( "Old/Graph/Arrow", Icon16x16 ) );
		Set( "Graph.ExecutionBubble", new IMAGE_BRUSH( "Old/Graph/ExecutionBubble", Icon16x16 ) );

		Set( "Graph.PlayInEditor", new BOX_BRUSH( "/Graph/RegularNode_shadow_selected", FMargin(18.0f/64.0f) ) );
		Set( "Graph.ReadOnlyBorder", new BOX_BRUSH( "/Graph/Graph_readonly_border", FMargin(18.0f / 64.0f) ) );

		Set( "Graph.Panel.SolidBackground", new IMAGE_BRUSH( "/Graph/GraphPanel_SolidBackground", FVector2D(16, 16), FLinearColor::White, ESlateBrushTileType::Both) );
		Set( "Graph.Panel.GridLineColor",   FLinearColor(0.035f, 0.035f, 0.035f) );
		Set( "Graph.Panel.GridRuleColor",   FLinearColor(0.008f, 0.008f, 0.008f) );
		Set( "Graph.Panel.GridCenterColor", FLinearColor(0.000f, 0.000f, 0.000f) );
		
		Set( "Graph.Panel.GridRulePeriod", 8.0f ); // should be a strictly positive integral value

		Set( "Graph.Node.Separator", new IMAGE_BRUSH( "Old/Graph/NodeVerticalSeparator", FVector2D(8,8) ) );
		Set( "Graph.Node.TitleBackground", new BOX_BRUSH( "Old/Graph/NodeTitleBackground", FMargin(12.0f/64) ) );
		Set( "Graph.Node.NodeBackground", new BOX_BRUSH( "Old/Graph/NodeBackground", FMargin(12.0f/64) ) );

		Set( "Graph.Node.Body", new BOX_BRUSH( "/Graph/RegularNode_body", FMargin(16.f/64.f, 25.f/64.f, 16.f/64.f, 16.f/64.f) ) );
		Set( "Graph.Node.TintedBody", new BOX_BRUSH( "/Graph/TintedNode_body", FMargin(16.f/64.f, 25.f/64.f, 16.f/64.f, 16.f/64.f) ) );
		Set( "Graph.Node.DisabledBanner", new IMAGE_BRUSH( "/Graph/GraphPanel_StripesBackground", FVector2D(64, 64), FLinearColor(0.5f, 0.5f, 0.5f, 0.3f), ESlateBrushTileType::Both ) );
		Set( "Graph.Node.DevelopmentBanner", new IMAGE_BRUSH( "/Graph/GraphPanel_StripesBackground", FVector2D(64, 64), FLinearColor::Yellow * FLinearColor(1.f, 1.f, 1.f, 0.3f), ESlateBrushTileType::Both ) );
		Set( "Graph.Node.TitleGloss", new BOX_BRUSH( "/Graph/RegularNode_title_gloss", FMargin(12.0f/64.0f) ) );
		Set( "Graph.Node.ColorSpill", new BOX_BRUSH( "/Graph/RegularNode_color_spill", FMargin(8.0f/64.0f, 3.0f/32.0f, 0, 0) ) );
		Set( "Graph.Node.TitleHighlight", new BOX_BRUSH( "/Graph/RegularNode_title_highlight", FMargin(16.0f/64.0f, 1.0f, 16.0f/64.0f, 0.0f) ) );
		Set( "Graph.Node.IndicatorOverlay", new IMAGE_BRUSH( "/Graph/IndicatorOverlay_color_spill", FVector2D(128.f, 32.f) ) );

		Set( "Graph.Node.ShadowSize", FVector2D(12,12) );
		Set( "Graph.Node.ShadowSelected", new BOX_BRUSH( "/Graph/RegularNode_shadow_selected", FMargin(18.0f/64.0f) ) );
		Set( "Graph.Node.Shadow", new BOX_BRUSH( "/Graph/RegularNode_shadow", FMargin(18.0f/64.0f) ) );

		Set( "Graph.Node.RerouteShadow", new IMAGE_BRUSH( "/Graph/RerouteNode_shadow", FVector2D(64.0f, 64.0f) ) );
		Set( "Graph.Node.RerouteShadowSelected", new IMAGE_BRUSH( "/Graph/RerouteNode_shadow_selected", FVector2D(64.0f, 64.0f) ) );

		Set( "Graph.CompactNode.ShadowSelected", new BOX_BRUSH( "/Graph/MathNode_shadow_selected", FMargin(18.0f/64.0f) ) );

		Set( "Graph.Node.CommentBubble", new BOX_BRUSH( "Old/Graph/CommentBubble", FMargin(8/32.0f) ) );
		Set( "Graph.Node.CommentArrow", new IMAGE_BRUSH( "Old/Graph/CommentBubbleArrow", FVector2D(8,8) ) );
		Set( "Graph.Node.CommentFont", DEFAULT_FONT( "Regular", 10 ) );
		Set( "Graph.Node.Comment.BubbleOffset", FMargin(8,0,0,0) );
		Set( "Graph.Node.Comment.PinIconPadding", FMargin(0,2,0,0) );
		Set("Graph.Node.Comment.Handle", new IMAGE_BRUSH_SVG("Starship/GraphEditors/Comment_Handle", FVector2D(16.0f, 16.0f)));
		Set("Graph.Node.Comment.BubblePadding", FVector2D(3, 3));
		Set("Graph.Node.Comment.BubbleWidgetMargin", FMargin(4, 4));

		const FCheckBoxStyle CommentTitleButton = FCheckBoxStyle()
			.SetCheckBoxType( ESlateCheckBoxType::CheckBox )
			.SetUncheckedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(16,16), FLinearColor(1.f, 1.f, 1.f, 0.8f)))
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(16,16), FLinearColor(1.f, 1.f, 1.f, 0.9f)))
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(16,16), FLinearColor(1.f, 1.f, 1.f, 1.f)))
			.SetCheckedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(16,16), FLinearColor(1.f, 1.f, 1.f, 0.8f)))
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(16,16), FLinearColor(1.f, 1.f, 1.f, 1.f)))
			.SetCheckedPressedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOff_16x", FVector2D(16,16), FLinearColor(1.f, 1.f, 1.f, 0.6f)));
		Set( "CommentTitleButton", CommentTitleButton );

		const FCheckBoxStyle CommentBubbleButton = FCheckBoxStyle()
			.SetCheckBoxType( ESlateCheckBoxType::CheckBox )
			.SetUncheckedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.5f)))
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.9f)))
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 1.f)))
			.SetCheckedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.8f)))
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 1.f)))
			.SetCheckedPressedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleOn_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.6f)));
		Set( "CommentBubbleButton", CommentBubbleButton );

		const FCheckBoxStyle CommentBubblePin = FCheckBoxStyle()
			.SetCheckBoxType( ESlateCheckBoxType::CheckBox )
			.SetUncheckedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleUnPin_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.5f)))
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleUnPin_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.9f)))
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubblePin_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 1.f)))
			.SetCheckedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubblePin_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.8f)))
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubblePin_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 1.f)))
			.SetCheckedPressedImage( IMAGE_BRUSH( "Icons/icon_Blueprint_CommentBubbleUnPin_16x", FVector2D(10,10), FLinearColor(1.f, 1.f, 1.f, 0.6f)));
		Set( "CommentBubblePin", CommentBubblePin );


		Set( "Graph.VarNode.Body", new BOX_BRUSH( "/Graph/VarNode_body", FMargin(16.f/64.f, 12.f/28.f) ) );
		Set( "Graph.VarNode.ColorSpill", new IMAGE_BRUSH( "/Graph/VarNode_color_spill", FVector2D(132,28) ) );
		Set( "Graph.VarNode.Gloss", new BOX_BRUSH( "/Graph/VarNode_gloss", FMargin(16.f/64.f, 16.f/28.f, 16.f/64.f, 4.f/28.f) ) );
		Set( "Graph.VarNode.IndicatorOverlay", new IMAGE_BRUSH("/Graph/IndicatorOverlay_color_spill", FVector2D(64.f, 28.f)));
		
		Set( "Graph.VarNode.ShadowSelected", new BOX_BRUSH( "/Graph/VarNode_shadow_selected", FMargin(26.0f/64.0f) ) );
		Set( "Graph.VarNode.Shadow", new BOX_BRUSH( "/Graph/VarNode_shadow", FMargin(26.0f/64.0f) ) );

		Set( "Graph.CollapsedNode.Body", new BOX_BRUSH( "/Graph/RegularNode_body", FMargin(16.f/64.f, 25.f/64.f, 16.f/64.f, 16.f/64.f) ) );
		Set( "Graph.CollapsedNode.BodyColorSpill", new BOX_BRUSH( "/Graph/CollapsedNode_Body_ColorSpill", FMargin(16.f/64.f, 25.f/64.f, 16.f/64.f, 16.f/64.f) ) );

		{
			// State or conduit node
			{
				Set( "Graph.StateNode.Body", new BOX_BRUSH( "/Persona/StateMachineEditor/StateNode_Node_Body", FMargin(16.f/64.f, 25.f/64.f, 16.f/64.f, 16.f/64.f) ) );
				Set( "Graph.StateNode.ColorSpill", new BOX_BRUSH( "/Persona/StateMachineEditor/StateNode_Node_ColorSpill", FMargin(4.0f/64.0f, 4.0f/32.0f) ) );

				Set( "Graph.StateNode.Icon", new IMAGE_BRUSH( "/Persona/StateMachineEditor/State_Node_Icon_32x", Icon16x16 ) );
				Set( "Graph.ConduitNode.Icon", new IMAGE_BRUSH( "/Persona/StateMachineEditor/Conduit_Node_Icon_32x", Icon16x16 ) );

				Set( "Graph.StateNode.Pin.BackgroundHovered", new BOX_BRUSH( "/Persona/StateMachineEditor/StateNode_Pin_HoverCue", FMargin(12.0f/64.0f,12.0f/64.0f,12.0f/64.0f,12.0f/64.0f)));
				Set( "Graph.StateNode.Pin.Background", new FSlateNoResource() );
			}

			{
				FTextBlockStyle GraphStateNodeTitle = FTextBlockStyle(NormalText)
					.SetFont( DEFAULT_FONT( "Bold", 14 ) )
					.SetColorAndOpacity( FLinearColor(230.0f/255.0f,230.0f/255.0f,230.0f/255.0f) )
					.SetShadowOffset( FVector2D( 2,2 ) )
					.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f) );
				Set( "Graph.StateNode.NodeTitle", GraphStateNodeTitle );

				FEditableTextBoxStyle GraphStateNodeTitleEditableText = FEditableTextBoxStyle()
					.SetFont(NormalText.Font)
					.SetBackgroundImageNormal( BOX_BRUSH( "Common/TextBox", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageHovered( BOX_BRUSH( "Common/TextBox_Hovered", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageFocused( BOX_BRUSH( "Common/TextBox_Hovered", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageReadOnly( BOX_BRUSH( "Common/TextBox_ReadOnly", FMargin(4.0f/16.0f) ) )
					.SetScrollBarStyle( ScrollBar );
				Set( "Graph.StateNode.NodeTitleEditableText", GraphStateNodeTitleEditableText );

				Set( "Graph.StateNode.NodeTitleInlineEditableText", FInlineEditableTextBlockStyle()
					.SetTextStyle(GraphStateNodeTitle)
					.SetEditableTextBoxStyle(GraphStateNodeTitleEditableText)
					);
			}

			// Transition node
			{
				FMargin TestMargin(16.f/64.f, 16.f/28.f, 16.f/64.f, 4.f/28.f);
				Set( "Graph.TransitionNode.ColorSpill", new BOX_BRUSH( "/Persona/StateMachineEditor/Trans_Node_ColorSpill", TestMargin ) );
				Set( "Graph.TransitionNode.Icon", new IMAGE_BRUSH( "/Persona/StateMachineEditor/Trans_Node_Icon", FVector2D(25,25) ) );
				Set( "Graph.TransitionNode.Icon_Inertialization", new IMAGE_BRUSH( "/Persona/StateMachineEditor/Trans_Node_Icon_Inertialization", FVector2D(25,25) ) );
			}

			// Transition rule tooltip name 
			{
				Set( "Graph.TransitionNode.TooltipName", FTextBlockStyle(NormalText)
					.SetFont( DEFAULT_FONT( "Bold", 12 ) )
					.SetColorAndOpacity( FLinearColor(218.0f/255.0f,218.0f/255.0f,218.0f/255.0f) )
					.SetShadowOffset( FVector2D(1.0f, 1.0f) )
					.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f) )
				);
			}

			// Transition rule tooltip caption
			{
				Set( "Graph.TransitionNode.TooltipRule", FTextBlockStyle(NormalText)
					.SetFont( DEFAULT_FONT( "Bold", 8 ) )
					.SetColorAndOpacity( FLinearColor(180.0f/255.0f,180.0f/255.0f,180.0f/255.0f) )
					.SetShadowOffset( FVector2D(1.0f, 1.0f) )
					.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f) )
				);
			}

			Set( "Persona.RetargetManager.BoldFont",							DEFAULT_FONT( "Bold", 12 ) );
			Set( "Persona.RetargetManager.SmallBoldFont",						DEFAULT_FONT( "Bold", 10 ) );
			Set( "Persona.RetargetManager.FilterFont",							DEFAULT_FONT( "Regular", 10 ) );
			Set( "Persona.RetargetManager.ItalicFont",							DEFAULT_FONT( "Italic", 9 ) );

			Set("Persona.RetargetManager.ImportantText", FTextBlockStyle(NormalText)
				.SetFont(DEFAULT_FONT("Bold", 11))
				.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
				.SetShadowOffset(FVector2D(1, 1))
				.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));
		}

		// Behavior Tree Editor
		{
			Set( "BTEditor.Graph.BTNode.Body", new BOX_BRUSH( "/BehaviorTree/BTNode_ColorSpill", FMargin(16.f/64.f, 25.f/64.f, 16.f/64.f, 16.f/64.f) ) );
			Set( "BTEditor.Graph.BTNode.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Run_Behaviour_24x", Icon16x16 ) );

			Set( "BTEditor.Graph.BTNode.Root.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Run_Behaviour_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Composite.Selector.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Selector_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Composite.Sequence.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Sequence_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Composite.SimpleParallel.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Simple_Parallel_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.Blackboard.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Blackboard_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.CompareBlackboardEntries.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Compare_Blackboard_Entries_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.Conditional.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Conditional_Decorator_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.ConeCheck.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Cone_Check_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.Cooldown.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Cooldown_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.DoesPathExist.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Does_Path_Exist_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.ForceSuccess.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Force_Success_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.KeepInCone.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Keep_In_Cone_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.Loop.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Loop_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.NonConditional.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Non_Conditional_Decorator_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.Optional.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Optional_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.ReachedMoveGoal.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Reached_Move_Goal_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Decorator.TimeLimit.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Time_Limit_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Service.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Service_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Service.DefaultFocus.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Default_Focus_Service_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Task_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.MakeNoise.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Make_Noise_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.MoveDirectlyToward.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Move_Directly_Toward_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.MoveTo.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Move_To_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.PlaySound.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Play_Sound_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.RunBehavior.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Run_Behaviour_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.RunEQSQuery.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/EQS_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Task.Wait.Icon", new IMAGE_BRUSH( "/BehaviorTree/Icons/Wait_24x", Icon24x24 ) );
			Set( "BTEditor.Graph.BTNode.Blueprint", new IMAGE_BRUSH( "/BehaviorTree/Icons/Blueprint_Referencer_16x", Icon16x16 ) );
			Set( "BTEditor.Graph.BTNode.Index", new BOX_BRUSH( "/BehaviorTree/IndexCircle", Icon20x20, FMargin(9.0f/20.0f, 1.0f/20.0f, 9.0f/20.0f, 3.0f/20.0f) ) );

			Set( "BTEditor.Graph.BTNode.Index.Color", FLinearColor(0.3f, 0.3f, 0.3f, 1.0f) );
			Set( "BTEditor.Graph.BTNode.Index.HoveredColor", FLinearColor(1.0f, 0.0f, 0.0f, 1.0f) );
			

			FTextBlockStyle GraphNodeTitle = FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Bold", 8 ) );
			Set( "BTEditor.Graph.BTNode.IndexText", GraphNodeTitle );

			Set( "BTEditor.Debugger.BackOver", new IMAGE_BRUSH( "Icons/icon_step_back_40x", Icon40x40 ) );
			Set( "BTEditor.Debugger.BackOver.Small", new IMAGE_BRUSH( "Icons/icon_step_back_40x", Icon20x20 ) );
			Set( "BTEditor.Debugger.BackInto", new IMAGE_BRUSH("Icons/icon_step_back_40x", Icon40x40));
			Set( "BTEditor.Debugger.BackInto.Small", new IMAGE_BRUSH("Icons/icon_step_back_40x", Icon20x20));
			Set( "BTEditor.Debugger.ForwardInto", new IMAGE_BRUSH("Icons/icon_step_40x", Icon40x40));
			Set( "BTEditor.Debugger.ForwardInto.Small", new IMAGE_BRUSH( "Icons/icon_step_40x", Icon20x20 ) );
			Set( "BTEditor.Debugger.ForwardOver", new IMAGE_BRUSH("Icons/icon_step_40x", Icon40x40));
			Set( "BTEditor.Debugger.ForwardOver.Small", new IMAGE_BRUSH("Icons/icon_step_40x", Icon20x20));
			Set( "BTEditor.Debugger.StepOut", new IMAGE_BRUSH("Icons/icon_step_40x", Icon40x40));
			Set( "BTEditor.Debugger.StepOut.Small", new IMAGE_BRUSH("Icons/icon_step_40x", Icon20x20));
			Set( "BTEditor.Debugger.SingleStep", new IMAGE_BRUSH("Icons/icon_advance_40x", Icon40x40));
			Set( "BTEditor.Debugger.SingleStep.Small", new IMAGE_BRUSH( "Icons/icon_advance_40x", Icon20x20 ) );

			Set( "BTEditor.Debugger.PausePlaySession", new IMAGE_BRUSH( "Icons/icon_pause_40x", Icon40x40 ) );
			Set( "BTEditor.Debugger.PausePlaySession.Small", new IMAGE_BRUSH( "Icons/icon_pause_40x", Icon20x20 ) );
			Set("BTEditor.Debugger.ResumePlaySession", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon40x40));
			Set("BTEditor.Debugger.ResumePlaySession.Small", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));
			Set( "BTEditor.Debugger.StopPlaySession", new IMAGE_BRUSH( "Icons/icon_stop_40x", Icon40x40 ) );
			Set( "BTEditor.Debugger.StopPlaySession.Small", new IMAGE_BRUSH( "Icons/icon_stop_40x", Icon20x20 ) );
			Set("BTEditor.Debugger.LateJoinSession", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon40x40));
			Set("BTEditor.Debugger.LateJoinSession.Small", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));

			Set( "BTEditor.Debugger.CurrentValues", new IMAGE_BRUSH( "BehaviorTree/Debugger_Current_40x", Icon40x40 ) );
			Set( "BTEditor.Debugger.CurrentValues.Small", new IMAGE_BRUSH( "BehaviorTree/Debugger_Current_40x", Icon20x20 ) );
			Set( "BTEditor.Debugger.SavedValues", new IMAGE_BRUSH( "BehaviorTree/Debugger_Saved_40x", Icon40x40 ) );
			Set( "BTEditor.Debugger.SavedValues.Small", new IMAGE_BRUSH( "BehaviorTree/Debugger_Saved_40x", Icon20x20 ) );

			Set( "BTEditor.DebuggerOverlay.Breakpoint.Disabled", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Disabled", Icon32x32 ) );
			Set( "BTEditor.DebuggerOverlay.Breakpoint.Enabled", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Valid", Icon32x32 ) );
			Set( "BTEditor.DebuggerOverlay.ActiveNodePointer", new IMAGE_BRUSH( "Old/Kismet2/IP_Normal", FVector2D(128,96)) );
			Set( "BTEditor.DebuggerOverlay.SearchTriggerPointer", new IMAGE_BRUSH( "/BehaviorTree/SearchTriggerPointer", FVector2D(48,64)) );
			Set( "BTEditor.DebuggerOverlay.FailedTriggerPointer", new IMAGE_BRUSH( "/BehaviorTree/FailedTriggerPointer", FVector2D(48,64)) );
			Set( "BTEditor.DebuggerOverlay.BreakOnBreakpointPointer", new IMAGE_BRUSH( "Old/Kismet2/IP_Breakpoint", FVector2D(128,96)) );

			Set( "BTEditor.Blackboard.NewEntry", new IMAGE_BRUSH( "BehaviorTree/Blackboard_AddKey_40x", Icon40x40 ) );
			Set( "BTEditor.Blackboard.NewEntry.Small", new IMAGE_BRUSH( "BehaviorTree/Blackboard_AddKey_40x", Icon20x20 ) );

			Set( "BTEditor.SwitchToBehaviorTreeMode", new IMAGE_BRUSH( "BehaviorTree/BehaviorTreeMode_40x", Icon20x20));
			Set( "BTEditor.SwitchToBlackboardMode", new IMAGE_BRUSH( "BehaviorTree/BlackboardMode_40x", Icon20x20));


			// Blackboard classes
			Set( "ClassIcon.BlackboardKeyType_Bool", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.300000f, 0.0f, 0.0f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Class", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.1f, 0.0f, 0.5f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Enum", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.0f, 0.160000f, 0.131270f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Float", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.357667f, 1.0f, 0.060000f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Int", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.013575f, 0.770000f, 0.429609f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Name", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.607717f, 0.224984f, 1.0f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_NativeEnum", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.0f, 0.160000f, 0.131270f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Object", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.0f, 0.4f, 0.910000f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Rotator", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(0.353393f, 0.454175f, 1.0f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_String", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(1.0f, 0.0f, 0.660537f, 1.0f) ) );
			Set( "ClassIcon.BlackboardKeyType_Vector", new IMAGE_BRUSH( "Icons/pill_16x", Icon16x16, FLinearColor(1.0f, 0.591255f, 0.016512f, 1.0f) ) );

			Set( "BTEditor.Common.NewBlackboard", new IMAGE_BRUSH( "BehaviorTree/NewBlackboard_40x", Icon40x40));
			Set( "BTEditor.Common.NewBlackboard.Small", new IMAGE_BRUSH( "BehaviorTree/NewBlackboard_20x", Icon20x20));
			Set( "BTEditor.Graph.NewTask", new IMAGE_BRUSH( "BehaviorTree/NewTask_40x", Icon40x40));
			Set( "BTEditor.Graph.NewTask.Small", new IMAGE_BRUSH( "BehaviorTree/NewTask_20x", Icon20x20));
			Set( "BTEditor.Graph.NewDecorator", new IMAGE_BRUSH( "BehaviorTree/NewDecorator_40x", Icon40x40));
			Set( "BTEditor.Graph.NewDecorator.Small", new IMAGE_BRUSH( "BehaviorTree/NewDecorator_20x", Icon20x20));
			Set( "BTEditor.Graph.NewService", new IMAGE_BRUSH( "BehaviorTree/NewService_40x", Icon40x40));
			Set( "BTEditor.Graph.NewService.Small", new IMAGE_BRUSH( "BehaviorTree/NewService_20x", Icon20x20));
		}
		
		{
			Set("EnvQueryEditor.Profiler.LoadStats", new IMAGE_BRUSH("Icons/LV_Load", Icon40x40));
			Set("EnvQueryEditor.Profiler.SaveStats", new IMAGE_BRUSH("Icons/LV_Save", Icon40x40));
		}

		// Visible on hover button for transition node
		{
			Set( "TransitionNodeButton.Normal", new FSlateNoResource() );
			Set( "TransitionNodeButton.Hovered", new IMAGE_BRUSH( "/Persona/StateMachineEditor/Trans_Button_Hovered", FVector2D(12,25) ) );
			Set( "TransitionNodeButton.Pressed", new IMAGE_BRUSH( "/Persona/StateMachineEditor/Trans_Button_Pressed", FVector2D(12,25) ) );
		}

		{
			Set( "Graph.AnimationResultNode.Body", new IMAGE_BRUSH( "/Graph/Animation/AnimationNode_Result_128x", FVector2D(128, 128) ) );
			Set( "Graph.AnimationFastPathIndicator", new IMAGE_BRUSH( "/Graph/Animation/AnimationNode_FastPath", Icon32x32 ) );
		}

		// SoundCueEditor Graph Nodes
		{
			Set( "Graph.SoundResultNode.Body", new IMAGE_BRUSH( "/Graph/SoundCue_SpeakerIcon", FVector2D(144, 144) ) );
		}

		Set( "Graph.Node.NodeEntryTop", new IMAGE_BRUSH( "Old/Graph/NodeEntryTop", FVector2D(64,12) ) );
		Set( "Graph.Node.NodeEntryBottom", new IMAGE_BRUSH( "Old/Graph/NodeEntryBottom", FVector2D(64,12) ) );
		Set( "Graph.Node.NodeExitTop", new IMAGE_BRUSH( "Old/Graph/NodeExitTop", FVector2D(64,12) ) );
		Set( "Graph.Node.NodeExitBottom", new IMAGE_BRUSH( "Old/Graph/NodeExitBottom", FVector2D(64,12) ) );

		Set( "Graph.Node.NodeEntryShadow", new BOX_BRUSH( "Old/Graph/NodeEntryShadow", FMargin(5.f/80, 21.f/52) ) );
		Set( "Graph.Node.NodeEntryShadowSelected", new BOX_BRUSH( "Old/Graph/NodeEntryShadowSelected", FMargin(5.f/80, 21.f/52) ) );
		Set( "Graph.Node.NodeExitShadow", new BOX_BRUSH( "Old/Graph/NodeExitShadow", FMargin(5.f/80, 21.f/52) ) );
		Set( "Graph.Node.NodeExitShadowSelected", new BOX_BRUSH( "Old/Graph/NodeExitShadowSelected", FMargin(5.f/80, 21.f/52) ) );

		Set( "Graph.Node.Autoplay", new IMAGE_BRUSH( "Graph/Icons/Overlay_Autoplay", FVector2D(22,22) ) );
		Set( "Graph.Node.Loop", new IMAGE_BRUSH( "Graph/Icons/Overlay_Loop", FVector2D(22,22) ) );

		{
			FTextBlockStyle GraphNodeTitle = FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Bold", 10 ) )
				.SetColorAndOpacity( FStyleColors::Foreground )
				.SetShadowOffset( FVector2D::ZeroVector )
				.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f) );
			Set( "Graph.Node.NodeTitle", GraphNodeTitle );

			FEditableTextBoxStyle GraphNodeTitleEditableText = FEditableTextBoxStyle(NormalEditableTextBoxStyle)
				.SetFont(NormalText.Font)
				.SetForegroundColor(FStyleColors::Input)
				.SetBackgroundImageNormal(FSlateRoundedBoxBrush(FStyleColors::Foreground, FStyleColors::Secondary, 1.0f))
				.SetBackgroundImageHovered(FSlateRoundedBoxBrush(FStyleColors::Foreground, FStyleColors::Hover, 1.0f))
				.SetBackgroundImageFocused(FSlateRoundedBoxBrush(FStyleColors::Foreground, FStyleColors::Primary, 1.0f))
				.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(FStyleColors::Header, FStyleColors::InputOutline, 1.0f))
				.SetScrollBarStyle( ScrollBar );
			Set( "Graph.Node.NodeTitleEditableText", GraphNodeTitleEditableText );

			Set( "Graph.Node.NodeTitleInlineEditableText", FInlineEditableTextBlockStyle()
				.SetTextStyle(GraphNodeTitle)
				.SetEditableTextBoxStyle(GraphNodeTitleEditableText)
			);

			Set( "Graph.Node.NodeTitleExtraLines", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Italic", 9 ) )
				.SetColorAndOpacity( FLinearColor(218.0f/255.0f,218.0f/255.0f,96.0f/255.0f, 0.5f) )
				.SetShadowOffset( FVector2D::ZeroVector )
				.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f) )
			);
		
			FTextBlockStyle GraphCommentBlockTitle = FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Bold", 18 ) )
				.SetColorAndOpacity( FLinearColor(218.0f/255.0f,218.0f/255.0f,218.0f/255.0f) )
				.SetShadowOffset( FVector2D(1.5f, 1.5f) )
				.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f) );
			Set( "Graph.CommentBlock.Title", GraphCommentBlockTitle );

			FEditableTextBoxStyle GraphCommentBlockTitleEditableText = FEditableTextBoxStyle(GraphNodeTitleEditableText)
				.SetFont(GraphCommentBlockTitle.Font)
				.SetScrollBarStyle( ScrollBar );
			Set( "Graph.CommentBlock.TitleEditableText", GraphCommentBlockTitleEditableText );

			Set( "Graph.CommentBlock.TitleInlineEditableText", FInlineEditableTextBlockStyle()
				.SetTextStyle(GraphCommentBlockTitle)
				.SetEditableTextBoxStyle(GraphCommentBlockTitleEditableText)
				);

			Set( "Graph.CompactNode.Title", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 20 ) )
				.SetColorAndOpacity( FLinearColor(1.0f, 1.0f, 1.0f, 0.5f) )
				.SetShadowOffset( FVector2D::ZeroVector )
				.SetShadowColorAndOpacity( FLinearColor::White )
			);

			Set( "Graph.ArrayCompactNode.Title", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 20 ) )
				.SetColorAndOpacity( FLinearColor(1.0f, 1.0f, 1.0f, 0.5f) ) //218.0f/255.0f, 218.0f/255.0f, 218.0f/255.0f, 0.25f) )
				.SetShadowOffset( FVector2D::ZeroVector )
				.SetShadowColorAndOpacity( FLinearColor::White )
				);

			Set( "Graph.Node.PinName", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Regular", 9 ) )
				.SetColorAndOpacity( FLinearColor(218.0f/255.0f,218.0f/255.0f,218.0f/255.0f) )
				.SetShadowOffset( FVector2D::ZeroVector )
				.SetShadowColorAndOpacity( FLinearColor(0.8f,0.8f,0.8f, 0.5) )
			);

			// Inline Editable Text Block
			{
				FTextBlockStyle InlineEditableTextBlockReadOnly = FTextBlockStyle(NormalText)
					.SetFont(DEFAULT_FONT("Regular", 9))
					.SetColorAndOpacity(FLinearColor(218.0f / 255.0f, 218.0f / 255.0f, 218.0f / 255.0f))
					.SetShadowOffset(FVector2D::ZeroVector)
					.SetShadowColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 0.5));

				FEditableTextBoxStyle InlineEditableTextBlockEditable = FEditableTextBoxStyle()
					.SetFont(DEFAULT_FONT("Regular", 9))
					.SetBackgroundImageNormal(BOX_BRUSH("Common/TextBox", FMargin(4.0f / 16.0f)))
					.SetBackgroundImageHovered(BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
					.SetBackgroundImageFocused(BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
					.SetBackgroundImageReadOnly(BOX_BRUSH("Common/TextBox_ReadOnly", FMargin(4.0f / 16.0f)))
					.SetScrollBarStyle(ScrollBar);

				FInlineEditableTextBlockStyle InlineEditableTextBlockStyle = FInlineEditableTextBlockStyle()
					.SetTextStyle(InlineEditableTextBlockReadOnly)
					.SetEditableTextBoxStyle(InlineEditableTextBlockEditable);

				Set("Graph.Node.InlineEditablePinName", InlineEditableTextBlockStyle);
			}
		}

		{
			const FLinearColor BrighterColor(1.0f, 1.0f, 1.0f, 0.4f);
			const FLinearColor DarkerColor(0.8f, 0.8f, 0.8f, 0.4f);
			const float MarginSize = 9.0f/16.0f;

			/* Set states for various SCheckBox images ... */
			const FCheckBoxStyle GraphNodeAdvancedViewCheckBoxStyle = FCheckBoxStyle()
				.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
				.SetUncheckedImage( FSlateNoResource() )
				.SetUncheckedHoveredImage( BOX_BRUSH( "Common/RoundedSelection_16x", MarginSize, DarkerColor ) )
				.SetCheckedPressedImage( BOX_BRUSH( "Common/RoundedSelection_16x", MarginSize, BrighterColor ) )
				.SetCheckedImage( FSlateNoResource() )
				.SetCheckedHoveredImage( BOX_BRUSH( "Common/RoundedSelection_16x", MarginSize, DarkerColor ) )
				.SetCheckedPressedImage( BOX_BRUSH( "Common/RoundedSelection_16x", MarginSize, BrighterColor ) );
			/* ... and add new style */
			Set( "Graph.Node.AdvancedView", GraphNodeAdvancedViewCheckBoxStyle );
		}

		// Special style for switch statements default pin label
		{
			Set( "Graph.Node.DefaultPinName", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "Italic", 9 ) )
				.SetColorAndOpacity( FLinearColor(218.0f/255.0f,218.0f/255.0f,218.0f/255.0f) )
				.SetShadowOffset( FVector2D::ZeroVector )
				.SetShadowColorAndOpacity( FLinearColor(0.8f,0.8f,0.8f, 0.5) )
			);
		}
		Set( "Graph.Pin.DefaultPinSeparator", new IMAGE_BRUSH( "/Graph/PinSeparator", FVector2D(64,8) ) );

		/** Original Pin Styles */
		Set( "Graph.Pin.Connected", new IMAGE_BRUSH( "/Graph/Pin_connected", FVector2D(11,11) ) );
		Set( "Graph.Pin.Disconnected", new IMAGE_BRUSH( "/Graph/Pin_disconnected", FVector2D(11,11) ) );
		Set( "Graph.ArrayPin.Connected", new IMAGE_BRUSH( "/Graph/ArrayPin_connected", FVector2D(11,11) ) );
		Set( "Graph.ArrayPin.Disconnected", new IMAGE_BRUSH( "/Graph/ArrayPin_disconnected", FVector2D(11,11) ) );
		Set( "Graph.RefPin.Connected", new IMAGE_BRUSH( "/Graph/RefPin_connected", FVector2D(11,11) ) );
		Set( "Graph.RefPin.Disconnected", new IMAGE_BRUSH( "/Graph/RefPin_disconnected", FVector2D(11,11) ) );

		Set("Graph.Pin.CopyNodePinLeft_Connected", new IMAGE_BRUSH("/Graph/CopyNodePinLeft_connected", FVector2D(12, 24)));
		Set("Graph.Pin.CopyNodePinLeft_Disconnected", new IMAGE_BRUSH("/Graph/CopyNodePinLeft_disconnected", FVector2D(12, 24)));

		Set("Graph.Pin.CopyNodePinRight_Connected", new IMAGE_BRUSH("/Graph/CopyNodePinRight_connected", FVector2D(12, 24)));
		Set("Graph.Pin.CopyNodePinRight_Disconnected", new IMAGE_BRUSH("/Graph/CopyNodePinRight_disconnected", FVector2D(12, 24)));

		/** Variant A Pin Styles */
		Set( "Graph.Pin.Connected_VarA", new IMAGE_BRUSH( "/Graph/Pin_connected_VarA", FVector2D(15,11)) );
		Set( "Graph.Pin.Disconnected_VarA", new IMAGE_BRUSH( "/Graph/Pin_disconnected_VarA", FVector2D(15,11)) );

		Set( "Graph.DelegatePin.Connected", new IMAGE_BRUSH( "/Graph/DelegatePin_Connected", FVector2D(11,11) ) );
		Set( "Graph.DelegatePin.Disconnected", new IMAGE_BRUSH( "/Graph/DelegatePin_Disconnected", FVector2D(11,11) ) );

		Set( "Graph.Replication.AuthorityOnly", new IMAGE_BRUSH( "/Graph/AuthorityOnly", FVector2D(32,32) ) );
		Set( "Graph.Replication.ClientEvent", new IMAGE_BRUSH( "/Graph/ClientEvent", FVector2D(32,32) ) );
		Set( "Graph.Replication.Replicated", new IMAGE_BRUSH( "/Graph/Replicated", FVector2D(32,32) ) );

		Set("Graph.Editor.EditorOnlyIcon", new IMAGE_BRUSH("/Graph/EditorOnly", FVector2D(32, 32)));

		Set( "Graph.Event.InterfaceEventIcon", new IMAGE_BRUSH("/Graph/InterfaceEventIcon", FVector2D(32,32) ) );

		Set( "Graph.Latent.LatentIcon", new IMAGE_BRUSH("/Graph/LatentIcon", FVector2D(32,32) ) );
		Set( "Graph.Message.MessageIcon", new IMAGE_BRUSH("/Graph/MessageIcon", FVector2D(32,32) ) );

		Set( "Graph.ExecPin.Connected", new IMAGE_BRUSH( "Old/Graph/ExecPin_Connected", Icon12x16 ) );
		Set( "Graph.ExecPin.Disconnected", new IMAGE_BRUSH( "Old/Graph/ExecPin_Disconnected", Icon12x16 ) );
		Set( "Graph.ExecPin.ConnectedHovered", new IMAGE_BRUSH( "Old/Graph/ExecPin_Connected", Icon12x16, FLinearColor(0.8f,0.8f,0.8f) ) );
		Set( "Graph.ExecPin.DisconnectedHovered", new IMAGE_BRUSH( "Old/Graph/ExecPin_Disconnected", Icon12x16, FLinearColor(0.8f,0.8f,0.8f) ) );

		const FVector2D Icon15x28(15.0f, 28.0f);
		Set("Graph.PosePin.Connected", new IMAGE_BRUSH("Graph/Animation/PosePin_Connected_15x28", Icon15x28));
		Set("Graph.PosePin.Disconnected", new IMAGE_BRUSH("Graph/Animation/PosePin_Disconnected_15x28", Icon15x28));
		Set("Graph.PosePin.ConnectedHovered", new IMAGE_BRUSH("Graph/Animation/PosePin_Connected_15x28", Icon15x28, FLinearColor(0.8f, 0.8f, 0.8f)));
		Set("Graph.PosePin.DisconnectedHovered", new IMAGE_BRUSH("Graph/Animation/PosePin_Disconnected_15x28", Icon15x28, FLinearColor(0.8f, 0.8f, 0.8f)));

		// Events Exec Pins
		Set( "Graph.ExecEventPin.Connected", new IMAGE_BRUSH( "Graph/EventPin_Connected", Icon16x16 ) );
		Set( "Graph.ExecEventPin.Disconnected", new IMAGE_BRUSH( "Graph/EventPin_Disconnected", Icon16x16 ) );
		Set( "Graph.ExecEventPin.ConnectedHovered", new IMAGE_BRUSH( "Graph/EventPin_Connected", Icon16x16, FLinearColor(0.8f,0.8f,0.8f) ) );
		Set( "Graph.ExecEventPin.DisconnectedHovered", new IMAGE_BRUSH( "Graph/EventPin_Disconnected", Icon16x16, FLinearColor(0.8f,0.8f,0.8f) ) );

		Set( "Graph.WatchedPinIcon_Pinned", new IMAGE_BRUSH( "Old/Graph/WatchedPinIcon_Pinned", Icon16x16 ) );

		Set( "Graph.Pin.BackgroundHovered", new IMAGE_BRUSH( "/Graph/Pin_hover_cue", FVector2D(32,8)));
		Set( "Graph.Pin.Background", new FSlateNoResource() );

		Set( "Graph.Pin.ObjectSet", new IMAGE_BRUSH( "Old/Graph/Pin_ObjectSet", Icon12x12 ) );
		Set( "Graph.Pin.ObjectEmpty", new IMAGE_BRUSH( "Old/Graph/Pin_ObjectEmpty", Icon12x12 ) );

		Set( "Graph.ConnectorFeedback.Border", new BOX_BRUSH( "Old/Menu_Background", FMargin(8.0f/64.0f) ) );
		Set( "Graph.ConnectorFeedback.OK", new IMAGE_BRUSH( "Old/Graph/Feedback_OK", Icon16x16 ) );
		Set( "Graph.ConnectorFeedback.OKWarn", new IMAGE_BRUSH( "Old/Graph/Feedback_OKWarn", Icon16x16 ) );
		Set( "Graph.ConnectorFeedback.Error", new IMAGE_BRUSH( "Old/Graph/Feedback_Error", Icon16x16 ) );
		Set( "Graph.ConnectorFeedback.NewNode", new IMAGE_BRUSH( "Old/Graph/Feedback_NewNode", Icon16x16 ) );
		Set( "Graph.ConnectorFeedback.ViaCast", new IMAGE_BRUSH( "Old/Graph/Feedback_ConnectViaCast", Icon16x16 ) );
		Set( "Graph.ConnectorFeedback.ShowNode", new IMAGE_BRUSH( "Graph/Feedback_ShowNode", Icon16x16 ) );

		{
			Set( "Graph.CornerText", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 48 ) )
				.SetColorAndOpacity( FLinearColor(0.8, 0.8f, 0.8f, 0.2f) )
				.SetShadowOffset( FVector2D::ZeroVector )
			);

			Set( "Graph.SimulatingText", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 48 ) )
				.SetColorAndOpacity( FLinearColor(0.8, 0.8f, 0.0f, 0.2f) )
				.SetShadowOffset( FVector2D::ZeroVector )
			);

			Set( "GraphPreview.CornerText", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 16 ) )
				.SetColorAndOpacity( FLinearColor(0.8, 0.8f, 0.8f, 0.2f) )
				.SetShadowOffset( FVector2D::ZeroVector )
			);

			Set( "Graph.InstructionText", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 24 ) )
				.SetColorAndOpacity( FLinearColor(1.f, 1.f, 1.f, 0.6f) )
				.SetShadowOffset( FVector2D::ZeroVector )
			);

			Set( "Graph.InstructionBackground", new BOX_BRUSH("Common/GroupBorder", FMargin(4.0f / 16.0f), FLinearColor(0.1f, 0.1f, 0.1f, 0.7f)) );
		}

		{
			Set( "Graph.ZoomText", FTextBlockStyle(NormalText)
				.SetFont( DEFAULT_FONT( "BoldCondensed", 16 ) )
			);
		}

		Set("ClassIcon.K2Node_CallFunction",	new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Function", Icon16x16));
		Set("ClassIcon.K2Node_FunctionEntry",	new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Function", Icon16x16));
		Set("ClassIcon.K2Node_CustomEvent",		new IMAGE_BRUSH("Icons/icon_Blueprint_Event_16x", Icon16x16));
		Set("ClassIcon.K2Node_Event",			new IMAGE_BRUSH("Icons/icon_Blueprint_Event_16x", Icon16x16));
		Set("ClassIcon.K2Node_Variable",		new IMAGE_BRUSH("Graph/Icons/Node", Icon16x16, FLinearColor::White));
		Set("ClassIcon.K2Node_VariableGet",		new IMAGE_BRUSH("Graph/Icons/FIB_VarGet", Icon16x16, FLinearColor::White));
		Set("ClassIcon.K2Node_VariableSet",		new IMAGE_BRUSH("Graph/Icons/FIB_VarSet", Icon16x16, FLinearColor::White));
		Set("ClassIcon.K2Node_DynamicCast",		new IMAGE_BRUSH("Icons/icon_Blueprint_Cast_16x", Icon16x16));
		Set("ClassIcon.EdGraphNode_Comment",	new IMAGE_BRUSH("Icons/icon_Blueprint_Comment_16x", Icon16x16));

		Set("GraphEditor.Clean",				new IMAGE_BRUSH_SVG("Starship/GraphEditors/CleanUp", Icon20x20));
		Set("GraphEditor.OverrideFunction_16x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_OverrideFunction", Icon16x16));
		Set("GraphEditor.OverrideFunction_24x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_OverrideFunction", Icon24x24));
		Set("GraphEditor.EventGraph_16x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_BlueprintEditor_EventGraph", Icon16x16));
		Set("GraphEditor.EventGraph_24x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_BlueprintEditor_EventGraph", Icon24x24));
		Set("GraphEditor.Macro_16x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Macro", Icon16x16));
		Set("GraphEditor.Macro_24x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Macro", Icon24x24));
		Set("GraphEditor.Function_16x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Function", Icon16x16));
		Set("GraphEditor.Function_24x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Function", Icon24x24));
		Set("GraphEditor.Delegate_16x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Delegate", Icon16x16));
		Set("GraphEditor.Delegate_24x", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Delegate", Icon24x24));



		Set( "GraphEditor.Default_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Node_16x", Icon16x16));
		Set( "GraphEditor.InterfaceFunction_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Interfacefunction_16x", Icon16x16));
		Set( "GraphEditor.PureFunction_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_NewPureFunction_16x", Icon16x16 ) );
		Set( "GraphEditor.PotentialOverrideFunction_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_OverrideableFunction_16x", Icon16x16 ) );
		Set( "GraphEditor.OverridePureFunction_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_OverridePureFunction_16x", Icon16x16 ) );
		Set( "GraphEditor.SubGraph_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_SubgraphComposite_16x", Icon16x16 ) );
		Set( "GraphEditor.Animation_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Anim_16x", Icon16x16 ) );
		Set( "GraphEditor.Conduit_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Conduit_16x", Icon16x16 ) );
		Set( "GraphEditor.Rule_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Rule_16x", Icon16x16 ) );
		Set( "GraphEditor.State_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_State_16x", Icon16x16 ) );
		Set( "GraphEditor.StateMachine_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_StateMachine_16x", Icon16x16 ) );
		Set( "GraphEditor.Event_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Event_16x", Icon16x16 ) );
		Set( "GraphEditor.CustomEvent_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_CustomEvent_16x", Icon16x16 ) );
		Set( "GraphEditor.CallInEditorEvent_16x", new IMAGE_BRUSH( "Icons/icon_Blueprint_CallInEditor_16x", Icon16x16 ) );
		Set( "GraphEditor.Timeline_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Timeline_16x", Icon16x16));
		Set( "GraphEditor.Comment_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Comment_16x", Icon16x16));
		Set( "GraphEditor.Documentation_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Documentation_16x", Icon16x16));
		Set( "GraphEditor.Switch_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Switch_16x", Icon16x16));
		Set( "GraphEditor.BreakStruct_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_BreakStruct_16x", Icon16x16));
		Set( "GraphEditor.MakeStruct_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_MakeStruct_16x", Icon16x16));
		Set( "GraphEditor.Sequence_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Sequence_16x", Icon16x16));
		Set( "GraphEditor.Branch_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Branch_16x", Icon16x16));
		Set( "GraphEditor.SpawnActor_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_SpawnActor_16x", Icon16x16));
		Set( "GraphEditor.PadEvent_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_PadEvent_16x", Icon16x16));
		Set( "GraphEditor.MouseEvent_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_MouseEvent_16x", Icon16x16));
		Set( "GraphEditor.KeyEvent_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_KeyboardEvent_16x", Icon16x16));
		Set( "GraphEditor.TouchEvent_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_TouchEvent_16x", Icon16x16));
		Set( "GraphEditor.MakeArray_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_MakeArray_16x", Icon16x16));
		Set( "GraphEditor.MakeSet_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_MakeSet_16x", Icon16x16));
		Set( "GraphEditor.MakeMap_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_MakeMap_16x", Icon16x16));
		Set( "GraphEditor.Enum_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Enum_16x", Icon16x16));
		Set( "GraphEditor.Select_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Select_16x", Icon16x16));
		Set( "GraphEditor.Cast_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Cast_16x", Icon16x16));

		Set( "GraphEditor.Macro.Loop_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Loop_16x", Icon16x16));
		Set( "GraphEditor.Macro.Gate_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_Gate_16x", Icon16x16));
		Set( "GraphEditor.Macro.DoN_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_DoN_16x", Icon16x16));
		Set( "GraphEditor.Macro.DoOnce_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_DoOnce_16x", Icon16x16));
		Set( "GraphEditor.Macro.IsValid_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_IsValid_16x", Icon16x16));
		Set( "GraphEditor.Macro.FlipFlop_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_FlipFlop_16x", Icon16x16));
		Set( "GraphEditor.Macro.ForEach_16x", new IMAGE_BRUSH("Icons/icon_Blueprint_ForEach_16x", Icon16x16));



		Set( "GraphEditor.InterfaceFunction_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_InterfaceFunction_24x", Icon24x24 ) );

		Set( "GraphEditor.PotentialOverrideFunction_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_OverrideableFunction_24x", Icon24x24 ) );

		Set( "GraphEditor.SubGraph_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_SubgraphComposite_24x", Icon24x24 ) );
		Set( "GraphEditor.Animation_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Anim_24x", Icon24x24 ) );
		Set( "GraphEditor.Conduit_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Conduit_24x", Icon24x24 ) );
		Set( "GraphEditor.Rule_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_Rule_24x", Icon24x24 ) );
		Set( "GraphEditor.State_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_State_24x", Icon24x24 ) );
		Set( "GraphEditor.StateMachine_24x", new IMAGE_BRUSH( "Icons/icon_Blueprint_StateMachine_24x", Icon24x24 ) );

		Set( "GraphEditor.FunctionGlyph", new IMAGE_BRUSH( "Graph/Icons/Function", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.NodeGlyph", new IMAGE_BRUSH( "Graph/Icons/Node", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.PinIcon", new IMAGE_BRUSH( "Graph/Icons/Pin", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.ArrayPinIcon", new IMAGE_BRUSH( "Graph/Icons/ArrayPin", Icon22x22, FLinearColor::White ) );
		Set( "GraphEditor.RefPinIcon", new IMAGE_BRUSH( "Graph/Icons/RefPin", Icon22x22, FLinearColor::White ) );
		Set( "GraphEditor.UbergraphGlyph", new IMAGE_BRUSH( "Graph/Icons/EventGraph", Icon22x22, FLinearColor::White) );		
		Set( "GraphEditor.SubgraphGlyph", new IMAGE_BRUSH( "Graph/Icons/Subgraph", Icon22x22, FLinearColor::White) );		
		Set( "GraphEditor.AnimationGlyph", new IMAGE_BRUSH( "Graph/Icons/Robot", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.MacroGlyph", new IMAGE_BRUSH( "Graph/Icons/Macro", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.EnumGlyph", new IMAGE_BRUSH( "Graph/Icons/Enum", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.TimelineGlyph", new IMAGE_BRUSH( "Graph/Icons/Timeline", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.EventGlyph", new IMAGE_BRUSH( "Graph/Icons/Event", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.EventCustomGlyph", new IMAGE_BRUSH( "Graph/Icons/Event_Custom", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.SCSGlyph", new IMAGE_BRUSH( "Graph/Icons/Hammer", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.StructGlyph", new IMAGE_BRUSH( "Graph/Icons/Struct", Icon22x22, FLinearColor::White) );
		// Find In Blueprints
		Set( "GraphEditor.FIB_CallFunction", new IMAGE_BRUSH( "Graph/Icons/FIB_CallFunction", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.FIB_MacroInstance", new IMAGE_BRUSH( "Graph/Icons/FIB_MacroInstance", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.FIB_Event", new IMAGE_BRUSH( "Graph/Icons/FIB_Event", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.FIB_VariableGet", new IMAGE_BRUSH( "Graph/Icons/FIB_VarGet", Icon22x22, FLinearColor::White) );
		Set( "GraphEditor.FIB_VariableSet", new IMAGE_BRUSH( "Graph/Icons/FIB_VarSet", Icon22x22, FLinearColor::White) );

		Set( "GraphEditor.FunctionOL.Interface", new IMAGE_BRUSH( "Graph/Icons/Overlay_Interface", Icon22x22 ) );
		Set( "GraphEditor.FunctionOL.New", new IMAGE_BRUSH( "Graph/Icons/Overlay_New", Icon22x22 ) );
		Set( "GraphEditor.FunctionOL.Override", new IMAGE_BRUSH( "Graph/Icons/Overlay_Override", Icon22x22 ) );
		Set( "GraphEditor.FunctionOL.PotentialOverride", new IMAGE_BRUSH( "Graph/Icons/Overlay_PotentialOverride", Icon22x22 ) );

		Set( "GraphEditor.GetSequenceBinding", new IMAGE_BRUSH("Icons/icon_Blueprint_GetSequenceBinding_16x", Icon16x16));

		Set( "GraphEditor.HideUnusedPins", new IMAGE_BRUSH( "Icons/hide_unusedpins", Icon40x40 ) );
		Set( "GraphEditor.HideUnusedPins.Small", new IMAGE_BRUSH( "Icons/hide_unusedpins", Icon20x20 ) );

		Set( "GraphEditor.GoToDocumentation", new IMAGE_BRUSH( "Common/icon_Help_Hover_16x", Icon16x16 ) );

		Set( "GraphEditor.AlignNodesTop", new IMAGE_BRUSH( "Icons/GraphEditor/icon_AlignNodesTop_20px", Icon20x20 ) );
		Set( "GraphEditor.AlignNodesMiddle", new IMAGE_BRUSH( "Icons/GraphEditor/icon_AlignNodesMiddle_20px", Icon20x20 ) );
		Set( "GraphEditor.AlignNodesBottom", new IMAGE_BRUSH( "Icons/GraphEditor/icon_AlignNodesBottom_20px", Icon20x20 ) );
		Set( "GraphEditor.AlignNodesLeft", new IMAGE_BRUSH( "Icons/GraphEditor/icon_AlignNodesLeft_20px", Icon20x20 ) );
		Set( "GraphEditor.AlignNodesCenter", new IMAGE_BRUSH( "Icons/GraphEditor/icon_AlignNodesCenter_20px", Icon20x20 ) );
		Set( "GraphEditor.AlignNodesRight", new IMAGE_BRUSH( "Icons/GraphEditor/icon_AlignNodesRight_20px", Icon20x20 ) );

		Set( "GraphEditor.StraightenConnections", new IMAGE_BRUSH( "Icons/GraphEditor/icon_StraightenConnections_20px", Icon20x20 ) );

		Set( "GraphEditor.DistributeNodesHorizontally", new IMAGE_BRUSH( "Icons/GraphEditor/icon_DistributeNodesHorizontally_20px", Icon20x20 ) );
		Set( "GraphEditor.DistributeNodesVertically", new IMAGE_BRUSH( "Icons/GraphEditor/icon_DistributeNodesVertically_20px", Icon20x20 ) );
		
		Set( "GraphEditor.ToggleHideUnrelatedNodes", new IMAGE_BRUSH_SVG( "Starship/GraphEditors/HideUnrelated", Icon20x20 ) );
		Set("GraphEditor.Bookmark", new IMAGE_BRUSH_SVG("Starship/Common/Bookmarks", Icon16x16));

		// Graph editor widgets
		{
			// EditableTextBox
			{
				Set( "Graph.EditableTextBox", FEditableTextBoxStyle()
					.SetBackgroundImageNormal( BOX_BRUSH( "Graph/CommonWidgets/TextBox", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageHovered( BOX_BRUSH( "Graph/CommonWidgets/TextBox_Hovered", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageFocused( BOX_BRUSH( "Graph/CommonWidgets/TextBox_Hovered", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageReadOnly( BOX_BRUSH( "Graph/CommonWidgets/TextBox", FMargin(4.0f/16.0f) ) )
					.SetScrollBarStyle( ScrollBar )
					);
			}

			// VectorEditableTextBox
			{
				Set( "Graph.VectorEditableTextBox", FEditableTextBoxStyle()
					.SetBackgroundImageNormal( BOX_BRUSH( "Graph/CommonWidgets/TextBox", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageHovered( BOX_BRUSH( "Graph/CommonWidgets/TextBox_Hovered", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageFocused( BOX_BRUSH( "Graph/CommonWidgets/TextBox_Hovered", FMargin(4.0f/16.0f) ) )
					.SetBackgroundImageReadOnly( BOX_BRUSH( "Graph/CommonWidgets/TextBox", FMargin(4.0f/16.0f) ) )
					.SetScrollBarStyle( ScrollBar )
					.SetForegroundColor( FLinearColor::White )
					.SetBackgroundColor( FLinearColor::Blue )
					);
			}

			// Check Box
			{
				/* Set images for various SCheckBox states of style Graph.Checkbox ... */
				const FCheckBoxStyle BasicGraphCheckBoxStyle = FCheckBoxStyle()
					.SetUncheckedImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox", Icon20x20 ) )
					.SetUncheckedHoveredImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Hovered", Icon20x20 ) )
					.SetUncheckedPressedImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Hovered", Icon20x20 ) )
					.SetCheckedImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Checked", Icon20x20 ) )
					.SetCheckedHoveredImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Checked_Hovered", Icon20x20 ) )
					.SetCheckedPressedImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Checked", Icon20x20, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
					.SetUndeterminedImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Undetermined", Icon20x20 ) )
					.SetUndeterminedHoveredImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Undetermined_Hovered", Icon20x20 ) )
					.SetUndeterminedPressedImage( IMAGE_BRUSH( "/Graph/CommonWidgets/CheckBox_Undetermined_Hovered", Icon20x20, FLinearColor( 0.5f, 0.5f, 0.5f ) ) );

				/* ... and add the new style */
				Set( "Graph.Checkbox", BasicGraphCheckBoxStyle );
			}
		}

		// Timeline Editor
		{
			Set( "TimelineEditor.AddFloatTrack", new IMAGE_BRUSH( "Icons/icon_TrackAddFloat_36x24px", Icon36x24 ) );
			Set( "TimelineEditor.AddVectorTrack", new IMAGE_BRUSH( "Icons/icon_TrackAddVector_36x24px", Icon36x24 ) );
			Set( "TimelineEditor.AddEventTrack", new IMAGE_BRUSH( "Icons/icon_TrackAddEvent_36x24px", Icon36x24 ) );
			Set( "TimelineEditor.AddColorTrack", new IMAGE_BRUSH( "Icons/icon_TrackAddColor_36x24px", Icon36x24 ) );
			Set( "TimelineEditor.AddCurveAssetTrack", new IMAGE_BRUSH( "Icons/icon_TrackAddCurve_36x24px", Icon36x24 ) );
			Set( "TimelineEditor.DeleteTrack", new IMAGE_BRUSH( "Icons/icon_TrackDelete_36x24px", Icon36x24 ) );

			Set("TimelineEditor.TrackRowSubtleHighlight", FTableRowStyle(NormalTableRowStyle)
				.SetActiveBrush(FSlateColorBrush(FStyleColors::Panel))
				.SetActiveHoveredBrush(FSlateColorBrush(FStyleColors::Header))
				.SetInactiveBrush(FSlateColorBrush(FStyleColors::Recessed))
				.SetInactiveHoveredBrush(FSlateColorBrush(FStyleColors::Panel))
				.SetActiveHighlightedBrush(FSlateColorBrush(FStyleColors::Panel)) // This is the parent hightlight
				.SetInactiveHighlightedBrush(FSlateColorBrush(FStyleColors::Recessed))// This is the parent highlight
			);
		}
	}

		// SCSEditor
	{
		Set("SCSEditor.TileViewTooltip.NonContentBorder", new BOX_BRUSH("/Docking/TabContentArea", FMargin(4 / 16.0f)));

		Set("SCSEditor.PromoteToBlueprintIcon", new IMAGE_BRUSH_SVG("Starship/Common/blueprint", Icon16x16));

		Set("SCSEditor.TopBar.Font", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		Set("SCSEditor.TreePanel", new FSlateNoResource());
		Set("SCSEditor.Background", new FSlateRoundedBoxBrush(FStyleColors::Recessed, 4.0f));

		//

		Set("SCSEditor.ComponentTooltip.Title",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 12))
			.SetColorAndOpacity(FLinearColor::Black)
			);

		Set("SCSEditor.ComponentTooltip.Label",
			FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FLinearColor(0.075f, 0.075f, 0.075f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		);
		Set("SCSEditor.ComponentTooltip.ImportantLabel",
			FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		);


		Set("SCSEditor.ComponentTooltip.Value", 
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		);
		Set("SCSEditor.ComponentTooltip.ImportantValue",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(FLinearColor(0.3f, 0.0f, 0.0f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		);

		Set("SCSEditor.ComponentTooltip.ClassDescription",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Italic", 10))
			.SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		);
	}

	// Notify editor
	{
		Set( "Persona.NotifyEditor.NotifyTrackBackground", new BOX_BRUSH( "/Persona/NotifyEditor/NotifyTrackBackground", FMargin(8.0f/64.0f, 3.0f/32.0f) ) );
	}

	// Blueprint modes
	{
		Set( "ModeSelector.ToggleButton.Normal", new FSlateNoResource() );		// Note: Intentionally transparent background
		Set( "ModeSelector.ToggleButton.Pressed", new BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor_Pressed ) );
		Set( "ModeSelector.ToggleButton.Hovered", new BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor ) );


		Set( "BlueprintEditor.PipelineSeparator", new BOX_BRUSH( "Old/Kismet2/BlueprintModeSeparator", FMargin(15.0f/16.0f, 20.0f/20.0f, 1.0f/16.0f, 0.0f/20.0f), FLinearColor(1,1,1,0.5f) ) );
	}

	// Persona modes
	{
		Set( "Persona.PipelineSeparator", new BOX_BRUSH( "Persona/Modes/PipelineSeparator", FMargin(15.0f/16.0f, 22.0f/24.0f, 1.0f/16.0f, 1.0f/24.0f), FLinearColor(1,1,1,0.5f) ) );
	}

	// montage editor
	{
		Set("Persona.MontageEditor.ChildMontageInstruction", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("BoldCondensed", 14))
			.SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.0f))
			.SetShadowOffset(FVector2D::ZeroVector)
		);
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	}

void FStarshipEditorStyle::FStyle::SetupLevelEditorStyle()
	{
	// Level editor tool bar icons
#if WITH_EDITOR
	{
		Set("LevelEditor.BrowseDocumentation",     new IMAGE_BRUSH_SVG("Starship/Common/Documentation", Icon16x16));
		Set("LevelEditor.BrowseAPIReference",      new IMAGE_BRUSH_SVG("Starship/Common/Documentation", Icon16x16));
		Set("LevelEditor.BrowseCVars",             new IMAGE_BRUSH_SVG("Starship/Common/Console", Icon16x16));
		Set("LevelEditor.Tutorials",               new IMAGE_BRUSH_SVG("Starship/Common/Tutorials", Icon16x16));
		Set("LevelEditor.BrowseViewportControls",  new IMAGE_BRUSH_SVG("Starship/Common/ViewportControls", Icon16x16));

		Set("MainFrame.ToggleFullscreen",          new IMAGE_BRUSH_SVG("Starship/Common/EnableFullscreen", Icon16x16));
		Set("MainFrame.LoadLayout",                new IMAGE_BRUSH_SVG("Starship/Common/LayoutLoad", Icon16x16));
		Set("MainFrame.SaveLayout",                new IMAGE_BRUSH_SVG("Starship/Common/LayoutSave", Icon16x16));
		Set("MainFrame.RemoveLayout",              new IMAGE_BRUSH_SVG("Starship/Common/LayoutRemove", Icon16x16));

		Set("MainFrame.OpenIssueTracker",          new IMAGE_BRUSH_SVG("Starship/Common/IssueTracker", Icon16x16));
		Set("MainFrame.ReportABug",                new IMAGE_BRUSH_SVG("Starship/Common/Bug", Icon16x16));

		Set("MainFrame.VisitAskAQuestionPage",     new IMAGE_BRUSH_SVG("Starship/Menus/Bug", Icon16x16));
		Set("MainFrame.VisitOnlineLearning",	   new IMAGE_BRUSH_SVG("Starship/Common/Documentation", Icon16x16));
		Set("MainFrame.VisitWiki",                 new IMAGE_BRUSH_SVG("Starship/Common/WebBrowser", Icon16x16));
		Set("MainFrame.VisitForums",               new IMAGE_BRUSH_SVG("Starship/Common/WebBrowser", Icon16x16));
		Set("MainFrame.VisitSearchForAnswersPage", new IMAGE_BRUSH_SVG("Starship/Common/WebBrowser", Icon16x16));
		Set("MainFrame.VisitSupportWebSite",       new IMAGE_BRUSH_SVG("Starship/Common/Support", Icon16x16));
		Set("MainFrame.VisitEpicGamesDotCom",      new IMAGE_BRUSH("Icons/Help/icon_Help_epic_16x", Icon16x16));
		Set("MainFrame.AboutUnrealEd",             new IMAGE_BRUSH("Icons/Help/icon_Help_unreal_16x", Icon16x16));
		Set("MainFrame.CreditsUnrealEd",           new IMAGE_BRUSH_SVG("Starship/Common/Credits", Icon16x16));

		Set( "EditorViewport.SelectMode", new IMAGE_BRUSH_SVG("Starship/EditorViewport/select", Icon16x16) );
		Set( "EditorViewport.TranslateMode", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/translate", Icon16x16 ) );
		Set( "EditorViewport.RotateMode", new IMAGE_BRUSH_SVG("Starship/EditorViewport/rotate", Icon16x16 ) );
		Set( "EditorViewport.ScaleMode", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/scale", Icon16x16 ) );

		Set( "EditorViewport.TranslateRotateMode", new IMAGE_BRUSH( "Icons/icon_translate_rotate_40x", Icon20x20 ) );
		Set( "EditorViewport.TranslateRotate2DMode", new IMAGE_BRUSH("Icons/icon_translate_rotate_2d_40x", Icon20x20));

		Set( "EditorViewport.ToggleRealTime", new IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon16x16));

		Set( "EditorViewport.LocationGridSnap", new IMAGE_BRUSH_SVG("Starship/EditorViewport/grid", Icon16x16));
		Set( "EditorViewport.RotationGridSnap", new IMAGE_BRUSH_SVG("Starship/EditorViewport/angle", Icon16x16));

		Set( "EditorViewport.Layer2DSnap", new IMAGE_BRUSH("Old/LevelEditor/Layer2DSnap", Icon14x14));

		Set("EditorViewport.ScaleGridSnap", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/scale-grid-snap", Icon16x16 ) );
		Set("EditorViewport.ToggleSurfaceSnapping", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/surface-snap", Icon16x16 ) );
		Set("EditorViewport.ToggleSurfaceSnapping", new IMAGE_BRUSH_SVG("Starship/EditorViewport/surface-snap", Icon16x16));

		Set( "EditorViewport.RelativeCoordinateSystem_Local", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/transform-local", Icon16x16 ) );
		Set( "EditorViewport.RelativeCoordinateSystem_World", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/globe", Icon16x16 ) );

		Set( "EditorViewport.CamSpeedSetting", new IMAGE_BRUSH_SVG( "Starship/EditorViewport/camera", Icon16x16) );
		
		Set( "EditorViewport.LitMode",            	  new IMAGE_BRUSH_SVG("Starship/Common/LitCube", Icon16x16 ) );
		Set( "EditorViewport.UnlitMode",          	  new IMAGE_BRUSH_SVG("Starship/Common/UnlitCube", Icon16x16 ) );
		Set( "EditorViewport.WireframeMode",      	  new IMAGE_BRUSH_SVG("Starship/Common/BrushWireframe", Icon16x16 ) );
		Set( "EditorViewport.DetailLightingMode", 	  new IMAGE_BRUSH_SVG("Starship/Common/DetailLighting", Icon16x16 ) );
		Set( "EditorViewport.LightingOnlyMode",   	  new IMAGE_BRUSH_SVG("Starship/Common/LightBulb", Icon16x16 ) );
		
		Set( "EditorViewport.PathTracingMode",   	  new IMAGE_BRUSH_SVG("Starship/Common/PathTracing", Icon16x16 ) );
		Set( "EditorViewport.RayTracingDebugMode",    new IMAGE_BRUSH_SVG("Starship/Common/RayTracingDebug", Icon16x16 ) );

		Set( "EditorViewport.LightComplexityMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_LightComplexity_16px", Icon16x16 ) );
		Set( "EditorViewport.ShaderComplexityMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_Shadercomplexity_16px", Icon16x16 ) );

		Set( "EditorViewport.QuadOverdrawMode", new IMAGE_BRUSH_SVG("Starship/Common/OptimizationViewmodes", Icon16x16 ) );

		Set( "EditorViewport.ShaderComplexityWithQuadOverdrawMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_Shadercomplexity_16px", Icon16x16 ) );
		Set( "EditorViewport.TexStreamAccPrimitiveDistanceMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_TextureStreamingAccuracy_16px", Icon16x16 ) );
		Set( "EditorViewport.TexStreamAccMeshUVDensityMode", new IMAGE_BRUSH("Icons/icon_ViewMode_TextureStreamingAccuracy_16px", Icon16x16));
		Set( "EditorViewport.TexStreamAccMaterialTextureScaleMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_TextureStreamingAccuracy_16px", Icon16x16 ) );
		Set( "EditorViewport.RequiredTextureResolutionMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_TextureStreamingAccuracy_16px", Icon16x16 ) );
		Set( "EditorViewport.StationaryLightOverlapMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_StationaryLightOverlap_16px", Icon16x16 ) );
		Set( "EditorViewport.LightmapDensityMode", new IMAGE_BRUSH( "Icons/icon_ViewMode_LightmapDensity_16px", Icon16x16 ) );

		Set( "EditorViewport.LODColorationMode", new IMAGE_BRUSH("Icons/icon_ViewMode_LODColoration_16px", Icon16x16) );
		Set( "EditorViewport.HLODColorationMode", new IMAGE_BRUSH("Icons/icon_ViewMode_LODColoration_16px", Icon16x16));	
		Set( "EditorViewport.GroupLODColorationMode", new IMAGE_BRUSH_SVG("Starship/Common/LODColorization", Icon16x16) );

		Set( "EditorViewport.VisualizeGBufferMode",   new IMAGE_BRUSH_SVG("Starship/Common/BufferVisualization", Icon16x16) );

		Set( "EditorViewport.Visualizers", 			  new IMAGE_BRUSH_SVG("Starship/Common/Visualizer", Icon16x16) );
		Set( "EditorViewport.LOD", 			  		  new IMAGE_BRUSH_SVG("Starship/Common/LOD", Icon16x16) );

		Set( "EditorViewport.ReflectionOverrideMode", new IMAGE_BRUSH_SVG("Starship/Common/Reflections", Icon16x16 ) );
		Set( "EditorViewport.VisualizeBufferMode",    new IMAGE_BRUSH_SVG("Starship/Common/BufferVisualization", Icon16x16 ) );
		Set( "EditorViewport.VisualizeNaniteMode",    new IMAGE_BRUSH_SVG("Starship/Common/BufferVisualization", Icon16x16 ) );
		Set( "EditorViewport.CollisionPawn",          new IMAGE_BRUSH_SVG("Starship/Common/PlayerCollision", Icon16x16 ) );
		Set( "EditorViewport.CollisionVisibility",    new IMAGE_BRUSH_SVG("Starship/Common/VisibilityCollision", Icon16x16 ) );
		
		Set( "EditorViewport.Perspective", new IMAGE_BRUSH_SVG("Starship/Common/ViewPerspective", Icon16x16 ) );
		Set( "EditorViewport.Top",         new IMAGE_BRUSH_SVG("Starship/Common/ViewTop", Icon16x16 ) );
		Set( "EditorViewport.Left",        new IMAGE_BRUSH_SVG("Starship/Common/ViewLeft", Icon16x16 ) );
		Set( "EditorViewport.Front",       new IMAGE_BRUSH_SVG("Starship/Common/ViewFront", Icon16x16 ) );
		Set( "EditorViewport.Bottom",      new IMAGE_BRUSH_SVG("Starship/Common/ViewBottom", Icon16x16 ) );
		Set( "EditorViewport.Right",       new IMAGE_BRUSH_SVG("Starship/Common/ViewRight", Icon16x16 ) );
		Set( "EditorViewport.Back",        new IMAGE_BRUSH_SVG("Starship/Common/ViewBack", Icon16x16 ) );

		Set("EditorViewport.ToggleStats", new IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon16x16));
		Set("EditorViewport.ToggleFPS", new IMAGE_BRUSH_SVG("Starship/Common/FPS", Icon16x16));
		Set("EditorViewport.ToggleViewportToolbar", new IMAGE_BRUSH_SVG("Starship/Common/Toolbar", Icon16x16));

		Set("EditorViewport.SubMenu.Stats", new IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon16x16));
		Set("EditorViewport.SubMenu.Bookmarks", new IMAGE_BRUSH_SVG("Starship/Common/Bookmarks", Icon16x16));
		Set("EditorViewport.SubMenu.CreateCamera", new IMAGE_BRUSH_SVG("Starship/Common/CreateCamera", Icon16x16));
		Set("EditorViewport.SubMenu.Layouts", new IMAGE_BRUSH_SVG("Starship/Common/Layout", Icon16x16));

		Set("LevelViewport.ToggleGameView", new IMAGE_BRUSH_SVG("Starship/Common/GameView", Icon16x16));
		Set("LevelViewport.ToggleImmersive", new IMAGE_BRUSH_SVG("Starship/Common/EnableFullscreen", Icon16x16));
		Set("LevelViewport.HighResScreenshot", new IMAGE_BRUSH_SVG("Starship/Common/HighResolutionScreenshot", Icon16x16));
		Set("LevelViewport.ToggleCinematicPreview", new IMAGE_BRUSH_SVG("Starship/Common/Cinematics", Icon16x16));
		Set("LevelViewport.ToggleCameraShakePreview", new IMAGE_BRUSH_SVG("Starship/Common/CameraShake", Icon16x16));
		Set("LevelViewport.AdvancedSettings", new CORE_IMAGE_BRUSH_SVG("Starship/Common/settings", Icon16x16));

		Set("EditorViewport.ActiveBorderColor", FStyleColors::Primary);

#endif

#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
		{
			Set( "LevelEditor.Tabs.Details",                new IMAGE_BRUSH_SVG("Starship/Common/Details", Icon16x16) );
			Set( "LevelEditor.Tabs.Cinematics",             new IMAGE_BRUSH_SVG("Starship/Common/Cinematics", Icon16x16) );
			Set( "LevelEditor.Tabs.VirtualProduction",      new IMAGE_BRUSH_SVG("Starship/Common/VirtualProduction", Icon16x16) );
			Set( "LevelEditor.Tabs.EditorModes",            new IMAGE_BRUSH_SVG("Starship/Common/EditorModes", Icon16x16) );
			Set( "LevelEditor.Tabs.Modes",                  new IMAGE_BRUSH_SVG("Starship/Common/EditorModes", Icon16x16) );
			Set( "LevelEditor.Tabs.PlacementBrowser",       new IMAGE_BRUSH_SVG("Starship/Common/PlaceActors", Icon16x16) );
			Set( "LevelEditor.Tabs.Properties",             new IMAGE_BRUSH_SVG("Starship/Common/Properties", Icon16x16) );
			Set( "LevelEditor.Tabs.Outliner",               new IMAGE_BRUSH_SVG("Starship/Common/WorldOutliner", Icon16x16) );
			Set( "LevelEditor.Tabs.ContentBrowser",         new IMAGE_BRUSH_SVG("Starship/Common/ContentBrowser", Icon16x16) );
			Set( "LevelEditor.Tabs.Levels",                 new IMAGE_BRUSH_SVG("Starship/Common/Levels", Icon16x16) );
			Set( "LevelEditor.Tabs.WorldBrowser",           new IMAGE_BRUSH_SVG("Starship/Common/Levels", Icon16x16) );
			Set( "LevelEditor.Tabs.WorldBrowserDetails",    new IMAGE_BRUSH_SVG("Starship/Common/WorldSettings", Icon16x16) );

			Set( "LevelEditor.Tabs.WorldBrowserComposition",new IMAGE_BRUSH("Icons/icon_levels_compositionbutton_16x", Icon16x16 ) );
			Set( "LevelEditor.Tabs.WorldPartition", new IMAGE_BRUSH( "/Icons/icon_levels_partitionbutton_16x", Icon16x16 ) );

			Set( "LevelEditor.Tabs.Layers",                 new IMAGE_BRUSH_SVG("Starship/Common/Layers", Icon16x16) );
			Set( "LevelEditor.Tabs.DataLayers",				new IMAGE_BRUSH_SVG("Starship/Common/DataLayers", Icon16x16));
			Set( "LevelEditor.Tabs.ComposureCompositing",   new IMAGE_BRUSH_SVG("Starship/Common/ComposureCompositing", Icon16x16) );
			Set( "LevelEditor.Tabs.USDStage",   			new IMAGE_BRUSH_SVG("Starship/Common/USDStage", Icon16x16) );

			Set( "LevelEditor.Tabs.StatsViewer",            new IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon16x16) );

			Set( "LevelEditor.Tabs.Toolbar",                new IMAGE_BRUSH("Icons/icon_tab_Toolbars_16x", Icon16x16 ) );

			Set( "LevelEditor.Tabs.Viewports",              new IMAGE_BRUSH_SVG("Starship/Common/Viewports", Icon16x16) );
			Set( "LevelEditor.Tabs.HLOD",                   new IMAGE_BRUSH_SVG("Starship/Common/HierarchicalLODOutliner", Icon16x16) );
			Set( "LevelEditor.Tabs.Debug",                  new IMAGE_BRUSH_SVG("Starship/Common/Bug", Icon16x16) );

			Set( "LevelEditor.Audit",                  		new IMAGE_BRUSH_SVG("Starship/Common/AssetAudit", Icon16x16) );
			Set( "LevelEditor.Profile",                  	new IMAGE_BRUSH_SVG("Starship/Common/Profile", Icon16x16) );
			Set( "LevelEditor.Platforms",                  	new IMAGE_BRUSH_SVG("Starship/Common/DeviceManager", Icon16x16) );
		}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

#if WITH_EDITOR
		Set( "LevelEditor.NewLevel",      new IMAGE_BRUSH_SVG( "Starship/Common/LevelNew", Icon16x16 ) );
		Set( "LevelEditor.OpenLevel",     new IMAGE_BRUSH_SVG( "Starship/Common/LevelOpen", Icon16x16 ) );
		Set( "LevelEditor.Save",          new IMAGE_BRUSH_SVG( "Starship/Common/SaveCurrent", Icon16x16 ) );
		Set( "LevelEditor.SaveAs",        new IMAGE_BRUSH_SVG( "Starship/Common/SaveCurrentAs", Icon16x16 ) );
		Set( "LevelEditor.SaveAllLevels", new IMAGE_BRUSH_SVG( "Starship/Common/LevelSaveAll", Icon16x16 ) );

		Set( "LevelEditor.ImportScene",    new IMAGE_BRUSH_SVG( "Starship/Common/LevelImportInto", Icon16x16 ) );
		Set( "LevelEditor.ExportAll",      new IMAGE_BRUSH_SVG( "Starship/Common/ExportAll", Icon16x16 ) );
		Set( "LevelEditor.ExportSelected", new IMAGE_BRUSH_SVG( "Starship/Common/ExportSelected", Icon16x16 ) );

		Set( "LevelEditor.Recompile", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/compile", Icon40x40 ) );
		Set( "LevelEditor.Recompile.Small", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/compile", Icon20x20 ) );

		Set("LevelEditor.PreviewMode.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_SM5_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_SM5_Disabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.SM5.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_SM5_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.SM5.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_SM5_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.AndroidES31.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_AndroidES31_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.AndroidES31.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_AndroidES31_Disabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.AndroidVulkan.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_AndroidVulkan_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.AndroidVulkan.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_AndroidVulkan_Disabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.AndroidVulkanSM5.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_AndroidVulkanSM5_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.AndroidVulkanSM5.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_AndroidVulkanSM5_Disabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.iOS.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_iOS_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.iOS.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_iOS_Disabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.iOSSM5.Enabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_iOSSM5_Enabled_40x", Icon40x40));
		Set("LevelEditor.PreviewMode.iOSSM5.Disabled", new IMAGE_BRUSH("Icons/icon_PreviewMode_iOSSM5_Disabled_40x", Icon40x40));

		Set("LevelEditor.ViewOptions", new IMAGE_BRUSH("Icons/icon_view_40x", Icon40x40));
		Set( "LevelEditor.ViewOptions.Small", new IMAGE_BRUSH( "Icons/icon_view_40x", Icon20x20 ) );

		Set( "LevelEditor.GameSettings", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/settings", Icon20x20) );

		Set( "LevelEditor.Create", new IMAGE_BRUSH( "Icons/icon_Mode_Placement_40px", Icon40x40 ) );
		Set( "LevelEditor.Create.Small", new IMAGE_BRUSH( "Icons/icon_Mode_Placement_40px", Icon20x20 ) );
		Set( "LevelEditor.Create.OutlineHoriz", new IMAGE_BRUSH( "Common/WorkingFrame_Marquee", FVector2D(34.0f, 3.0f), FLinearColor::White, ESlateBrushTileType::Horizontal) );
		Set( "LevelEditor.Create.OutlineVert", new IMAGE_BRUSH( "Common/WorkingFrame_Marquee_Vert", FVector2D(3.0f, 34.0f), FLinearColor::White, ESlateBrushTileType::Vertical) );

		Set( "ToolPalette.DockingTab", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetPadding( FMargin(16.0f, 2.0f, 16.0f, 2.0f ) )
			.SetCheckedImage(         CORE_BOX_BRUSH("Docking/Tab_Shape",  2.f/8.0f, FLinearColor(FColor(62, 62, 62)) ) )
			.SetCheckedHoveredImage(  CORE_BOX_BRUSH("Docking/Tab_Shape",  2.f/8.0f, FLinearColor(FColor(62, 62, 62)) ) )
			.SetCheckedPressedImage(  CORE_BOX_BRUSH("Docking/Tab_Shape",  2.f/8.0f, FLinearColor(FColor(62, 62, 62)) ) )
			.SetUncheckedImage(       CORE_BOX_BRUSH("Docking/Tab_Shape",  2.f/8.0f, FLinearColor(FColor(45, 45, 45)) ) )
			.SetUncheckedHoveredImage(CORE_BOX_BRUSH("Docking/Tab_Shape",2.f/8.0f, FLinearColor(FColor(54, 54, 54)) ) )
			.SetUncheckedPressedImage(CORE_BOX_BRUSH("Docking/Tab_Shape",2.f/8.0f, FLinearColor(FColor(54, 54, 54)) ) )
			.SetUndeterminedImage(        FSlateNoResource() )
			.SetUndeterminedHoveredImage( FSlateNoResource() )
			.SetUndeterminedPressedImage( FSlateNoResource() )
		);	
		Set( "ToolPalette.DockingWell", new FSlateColorBrush(FLinearColor(FColor(34, 34, 34, 255))));

		Set( "ToolPalette.DockingLabel", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 9 ) ) 
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f))
		);


		Set("LevelEditor.SelectMode", new IMAGE_BRUSH_SVG("Starship/MainToolbar/select", Icon20x20));

		Set( "LevelEditor.MeshPaintMode", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/paint", Icon20x20 ) );
		
		Set("LevelEditor.MeshPaintMode.TexturePaint", new IMAGE_BRUSH("Icons/TexturePaint_40x", Icon40x40));
		Set("LevelEditor.MeshPaintMode.TexturePaint.Small", new IMAGE_BRUSH("Icons/TexturePaint_40x", Icon20x20));
		Set("LevelEditor.MeshPaintMode.ColorPaint", new IMAGE_BRUSH("Icons/VertexColorPaint_40x", Icon40x40));
		Set("LevelEditor.MeshPaintMode.ColorPaint.Small", new IMAGE_BRUSH("Icons/VertexColorPaint_40x", Icon20x20));
		Set("LevelEditor.MeshPaintMode.WeightPaint", new IMAGE_BRUSH("Icons/WeightPaint_40x", Icon40x40));
		Set("LevelEditor.MeshPaintMode.WeightPaint.Small", new IMAGE_BRUSH("Icons/WeightPaint_40x", Icon20x20));

		Set( "LevelEditor.LandscapeMode", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/landscape", Icon20x20 ) );
		Set( "LevelEditor.LandscapeMode.Selected", new IMAGE_BRUSH( "Icons/icon_Mode_Landscape_selected_40x", Icon40x40 ) );
		Set( "LevelEditor.LandscapeMode.Selected.Small", new IMAGE_BRUSH( "Icons/icon_Mode_Landscape_selected_40x", Icon20x20 ) );

		Set( "LevelEditor.FoliageMode", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/foliage", Icon20x20 ) );
		Set( "LevelEditor.FoliageMode.Selected", new IMAGE_BRUSH( "Icons/icon_Mode_Foliage_selected_40x", Icon40x40 ) );
		Set( "LevelEditor.FoliageMode.Selected.Small", new IMAGE_BRUSH( "Icons/icon_Mode_Foliage_selected_40x", Icon20x20 ) );

		Set( "LevelEditor.WorldProperties", new IMAGE_BRUSH( "Icons/icon_worldscript_40x", Icon40x40 ) );
		Set( "LevelEditor.WorldProperties.Small", new IMAGE_BRUSH( "Icons/icon_worldscript_40x", Icon20x20 ) );

		Set( "LevelEditor.WorldProperties.Tab", new IMAGE_BRUSH_SVG( "Starship/Common/WorldSettings", Icon16x16 ) );

		Set("LevelEditor.BrushEdit", new IMAGE_BRUSH_SVG("Starship/MainToolbar/brush_edit", Icon20x20));

		Set( "LevelEditor.OpenPlaceActors", new IMAGE_BRUSH_SVG( "Starship/Common/PlaceActors", Icon20x20 ) );
		Set( "LevelEditor.OpenContentBrowser", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/content", Icon20x20 ) );
		Set( "LevelEditor.OpenMarketplace", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/marketplace", Icon20x20) );
		Set( "LevelEditor.OpenLevelBlueprint", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/blueprints", Icon20x20) );

		Set( "LevelEditor.CreateClassBlueprint", new IMAGE_BRUSH("Icons/icon_class_Blueprint_New_16x", Icon16x16));
		Set( "LevelEditor.OpenClassBlueprint", new IMAGE_BRUSH("Icons/icon_class_Blueprint_Open_16x", Icon16x16));
		Set( "LevelEditor.EditMatinee", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/cinematics", Icon20x20 ) );

		Set( "LevelEditor.ToggleVR", new IMAGE_BRUSH( "Icons/VREditor/VR_Editor_Toolbar_Icon", Icon40x40 ) );
		Set( "LevelEditor.ToggleVR.Small", new IMAGE_BRUSH( "Icons/VREditor/VR_Editor_Toolbar_Icon_Small", Icon20x20 ) );

		Set( "MergeActors.MeshMergingTool", new IMAGE_BRUSH_SVG( "Starship/MergeActors/MeshMerging_16", Icon16x16 ) );
		Set( "MergeActors.MeshProxyTool", new IMAGE_BRUSH_SVG( "Starship/MergeActors/MeshProxy_16", Icon16x16 ) );
		Set( "MergeActors.MeshInstancingTool", new IMAGE_BRUSH_SVG("Starship/AssetIcons/StaticMeshActor_16", Icon16x16 ) );
		Set( "MergeActors.TabIcon", new IMAGE_BRUSH_SVG("Starship/Common/Merge", Icon16x16));
		
		Set( "PlacementBrowser.OptionsMenu", new IMAGE_BRUSH( "Icons/icon_Blueprint_Macro_16x", Icon16x16 ) );

		Set( "PlacementBrowser.AssetToolTip.AssetName", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Bold", 9 ) ) );
		Set( "PlacementBrowser.AssetToolTip.AssetClassName", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 9 ) ) );
		Set( "PlacementBrowser.AssetToolTip.AssetPath", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 8 ) ) );

		Set( "PlacementBrowser.Asset", FButtonStyle( Button )
			.SetNormal( FSlateRoundedBoxBrush(FLinearColor::Transparent, 6.0f, FStyleColors::Dropdown, 1.0f) )
			.SetHovered( FSlateRoundedBoxBrush(FLinearColor::Transparent, 6.0f, FStyleColors::Hover, 1.0f) )
			.SetPressed( FSlateRoundedBoxBrush(FLinearColor::Transparent, 6.0f, FStyleColors::Primary, 1.0f) )
			.SetNormalPadding( 0 )
			.SetPressedPadding( 0 )
			);

		Set( "PlacementBrowser.Asset.Background", new FSlateRoundedBoxBrush(FStyleColors::Recessed, 6.f));
		Set( "PlacementBrowser.Asset.LabelBack", new BOX_BRUSH("Starship/PlacementBrowser/LabelBack_18x", 6.f/18.f, FStyleColors::Dropdown));

		FLinearColor DimBackground = FLinearColor( FColor( 64, 64, 64 ) );
		FLinearColor DimBackgroundHover = FLinearColor( FColor( 50, 50, 50 ) );
		FLinearColor DarkBackground = FLinearColor( FColor( 42, 42, 42 ) );

		Set( "PlacementBrowser.Tab", FCheckBoxStyle()
			.SetCheckBoxType( ESlateCheckBoxType::ToggleButton )
			.SetUncheckedImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, DimBackground ) )
			.SetUncheckedPressedImage( BOX_BRUSH( "PlacementMode/TabActive", 8.0f / 32.0f ) )
			.SetUncheckedHoveredImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, DimBackgroundHover ) )
			.SetCheckedImage( BOX_BRUSH( "PlacementMode/TabActive", 8.0f / 32.0f ) )
			.SetCheckedHoveredImage( BOX_BRUSH( "PlacementMode/TabActive", 8.0f / 32.0f ) )
			.SetCheckedPressedImage( BOX_BRUSH( "PlacementMode/TabActive", 8.0f / 32.0f ) )
			.SetPadding( 0 ) );

		Set( "PlacementBrowser.Tab.Text", FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Bold", 10 ) )
			.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f, 0.9f ) )
			.SetShadowOffset( FVector2D( 1, 1 ) )
			.SetShadowColorAndOpacity( FLinearColor( 0, 0, 0, 0.9f ) ) );

		Set( "PlacementBrowser.Asset.Name", FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Regular", 10 ) )
			.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f, 0.9f ) )
			.SetShadowOffset( FVector2D( 1, 1 ) )
			.SetShadowColorAndOpacity( FLinearColor( 0, 0, 0, 0.9f ) ) );

		Set( "PlacementBrowser.Asset.Type", FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Regular", 8 ) )
			.SetColorAndOpacity( FLinearColor( 0.8f, 0.8f, 0.8f, 0.9f ) )
			.SetShadowOffset( FVector2D( 1, 1 ) )
			.SetShadowColorAndOpacity( FLinearColor( 0, 0, 0, 0.9f ) ) );

		Set( "PlacementBrowser.ActiveTabNub", new IMAGE_BRUSH( "Icons/TabTriangle_24x", Icon24x24, FLinearColor( FColor( 42, 42, 42 ) ) ) );
		Set( "PlacementBrowser.ActiveTabBar", new IMAGE_BRUSH( "Common/Selection", FVector2D(2.0f, 2.0f), SelectionColor ) );

		Set( "PlacementBrowser.ShowAllContent", new IMAGE_BRUSH( "Icons/icon_Placement_AllContent_20px", Icon20x20 ) );
		Set( "PlacementBrowser.ShowAllContent.Small", new IMAGE_BRUSH( "Icons/icon_Placement_AllContent_20px", Icon20x20 ) );
		Set( "PlacementBrowser.ShowCollections", new IMAGE_BRUSH( "Icons/icon_Placement_Collections_20px", Icon20x20 ) );
		Set( "PlacementBrowser.ShowCollections.Small", new IMAGE_BRUSH( "Icons/icon_Placement_Collections_20px", Icon20x20 ) );


		const FTableRowStyle PlaceItemTableRowStyle = FTableRowStyle()
			.SetEvenRowBackgroundBrush(FSlateNoResource())
			.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())

			.SetOddRowBackgroundBrush(FSlateNoResource())
			.SetOddRowBackgroundHoveredBrush(FSlateNoResource())

			.SetSelectorFocusedBrush(BORDER_BRUSH("Common/Selector", FMargin(4.f / 16.f), SelectorColor))

			.SetActiveBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())

			.SetInactiveBrush(FSlateNoResource())
			.SetInactiveHoveredBrush(FSlateNoResource())

			.SetActiveHighlightedBrush(FSlateNoResource())
			.SetInactiveHighlightedBrush(FSlateNoResource())

			.SetTextColor(FStyleColors::Foreground)
			.SetSelectedTextColor(FStyleColors::Foreground)

			.SetDropIndicator_Above(BOX_BRUSH("Common/DropZoneIndicator_Above", FMargin(10.0f / 16.0f, 10.0f / 16.0f, 0, 0), SelectionColor))
			.SetDropIndicator_Onto(BOX_BRUSH("Common/DropZoneIndicator_Onto", FMargin(4.0f / 16.0f), SelectionColor))
			.SetDropIndicator_Below(BOX_BRUSH("Common/DropZoneIndicator_Below", FMargin(10.0f / 16.0f, 0, 0, 10.0f / 16.0f), SelectionColor));

		Set("PlacementBrowser.PlaceableItemRow", PlaceItemTableRowStyle);


		const FCheckBoxStyle PlacementSegmentedBox = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(FSlateNoResource())
			.SetUncheckedHoveredImage(FSlateNoResource())
			.SetUncheckedPressedImage(FSlateNoResource())
			.SetCheckedImage(FSlateNoResource())
			.SetCheckedHoveredImage(FSlateNoResource())
			.SetCheckedPressedImage(FSlateNoResource())
			.SetForegroundColor(FStyleColors::Foreground)
			.SetHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetPressedForegroundColor(FStyleColors::ForegroundHover)
			.SetCheckedForegroundColor(FStyleColors::Primary)
			.SetCheckedHoveredForegroundColor(FStyleColors::Primary)
			.SetCheckedPressedForegroundColor(FStyleColors::Primary)
			.SetPadding(FMargin(6.f, 2.f));

		Set("PlacementBrowser.CategoryControl", FSegmentedControlStyle()
			.SetControlStyle(PlacementSegmentedBox)
			.SetFirstControlStyle(PlacementSegmentedBox)
			.SetLastControlStyle(PlacementSegmentedBox)
		);

		Set("PlacementBrowser.Icons.Recent",        new IMAGE_BRUSH_SVG("Starship/Common/Recent",         Icon16x16));
		Set("PlacementBrowser.Icons.Basic",         new IMAGE_BRUSH_SVG("Starship/Common/Basic",          Icon16x16));
		Set("PlacementBrowser.Icons.Lights",        new IMAGE_BRUSH_SVG("Starship/Common/LightBulb",      Icon16x16));
		Set("PlacementBrowser.Icons.Cinematics",    new IMAGE_BRUSH_SVG("Starship/Common/Cinematics",     Icon16x16));
		Set("PlacementBrowser.Icons.VisualEffects", new IMAGE_BRUSH_SVG("Starship/Common/VisualEffects",  Icon16x16));
		Set("PlacementBrowser.Icons.BSP",           new IMAGE_BRUSH_SVG("Starship/Common/Geometry",       Icon16x16));
		Set("PlacementBrowser.Icons.Volumes",       new IMAGE_BRUSH_SVG("Starship/Common/Volumes",        Icon16x16));
		Set("PlacementBrowser.Icons.All",           new IMAGE_BRUSH_SVG("Starship/Common/AllClasses",     Icon16x16));
		Set("PlacementBrowser.Icons.Testing",       new IMAGE_BRUSH_SVG("Starship/Common/Test",           Icon16x16));

		Set( "ContentPalette.ShowAllPlaceables", new IMAGE_BRUSH( "Icons/icon_Placement_FilterAll_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowAllPlaceables.Small", new IMAGE_BRUSH( "Icons/icon_Placement_FilterAll_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowProps", new IMAGE_BRUSH( "Icons/icon_Placement_FilterProps_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowProps.Small", new IMAGE_BRUSH( "Icons/icon_Placement_FilterProps_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowParticles", new IMAGE_BRUSH( "Icons/icon_Placement_FilterParticles_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowParticles.Small", new IMAGE_BRUSH( "Icons/icon_Placement_FilterParticles_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowAudio", new IMAGE_BRUSH( "Icons/icon_Placement_FilterAudio_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowAudio.Small", new IMAGE_BRUSH( "Icons/icon_Placement_FilterAudio_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowMisc", new IMAGE_BRUSH( "Icons/icon_Placement_FilterMisc_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowMisc.Small", new IMAGE_BRUSH( "Icons/icon_Placement_FilterMisc_20px", Icon20x20 ) );
		Set( "ContentPalette.ShowRecentlyPlaced", new IMAGE_BRUSH( "Icons/icon_Placement_RecentlyPlaced_20x", Icon20x20 ) );
		Set( "ContentPalette.ShowRecentlyPlaced.Small", new IMAGE_BRUSH( "Icons/icon_Placement_RecentlyPlaced_20x", Icon20x20 ) );
	}

	{

		Set( "AssetDeleteDialog.Background", new IMAGE_BRUSH( "Common/Selection", Icon8x8, FLinearColor( 0.016, 0.016, 0.016 ) ) );
	}

	// Level editor tool box icons
	{
		Set( "LevelEditor.RecompileGameCode", new IMAGE_BRUSH( "Old/MainToolBar/RecompileGameCode", Icon40x40 ) );
	}

	// Editor viewport layout command icons
	{
		const FVector2D IconLayoutSize(47.0f, 37.0f);
		const FVector2D IconLayoutSizeSmall(47.0f, 37.0f);		// small version set to same size as these are in their own menu and don't clutter the UI

		Set("EditorViewport.ViewportConfig_OnePane", new IMAGE_BRUSH("Icons/ViewportLayout_OnePane", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_OnePane.Small", new IMAGE_BRUSH("Icons/ViewportLayout_OnePane", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_TwoPanesH", new IMAGE_BRUSH("Icons/ViewportLayout_TwoPanesHoriz", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_TwoPanesH.Small", new IMAGE_BRUSH("Icons/ViewportLayout_TwoPanesHoriz", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_TwoPanesV", new IMAGE_BRUSH("Icons/ViewportLayout_TwoPanesVert", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_TwoPanesV.Small", new IMAGE_BRUSH("Icons/ViewportLayout_TwoPanesVert", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_ThreePanesLeft", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesLeft", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_ThreePanesLeft.Small", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesLeft", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_ThreePanesRight", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesRight", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_ThreePanesRight.Small", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesRight", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_ThreePanesTop", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesTop", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_ThreePanesTop.Small", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesTop", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_ThreePanesBottom", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesBottom", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_ThreePanesBottom.Small", new IMAGE_BRUSH("Icons/ViewportLayout_ThreePanesBottom", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_FourPanesLeft", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesLeft", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_FourPanesLeft.Small", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesLeft", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_FourPanesRight", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesRight", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_FourPanesRight.Small", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesRight", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_FourPanesTop", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesTop", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_FourPanesTop.Small", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesTop", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_FourPanesBottom", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesBottom", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_FourPanesBottom.Small", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanesBottom", IconLayoutSizeSmall));
		Set("EditorViewport.ViewportConfig_FourPanes2x2", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanes2x2", IconLayoutSize));
		Set("EditorViewport.ViewportConfig_FourPanes2x2.Small", new IMAGE_BRUSH("Icons/ViewportLayout_FourPanes2x2", IconLayoutSizeSmall));
	}

	// Level viewport layout command icons
	{
		const FVector2D IconLayoutSize(47.0f, 37.0f);
		const FVector2D IconLayoutSizeSmall(47.0f, 37.0f);		// small version set to same size as these are in their own menu and don't clutter the UI

		Set( "LevelViewport.ViewportConfig_OnePane", new IMAGE_BRUSH("Icons/ViewportLayout_OnePane", IconLayoutSize) );
		Set( "LevelViewport.ViewportConfig_OnePane.Small", new IMAGE_BRUSH("Icons/ViewportLayout_OnePane", IconLayoutSizeSmall) );
		Set( "LevelViewport.ViewportConfig_TwoPanesH", new IMAGE_BRUSH( "Icons/ViewportLayout_TwoPanesHoriz", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_TwoPanesH.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_TwoPanesHoriz", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_TwoPanesV", new IMAGE_BRUSH( "Icons/ViewportLayout_TwoPanesVert", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_TwoPanesV.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_TwoPanesVert", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesLeft", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesLeft", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesLeft.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesLeft", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesRight", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesRight", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesRight.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesRight", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesTop", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesTop", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesTop.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesTop", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesBottom", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesBottom", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_ThreePanesBottom.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_ThreePanesBottom", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_FourPanesLeft", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesLeft", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_FourPanesLeft.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesLeft", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_FourPanesRight", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesRight", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_FourPanesRight.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesRight", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_FourPanesTop", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesTop", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_FourPanesTop.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesTop", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_FourPanesBottom", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesBottom", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_FourPanesBottom.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanesBottom", IconLayoutSizeSmall ) );
		Set( "LevelViewport.ViewportConfig_FourPanes2x2", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanes2x2", IconLayoutSize ) );
		Set( "LevelViewport.ViewportConfig_FourPanes2x2.Small", new IMAGE_BRUSH( "Icons/ViewportLayout_FourPanes2x2", IconLayoutSizeSmall ) );

		Set( "LevelViewport.EjectActorPilot", new IMAGE_BRUSH( "Icons/icon_EjectActorPilot_16x", Icon16x16 ) );
		Set( "LevelViewport.EjectActorPilot.Small", new IMAGE_BRUSH( "Icons/icon_EjectActorPilot_16x", Icon16x16 ) );
		Set( "LevelViewport.PilotSelectedActor", new IMAGE_BRUSH( "Icons/icon_PilotSelectedActor_16x", Icon16x16 ) );
		Set( "LevelViewport.PilotSelectedActor.Small", new IMAGE_BRUSH( "Icons/icon_PilotSelectedActor_16x", Icon16x16 ) );
		
		Set( "LevelViewport.ToggleActorPilotCameraView",       new IMAGE_BRUSH_SVG( "Starship/Common/CameraShake", Icon16x16 ) );
		Set( "LevelViewport.ToggleActorPilotCameraView.Small", new IMAGE_BRUSH_SVG( "Starship/Common/CameraShake", Icon16x16 ) );

		Set( "LevelViewport.ActorPilotText", FTextBlockStyle()
			.SetFont( DEFAULT_FONT( "BoldCondensed", 12 ) )
			.SetColorAndOpacity( FLinearColor(0.9f, 0.9f, 0.9f, 1.f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f, 0.f, 0.f, 0.4f) )
			.SetShadowOffset( FVector2D(1.f, 1.f) )
		);
	}

	// Level editor status bar
	{
		Set( "TransformSettings.RelativeCoordinateSettings", new IMAGE_BRUSH( "Icons/icon_axis_16px", FVector2D( 16, 16 ) ) );
	}

	// Mesh Proxy Window
	{
		Set("MeshProxy.SimplygonLogo", new IMAGE_BRUSH( "Icons/SimplygonBanner_Sml", FVector2D(174, 36) ) );
	}
#endif // WITH_EDITOR

	// Level viewport 
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{

		Set( "LevelViewport.DebugBorder", new BOX_BRUSH( "Old/Window/ViewportDebugBorder", 0.8f, FLinearColor(.7,0,0,.5) ) );
		Set( "LevelViewport.BlackBackground", new FSlateColorBrush( FLinearColor::Black ) ); 
		Set( "LevelViewport.StartingPlayInEditorBorder", new BOX_BRUSH( "Old/Window/ViewportDebugBorder", 0.8f, FLinearColor(0.1f,1.0f,0.1f,1.0f) ) );
		Set( "LevelViewport.StartingSimulateBorder", new BOX_BRUSH( "Old/Window/ViewportDebugBorder", 0.8f, FLinearColor(1.0f,1.0f,0.1f,1.0f) ) );
		Set( "LevelViewport.ReturningToEditorBorder", new BOX_BRUSH( "Old/Window/ViewportDebugBorder", 0.8f, FLinearColor(0.1f,0.1f,1.0f,1.0f) ) );
		Set( "LevelViewport.ActorLockIcon", new IMAGE_BRUSH( "Icons/ActorLockedViewport", Icon32x32 ) );
		Set( "LevelViewport.Icon", new IMAGE_BRUSH( "Icons/icon_tab_viewport_16px", Icon16x16 ) );

		Set( "LevelViewportContextMenu.ActorType.Text", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 8 ) )
			.SetColorAndOpacity( FLinearColor(0.72f, 0.72f, 0.72f, 1.f) ) );

		Set( "LevelViewportContextMenu.AssetLabel.Text", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 9 ) ) );

		Set( "LevelViewport.CursorIcon", new IMAGE_BRUSH( "Common/Cursor", Icon16x16 ) );
	}

	// Level editor ui command icons
	{
		Set( "LevelEditor.ShowAll", new IMAGE_BRUSH( "Old/SelectionDetails/ShowAll", FVector2D(32,32) ) );
		Set( "LevelEditor.ShowSelectedOnly", new IMAGE_BRUSH( "Old/SelectionDetails/ShowSelected", FVector2D(32,32) ) );
		Set( "LevelEditor.ShowSelected", new IMAGE_BRUSH( "Old/SelectionDetails/ShowSelected", FVector2D(32,32) ) );
		Set( "LevelEditor.HideSelected", new IMAGE_BRUSH( "Old/SelectionDetails/HideSelected", FVector2D(32,32) ) );
	}

	// Show flags menus
	{
		Set( "ShowFlagsMenu.AntiAliasing", new IMAGE_BRUSH_SVG( "Starship/Common/AntiAliasing", Icon16x16 ) );
		Set( "ShowFlagsMenu.Atmosphere", new IMAGE_BRUSH_SVG( "Starship/Common/Atmosphere", Icon16x16 ) );
		Set( "ShowFlagsMenu.BSP", new IMAGE_BRUSH_SVG( "Starship/Common/BSP", Icon16x16 ) );
		Set( "ShowFlagsMenu.Collision", new IMAGE_BRUSH_SVG( "Starship/Common/Collision", Icon16x16 ) );
		Set( "ShowFlagsMenu.Decals", new IMAGE_BRUSH_SVG( "Starship/Common/Decals", Icon16x16 ) );
		Set( "ShowFlagsMenu.Fog", new IMAGE_BRUSH_SVG( "Starship/Common/Fog", Icon16x16 ) );
		Set( "ShowFlagsMenu.Grid", new IMAGE_BRUSH_SVG( "Starship/Common/Grid", Icon16x16 ) );
		Set( "ShowFlagsMenu.Landscape", new IMAGE_BRUSH_SVG( "Starship/Common/Landscape", Icon16x16 ) );
		Set( "ShowFlagsMenu.MediaPlanes", new IMAGE_BRUSH_SVG( "Starship/Common/MediaPlanes", Icon16x16 ) );
		Set( "ShowFlagsMenu.Navigation", new IMAGE_BRUSH_SVG( "Starship/Common/Navigation", Icon16x16 ) );
		Set( "ShowFlagsMenu.Particles", new IMAGE_BRUSH_SVG( "Starship/Common/ParticleSprites", Icon16x16 ) );
		Set( "ShowFlagsMenu.SkeletalMeshes", new IMAGE_BRUSH_SVG( "Starship/Common/SkeletalMesh", Icon16x16 ) );
		Set( "ShowFlagsMenu.StaticMeshes", new IMAGE_BRUSH_SVG( "Starship/Common/StaticMesh", Icon16x16 ) );
		Set( "ShowFlagsMenu.Translucency", new IMAGE_BRUSH_SVG( "Starship/Common/Transparency", Icon16x16 ) );
		Set( "ShowFlagsMenu.WidgetComponents", new IMAGE_BRUSH_SVG( "Starship/Common/WidgetComponents", Icon16x16 ) );

		Set("ShowFlagsMenu.SubMenu.PostProcessing", new IMAGE_BRUSH_SVG("Starship/Common/PostProcessing", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.LightTypes", new IMAGE_BRUSH_SVG("Starship/Common/LightTypes", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.LightingComponents", new IMAGE_BRUSH_SVG("Starship/Common/LightingComponents", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.LightingFeatures", new IMAGE_BRUSH_SVG("Starship/Common/LightingFeatures", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.Developer", new IMAGE_BRUSH_SVG("Starship/Common/Developer", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.Visualize", new IMAGE_BRUSH_SVG("Starship/Common/Visualize", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.Advanced", new IMAGE_BRUSH_SVG("Starship/Common/Advanced", Icon16x16));

		Set("ShowFlagsMenu.SubMenu.Volumes", new IMAGE_BRUSH_SVG("Starship/Common/Volume", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.Layers", new IMAGE_BRUSH_SVG("Starship/Common/Layers", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.FoliageTypes", new IMAGE_BRUSH_SVG("Starship/Common/FoliageTypes", Icon16x16));
		Set("ShowFlagsMenu.SubMenu.Sprites", new IMAGE_BRUSH_SVG("Starship/Common/Sprite", Icon16x16));
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Mobility Icons
	{
		Set("Mobility.Movable", new IMAGE_BRUSH("/Icons/Mobility/Movable_16x", Icon16x16));
		Set("Mobility.Stationary", new IMAGE_BRUSH("/Icons/Mobility/Adjustable_16x", Icon16x16));
		Set("Mobility.Static", new IMAGE_BRUSH("/Icons/Mobility/Static_16x", Icon16x16));

		const FString SmallRoundedButton(TEXT("Common/SmallRoundedToggle"));
		const FString SmallRoundedButtonStart(TEXT("Common/SmallRoundedToggleLeft"));
		const FString SmallRoundedButtonMiddle(TEXT("Common/SmallRoundedToggleCenter"));
		const FString SmallRoundedButtonEnd(TEXT("Common/SmallRoundedToggleRight"));

		const FLinearColor NormalColor(0.15, 0.15, 0.15, 1);

		Set("Property.ToggleButton", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButton, FMargin(7.f / 16.f), SelectionColor)));

		Set("Property.ToggleButton.Start", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButtonStart, FMargin(7.f / 16.f), SelectionColor)));

		Set("Property.ToggleButton.Middle", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButtonMiddle, FMargin(7.f / 16.f), SelectionColor)));

		Set("Property.ToggleButton.End", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), NormalColor))
			.SetUncheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetUncheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor_Pressed))
			.SetCheckedHoveredImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedPressedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor))
			.SetCheckedImage(BOX_BRUSH(*SmallRoundedButtonEnd, FMargin(7.f / 16.f), SelectionColor)));

		// Experimental/early access stuff
		Set("PropertyEditor.ExperimentalClass", new IMAGE_BRUSH("/PropertyView/ExperimentalClassWarning", Icon40x40));
		Set("PropertyEditor.EarlyAccessClass", new IMAGE_BRUSH("/PropertyView/EarlyAccessClassWarning", Icon40x40));
	}

	// Mesh Paint
	{
		Set("MeshPaint.FindInCB",       new IMAGE_BRUSH("/Icons/icon_toolbar_genericfinder_40px", Icon20x20) );
		Set("MeshPaint.Swap",           new IMAGE_BRUSH("/Icons/icon_MeshPaint_Swap_16x", Icon12x12));

		Set("MeshPaint.Fill",           new IMAGE_BRUSH("/Icons/GeneralTools/PaintBucket_40x", Icon20x20) );
		Set("MeshPaint.Fill.Small",     new IMAGE_BRUSH("/Icons/GeneralTools/PaintBucket_40x", Icon20x20) );
		Set("MeshPaint.Propagate",      new IMAGE_BRUSH("/Icons/Paint/Paint_Propagate_40x", Icon20x20) );
		Set("MeshPaint.Propagate.Small",new IMAGE_BRUSH("/Icons/Paint/Paint_Propagate_40x", Icon20x20) );
		Set("MeshPaint.Import",         new IMAGE_BRUSH("/Icons/GeneralTools/Import_40x", Icon20x20) );
		Set("MeshPaint.Import.Small",   new IMAGE_BRUSH("/Icons/GeneralTools/Import_40x", Icon20x20) );
		Set("MeshPaint.Save",           new IMAGE_BRUSH("/Icons/GeneralTools/Save_40x", Icon20x20) );
		Set("MeshPaint.Save.Small",     new IMAGE_BRUSH("/Icons/GeneralTools/Save_40x", Icon20x20) );
		Set("MeshPaint.Fix",            new IMAGE_BRUSH("/Icons/GeneralTools/Fix_40x", Icon20x20) );
		Set("MeshPaint.Fix.Small",      new IMAGE_BRUSH("/Icons/GeneralTools/Fix_40x", Icon20x20) );
		Set("MeshPaint.Remove",         new IMAGE_BRUSH("/Icons/GeneralTools/Delete_40x", Icon20x20));
		Set("MeshPaint.Remove.Small",   new IMAGE_BRUSH("/Icons/GeneralTools/Delete_40x", Icon20x20));
		Set("MeshPaint.Copy",           new IMAGE_BRUSH("/Icons/GeneralTools/Copy_40x", Icon20x20));
		Set("MeshPaint.Copy.Small",     new IMAGE_BRUSH("/Icons/GeneralTools/Copy_40x", Icon20x20));
		Set("MeshPaint.Paste",          new IMAGE_BRUSH("/Icons/GeneralTools/Paste_40x", Icon20x20));
		Set("MeshPaint.Paste.Small",    new IMAGE_BRUSH("/Icons/GeneralTools/Paste_40x", Icon20x20));
		Set("MeshPaint.PropagateTexturePaint",             new IMAGE_BRUSH("/Icons/Paint/Paint_PropagateTexture_40x", Icon20x20));
		Set("MeshPaint.PropagateTexturePaint.Small",       new IMAGE_BRUSH("/Icons/Paint/Paint_PropagateTexture_40x", Icon20x20));
		Set("MeshPaint.SaveTexturePaint",                  new IMAGE_BRUSH("/Icons/GeneralTools/Save_40x", Icon20x20));
		Set("MeshPaint.SaveTexturePaint.Small",            new IMAGE_BRUSH("/Icons/GeneralTools/Save_40x", Icon20x20));
		Set("MeshPaint.PropagateVertexColorsToLODs",       new IMAGE_BRUSH("Icons/Paint/Paint_AllLODs_40x", Icon20x20));
		Set("MeshPaint.PropagateVertexColorsToLODs.Small", new IMAGE_BRUSH("Icons/Paint/Paint_AllLODs_40x", Icon20x20));
		Set("MeshPaint.TexturePaint",       new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("MeshPaint.TexturePaint.Small", new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("MeshPaint.ColorPaint",         new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("MeshPaint.ColorPaint.Small",   new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("MeshPaint.WeightPaint",        new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("MeshPaint.WeightPaint.Small",  new IMAGE_BRUSH("Icons/GeneralTools/Paint_40x", Icon20x20));
		Set("MeshPaint.VertexSelect",             new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Select_40x"), Icon20x20));
		Set("MeshPaint.VertexSelect.Small",       new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Select_40x"), Icon20x20));
		Set("MeshPaint.TextureSelect",            new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Select_40x"), Icon20x20));
		Set("MeshPaint.TextureSelect.Small",      new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Select_40x"), Icon20x20));
		Set("MeshPaint.PreviousTexture",          new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Previous_40x"), Icon20x20));
		Set("MeshPaint.PreviousTexture.Small",    new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Previous_40x"), Icon20x20));
		Set("MeshPaint.NextTexture",              new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Next_40x"), Icon20x20));
		Set("MeshPaint.NextTexture.Small",        new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Next_40x"), Icon20x20));
		Set("MeshPaint.CycleToPreviousLOD",       new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Previous_40x"), Icon20x20));
		Set("MeshPaint.CycleToPreviousLOD.Small", new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Previous_40x"), Icon20x20));
		Set("MeshPaint.CycleToNextLOD",           new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Next_40x"), Icon20x20));
		Set("MeshPaint.CycleToNextLOD.Small",     new IMAGE_BRUSH(TEXT("Icons/GeneralTools/Next_40x"), Icon20x20));
		Set("MeshPaint.SwitchForeAndBackgroundColor",       new IMAGE_BRUSH("Icons/Paint/Paint_SwapColors_40x", Icon20x20));
		Set("MeshPaint.SwitchForeAndBackgroundColor.Small", new IMAGE_BRUSH("Icons/Paint/Paint_SwapColors_40x", Icon20x20));
		Set("MeshPaint.Brush",			new IMAGE_BRUSH_SVG("Starship/Common/Paintbrush", Icon20x20));
	}

	// Scalability (Performance Warning)
	{
		Set( "Scalability.ScalabilitySettings", new IMAGE_BRUSH("Scalability/ScalabilitySettings", FVector2D(473.0f, 266.0f) ) );
	}

	Set("WorkspaceMenu.AdditionalUI", new IMAGE_BRUSH("Icons/icon_ViewMode_LODColoration_16px", Icon16x16));
}

void FStarshipEditorStyle::FStyle::SetupPersonaStyle()
{
	// Persona
#if WITH_EDITOR
	{
		// Persona viewport
		Set( "AnimViewportMenu.TranslateMode", new IMAGE_BRUSH( "Icons/icon_translate_40x", Icon32x32) );
		Set( "AnimViewportMenu.TranslateMode.Small", new IMAGE_BRUSH( "Icons/icon_translate_40x", Icon16x16 ) );
		Set( "AnimViewportMenu.RotateMode", new IMAGE_BRUSH( "Icons/icon_rotate_40x", Icon32x32) );
		Set( "AnimViewportMenu.RotateMode.Small", new IMAGE_BRUSH( "Icons/icon_rotate_40x", Icon16x16 ) );
		Set( "AnimViewportMenu.CameraFollow", new IMAGE_BRUSH( "Persona/Viewport/Camera_FollowBounds_40px", Icon32x32) );
		Set( "AnimViewportMenu.CameraFollow.Small", new IMAGE_BRUSH( "Persona/Viewport/Camera_FollowBounds_40px", Icon16x16 ) );
		Set( "AnimViewport.LocalSpaceEditing", new IMAGE_BRUSH( "Icons/icon_axis_local_16px", FVector2D( 16, 16 ) ) );
		Set( "AnimViewport.WorldSpaceEditing", new IMAGE_BRUSH( "Icons/icon_axis_world_16px", FVector2D( 16, 16 ) ) );
		Set( "AnimViewportMenu.SetShowNormals", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_Normals_40x"), Icon40x40 ) );
		Set( "AnimViewportMenu.SetShowNormals.Small", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_Normals_40x"), Icon20x20 ) );
		Set( "AnimViewportMenu.SetShowTangents", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_Tangents_40x"), Icon40x40 ) );
		Set( "AnimViewportMenu.SetShowTangents.Small", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_Tangents_40x"), Icon20x20 ) );
		Set( "AnimViewportMenu.SetShowBinormals", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_Binormals_40x"), Icon40x40 ) );
		Set( "AnimViewportMenu.SetShowBinormals.Small", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_Binormals_40x"), Icon20x20 ) );
		Set( "AnimViewportMenu.AnimSetDrawUVs", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_UVOverlay_40x"), Icon40x40 ) );
		Set( "AnimViewportMenu.AnimSetDrawUVs.Small", new IMAGE_BRUSH( TEXT("Icons/icon_StaticMeshEd_UVOverlay_40x"), Icon20x20 ) );

		Set("AnimViewportMenu.PlayBackSpeed", new IMAGE_BRUSH("Persona/Viewport/icon_Playback_speed_16x", Icon16x16));
		Set("AnimViewportMenu.TurnTableSpeed", new IMAGE_BRUSH("Persona/Viewport/icon_turn_table_16x", Icon16x16));
		Set("AnimViewportMenu.SceneSetup", new IMAGE_BRUSH("Icons/icon_tab_SceneOutliner_16x", Icon16x16));

		Set( "AnimViewport.MessageFont", DEFAULT_FONT("Bold", 9) );

		Set("AnimViewport.MessageText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f)));
		Set("AnimViewport.WarningText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize))
			.SetColorAndOpacity(FLinearColor::Yellow)
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f)));
		Set("AnimViewport.ErrorText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize))
			.SetColorAndOpacity(FLinearColor::Red)
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f)));
		
		// Viewport notifications 
		Set("AnimViewport.Notification.Error", new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, FLinearColor(0.728f, 0.0f, 0.0f)));
		Set("AnimViewport.Notification.Warning", new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, FLinearColor(0.728f, 0.364f, 0.003f)));
		Set("AnimViewport.Notification.Message", new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, FLinearColor(0.364f, 0.364f, 0.364f)));

		Set("AnimViewport.Notification.CloseButton", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("/Docking/CloseApp_Normal", Icon16x16))
			.SetPressed(IMAGE_BRUSH("/Docking/CloseApp_Pressed", Icon16x16))
			.SetHovered(IMAGE_BRUSH("/Docking/CloseApp_Hovered", Icon16x16)));

		// persona commands
		Set("Persona.AnimNotifyWindow", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationNotifies", Icon20x20));
		Set("Persona.RetargetManager", new IMAGE_BRUSH_SVG("Starship/Persona/RetargetManager", Icon20x20));
		Set("Persona.ImportMesh", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.ReimportMesh", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.ImportLODs", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.ImportAnimation", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.ReimportAnimation", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.ApplyCompression", new IMAGE_BRUSH_SVG("Starship/Common/Compress", Icon20x20));
		Set("Persona.ExportToFBX", new IMAGE_BRUSH_SVG("Starship/Common/Export", Icon20x20));
		Set("Persona.CreateAsset", new IMAGE_BRUSH_SVG("Starship/Persona/PersonaCreateAsset", Icon20x20));
		Set("Persona.StartRecordAnimation", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_StartRecord_40x"), Icon40x40));
		Set("Persona.StopRecordAnimation", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_StopRecord_40x"), Icon40x40));
		Set("Persona.StopRecordAnimation_Alt", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_StopRecord_Alt_40x"), Icon40x40));
		Set("Persona.SetKey", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_SetKey_40x"), Icon40x40));
		Set("Persona.ApplyAnimation", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_BakeAnim_40x"), Icon40x40));

		// preview set up
		Set("Persona.TogglePreviewAsset", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationPreviewMesh", Icon20x20));
		Set("Persona.TogglePreviewAnimation", new IMAGE_BRUSH_SVG("Starship/Persona/PersonaPreviewAnimation", Icon20x20));
		Set("Persona.ToggleReferencePose", new IMAGE_BRUSH_SVG("Starship/Persona/PersonaTPose", Icon20x20));
		Set("Persona.SavePreviewMeshCollection", new IMAGE_BRUSH(TEXT("Icons/Save_16x"), Icon16x16));

		// persona extras
		Set("Persona.ConvertAnimationGraph", new IMAGE_BRUSH("Old/Graph/ConvertIcon", Icon40x40));
		Set("Persona.ReimportAsset", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.ConvertToStaticMesh", new IMAGE_BRUSH_SVG("Starship/Persona/MakeStaticMesh", Icon20x20));
		Set("Persona.BakeMaterials", new IMAGE_BRUSH("Icons/icon_tab_Layers_40x", Icon40x40));

		// Anim Slot Manager
		Set("AnimSlotManager.SaveSkeleton", new IMAGE_BRUSH("Persona/AnimSlotManager/icon_SaveSkeleton_40x", Icon40x40));
		Set("AnimSlotManager.AddGroup", new IMAGE_BRUSH("Persona/AnimSlotManager/icon_AddGroup_40x", Icon40x40));
		Set("AnimSlotManager.AddSlot", new IMAGE_BRUSH("Persona/AnimSlotManager/icon_AddSlot_40x", Icon40x40));
		Set("AnimSlotManager.Warning", new IMAGE_BRUSH("Persona/AnimSlotManager/icon_Warning_14x", Icon16x16));

		// Anim Notify Editor
		Set("AnimNotifyEditor.BranchingPoint", new IMAGE_BRUSH("Persona/NotifyEditor/BranchingPoints_24x", Icon24x24));

		// AnimBlueprint Preview Warning Background
		FSlateColor PreviewPropertiesWarningColour(FLinearColor::Gray);
		Set("Persona.PreviewPropertiesWarning", new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, PreviewPropertiesWarningColour));

		// Persona-specific tabs
		Set("Persona.Tabs.SkeletonTree", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Skeleton_Tree_16x"), Icon16x16));
		Set("Persona.Tabs.MorphTargetPreviewer", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Morph_Target_Previewer_16x"), Icon16x16));
		Set("Persona.Tabs.AnimCurvePreviewer", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_AnimCurve_Previewer_16x"), Icon16x16));
		Set("Persona.Tabs.AnimationNotifies", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Animation_Notifies_16x"), Icon16x16));
		Set("Persona.Tabs.RetargetManager", new IMAGE_BRUSH_SVG("Starship/Persona/RetargetManager", Icon16x16));
		Set("Persona.Tabs.AnimSlotManager", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Anim_Slot_Manager_16x"), Icon16x16));
		Set("Persona.Tabs.SkeletonCurves", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Skeleton_Curves_16x"), Icon16x16));
		Set("Persona.Tabs.AnimAssetDetails", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Anim_Asset_Details_16x"), Icon16x16));
		Set("Persona.Tabs.ControlRigMappingWindow", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Skeleton_Tree_16x"), Icon16x16));

		// AssetFamilyIcons
		Set("Persona.AssetClass.Skeleton", new IMAGE_BRUSH_SVG("Starship/Persona/Skeleton", Icon20x20));
		Set("Persona.AssetClass.Animation", new IMAGE_BRUSH_SVG("Starship/Persona/Animation", Icon20x20));
		Set("Persona.AssetClass.SkeletalMesh", new IMAGE_BRUSH_SVG("Starship/Persona/SkeletalMesh", Icon20x20));
		Set("Persona.AssetClass.Blueprint", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/blueprints", Icon20x20) );
		Set("Persona.AssetClass.Physics", new IMAGE_BRUSH_SVG("Starship/Persona/Physics", Icon20x20));
	}

	// Skeleton editor
	{
		Set("SkeletonEditor.AnimNotifyWindow", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationNotifies", Icon20x20));
		Set("SkeletonEditor.RetargetManager", new IMAGE_BRUSH_SVG("Starship/Persona/RetargetManager", Icon20x20));
		Set("SkeletonEditor.ImportMesh", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));

		// Skeleton Tree
		Set("SkeletonTree.SkeletonSocket", new IMAGE_BRUSH("Persona/SkeletonTree/icon_SocketG_16px", Icon16x16));
		Set("SkeletonTree.MeshSocket", new IMAGE_BRUSH("Persona/SkeletonTree/icon_SocketC_16px", Icon16x16));
		Set("SkeletonTree.LODBone", new IMAGE_BRUSH(TEXT("Persona/SkeletonTree/icon_LODBone_16x"), Icon16x16));
		Set("SkeletonTree.Bone", 	new IMAGE_BRUSH_SVG(TEXT("Starship/Animation/Bone"), Icon16x16) );
		Set("SkeletonTree.BoneNonWeighted", new IMAGE_BRUSH_SVG(TEXT("Starship/Animation/BoneNonWeighted"), Icon16x16) );
		Set("SkeletonTree.NonRequiredBone", new IMAGE_BRUSH(TEXT("Persona/SkeletonTree/icon_NonRequiredBone_16x"), Icon16x16));
		Set("SkeletonTree.NormalFont", FTextBlockStyle(NormalText));
		Set("SkeletonTree.ItalicFont", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Italic", 10)));

		Set("SkeletonTree.HyperlinkSpinBox", FSpinBoxStyle()
			.SetBackgroundBrush(       FSlateRoundedBoxBrush(FStyleColors::Input, 3.f, FStyleColors::Transparent, 1.0f))
			.SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, 3.f, FStyleColors::Transparent, 1.0f))
			.SetActiveFillBrush(       FSlateRoundedBoxBrush(FStyleColors::Hover, 3.f, FStyleColors::Transparent, 1.0f))
			.SetInactiveFillBrush(     FSlateRoundedBoxBrush(FStyleColors::Secondary, 3.f, FStyleColors::Transparent, 1.0f))
			.SetArrowsImage(FSlateNoResource())
			.SetForegroundColor(FStyleColors::ForegroundHover)
			.SetTextPadding(FMargin(8.f, 4.f, 8.f, 3.f))
			.SetArrowsImage(FSlateNoResource())
		);


		const FButtonStyle BorderlessButton = FButtonStyle(GetWidgetStyle<FButtonStyle>("SimpleButton"))
			.SetNormalPadding(0.f)
			.SetPressedPadding(0.f);

		Set("SkeletonTree.RetargetingComboButton", FComboButtonStyle(GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.SetButtonStyle(BorderlessButton)
			.SetDownArrowPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f)));

		Set("SkeletonTree.BlendProfile", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_NewBlendSpace_16x"), Icon16x16));
		Set("SkeletonTree.InlineEditorShadowTop", new IMAGE_BRUSH(TEXT("Common/ScrollBoxShadowTop"), FVector2D(64, 8)));
		Set("SkeletonTree.InlineEditorShadowBottom", new IMAGE_BRUSH(TEXT("Common/ScrollBoxShadowBottom"), FVector2D(64, 8)));
	}

	// Animation editor
	{
		Set("AnimationEditor.ApplyCompression", new IMAGE_BRUSH_SVG("Starship/Common/Compress", Icon20x20));
		Set("AnimationEditor.ExportToFBX", new IMAGE_BRUSH_SVG("Starship/Common/Export", Icon20x20));
		Set("AnimationEditor.ReimportAnimation", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("AnimationEditor.CreateAsset", new IMAGE_BRUSH_SVG("Starship/Persona/PersonaCreateAsset", Icon20x20));
		Set("AnimationEditor.SetKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", Icon20x20));
		Set("AnimationEditor.ApplyAnimation", new IMAGE_BRUSH_SVG("Starship/Common/Apply", Icon20x20));

		Set("AnimTimeline.Outliner.DefaultBorder", new FSlateColorBrush(FLinearColor::White));

		const FSplitterStyle OutlinerSplitterStyle = FSplitterStyle()
			.SetHandleNormalBrush(FSlateColorBrush(FStyleColors::Recessed))
			.SetHandleHighlightBrush(FSlateColorBrush(FStyleColors::Secondary));

		Set("AnimTimeline.Outliner.Splitter", OutlinerSplitterStyle);

		Set("AnimTimeline.Outliner.Label", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f)));

		Set("AnimTimeline.Outliner.ItemColor", FLinearColor(0.04f, 0.04f, 0.04f, 0.8f));
		Set("AnimTimeline.Outliner.HeaderColor", FLinearColor(0.03f, 0.03f, 0.03f, 1.0f));

		Set("AnimTimeline.SectionMarker", new IMAGE_BRUSH(TEXT("Sequencer/SectionMarker"), FVector2D(11, 12)));
	}

	// Skeletal mesh editor
	{
		Set("SkeletalMeshEditor.ReimportMesh", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("SkeletalMeshEditor.ImportLODs", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));

		Set("SkeletalMeshEditor.MeshSectionSelection", new IMAGE_BRUSH_SVG("Starship/Persona/SectionSelection", Icon20x20));
	}

	// Kismet 2
	{
		Set( "FullBlueprintEditor.SwitchToScriptingMode", new IMAGE_BRUSH_SVG( "Starship/Blueprints/icon_BlueprintEditor_EventGraph", Icon20x20));

		// @todo - Icon Replacement - The UI commands using these icons are never visible in the editor
		Set( "FullBlueprintEditor.SwitchToBlueprintDefaultsMode", new IMAGE_BRUSH_SVG("Starship/Common/Defaults", Icon20x20));
		Set( "FullBlueprintEditor.SwitchToComponentsMode", new IMAGE_BRUSH_SVG("Starship/Common/Component", Icon20x20));

		Set( "FullBlueprintEditor.EditGlobalOptions", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/Settings", Icon20x20));
		Set("FullBlueprintEditor.EditClassDefaults", new IMAGE_BRUSH_SVG("Starship/Common/Details", Icon20x20));

		Set( "FullBlueprintEditor.Diff", new IMAGE_BRUSH( "Icons/BlueprintEditorDiff", Icon40x40 ) );
		Set( "FullBlueprintEditor.Diff.Small", new IMAGE_BRUSH( "Icons/BlueprintEditorDiff", Icon20x20 ) );

		Set( "BlueprintEditor.ActionMenu.ContextDescriptionFont",  DEFAULT_FONT("Regular", 12) );
		Set( "BlueprintEditor.ActionMenu.ContextDescriptionFont",  DEFAULT_FONT("Regular", 12) );

		Set("BlueprintEditor.FindInBlueprints.MenuIcon", new IMAGE_BRUSH_SVG("Starship/Common/FindInBlueprints", Icon16x16));
		Set("BlueprintEditor.FindInBlueprint", new IMAGE_BRUSH_SVG("Starship/Common/FindInBlueprints", Icon20x20));

		Set( "Kismet.DeleteUnusedVariables", new IMAGE_BRUSH_SVG("/Starship/Blueprints/icon_kismet_findunused", Icon16x16) );
		{
			Set( "Kismet.Tabs.Variables", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pill", Icon16x16 ) );
			Set( "Kismet.Tabs.Palette", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Palette", Icon16x16 ) );
			Set( "Kismet.Tabs.CompilerResults", new IMAGE_BRUSH_SVG("Starship/Common/OutputLog", Icon16x16 ) );
			Set( "Kismet.Tabs.FindResults", new CORE_IMAGE_BRUSH_SVG("Starship/Common/search", Icon16x16 ) );
			Set( "Kismet.Tabs.Bookmarks", new IMAGE_BRUSH_SVG( "Starship/Common/Bookmarks", Icon16x16 ) );
			Set( "Kismet.Tabs.Components", new IMAGE_BRUSH_SVG( "Starship/Common/Component", Icon16x16 ) );
			Set( "Kismet.Tabs.BlueprintDefaults", new IMAGE_BRUSH( "Icons/icon_BlueprintEditor_Defaults_40x", Icon16x16 ) );
		}

		const FCheckBoxStyle KismetFavoriteToggleStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
			.SetUncheckedImage( IMAGE_BRUSH("Icons/EmptyStar_16x", Icon10x10, FLinearColor(0.8f, 0.8f, 0.8f, 1.f)) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH("Icons/EmptyStar_16x", Icon10x10, FLinearColor(2.5f, 2.5f, 2.5f, 1.f)) )
			.SetUncheckedPressedImage( IMAGE_BRUSH("Icons/EmptyStar_16x", Icon10x10, FLinearColor(0.8f, 0.8f, 0.8f, 1.f)) )
			.SetCheckedImage( IMAGE_BRUSH("Icons/Star_16x", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)) )
			.SetCheckedHoveredImage( IMAGE_BRUSH("Icons/Star_16x", Icon10x10, FLinearColor(0.4f, 0.4f, 0.4f, 1.f)) )
			.SetCheckedPressedImage( IMAGE_BRUSH("Icons/Star_16x", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)) );
		Set("Kismet.Palette.FavoriteToggleStyle", KismetFavoriteToggleStyle);

		Set( "Kismet.Tooltip.SubtextFont", DEFAULT_FONT("Regular", 8) );

		Set("Blueprint.CompileStatus.Background", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Background", Icon20x20));

		// @todo - Icon Replacement - trying out tinting compile backgrounds
/*
		Set("Blueprint.CompileStatus.Background.Unknown", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Background", Icon20x20, FStyleColors::AccentYellow));
		Set("Blueprint.CompileStatus.Background.Warning", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Background", Icon20x20, FStyleColors::Warning));
		Set("Blueprint.CompileStatus.Background.Good", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Background", Icon20x20, FStyleColors::AccentGreen));
		Set("Blueprint.CompileStatus.Background.Error", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Background", Icon20x20, FStyleColors::Error));*/

		Set("Blueprint.CompileStatus.Overlay.Unknown", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Unknown_Badge", Icon20x20, FStyleColors::AccentYellow));
		Set("Blueprint.CompileStatus.Overlay.Warning", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Warning_Badge", Icon20x20, FStyleColors::Warning));
		Set("Blueprint.CompileStatus.Overlay.Good", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Good_Badge", Icon20x20, FStyleColors::AccentGreen));
		Set("Blueprint.CompileStatus.Overlay.Error", new IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Fail_Badge", Icon20x20, FStyleColors::Error));

		// @todo - Icon Replacement - these are hijacked by non-blueprint things. Keeping them around for now
		Set( "Kismet.Status.Unknown", new IMAGE_BRUSH( "Old/Kismet2/CompileStatus_Working", Icon40x40 ) );
		Set( "Kismet.Status.Error", new IMAGE_BRUSH( "Old/Kismet2/CompileStatus_Fail", Icon40x40 ) );
		Set( "Kismet.Status.Good", new IMAGE_BRUSH( "Old/Kismet2/CompileStatus_Good", Icon40x40 ) );
		Set( "Kismet.Status.Warning", new IMAGE_BRUSH( "Old/Kismet2/CompileStatus_Warning", Icon40x40 ) );

		Set("BlueprintEditor.AddNewVariable", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_AddVariable", Icon20x20));
		Set("BlueprintEditor.AddNewLocalVariable", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_AddVariable", Icon20x20));
		Set("BlueprintEditor.AddNewFunction", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_AddFunction", Icon20x20));
		Set("BlueprintEditor.AddNewMacroDeclaration", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_AddMacro", Icon20x20));


		Set( "BlueprintEditor.AddNewAnimationGraph", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-plus", Icon16x16));
		Set( "BlueprintEditor.AddNewEventGraph", new IMAGE_BRUSH_SVG( "Starship/Blueprints/icon_Blueprint_AddGraph", Icon20x20) );

		Set( "BlueprintEditor.AddNewDelegate", new IMAGE_BRUSH_SVG( "Starship/Blueprints/icon_Blueprint_AddDelegate", Icon20x20) );

		Set( "BlueprintEditor.AddNewAnimationLayer", new IMAGE_BRUSH_SVG( "Starship/Blueprints/icon_Blueprint_AddFunction", Icon20x20) );


		Set("Kismet.VariableList.TypeIcon", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pill", Icon16x16));
		Set("Kismet.VariableList.ArrayTypeIcon", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pillarray", Icon16x16));
		Set("Kismet.VariableList.SetTypeIcon", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pillset", Icon16x16));
		Set("Kismet.VariableList.SetTypeIconLarge", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pillset", Icon40x40));
		Set("Kismet.VariableList.MapValueTypeIcon", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pillmapvalue", Icon16x16));
		Set("Kismet.VariableList.MapKeyTypeIcon", new IMAGE_BRUSH_SVG( "Starship/Blueprints/pillmapkey", Icon16x16));

		Set("Kismet.VariableList.ExposeForInstance", new CORE_IMAGE_BRUSH_SVG("Starship/Common/visible", Icon16x16));
		Set("Kismet.VariableList.HideForInstance", new CORE_IMAGE_BRUSH_SVG("Starship/Common/hidden", Icon16x16));

		Set( "Kismet.Explorer.Title", FTextBlockStyle(NormalText) .SetFont(DEFAULT_FONT( "BoldCondensedItalic", 11)));
		Set( "Kismet.Explorer.SearchDepthFont", DEFAULT_FONT( "Bold", 14) );

		Set( "Kismet.Interfaces.Title",  FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT("Bold", 11 ) ) );
		Set( "Kismet.Interfaces.Implement", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/PreviousArrow", Icon16x16) );
		Set( "Kismet.Interfaces.Remove", new CORE_IMAGE_BRUSH_SVG( "Icons/assign_right_16x", Icon16x16) );

		Set( "Kismet.TypePicker.CategoryFont", DEFAULT_FONT( "BoldCondensedItalic", 11) );
		Set( "Kismet.TypePicker.NormalFont", DEFAULT_FONT( "Regular", 11) );

		Set( "Kismet.GraphPicker.Title", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT("BoldCondensedItalic", 11) ) );

		Set( "Kismet.CreateBlueprint", new IMAGE_BRUSH( "/Icons/CreateBlueprint", Icon16x16) );
		Set( "Kismet.HarvestBlueprintFromActors", new IMAGE_BRUSH_SVG( "Starship/Blueprints/HarvestBlueprintFromActors", Icon16x16) );

		Set( "Kismet.Comment.Background", new IMAGE_BRUSH( "Old/Kismet2/Comment_Background", FVector2D(100.0f, 68.0f)) );

		Set( "Kismet.AllClasses.VariableIcon", new IMAGE_BRUSH_SVG("Starship/Blueprints/pill", Icon16x16));
		Set( "Kismet.AllClasses.ArrayVariableIcon", new IMAGE_BRUSH_SVG("Starship/Blueprints/pillarray", Icon16x16));
		Set( "Kismet.AllClasses.SetVariableIcon", new IMAGE_BRUSH_SVG("Starship/Blueprints/pillset", Icon16x16));
		Set( "Kismet.AllClasses.MapValueVariableIcon", new IMAGE_BRUSH_SVG("Starship/Blueprints/pillmapvalue", Icon16x16));
		Set( "Kismet.AllClasses.MapKeyVariableIcon", new IMAGE_BRUSH_SVG("Starship/Blueprints/pillmapkey", Icon16x16));

		Set( "Kismet.AllClasses.FunctionIcon", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_Blueprint_Function", Icon16x16));

		Set( "BlueprintEditor.ResetCamera", new IMAGE_BRUSH_SVG("Starship/Common/ResetCamera", Icon16x16));
		Set( "BlueprintEditor.ShowFloor", new IMAGE_BRUSH_SVG( "Starship/Common/Floor", Icon16x16));
		Set( "BlueprintEditor.ShowGrid", new IMAGE_BRUSH_SVG( "Starship/Common/Grid", Icon16x16 ) );
		Set( "BlueprintEditor.EnableSimulation", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));
		Set( "BlueprintEditor.EnableProfiling", new IMAGE_BRUSH_SVG("Starship/Common/Statistics", Icon20x20) );
		Set( "SCS.NativeComponent", new IMAGE_BRUSH( "Icons/NativeSCSComponent", Icon20x20 ));
		Set( "SCS.Component", new IMAGE_BRUSH( "Icons/SCSComponent", Icon20x20 ));

		// curve viewer
		Set("AnimCurveViewer.MorphTargetOn", new IMAGE_BRUSH(TEXT("Persona/AnimCurveViewer/MorphTarget_On"), Icon16x16));
		Set("AnimCurveViewer.MaterialOn", new IMAGE_BRUSH(TEXT("Persona/AnimCurveViewer/Material_On"), Icon16x16));
		Set("AnimCurveViewer.MorphTargetOff", new IMAGE_BRUSH(TEXT("Persona/AnimCurveViewer/MorphTarget_Off"), Icon16x16));
		Set("AnimCurveViewer.MaterialOff", new IMAGE_BRUSH(TEXT("Persona/AnimCurveViewer/Material_Off"), Icon16x16));
		Set("AnimCurveViewer.MorphTargetHover", new IMAGE_BRUSH(TEXT("Persona/AnimCurveViewer/MorphTarget_On"), Icon16x16));
		Set("AnimCurveViewer.MaterialHover", new IMAGE_BRUSH(TEXT("Persona/AnimCurveViewer/Material_On"), Icon16x16));

		// blend space
		Set("BlendSpaceEditor.ToggleTriangulation", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/triangulation_16"), Icon16x16));
		Set("BlendSpaceEditor.ToggleLabels", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/label_16"), Icon16x16));
		Set("BlendSpaceEditor.ArrowDown", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/arrow_down_12x"), FVector2D(13.0f, 25.0f)));
		Set("BlendSpaceEditor.ArrowUp", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/arrow_up_12x"), FVector2D(13.0f, 25.0f)));
		Set("BlendSpaceEditor.ArrowRight", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/arrow_right_12x"), FVector2D(25.0f, 13.0f)));
		Set("BlendSpaceEditor.ArrowLeft", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/arrow_left_12x"), FVector2D(25.0f, 13.0f)));
		Set("BlendSpaceEditor.PreviewIcon", new IMAGE_BRUSH(TEXT("Persona/BlendSpace/preview_21x"), FVector2D(21.0f, 21.0f)));
		Set("BlendSpaceEditor.LabelBackground", new FSlateRoundedBoxBrush(FStyleColors::Background, FStyleColors::Foreground, 1.0f));
		Set("BlendSpaceEditor.ZoomToFit", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingSelected_48x", Icon16x16));

		const FButtonStyle BlueprintContextTargetsButtonStyle = FButtonStyle()
			.SetNormal(IMAGE_BRUSH("Common/TreeArrow_Collapsed_Hovered", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)))
			.SetHovered(IMAGE_BRUSH("Common/TreeArrow_Collapsed_Hovered", Icon10x10, FLinearColor(0.4f, 0.4f, 0.4f, 1.f)))
			.SetPressed(IMAGE_BRUSH("Common/TreeArrow_Collapsed_Hovered", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)));
// 		const FCheckBoxStyle BlueprintContextTargetsButtonStyle = FCheckBoxStyle()
// 			.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
// 			.SetUncheckedImage(IMAGE_BRUSH("Common/TreeArrow_Collapsed", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)))
// 			.SetUncheckedHoveredImage(IMAGE_BRUSH("Common/TreeArrow_Collapsed", Icon10x10, FLinearColor(0.4f, 0.4f, 0.4f, 1.f)))
// 			.SetUncheckedPressedImage(IMAGE_BRUSH("Common/TreeArrow_Collapsed", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)))
// 			.SetCheckedImage(IMAGE_BRUSH("Common/TreeArrow_Collapsed_Hovered", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)))
// 			.SetCheckedHoveredImage(IMAGE_BRUSH("Common/TreeArrow_Collapsed_Hovered", Icon10x10, FLinearColor(0.4f, 0.4f, 0.4f, 1.f)))
// 			.SetCheckedPressedImage(IMAGE_BRUSH("Common/TreeArrow_Collapsed_Hovered", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)));
		Set("BlueprintEditor.ContextMenu.TargetsButton", BlueprintContextTargetsButtonStyle);

		Set( "BlueprintEditor.CompactPinTypeSelector", FButtonStyle()
			.SetNormal        ( FSlateNoResource() )
			.SetPressed       ( BOX_BRUSH( "Common/Button_Pressed", 8.0f/32.0f, SelectionColor_Pressed ) )
			.SetHovered       ( BOX_BRUSH( "Common/Button_Hovered", 8.0f/32.0f, SelectionColor ) )
			.SetNormalPadding ( FMargin( 0,0,0,0 ) )
			.SetPressedPadding( FMargin( 1,1,2,2 ) )
			);

		const FComboButtonStyle& SimpleComboButton = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton");
		
		Set("BlueprintEditor.CompactVariableTypeSelector", 
			FComboButtonStyle(SimpleComboButton)
			.SetDownArrowPadding(0)
			.SetButtonStyle(
				FButtonStyle(SimpleComboButton.ButtonStyle)
				.SetNormalPadding(FMargin(0, 2, 2, 2))
				.SetPressedPadding(FMargin(0, 3, 2, 1)))
		);
	}

	// Access modifier display in MyBlueprint window for functions/variables
	{
		Set("BlueprintEditor.AccessModifier.Public", FTextBlockStyle().SetFont(DEFAULT_FONT("Bold", 9) ));
		Set("BlueprintEditor.AccessModifier.Default", FTextBlockStyle().SetFont(DEFAULT_FONT("Regular", 9)));
	}

	// Kismet linear expression display
	{
		Set( "KismetExpression.ReadVariable.Body", new BOX_BRUSH( "/Graph/Linear_VarNode_Background", FMargin(16.f/64.f, 12.f/28.f) ) );
		Set( "KismetExpression.ReadVariable", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 9 ) ) );
		Set( "KismetExpression.ReadVariable.Gloss", new BOX_BRUSH( "/Graph/Linear_VarNode_Gloss", FMargin(16.f/64.f, 12.f/28.f) ) );
		
		Set( "KismetExpression.ReadAutogeneratedVariable.Body", new BOX_BRUSH( "/Graph/Linear_VarNode_Background", FMargin(16.f/64.f, 12.f/28.f) ) );
		Set( "KismetExpression.ReadAutogeneratedVariable", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 9 ) ) );

		Set( "KismetExpression.OperatorNode", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "BoldCondensed", 20 ) ) );
		Set( "KismetExpression.FunctionNode", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Bold", 10 ) ) );
		Set( "KismetExpression.LiteralValue", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Bold", 10 ) ) );

	}

	//Find Results
	{
		Set("FindResults.FindInBlueprints", FTextBlockStyle(NormalText)
			.SetFont(ICON_FONT(10))
			.SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f))
		);
	}

	//Bookmarks
	{
		Set("Bookmarks.AddFolderButtonIcon", new IMAGE_BRUSH("Icons/icon_AddFolder_16x", Icon16x16));
		Set("Bookmarks.TreeViewItemFont", DEFAULT_FONT("Fonts/Roboto-Regular", 10));
		Set("Bookmarks.TreeViewRootItemFont", DEFAULT_FONT("Fonts/Roboto-Regular", 11));
	}

	//Blueprint Diff
	{
		Set("BlueprintDif.HasGraph", new IMAGE_BRUSH_SVG("Starship/Blueprints/blueprint_Dif_has_graph", Icon10x10));
		Set("BlueprintDif.MissingGraph", new IMAGE_BRUSH_SVG("Starship/Blueprints/blueprint_Dif_missing_graph", Icon8x8));
		Set("BlueprintDif.NextDiff", new IMAGE_BRUSH_SVG("/Starship/Blueprints/diff_next", Icon16x16));
		Set("BlueprintDif.PrevDiff", new IMAGE_BRUSH_SVG("/Starship/Blueprints/diff_prev", Icon16x16));
		Set("BlueprintDif.HorizontalDiff", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_horizontal_diff_view", Icon16x16));
		Set("BlueprintDif.VerticalDiff", new IMAGE_BRUSH_SVG("Starship/Blueprints/icon_vertical_diff_view", Icon16x16));

		Set("BlueprintDif.ItalicText", 
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Italic", 10))
			.SetColorAndOpacity(FLinearColor(.7f, .7f, .7f))
		);
	}

	//Blueprint Merge
	{
		Set("BlueprintMerge.NextDiff", new IMAGE_BRUSH_SVG("Starship/Blueprints/diff_next", Icon16x16));
		Set("BlueprintMerge.PrevDiff", new IMAGE_BRUSH_SVG("Starship/Blueprints/diff_prev", Icon16x16));

		Set("BlueprintMerge.Finish", new IMAGE_BRUSH("/Icons/LV_Save", Icon16x16));

		Set("BlueprintMerge.Cancel", new IMAGE_BRUSH("/Icons/LV_Remove", Icon16x16));

		Set("BlueprintMerge.AcceptSource", new IMAGE_BRUSH("/Icons/AcceptMergeSource_40x", Icon16x16));

		Set("BlueprintMerge.AcceptTarget", new IMAGE_BRUSH("/Icons/AcceptMergeTarget_40x", Icon16x16));

		Set("BlueprintMerge.StartMerge", new IMAGE_BRUSH("/Icons/StartMerge_42x", Icon16x16));

	}

	// Play in editor / play in world
	{
		FLinearColor GreenHSV = FStyleColors::AccentGreen.GetSpecifiedColor().LinearRGBToHSV();
		
		FLinearColor GreenHover = FLinearColor(GreenHSV.R, GreenHSV.G * .5, GreenHSV.B, GreenHSV.A).HSVToLinearRGB();
		FLinearColor GreenPress = FLinearColor(GreenHSV.R, GreenHSV.G, GreenHSV.B*.5, GreenHSV.A).HSVToLinearRGB(); 

		FToolBarStyle PlayToolbar = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("AssetEditorToolbar");

		const FButtonStyle PlayToolbarButton = FButtonStyle(PlayToolbar.ButtonStyle)
			.SetNormalForeground(FStyleColors::AccentGreen)
			.SetPressedForeground(GreenPress)
			.SetHoveredForeground(GreenHover);

		FComboButtonStyle PlayToolbarComboButton = FComboButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
			.SetContentPadding(0)
			.SetDownArrowImage(CORE_IMAGE_BRUSH_SVG("Starship/Common/ellipsis-vertical-narrow", FVector2D(6, 24)));
		PlayToolbarComboButton.ButtonStyle = PlayToolbarButton;

		PlayToolbar.SetButtonStyle(PlayToolbarButton);
		PlayToolbar.SetSettingsComboButtonStyle(PlayToolbarComboButton);
		PlayToolbar.SetShowLabels(true);

		Set("PlayToolBar", PlayToolbar);

		Set("PlayWorld.Simulate", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));
		Set( "PlayWorld.RepeatLastPlay", new IMAGE_BRUSH_SVG("Starship/MainToolbar/play", Icon20x20));
		Set( "PlayWorld.PlayInViewport", new IMAGE_BRUSH_SVG("Starship/MainToolbar/play", Icon20x20));

		Set( "PlayWorld.PlayInEditorFloating", new IMAGE_BRUSH_SVG("Starship/MainToolbar/PlayNewEditorWindow", Icon20x20));
		Set( "PlayWorld.PlayInVR", new IMAGE_BRUSH_SVG("Starship/MainToolbar/PlayVRPreview", Icon20x20));
		Set( "PlayWorld.PlayInMobilePreview", new IMAGE_BRUSH_SVG("Starship/MainToolbar/PlayMobilePreview", Icon20x20));
		Set( "PlayWorld.PlayInVulkanPreview", new IMAGE_BRUSH_SVG("Starship/MainToolbar/PlayMobilePreview", Icon20x20));
		Set( "PlayWorld.PlayInNewProcess", new IMAGE_BRUSH_SVG("Starship/MainToolbar/PlayStandaloneGame", Icon20x20));

		Set( "PlayWorld.RepeatLastLaunch", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/launch", Icon20x20 ) );

		Set( "PlayWorld.PlayInCameraLocation", new IMAGE_BRUSH_SVG( "Starship/AssetIcons/CameraActor_16", Icon20x20 ) );
		Set( "PlayWorld.PlayInDefaultPlayerStart", new IMAGE_BRUSH_SVG( "Starship/AssetIcons/PlayerStart_16", Icon20x20 ) );

		Set("PlayWorld.ResumePlaySession", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon40x40));
		Set("PlayWorld.ResumePlaySession.Small", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));
		Set( "PlayWorld.PausePlaySession", new IMAGE_BRUSH_SVG("Starship/MainToolbar/pause", Icon40x40));
		Set( "PlayWorld.PausePlaySession.Small", new IMAGE_BRUSH_SVG("Starship/MainToolbar/pause", Icon20x20));
		Set( "PlayWorld.SingleFrameAdvance", new IMAGE_BRUSH( "Icons/icon_advance_40x", Icon40x40 ) );
		Set( "PlayWorld.SingleFrameAdvance.Small", new IMAGE_BRUSH( "Icons/icon_advance_40x", Icon20x20 ) );

		Set( "PlayWorld.StopPlaySession", new IMAGE_BRUSH_SVG("Starship/MainToolbar/stop", Icon40x40));
		Set( "PlayWorld.StopPlaySession.Small", new IMAGE_BRUSH_SVG("Starship/MainToolbar/stop", Icon20x20));

		Set("PlayWorld.LateJoinSession", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon40x40));
		Set("PlayWorld.LateJoinSession.Small", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));

		Set( "PlayWorld.PossessPlayer", new IMAGE_BRUSH_SVG("Starship/AssetIcons/PlayerController_16", Icon20x20));
		Set( "PlayWorld.EjectFromPlayer", new IMAGE_BRUSH_SVG("Starship/MainToolbar/eject", Icon40x40));

		Set( "PlayWorld.ShowCurrentStatement", new IMAGE_BRUSH( "Icons/icon_findnode_40x", Icon40x40 ) );
		Set( "PlayWorld.ShowCurrentStatement.Small", new IMAGE_BRUSH( "Icons/icon_findnode_40x", Icon20x20 ) );
		Set( "PlayWorld.StepOut", new IMAGE_BRUSH("Icons/icon_DebugStepOut_40x", Icon40x40));
		Set( "PlayWorld.StepOut.Small", new IMAGE_BRUSH("Icons/icon_DebugStepOut_40x", Icon20x20));
		Set( "PlayWorld.StepInto", new IMAGE_BRUSH( "Icons/icon_DebugStepIn_40x", Icon40x40 ) );
		Set( "PlayWorld.StepInto.Small", new IMAGE_BRUSH( "Icons/icon_DebugStepIn_40x", Icon20x20 ) );
		Set( "PlayWorld.StepOver", new IMAGE_BRUSH( "Icons/icon_DebugStepOver_40x", Icon40x40 ) );
		Set( "PlayWorld.StepOver.Small", new IMAGE_BRUSH("Icons/icon_DebugStepOver_40x", Icon20x20));
	}


	// Kismet 2 debugger
	{
		Set( "Kismet.Breakpoint.Disabled", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Disabled", Icon16x16, FStyleColors::AccentRed ) );
		Set( "Kismet.Breakpoint.EnabledAndInvalid", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Invalid", Icon16x16, FStyleColors::AccentRed) );
		Set( "Kismet.Breakpoint.EnabledAndValid", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Valid", Icon16x16, FStyleColors::AccentRed) );
		Set( "Kismet.Breakpoint.MixedStatus", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Mixed", Icon16x16, FStyleColors::AccentRed) );
		
		Set( "Kismet.WatchIcon", new IMAGE_BRUSH_SVG( "Starship/GraphEditors/WatchVariable", Icon16x16 ) );
		Set( "Kismet.LatentActionIcon", new IMAGE_BRUSH_SVG( "Starship/Common/Timecode", Icon16x16 ) );

		Set( "Kismet.Trace.CurrentIndex", new IMAGE_BRUSH_SVG( "Starship/GraphEditors/NextArrow", Icon16x16 ) );
		Set( "Kismet.Trace.PreviousIndex", new IMAGE_BRUSH_SVG( "Starship/GraphEditors/PreviousArrow", Icon16x16 ) );

		Set( "Kismet.DebuggerOverlay.Breakpoint.Disabled", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Disabled", Icon32x32, FStyleColors::AccentRed) );
		Set( "Kismet.DebuggerOverlay.Breakpoint.EnabledAndInvalid", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Invalid", Icon32x32, FStyleColors::AccentRed) );
		Set( "Kismet.DebuggerOverlay.Breakpoint.EnabledAndValid", new IMAGE_BRUSH_SVG( "Starship/Blueprints/Breakpoint_Valid", Icon32x32, FStyleColors::AccentRed) );

		Set("Kismet.DebuggerOverlay.InstructionPointer", new IMAGE_BRUSH_SVG( "Starship/Blueprints/IP_Breakpoint", FVector2D(128,96)) );
		Set("Kismet.DebuggerOverlay.InstructionPointerBreakpoint", new IMAGE_BRUSH_SVG( "Starship/Blueprints/IP_Breakpoint", FVector2D(128,96), FStyleColors::AccentRed) );

		Set("Kismet.CallStackViewer.CurrentStackFrame", new IMAGE_BRUSH_SVG( "Starship/Blueprints/DebuggerArrow", Icon12x12 ));
		Set("Kismet.CallStackViewer.CurrentStackFrameColor", FLinearColor(0.728f, 0.364f, 0.003f) );
		Set("Kismet.CallStackViewer.LastStackFrameNavigatedToColor", FLinearColor( 0.4f, 0.5f, 0.7f, 1.0f ) );
	}

	// Asset context menu
	{
		Set("Persona.AssetActions.CreateAnimAsset", new IMAGE_BRUSH_SVG("Starship/Persona/PersonaCreateAsset", Icon20x20));
		Set("Persona.AssetActions.ReimportAnim", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));
		Set("Persona.AssetActions.Retarget", new IMAGE_BRUSH(TEXT("Icons/icon_Persona_Retarget_16x"), Icon16x16));
		Set("Persona.AssetActions.RetargetSkeleton", new IMAGE_BRUSH(TEXT("Icons/icon_Animation_Retarget_Skeleton_16x"), Icon16x16));
		Set("Persona.AssetActions.FindSkeleton", new IMAGE_BRUSH(TEXT("Icons/icon_Genericfinder_16x"), Icon16x16));
		Set("Persona.AssetActions.DuplicateAndRetargetSkeleton", new IMAGE_BRUSH(TEXT("Icons/icon_Animation_Duplicate_Retarget_Skeleton_16x"), Icon16x16));
		Set("Persona.AssetActions.AssignSkeleton", new IMAGE_BRUSH(TEXT("Icons/icon_Animation_Assign_Skeleton_16x"), Icon16x16));
	}

	// Blend space colors
	{
		Set("BlendSpaceKey.Regular", DefaultForeground);
		Set("BlendSpaceKey.Highlight", SelectionColor);
		Set("BlendSpaceKey.Pressed", SelectionColor_Pressed);
		Set("BlendSpaceKey.Drag", SelectionColor_Subdued);
		Set("BlendSpaceKey.Drop", SelectionColor_Inactive);
		Set("BlendSpaceKey.Invalid", LogColor_Error);
		Set("BlendSpaceKey.Preview", LogColor_Command);
		Set("BlendSpaceKey.UnSnapped", LogColor_Warning);
	}

	// Custom menu style for recent commands list
	{
		Set( "PinnedCommandList.Background", new BOX_BRUSH( "Common/RoundedSelection_16x", FMargin(4.0f/16.0f), FLinearColor( 0.2f, 0.2f, 0.2f, 0.2f ) ) );
		Set( "PinnedCommandList.Icon", new IMAGE_BRUSH( "Icons/icon_tab_toolbar_16px", Icon16x16 ) );
		Set( "PinnedCommandList.Expand", new IMAGE_BRUSH( "Icons/toolbar_expand_16x", Icon16x16) );
		Set( "PinnedCommandList.SubMenuIndicator", new IMAGE_BRUSH( "Common/SubmenuArrow", Icon8x8 ) );
		Set( "PinnedCommandList.SToolBarComboButtonBlock.Padding", FMargin(4.0f));
		Set( "PinnedCommandList.SToolBarButtonBlock.Padding", FMargin(4.0f));
		Set( "PinnedCommandList.SToolBarCheckComboButtonBlock.Padding", FMargin(4.0f));
		Set( "PinnedCommandList.SToolBarButtonBlock.CheckBox.Padding", FMargin(0.0f) );
		Set( "PinnedCommandList.SToolBarComboButtonBlock.ComboButton.Color", DefaultForeground );

		Set( "PinnedCommandList.Block.IndentedPadding", FMargin( 0.0f, 0.0f, 0.0f, 0.0f ) );
		Set( "PinnedCommandList.Block.Padding", FMargin( 0.0f, 0.0f, 0.0f, 0.0f ) );

		Set( "PinnedCommandList.Separator", new BOX_BRUSH( "Old/Button", 4.0f/32.0f ) );
		Set( "PinnedCommandList.Separator.Padding", FMargin( 0.5f ) );

		Set( "PinnedCommandList.Label", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 9 ) ) );
		Set( "PinnedCommandList.EditableText", FEditableTextBoxStyle(NormalEditableTextBoxStyle) .SetFont( DEFAULT_FONT( "Regular", 9 ) ) );
		Set( "PinnedCommandList.Keybinding", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 8 ) ) );

		Set( "PinnedCommandList.Heading", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT( "Regular", 8))
			.SetColorAndOpacity(FLinearColor(0.4f, 0.4, 0.4f, 1.0f)));

		/* Set images for various SCheckBox states associated with menu check box items... */
		const FCheckBoxStyle BasicMenuCheckBoxStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/SmallCheckBox", Icon14x14 ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/SmallCheckBox_Hovered", Icon14x14 ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Hovered", Icon14x14, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Checked", Icon14x14 ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/SmallCheckBox_Checked_Hovered", Icon14x14 ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Checked_Hovered", Icon14x14, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined", Icon14x14 ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined_Hovered", Icon14x14 ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined_Hovered", Icon14x14, FLinearColor( 0.5f, 0.5f, 0.5f ) ) );
 
		/* ...and add the new style */
		Set( "PinnedCommandList.CheckBox", BasicMenuCheckBoxStyle );
						
		/* Read-only checkbox that appears next to a menu item */
		/* Set images for various SCheckBox states associated with read-only menu check box items... */
		const FCheckBoxStyle BasicMenuCheckStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Icons/Empty_14x", Icon14x14 ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Icons/Empty_14x", Icon14x14 ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Hovered", Icon14x14 ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/SmallCheck", Icon14x14 ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/SmallCheck", Icon14x14 ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheck", Icon14x14 ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Icons/Empty_14x", Icon14x14 ) )
			.SetUndeterminedHoveredImage( FSlateNoResource() )
			.SetUndeterminedPressedImage( FSlateNoResource() );

		/* ...and add the new style */
		Set( "PinnedCommandList.Check", BasicMenuCheckStyle );

		/* This radio button is actually just a check box with different images */
		/* Set images for various Menu radio button (SCheckBox) states... */
		const FCheckBoxStyle BasicMenuRadioButtonStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16 ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, SelectionColor ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, SelectionColor_Pressed ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, SelectionColor ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, SelectionColor_Pressed ) );

		/* ...and set new style */
		Set( "PinnedCommandList.RadioButton", BasicMenuRadioButtonStyle );

		/* Create style for "Menu.ToggleButton" widget ... */
		const FCheckBoxStyle MenuToggleButtonCheckBoxStyle = FCheckBoxStyle()
			.SetCheckBoxType( ESlateCheckBoxType::ToggleButton )
			.SetUncheckedImage( FSlateNoResource() )
			.SetUncheckedPressedImage( BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor_Pressed ) )
			.SetUncheckedHoveredImage( BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor ) )
			.SetCheckedImage( BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) )
			.SetCheckedHoveredImage( BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) )
			.SetCheckedPressedImage( BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor ) );
		/* ... and add new style */
		Set( "PinnedCommandList.ToggleButton", MenuToggleButtonCheckBoxStyle );

		Set( "PinnedCommandList.Button", FButtonStyle( NoBorder )
			.SetNormal ( FSlateNoResource() )
			.SetPressed( BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor_Pressed ) )
			.SetHovered( BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor ) )
			.SetNormalPadding( FMargin(0,1) )
			.SetPressedPadding( FMargin(0,2,0,0) )
		);

		Set( "PinnedCommandList.Button.Checked", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) );
		Set( "PinnedCommandList.Button.Checked_Hovered", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) );
		Set( "PinnedCommandList.Button.Checked_Pressed", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor ) );

		/* The style of a menu bar button when it has a sub menu open */
		Set( "PinnedCommandList.Button.SubMenuOpen", new BORDER_BRUSH( "Common/Selection", FMargin(4.f/16.f), FLinearColor(0.10f, 0.10f, 0.10f) ) );
	}

	{
		Set( "ViewportPinnedCommandList.Background", new FSlateNoResource() );
		Set( "ViewportPinnedCommandList.Icon", new IMAGE_BRUSH( "Icons/icon_tab_toolbar_16px", Icon16x16 ) );
		Set( "ViewportPinnedCommandList.Expand", new IMAGE_BRUSH( "Icons/toolbar_expand_16x", Icon16x16) );
		Set( "ViewportPinnedCommandList.SubMenuIndicator", new IMAGE_BRUSH( "Common/SubmenuArrow", Icon8x8 ) );
		Set( "ViewportPinnedCommandList.SToolBarComboButtonBlock.Padding", FMargin(4.0f));
		Set( "ViewportPinnedCommandList.SToolBarButtonBlock.Padding", FMargin(4.0f));
		Set( "ViewportPinnedCommandList.SToolBarCheckComboButtonBlock.Padding", FMargin(4.0f));
		Set( "ViewportPinnedCommandList.SToolBarButtonBlock.CheckBox.Padding", FMargin(0.0f) );
		Set( "ViewportPinnedCommandList.SToolBarComboButtonBlock.ComboButton.Color", DefaultForeground );

		Set( "ViewportPinnedCommandList.Block.IndentedPadding", FMargin( 0.0f, 0.0f, 0.0f, 0.0f ) );
		Set( "ViewportPinnedCommandList.Block.Padding", FMargin( 0.0f, 0.0f, 0.0f, 0.0f ) );

		Set( "ViewportPinnedCommandList.Separator", new BOX_BRUSH( "Old/Button", 4.0f/32.0f ) );
		Set( "ViewportPinnedCommandList.Separator.Padding", FMargin( 0.5f ) );

		Set( "ViewportPinnedCommandList.Label", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Bold", 9 ) ).SetColorAndOpacity(FLinearColor::Black) );
		Set( "ViewportPinnedCommandList.EditableText", FEditableTextBoxStyle(NormalEditableTextBoxStyle) .SetFont( DEFAULT_FONT( "Bold", 9 ) ).SetForegroundColor(FLinearColor::Black) );
		Set( "ViewportPinnedCommandList.Keybinding", FTextBlockStyle(NormalText) .SetFont( DEFAULT_FONT( "Regular", 8 ) ).SetColorAndOpacity(FLinearColor::Gray) );

		Set( "ViewportPinnedCommandList.Heading", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT( "Regular", 8))
			.SetColorAndOpacity(FLinearColor(0.4f, 0.4, 0.4f, 1.0f)));

		/* Set images for various SCheckBox states associated with menu check box items... */
		const FCheckBoxStyle BasicMenuCheckBoxStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/SmallCheckBox", Icon14x14, FLinearColor::Black ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/SmallCheckBox_Hovered", Icon14x14, FLinearColor::Black ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Hovered", Icon14x14, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Checked", Icon14x14, FLinearColor::Black ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/SmallCheckBox_Checked_Hovered", Icon14x14, FLinearColor::Black ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Checked_Hovered", Icon14x14, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined", Icon14x14, FLinearColor::Black ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined_Hovered", Icon14x14, FLinearColor::Black ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/CheckBox_Undetermined_Hovered", Icon14x14, FLinearColor( 0.5f, 0.5f, 0.5f ) ) )
			.SetPadding(FMargin(2.0f))
			.SetForegroundColor(FLinearColor::Black);
 
		/* ...and add the new style */
		Set( "ViewportPinnedCommandList.CheckBox", BasicMenuCheckBoxStyle );
						
		/* Read-only checkbox that appears next to a menu item */
		/* Set images for various SCheckBox states associated with read-only menu check box items... */
		const FCheckBoxStyle BasicMenuCheckStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Icons/Empty_14x", Icon14x14, FLinearColor::Black ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Icons/Empty_14x", Icon14x14, FLinearColor::Black ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheckBox_Hovered", Icon14x14, FLinearColor::Black ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/SmallCheck", Icon14x14, FLinearColor::Black ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/SmallCheck", Icon14x14, FLinearColor::Black ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/SmallCheck", Icon14x14, FLinearColor::Black ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Icons/Empty_14x", Icon14x14, FLinearColor::Black ) )
			.SetUndeterminedHoveredImage( FSlateNoResource() )
			.SetUndeterminedPressedImage( FSlateNoResource() )
			.SetPadding(FMargin(2.0f))
			.SetForegroundColor(FLinearColor::Black);

		/* ...and add the new style */
		Set( "ViewportPinnedCommandList.Check", BasicMenuCheckStyle );

		/* This radio button is actually just a check box with different images */
		/* Set images for various Menu radio button (SCheckBox) states... */
		const FCheckBoxStyle BasicMenuRadioButtonStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16 ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16 ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16 ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16 ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, SelectionColor ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, SelectionColor_Pressed ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16 ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, SelectionColor ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, SelectionColor_Pressed ) )
			.SetPadding(FMargin(2.0f))
			.SetForegroundColor(FLinearColor::Black);

		/* ...and set new style */
		Set( "ViewportPinnedCommandList.RadioButton", BasicMenuRadioButtonStyle );

		/* Create style for "Menu.ToggleButton" widget ... */
		const FCheckBoxStyle MenuToggleButtonCheckBoxStyle = FCheckBoxStyle()
			.SetCheckBoxType( ESlateCheckBoxType::ToggleButton )
			.SetUncheckedImage( FSlateNoResource() )
			.SetUncheckedPressedImage( BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor_Pressed ) )
			.SetUncheckedHoveredImage( BOX_BRUSH( "Common/RoundedSelection_16x", 4.0f/16.0f, SelectionColor ) )
			.SetCheckedImage( BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) )
			.SetCheckedHoveredImage( BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) )
			.SetCheckedPressedImage( BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor ) )
			.SetPadding(FMargin(2.0f))
			.SetForegroundColor(FLinearColor::Black);
		/* ... and add new style */
		Set( "ViewportPinnedCommandList.ToggleButton", MenuToggleButtonCheckBoxStyle );

		const FButtonStyle ViewportBaseButtonStyle = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateNoResource())
			.SetPressed(FSlateNoResource())
			.SetNormalPadding(FMargin(2, 2, 2, 3))
			.SetPressedPadding(FMargin(2, 3, 2, 2));

		Set( "ViewportPinnedCommandList.Button", FButtonStyle(ViewportBaseButtonStyle)
			.SetNormal(BOX_BRUSH( "Common/SmallRoundedButton", FMargin(7.f/16.f), FLinearColor(1,1,1,0.75f)))
			.SetHovered(BOX_BRUSH( "Common/SmallRoundedButton", FMargin(7.f/16.f), FLinearColor(1,1,1, 1.0f)))
			.SetPressed(BOX_BRUSH( "Common/SmallRoundedButton", FMargin(7.f/16.f)))
			.SetNormalPadding( FMargin(2,3) )
			.SetPressedPadding( FMargin(2,4,2,2) )
		);

		Set( "ViewportPinnedCommandList.Button.Checked", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) );
		Set( "ViewportPinnedCommandList.Button.Checked_Hovered", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor_Pressed ) );
		Set( "ViewportPinnedCommandList.Button.Checked_Pressed", new BOX_BRUSH( "Common/RoundedSelection_16x",  4.0f/16.0f, SelectionColor ) );

		/* The style of a menu bar button when it has a sub menu open */
		Set( "ViewportPinnedCommandList.Button.SubMenuOpen", new BORDER_BRUSH( "Common/Selection", FMargin(4.f/16.f), FLinearColor(0.10f, 0.10f, 0.10f) ) );

		Set( "ViewportPinnedCommandList.ComboButton", FComboButtonStyle()
			.SetButtonStyle(ViewportBaseButtonStyle)
			.SetDownArrowImage(IMAGE_BRUSH("Common/ComboArrow", Icon8x8))
			.SetMenuBorderBrush(BOX_BRUSH("Old/Menu_Background", FMargin(8.0f/64.0f)))
			.SetMenuBorderPadding(FMargin(0.0f))
		);
	}

	// Animation blueprint
	{
		Set("AnimGraph.Attribute.Border.Solid", new FSlateRoundedBoxBrush(FStyleColors::White));
		Set("AnimGraph.Attribute.Border.Outline", new FSlateRoundedBoxBrush(FStyleColors::Transparent, FStyleColors::White, 1.0f));
		Set("AnimGraph.Attribute.DefaultColor", FStyleColors::AccentGray);

		FSlateColor AttributeTextColor = FStyleColors::AccentBlack;
		Set("AnimGraph.Attribute.TextColor", AttributeTextColor);
		Set("AnimGraph.Attribute.Text", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", FStarshipCoreStyle::SmallTextSize))
			.SetColorAndOpacity(AttributeTextColor));

		Set("AnimGraph.Attribute.Pose.Color", FStyleColors::White);
		Set("AnimGraph.Attribute.Curves.Icon", new IMAGE_BRUSH("GenericCurveEditor/Icons/GenericCurveEditor_48x", Icon10x10));
		Set("AnimGraph.Attribute.Curves.Color", FStyleColors::AccentPurple);
		Set("AnimGraph.Attribute.Attributes.Icon", new IMAGE_BRUSH("Icons/pillarray_16x", Icon10x10));
		Set("AnimGraph.Attribute.Attributes.Color", FStyleColors::AccentYellow);
		Set("AnimGraph.Attribute.Sync.Icon", new IMAGE_BRUSH("Icons/icon_tab_TimecodeProvider_16x", Icon10x10));
		Set("AnimGraph.Attribute.Sync.Color", FStyleColors::AccentBlue);
		Set("AnimGraph.Attribute.InertialBlending.Icon", new IMAGE_BRUSH("Icons/icon_tab_TimecodeProvider_16x", Icon10x10));
		Set("AnimGraph.Attribute.InertialBlending.Color", FStyleColors::AccentOrange);
	}	
#endif // WITH_EDITOR
}

void FStarshipEditorStyle::FStyle::SetupClassThumbnailOverlays()
{
	Set("ClassThumbnailOverlays.SkeletalMesh_NeedSkinning", new IMAGE_BRUSH("Icons/AssetIcons/SkeletalMeshNeedSkinning_16x", Icon16x16));
}

void FStarshipEditorStyle::FStyle::SetupClassIconsAndThumbnails()
{
#if WITH_EDITOR
	// Actor Classes Outliner
	{
		struct FClassIconInfo
		{
			FClassIconInfo(const TCHAR* InType, bool bInHas64Size = true)
				: Type(InType)
				, bHas64Size(bInHas64Size)
			{}

			const TCHAR* Type;
			bool bHas64Size;
		};

		Set("ClassIcon.Light", new IMAGE_BRUSH("Icons/ActorIcons/LightActor_16x", Icon16x16));
		Set("ClassIcon.BrushAdditive", new IMAGE_BRUSH("Icons/ActorIcons/Brush_Add_16x", Icon16x16));
		Set("ClassIcon.BrushSubtractive", new IMAGE_BRUSH("Icons/ActorIcons/Brush_Subtract_16x", Icon16x16));
		Set("ClassIcon.Deleted", new IMAGE_BRUSH("Icons/ActorIcons/DeletedActor_16px", Icon16x16));

		// Component classes
		Set("ClassIcon.AudioComponent", new IMAGE_BRUSH("Icons/ActorIcons/SoundActor_16x", Icon16x16));
		Set("ClassIcon.BlueprintCore", new IMAGE_BRUSH("Icons/AssetIcons/Blueprint_16x", Icon16x16));
		Set("ClassIcon.LightComponent", new IMAGE_BRUSH("Icons/ActorIcons/LightActor_16x", Icon16x16));
		Set("ClassIcon.ArrowComponent", new IMAGE_BRUSH("Icons/ActorIcons/Arrow_16px", Icon16x16));
		Set("ClassIcon.BoxComponent", new IMAGE_BRUSH("Icons/ActorIcons/Box_16px", Icon16x16));
		Set("ClassIcon.CapsuleComponent", new IMAGE_BRUSH("Icons/ActorIcons/Capsule_16px", Icon16x16));
		Set("ClassIcon.InstancedStaticMeshComponent", new IMAGE_BRUSH("Icons/ActorIcons/InstancedStaticMesh_16px", Icon16x16));
		Set("ClassIcon.MaterialBillboardComponent", new IMAGE_BRUSH("Icons/ActorIcons/MaterialSprite_16px", Icon16x16));
		Set("ClassIcon.SphereComponent", new IMAGE_BRUSH("Icons/ActorIcons/Sphere_16px", Icon16x16));
		Set("ClassIcon.SplineComponent", new IMAGE_BRUSH("Icons/ActorIcons/Spline_16px", Icon16x16));
		Set("ClassIcon.BillboardComponent", new IMAGE_BRUSH("Icons/ActorIcons/SpriteComponent_16px", Icon16x16));
		Set("ClassIcon.TimelineComponent", new IMAGE_BRUSH("Icons/ActorIcons/TimelineComponent_16px", Icon16x16));
		Set("ClassIcon.ChildActorComponent", new IMAGE_BRUSH("Icons/ActorIcons/ChildActorComponent_16px", Icon16x16));

		Set("ClassIcon.AtmosphericFogComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AtmosphericFog_16", Icon16x16));
		Set("ClassIcon.BrushComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/Brush_16", Icon16x16));
		Set("ClassIcon.CableComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/CableActor_16", Icon16x16));
		Set("ClassIcon.CameraComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/CameraActor_16", Icon16x16));
		Set("ClassIcon.DecalComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/DecalActor_16", Icon16x16));
		Set("ClassIcon.DirectionalLightComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/DirectionalLight_16", Icon16x16));
		Set("ClassIcon.ExponentialHeightFogComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/ExponentialHeightFog_16", Icon16x16));
		Set("ClassIcon.ForceFeedbackComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/ForceFeedbackEffect_16", Icon16x16));
		Set("ClassIcon.LandscapeComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/Landscape_16", Icon16x16));
		Set("ClassIcon.ParticleSystemComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/Emitter_16", Icon16x16));
		Set("ClassIcon.PlanarReflectionComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/PlaneReflectionCapture_16", Icon16x16));
		Set("ClassIcon.PointLightComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/PointLight_16", Icon16x16));
		Set("ClassIcon.RectLightComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/RectLight_16", Icon16x16));
		Set("ClassIcon.RadialForceComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/RadialForceActor_16", Icon16x16));
		Set("ClassIcon.SceneCaptureComponent2D", new IMAGE_BRUSH_SVG("Starship/AssetIcons/SceneCapture2D_16", Icon16x16));
		Set("ClassIcon.SceneCaptureComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/SphereReflectionCapture_16", Icon16x16));
		Set("ClassIcon.SingleAnimSkeletalComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/SkeletalMesh_16", Icon16x16));
		Set("ClassIcon.SkyAtmosphereComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/SkyAtmosphere_16", Icon16x16));
		Set("ClassIcon.SkeletalMeshComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/SkeletalMesh_16", Icon16x16));
		Set("ClassIcon.SpotLightComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/SpotLight_16", Icon16x16));
		Set("ClassIcon.StaticMeshComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/StaticMesh_16", Icon16x16));
		Set("ClassIcon.TextRenderComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/TextRenderActor_16", Icon16x16));
		Set("ClassIcon.VectorFieldComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/VectorFieldVolume_16", Icon16x16));
		Set("ClassIcon.VolumetricCloudComponent", new IMAGE_BRUSH_SVG("Starship/AssetIcons/VolumetricCloud_16", Icon16x16));

		Set("ClassIcon.MovableMobilityIcon", new IMAGE_BRUSH("Icons/ActorIcons/Light_Movable_16x", Icon16x16));
		Set("ClassIcon.StationaryMobilityIcon", new IMAGE_BRUSH("Icons/ActorIcons/Light_Adjustable_16x", Icon16x16));
		Set("ClassIcon.ComponentMobilityHeaderIcon", new IMAGE_BRUSH("Icons/ActorIcons/ComponentMobilityHeader_7x16", Icon7x16));

		// Asset Type Classes
		const TCHAR* AssetTypes[] = {

			TEXT("AbilitySystemComponent"),
			TEXT("AIPerceptionComponent"),
			TEXT("BlueprintInterface"),
			TEXT("BlueprintMacroLibrary"),
			TEXT("CameraAnim"),
			TEXT("Default"),
			TEXT("DirectionalLightMovable"),
			TEXT("DirectionalLightStatic"),
			TEXT("DirectionalLightStationary"),
			TEXT("Font"),
			TEXT("FontFace"),
			TEXT("ForceFeedbackEffect"),
			TEXT("InterpData"),
			TEXT("LevelSequence"),
			TEXT("LightmassCharacterIndirectDetailVolume"),
			TEXT("MassiveLODOverrideVolume"),
			TEXT("MaterialParameterCollection"),
			TEXT("MatineeActor"),
			TEXT("MultiFont"),
			TEXT("ParticleSystem"),
			TEXT("PhysicsConstraintComponent"),
			TEXT("PhysicsThrusterComponent"),
			TEXT("SkyLightComponent"),
			TEXT("SlateWidgetStyleAsset"),
			TEXT("StringTable"),
			TEXT("SpotLightMovable"),
			TEXT("SpotLightStatic"),
			TEXT("SpotLightStationary"),
			TEXT("UserDefinedEnum"),
			TEXT("Cube"),
			TEXT("Sphere"),
			TEXT("Cylinder"),
			TEXT("Cone"),
			TEXT("Plane"),
			TEXT("FoliageType_Actor"),
		};

		for (int32 TypeIndex = 0; TypeIndex < UE_ARRAY_COUNT(AssetTypes); ++TypeIndex)
		{
			const TCHAR* Type = AssetTypes[TypeIndex];
			Set( *FString::Printf(TEXT("ClassIcon.%s"), Type),		new IMAGE_BRUSH(FString::Printf(TEXT("Icons/AssetIcons/%s_%dx"), Type, 16), Icon16x16 ) );
			Set( *FString::Printf(TEXT("ClassThumbnail.%s"), Type),	new IMAGE_BRUSH(FString::Printf(TEXT("Icons/AssetIcons/%s_%dx"), Type, 64), Icon64x64 ) );
		}

		const FClassIconInfo AssetTypesSVG[] = {
			{TEXT("Actor")},
			{TEXT("ActorComponent")},
			{TEXT("AIController")},
			{TEXT("AimOffsetBlendSpace")},
			{TEXT("AimOffsetBlendSpace1D")},
			{TEXT("AmbientSound")},
			{TEXT("AnimationModifier")},
			{TEXT("AnimationSharingSetup")},
			{TEXT("AnimBlueprint")},
			{TEXT("AnimComposite")},
			{TEXT("AnimInstance")},
			{TEXT("AnimLayerInterface")},
			{TEXT("AnimMontage")},
			{TEXT("AnimSequence")},
			{TEXT("ApplicationLifecycleComponent")},
			{TEXT("AtmosphericFog")},
			{TEXT("AudioVolume")},
			{TEXT("BehaviorTree")},
			{TEXT("BlackboardData")},
			{TEXT("BlendSpace")},
			{TEXT("BlendSpace1D")},
			{TEXT("BlockingVolume")},
			{TEXT("Blueprint")},
			{TEXT("BlueprintFunctionLibrary")},
			{TEXT("BoxReflectionCapture")},
			{TEXT("Brush")},
			{TEXT("ButtonStyleAsset")},
			{TEXT("CableActor")},
			{TEXT("CameraActor")},
			{TEXT("CameraBlockingVolume")},
			{TEXT("CameraRig_Crane")},
			{TEXT("CameraRig_Rail")},
			{TEXT("Character")},
			{TEXT("CharacterMovementComponent")},
			{TEXT("CineCameraActor")},
			{TEXT("Class")},
			{TEXT("CompositingElement")},
			{TEXT("CullDistanceVolume")},
			{TEXT("CurveBase")},
			{TEXT("DataAsset")},
			{TEXT("DataTable")},
			{TEXT("DecalActor")},
			{TEXT("DefaultPawn")},
			{TEXT("DialogueVoice")},
			{TEXT("DialogueWave")},
			{TEXT("DirectionalLight")},
			{TEXT("DocumentationActor")},
			{TEXT("EditorTutorial")},
			{TEXT("EnvQuery")},
			{TEXT("Emitter")},
			{TEXT("EmptyActor")},
			{TEXT("ExponentialHeightFog")},
			{TEXT("FileMediaOutput")},
			{TEXT("FileMediaSource")},
			{TEXT("ForceFeedback")},
			{TEXT("GameModeBase")},
			{TEXT("GameStateBase")},
			{TEXT("GeometryCollection")},
			{TEXT("GroupActor")},
			{TEXT("HUD")},
			{TEXT("ImagePlate")},
			{TEXT("Interface")},
			{TEXT("KillZVolume")},
			{TEXT("Landscape")},
			{TEXT("LevelBounds")},
			{TEXT("LevelInstance")},
			{TEXT("LevelScriptActor")},
			{TEXT("LevelSequenceActor")},
			{TEXT("LevelStreamingVolume")},
			{TEXT("LightmassCharacterDetailIndirectVolume")},
			{TEXT("LightmassImportanceVolume")},
			{TEXT("LightmassVolume")},
			{TEXT("LiveLinkPreset")},
			{TEXT("Material")},
			{TEXT("MaterialFunction")},
			{TEXT("MaterialInstanceActor")},
			{TEXT("MaterialInstanceConstant")},
			{TEXT("MediaPlayer")},
			{TEXT("MediaTexture")},
			{TEXT("ModularSynthPresetBank")},
			{TEXT("NavLink")},
			{TEXT("NavLinkProxy")},
			{TEXT("NavMeshBoundsVolume")},
			{TEXT("NavModifierComponent")},
			{TEXT("NavModifierVolume")},
			{TEXT("Note")},
			{TEXT("Object")},
			{TEXT("ObjectLibrary")},
			{TEXT("PainCausingVolume")},
			{TEXT("Pawn")},
			{TEXT("PawnNoiseEmitterComponent")},
			{TEXT("PawnSensingComponent")},
			{TEXT("PhysicalMaterial")},
			{TEXT("PhysicsAsset")},
			{TEXT("PhysicsConstraintActor")},
			{TEXT("PhysicsHandleComponent")},
			{TEXT("PhysicsThruster")},
			{TEXT("PhysicsVolume")},
			{TEXT("PlanarReflectionCapture")},
			{TEXT("PlatformMediaSource")},
			{TEXT("PlayerController")},
			{TEXT("PlayerStart")},
			{TEXT("PointLight")},
			{TEXT("PoseAsset")},
			{TEXT("PostProcessVolume")},
			{TEXT("PrecomputedVisibilityOverrideVolume")},
			{TEXT("PrecomputedVisibilityVolume")},
			{TEXT("ProceduralFoliageBlockingVolume")},
			{TEXT("ProceduralFoliageVolume")},
			{TEXT("ProjectileMovementComponent")},
			{TEXT("RadialForceActor")},
			{TEXT("RectLight")},
			{TEXT("ReflectionCapture")},
			{TEXT("ReverbEffect")},
			{TEXT("RotatingMovementComponent")},
			{TEXT("SceneCapture2D")},
			{TEXT("SceneCaptureCube")},
			{TEXT("SceneComponent")},
			{TEXT("SkeletalMeshActor")},
			{TEXT("Skeleton")},
			{TEXT("SkyAtmosphere")},
			{TEXT("SkyLight")},
			{TEXT("SlateBrushAsset")},
			{TEXT("SoundAttenuation")},
			{TEXT("SoundClass")},
			{TEXT("SoundConcurrency")},
			{TEXT("SoundCue")},
			{TEXT("SoundEffectSourcePreset")},
			{TEXT("SoundMix")},
			{TEXT("SoundSubmix") },
			{TEXT("SphereReflectionCapture")},
			{TEXT("SpotLight")},
			{TEXT("SpringArmComponent")},
			{TEXT("StaticMesh")},
			{TEXT("StaticMeshActor")},
			{TEXT("StreamMediaSource")},
			{TEXT("SubsurfaceProfile")},
			{TEXT("TargetPoint")},
			{TEXT("TemplateSequence")},
			{TEXT("TextRenderActor")},
			{TEXT("Texture2D")},
			{TEXT("TextureRenderTarget2D")},
			{TEXT("TextureRenderTargetCube")},
			{TEXT("TimeCodeSynchronizer")},
			{TEXT("TouchInterface")},
			{TEXT("TriggerBase")},
			{TEXT("TriggerBox")},
			{TEXT("TriggerCapsule")},
			{TEXT("TriggerSphere")},
			{TEXT("TriggerVolume")},
			{TEXT("UserDefinedCaptureProtocol")},
			{TEXT("UserDefinedStruct") },
			{TEXT("UserWidget")},
			{TEXT("VectorField")},
			{TEXT("VectorFieldVolume")},
			{TEXT("Volume")},
			{TEXT("VolumetricCloud"), false},
			{TEXT("VolumetricLightmapDensityVolume")},
			{TEXT("WidgetBlueprint")},
			{TEXT("WindDirectionalSource")},
			{TEXT("World")},
		};
	
		// SVG Asset icons
		{
			for (int32 TypeIndex = 0; TypeIndex < UE_ARRAY_COUNT(AssetTypesSVG); ++TypeIndex)
			{
				const FClassIconInfo& Info = AssetTypesSVG[TypeIndex];

				// Look up if the brush already exists to audit old vs new icons during starship development.
				FString ClassIconName = FString::Printf(TEXT("ClassIcon.%s"), Info.Type);
				if (GetOptionalBrush(*ClassIconName, nullptr, nullptr))
				{
					UE_LOG(LogSlate, Log, TEXT("%s already found"), *ClassIconName);
				}

				Set(*FString::Printf(TEXT("ClassIcon.%s"), Info.Type), new IMAGE_BRUSH_SVG(FString::Printf(TEXT("Starship/AssetIcons/%s_%d"), Info.Type, 16), Icon16x16));
				if (Info.bHas64Size)
				{
					Set(*FString::Printf(TEXT("ClassThumbnail.%s"), Info.Type), new IMAGE_BRUSH_SVG(FString::Printf(TEXT("Starship/AssetIcons/%s_%d"), Info.Type, 64), Icon64x64));
				}
				else
				{
					// Temp to avoid missing icons while in progress. use the 64 variant for 16 for now.  
					Set(*FString::Printf(TEXT("ClassThumbnail.%s"), Info.Type), new IMAGE_BRUSH_SVG(FString::Printf(TEXT("Starship/AssetIcons/%s_%d"), Info.Type, 16), Icon64x64));
				}
			}
		}
	}
#endif
}

void FStarshipEditorStyle::FStyle::SetupContentBrowserStyle()
{
#if WITH_EDITOR
	// Content Browser
	{
		// Tab and menu icon
		Set("ContentBrowser.TabIcon", new IMAGE_BRUSH_SVG("Starship/Common/ContentBrowser", Icon16x16));

		// Sources View
		Set("ContentBrowser.SourceTitleFont", DEFAULT_FONT( "Regular", 12 ) );

		Set("ContentBrowser.SourceTreeItemFont", FStarshipCoreStyle::GetCoreStyle().GetFontStyle("NormalFont"));
		Set("ContentBrowser.SourceTreeRootItemFont", FStarshipCoreStyle::GetCoreStyle().GetFontStyle("NormalFont"));

		Set("ContentBrowser.BreadcrumbPathPickerFolder", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-closed", Icon16x16));

		Set("ContentBrowser.AssetTreeFolderClosed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-closed", Icon16x16));
		Set("ContentBrowser.AssetTreeFolderOpen", new CORE_IMAGE_BRUSH_SVG("Starship/Common/folder-open", Icon16x16));

		Set( "ContentBrowser.AssetTreeFolderDeveloper", new IMAGE_BRUSH( "Icons/FolderDeveloper", FVector2D(18, 16) ) );
		Set( "ContentBrowser.AssetTreeFolderOpenCode", new IMAGE_BRUSH( "Icons/FolderOpen_Code", FVector2D(18, 16) ) );
		Set( "ContentBrowser.AssetTreeFolderClosedCode", new IMAGE_BRUSH( "Icons/FolderClosed_Code", FVector2D(18, 16) ) );

		Set("ContentBrowser.DefaultFolderColor", FStyleColors::AccentFolder);

		// Asset list view
		Set( "ContentBrowser.AssetListViewNameFont", DEFAULT_FONT( "Regular", 12 ) );
		Set( "ContentBrowser.AssetListViewNameFontDirty", DEFAULT_FONT( "Bold", 12 ) );
		Set( "ContentBrowser.AssetListViewClassFont", DEFAULT_FONT( "Light", 10 ) );

		// Asset picker
		Set("ContentBrowser.NoneButton", FButtonStyle(Button)
			.SetNormal(FSlateNoResource())
			.SetHovered(BOX_BRUSH( "Common/Selection", 8.0f/32.0f, SelectionColor ))
			.SetPressed(BOX_BRUSH( "Common/Selection", 8.0f/32.0f, SelectionColor_Pressed ))
		);
		Set( "ContentBrowser.NoneButtonText", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Regular", 12 ) )
			.SetColorAndOpacity( FLinearColor::White )
		);

		// Tile view
		Set( "ContentBrowser.AssetTileViewNameFont", DEFAULT_FONT("Regular", 9));
		Set( "ContentBrowser.AssetTileViewNameFontSmall", DEFAULT_FONT( "VeryLight", 8 ) );
		Set( "ContentBrowser.AssetTileViewNameFontVerySmall", DEFAULT_FONT( "VeryLight", 7 ) );
		Set( "ContentBrowser.AssetTileViewNameFontDirty", FStyleFonts::Get().SmallBold);

		Set("ContentBrowser.AssetListView.ColumnListTableRow", FTableRowStyle()
			.SetEvenRowBackgroundBrush( FSlateNoResource() )
			.SetEvenRowBackgroundHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f) ) )
			.SetOddRowBackgroundBrush( FSlateNoResource() )
			.SetOddRowBackgroundHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f) ) )
			.SetSelectorFocusedBrush( BORDER_BRUSH( "Common/Selector", FMargin(4.f/16.f), SelectorColor ) )
			.SetActiveBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) )
			.SetActiveHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor ) )
			.SetInactiveBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
			.SetInactiveHoveredBrush( IMAGE_BRUSH( "Common/Selection", Icon8x8, SelectionColor_Inactive ) )
			.SetTextColor( DefaultForeground )
			.SetSelectedTextColor( InvertedForeground )
			);

		Set("ContentBrowser.AssetListView.TileTableRow", FTableRowStyle()
			.SetEvenRowBackgroundBrush(FSlateNoResource() )
			.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())
			.SetOddRowBackgroundBrush(FSlateNoResource())
			.SetOddRowBackgroundHoveredBrush(FSlateNoResource())
			.SetSelectorFocusedBrush(FSlateNoResource())
			.SetActiveBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())
			.SetInactiveBrush(FSlateNoResource())
			.SetInactiveHoveredBrush(FSlateNoResource())
			.SetTextColor(DefaultForeground)
			.SetSelectedTextColor(DefaultForeground)
			);

		Set( "ContentBrowser.TileViewTooltip.ToolTipBorder", new FSlateColorBrush( FLinearColor::Black ) );
		Set( "ContentBrowser.TileViewTooltip.NonContentBorder", new BOX_BRUSH( "/Docking/TabContentArea", FMargin(4/16.0f) ) );
		Set( "ContentBrowser.TileViewTooltip.ContentBorder", new FSlateColorBrush( FStyleColors::Panel));
		Set( "ContentBrowser.TileViewTooltip.NameFont", DEFAULT_FONT( "Regular", 12 ) );
		Set( "ContentBrowser.TileViewTooltip.AssetUserDescriptionFont", DEFAULT_FONT("Regular", 12 ) );

		// Columns view
		Set( "ContentBrowser.SortUp", new IMAGE_BRUSH( "Common/SortUpArrow", Icon8x4 ) );
		Set( "ContentBrowser.SortDown", new IMAGE_BRUSH( "Common/SortDownArrow", Icon8x4 ) );

		// Filter list
		/* Set images for various SCheckBox states associated with "ContentBrowser.FilterButton" ... */
		const FCheckBoxStyle ContentBrowserFilterButtonCheckBoxStyle = FCheckBoxStyle()
			.SetUncheckedImage(FSlateNoResource())
			.SetUncheckedHoveredImage(FSlateNoResource())
			.SetUncheckedPressedImage(FSlateNoResource())
			.SetCheckedImage(FSlateNoResource())
			.SetCheckedHoveredImage(FSlateNoResource())
			.SetCheckedPressedImage(FSlateNoResource())
			.SetForegroundColor(FStyleColors::Foreground)
			.SetHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetCheckedForegroundColor(FStyleColors::Foreground)
			.SetCheckedHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetCheckedPressedForegroundColor(FStyleColors::ForegroundHover)
			.SetPressedForegroundColor(FStyleColors::ForegroundHover);

		Set("ContentBrowser.FilterImage", new IMAGE_BRUSH_SVG("Starship/ContentBrowser/AssetFilterColorSegment", FVector2D(8, 22)));
		Set("ContentBrowser.FilterBackground", new FSlateRoundedBoxBrush(FStyleColors::Secondary, 3.0f));

		/* ... and add the new style */
		Set("ContentBrowser.FilterButton", ContentBrowserFilterButtonCheckBoxStyle );

		// Sources view
		Set("ContentBrowser.Sources.Paths", new IMAGE_BRUSH("ContentBrowser/Sources_Paths_16x", Icon16x16));
		Set("ContentBrowser.Sources.Collections", new IMAGE_BRUSH("ContentBrowser/Sources_Collections_Standard_16x", Icon16x16));
		Set("ContentBrowser.Sources.Collections.Compact", new IMAGE_BRUSH("ContentBrowser/Sources_Collections_Compact_16x", Icon16x16));

		// Asset tags (common)
		Set("ContentBrowser.AssetTagBackground", new FSlateRoundedBoxBrush(FStyleColors::White, 2.0));

		// Asset tags (standard)
		Set("ContentBrowser.AssetTagButton", FCheckBoxStyle()
			.SetUncheckedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f)))
			.SetUncheckedHoveredImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetUncheckedPressedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetUndeterminedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f)))
			.SetUndeterminedHoveredImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetUndeterminedPressedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetCheckedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f)))
			.SetCheckedHoveredImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetCheckedPressedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat", FVector2D(14.0f, 28.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetPadding(0.0f)
			);

		Set("ContentBrowser.AssetTagNamePadding", FMargin(4.0f));
		Set("ContentBrowser.AssetTagCountPadding", FMargin(4.0f));

		// Asset tags (compact)
		Set("ContentBrowser.AssetTagButton.Compact", FCheckBoxStyle()
			.SetUncheckedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f)))
			.SetUncheckedHoveredImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetUncheckedPressedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetUndeterminedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f)))
			.SetUndeterminedHoveredImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetUndeterminedPressedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetCheckedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f)))
			.SetCheckedHoveredImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetCheckedPressedImage(IMAGE_BRUSH("ContentBrowser/AssetTagCheckbox_Flat_Compact", FVector2D(10.0f, 20.0f), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.SetPadding(0.0f)
			);

		Set("ContentBrowser.AssetTagNamePadding.Compact", FMargin(2.0f));
		Set("ContentBrowser.AssetTagCountPadding.Compact", FMargin(2.0f));

			
		Set( "ContentBrowser.PrimitiveCustom", new IMAGE_BRUSH( "ContentBrowser/ThumbnailCustom", Icon32x32 ) );
		Set( "ContentBrowser.PrimitiveSphere", new IMAGE_BRUSH( "ContentBrowser/ThumbnailSphere", Icon32x32 ) );
		Set( "ContentBrowser.PrimitiveCube", new IMAGE_BRUSH( "ContentBrowser/ThumbnailCube", Icon32x32 ) );
		Set( "ContentBrowser.PrimitivePlane", new IMAGE_BRUSH( "ContentBrowser/ThumbnailPlane", Icon32x32 ) );
		Set( "ContentBrowser.PrimitiveCylinder", new IMAGE_BRUSH( "ContentBrowser/ThumbnailCylinder", Icon32x32 ) );
		Set( "ContentBrowser.ResetPrimitiveToDefault", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Undo", Icon20x20) );

		Set( "ContentBrowser.TopBar.Font", FTextBlockStyle( NormalText )
			.SetFont( DEFAULT_FONT( "Bold", 11 ) )
			.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f ) )
			.SetHighlightColor( FLinearColor( 1.0f, 1.0f, 1.0f ) )
			.SetShadowOffset( FVector2D( 1, 1 ) )
			.SetShadowColorAndOpacity( FLinearColor( 0, 0, 0, 0.9f ) ) );

		Set("ContentBrowser.ClassFont", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 7)));


		Set( "ContentBrowser.AddContent", new IMAGE_BRUSH( "Icons/icon_AddContent_40x", Icon25x25 ) );
		Set( "ContentBrowser.ImportPackage", new IMAGE_BRUSH( "Icons/icon_Import_40x", Icon25x25 ) );
	
		// Asset Context Menu
		Set( "ContentBrowser.AssetActions", new IMAGE_BRUSH( "Icons/icon_tab_Tools_16x", Icon16x16 ) );
		Set( "ContentBrowser.AssetActions.Edit", new IMAGE_BRUSH( "Icons/Edit/icon_Edit_16x", Icon16x16 ) );
		Set( "ContentBrowser.AssetActions.Delete", new IMAGE_BRUSH( "Icons/icon_delete_16px", Icon16x16, FLinearColor( 0.4f, 0.5f, 0.7f, 1.0f ) ) );
		//Set( "ContentBrowser.AssetActions.Delete", new IMAGE_BRUSH( "Icons/Edit/icon_Edit_Delete_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.Rename", new IMAGE_BRUSH( "Icons/Icon_Asset_Rename_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.Duplicate", new IMAGE_BRUSH( "Icons/Edit/icon_Edit_Duplicate_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.OpenSourceLocation", new IMAGE_BRUSH( "Icons/icon_Asset_Open_Source_Location_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.OpenInExternalEditor", new IMAGE_BRUSH( "Icons/icon_Asset_Open_In_External_Editor_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.ReimportAsset", new IMAGE_BRUSH( "Icons/icon_TextureEd_Reimport_40x", Icon16x16 ) );
		Set( "ContentBrowser.AssetActions.GoToCodeForAsset", new IMAGE_BRUSH( "GameProjectDialog/feature_code_32x", Icon16x16 ) );
		Set( "ContentBrowser.AssetActions.FindAssetInWorld", new IMAGE_BRUSH( "/Icons/icon_Genericfinder_16x", Icon16x16 ) );
		Set( "ContentBrowser.AssetActions.CreateThumbnail", new IMAGE_BRUSH( "Icons/icon_Asset_Create_Thumbnail_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.DeleteThumbnail", new IMAGE_BRUSH( "Icons/icon_Asset_Delete_Thumbnail_16x", Icon16x16) );
		Set( "ContentBrowser.AssetActions.GenericFind", new IMAGE_BRUSH( "Icons/icon_Genericfinder_16x", Icon16x16) );
		Set( "ContentBrowser.AssetLocalization", new IMAGE_BRUSH( "Icons/icon_localization_16x", Icon16x16 ) );

		Set( "MediaAsset.AssetActions.Play.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_PlayCue_16x", Icon16x16 ) );
		Set( "MediaAsset.AssetActions.Stop.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_Stop_16x", Icon16x16 ) );
		Set( "MediaAsset.AssetActions.Pause.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_Pause_16x", Icon16x16 ) );
		Set( "MediaAsset.AssetActions.Solo.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_Solo_16x", Icon16x16));
		Set( "MediaAsset.AssetActions.Mute.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_Mute_16x", Icon16x16));

		Set("MediaAsset.AssetActions.Play.Large", new IMAGE_BRUSH("Icons/icon_SCueEd_PlayCue_40x", Icon40x40));
		Set("MediaAsset.AssetActions.Stop.Large", new IMAGE_BRUSH("Icons/icon_SCueEd_Stop_40x", Icon40x40));
		Set("MediaAsset.AssetActions.Pause.Large", new IMAGE_BRUSH("Icons/icon_SCueEd_Pause_40x", Icon40x40));
		Set("MediaAsset.AssetActions.Solo.Large", new IMAGE_BRUSH("Icons/icon_SCueEd_Solo_40x", Icon40x40));
		Set("MediaAsset.AssetActions.Mute.Large", new IMAGE_BRUSH("Icons/icon_SCueEd_Mute_40x", Icon40x40));
				
		// Misc
		/** Should be moved, shared */ Set( "ContentBrowser.ThumbnailShadow", new BOX_BRUSH( "ContentBrowser/ThumbnailShadow" , FMargin( 4.0f / 64.0f ) ) );



		Set( "ContentBrowser.ColumnViewAssetIcon", new IMAGE_BRUSH( "Icons/doc_16x", Icon16x16 ) );

		Set( "ContentBrowser.ColumnViewFolderIcon", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/folder-closed", Icon16x16 ) );
		Set( "ContentBrowser.ColumnViewDeveloperFolderIcon", new IMAGE_BRUSH( "Icons/FolderDeveloper", FVector2D(18, 16) ) );

		Set( "ContentBrowser.ListViewFolderIcon", new IMAGE_BRUSH_SVG( "Starship/ContentBrowser/folder", FVector2D(64, 64) ) );
		Set( "ContentBrowser.ListViewDeveloperFolderIcon", new IMAGE_BRUSH( "Icons/Folders/FolderDev_Base_256x", FVector2D(256, 256) ) );

		Set("ContentBrowser.AssetTileItem.FolderAreaHoveredBackground", new FSlateRoundedBoxBrush(FStyleColors::Secondary, 4.0f));
		Set("ContentBrowser.AssetTileItem.ThumbnailAreaBackground", new FSlateRoundedBoxBrush(FStyleColors::Recessed, 4.0f));
		Set("ContentBrowser.AssetTileItem.NameAreaBackground", new FSlateRoundedBoxBrush(FStyleColors::Secondary, 4.0f));
		Set("ContentBrowser.AssetTileItem.SelectedBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Primary, 1.0f));
		Set("ContentBrowser.AssetTileItem.SelectedHoverBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::PrimaryHover, 1.0f));
		Set("ContentBrowser.AssetTileItem.HoverBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Hover, 1.0f));
		Set("ContentBrowser.AssetTileItem.DropShadow", new BOX_BRUSH("Starship/ContentBrowser/drop-shadow", FMargin(4.0f / 64.0f)));
		Set("ContentBrowser.FolderItem.DropShadow", new IMAGE_BRUSH("Starship/ContentBrowser/folder-drop-shadow", FVector2D(256, 256)));



		Set("ReferenceViewer.PathText", FEditableTextBoxStyle(NormalEditableTextBoxStyle)
			.SetFont(DEFAULT_FONT("Bold", 11)));

		Set( "ContentBrowser.ShowSourcesView", new IMAGE_BRUSH( "ContentBrowser/sourcestoggle_16x_collapsed", Icon16x16 ) );
		Set( "ContentBrowser.HideSourcesView", new IMAGE_BRUSH( "ContentBrowser/sourcestoggle_16x_expanded", Icon16x16 ) );

		Set( "ContentBrowser.DirectoryUp", new IMAGE_BRUSH("Icons/icon_folder_up_16x", Icon16x16) );
		Set( "ContentBrowser.PathPickerButton", new IMAGE_BRUSH("Icons/ellipsis_12x", Icon12x12, FLinearColor::Black) );

		Set( "ContentBrowser.ContentDirty", new IMAGE_BRUSH( "ContentBrowser/ContentDirty", Icon16x16) );
		Set( "ContentBrowser.AssetDragDropTooltipBackground", new BOX_BRUSH( "Old/Menu_Background", FMargin(8.0f/64.0f) ) );
		Set( "ContentBrowser.CollectionTreeDragDropBorder", new BOX_BRUSH( "Old/Window/ViewportDebugBorder", 0.8f ) );
		Set( "ContentBrowser.PopupMessageIcon", new IMAGE_BRUSH( "Icons/alert", Icon32x32) );
		Set( "ContentBrowser.NewFolderIcon", new IMAGE_BRUSH("Icons/icon_AddFolder_16x", Icon16x16 ) );

		Set( "ContentBrowser.Local", new IMAGE_BRUSH( "ContentBrowser/Content_Local_12x", Icon12x12 ) );
		Set( "ContentBrowser.Local.Small", new IMAGE_BRUSH( "ContentBrowser/Content_Local_16x", Icon16x16 ) );
		Set( "ContentBrowser.Local.Large", new IMAGE_BRUSH( "ContentBrowser/Content_Local_64x", Icon64x64 ) );
		Set( "ContentBrowser.Shared", new IMAGE_BRUSH( "ContentBrowser/Content_Shared_12x", Icon12x12 ) );
		Set( "ContentBrowser.Shared.Small", new IMAGE_BRUSH( "ContentBrowser/Content_Shared_16x", Icon16x16 ) );
		Set( "ContentBrowser.Shared.Large", new IMAGE_BRUSH( "ContentBrowser/Content_Shared_64x", Icon64x64 ) );
		Set( "ContentBrowser.Private", new IMAGE_BRUSH( "ContentBrowser/Content_Private_12x", Icon12x12 ) );
		Set( "ContentBrowser.Private.Small", new IMAGE_BRUSH( "ContentBrowser/Content_Private_16x", Icon16x16 ) );
		Set( "ContentBrowser.Private.Large", new IMAGE_BRUSH( "ContentBrowser/Content_Private_64x", Icon64x64 ) );
		Set( "ContentBrowser.CollectionStatus", new IMAGE_BRUSH( "/Icons/CollectionStatus_8x", Icon8x8 ) );

		Set( "AssetDiscoveryIndicator.MainStatusFont", DEFAULT_FONT( "Regular", 12 ) );
		Set( "AssetDiscoveryIndicator.SubStatusFont", DEFAULT_FONT( "Regular", 9 ) );

		Set( "ContentBrowser.SaveAllCurrentFolder", new IMAGE_BRUSH("Icons/icon_file_saveall_16px", Icon16x16) );
		Set( "ContentBrowser.ResaveAllCurrentFolder", new IMAGE_BRUSH("Icons/icon_file_saveall_16px", Icon16x16) );


		FToolBarStyle ContentBrowserToolBarStyle = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

		ContentBrowserToolBarStyle.SetSeparatorBrush(FSlateNoResource());
		ContentBrowserToolBarStyle.SetSeparatorPadding(FMargin(4.0f, 0.0f));
	
		Set("ContentBrowser.ToolBar", ContentBrowserToolBarStyle);
	}
#endif // #if WITH_EDITOR
}

void FStarshipEditorStyle::FStyle::SetupLandscapeEditorStyle()
{
#if WITH_EDITOR
	// Landscape Editor
	{
		// Modes
		Set("LandscapeEditor.ManageMode", new IMAGE_BRUSH("Icons/icon_Landscape_Mode_Manage_40x", Icon40x40));
		Set("LandscapeEditor.SculptMode", new IMAGE_BRUSH("Icons/icon_Landscape_Mode_Sculpt_40x", Icon40x40));
		Set("LandscapeEditor.PaintMode",  new IMAGE_BRUSH("Icons/icon_Landscape_Mode_Paint_40x",  Icon40x40));
		Set("LandscapeEditor.ManageMode.Small", new IMAGE_BRUSH("Icons/icon_Landscape_Mode_Manage_20x", Icon20x20));
		Set("LandscapeEditor.SculptMode.Small", new IMAGE_BRUSH("Icons/icon_Landscape_Mode_Sculpt_20x", Icon20x20));
		Set("LandscapeEditor.PaintMode.Small",  new IMAGE_BRUSH("Icons/icon_Landscape_Mode_Paint_20x",  Icon20x20));

		{
			// Tools
			Set("LandscapeEditor.NewLandscape",					new IMAGE_BRUSH("Icons/Landscape/Landscape_NewLandscape_x40",		Icon20x20));
			Set("LandscapeEditor.NewLandscape.Small",       	new IMAGE_BRUSH("Icons/Landscape/Landscape_NewLandscape_x40", 		Icon20x20));
			Set("LandscapeEditor.ResizeLandscape",				new IMAGE_BRUSH("Icons/Landscape/Landscape_Resize_x40",				Icon20x20));
			Set("LandscapeEditor.ResizeLandscape.Small",		new IMAGE_BRUSH("Icons/Landscape/Landscape_Resize_x40",				Icon20x20));
			Set("LandscapeEditor.ImportExportTool",				new IMAGE_BRUSH("Icons/Landscape/Landscape_ImportExport_x40",		Icon20x20));
			Set("LandscapeEditor.ImportExportTool.Small",		new IMAGE_BRUSH("Icons/Landscape/Landscape_ImportExport_x40",		Icon20x20));

			Set("LandscapeEditor.SculptTool",       			new IMAGE_BRUSH("Icons/Landscape/Landscape_Sculpt_x40",            Icon20x20));
			Set("LandscapeEditor.SculptTool.Small",       		new IMAGE_BRUSH("Icons/Landscape/Landscape_Sculpt_x40",            Icon20x20));
			Set("LandscapeEditor.EraseTool",					new IMAGE_BRUSH("Icons/Landscape/Landscape_Erase_x40",       	   Icon20x20));
			Set("LandscapeEditor.EraseTool.Small",				new IMAGE_BRUSH("Icons/Landscape/Landscape_Erase_x40",       	   Icon20x20));
			Set("LandscapeEditor.PaintTool",        			new IMAGE_BRUSH("Icons/Landscape/Landscape_PaintTool_x40",         Icon20x20));
			Set("LandscapeEditor.PaintTool.Small",        		new IMAGE_BRUSH("Icons/Landscape/Landscape_PaintTool_x40",         Icon20x20));
			Set("LandscapeEditor.SmoothTool",       			new IMAGE_BRUSH("Icons/Landscape/Landscape_Smooth_x40",            Icon20x20));
			Set("LandscapeEditor.SmoothTool.Small",       		new IMAGE_BRUSH("Icons/Landscape/Landscape_Smooth_x40",            Icon20x20));
			Set("LandscapeEditor.FlattenTool",      			new IMAGE_BRUSH("Icons/Landscape/Landscape_Flatten_x40",           Icon20x20));
			Set("LandscapeEditor.FlattenTool.Small",      		new IMAGE_BRUSH("Icons/Landscape/Landscape_Flatten_x40",           Icon20x20));
			Set("LandscapeEditor.RampTool",         			new IMAGE_BRUSH("Icons/Landscape/Landscape_Ramp_x40",              Icon20x20));
			Set("LandscapeEditor.RampTool.Small",         		new IMAGE_BRUSH("Icons/Landscape/Landscape_Ramp_x40",              Icon20x20));
			Set("LandscapeEditor.ErosionTool",      			new IMAGE_BRUSH("Icons/Landscape/Landscape_Erosion_x40",           Icon20x20));
			Set("LandscapeEditor.ErosionTool.Small",      		new IMAGE_BRUSH("Icons/Landscape/Landscape_Erosion_x40",           Icon20x20));
			Set("LandscapeEditor.HydroErosionTool", 			new IMAGE_BRUSH("Icons/Landscape/Landscape_HydroErosion_x40",      Icon20x20));
			Set("LandscapeEditor.HydroErosionTool.Small", 		new IMAGE_BRUSH("Icons/Landscape/Landscape_HydroErosion_x40",      Icon20x20));
			Set("LandscapeEditor.NoiseTool",        			new IMAGE_BRUSH("Icons/Landscape/Landscape_Noise_x40",             Icon20x20));
			Set("LandscapeEditor.NoiseTool.Small",        		new IMAGE_BRUSH("Icons/Landscape/Landscape_Noise_x40",             Icon20x20));
			Set("LandscapeEditor.RetopologizeTool", 			new IMAGE_BRUSH("Icons/Landscape/Landscape_Retopologize_x40",      Icon20x20));
			Set("LandscapeEditor.RetopologizeTool.Small", 		new IMAGE_BRUSH("Icons/Landscape/Landscape_Retopologize_x40",      Icon20x20));
			Set("LandscapeEditor.VisibilityTool",   			new IMAGE_BRUSH("Icons/Landscape/Landscape_VisibilityTool_x40",    Icon20x20));
			Set("LandscapeEditor.VisibilityTool.Small",   		new IMAGE_BRUSH("Icons/Landscape/Landscape_VisibilityTool_x40",    Icon20x20));
			Set("LandscapeEditor.BlueprintBrushTool", 			new IMAGE_BRUSH("Icons/Landscape/Landscape_BlueprintTool_x40",     Icon20x20));
			Set("LandscapeEditor.BlueprintBrushTool.Small", 	new IMAGE_BRUSH("Icons/Landscape/Landscape_BlueprintTool_x40",     Icon20x20));

			Set("LandscapeEditor.SelectComponentTool", 			new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentSelect_x40",   Icon20x20));
			Set("LandscapeEditor.SelectComponentTool.Small", 	new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentSelect_x40",   Icon20x20));
			Set("LandscapeEditor.AddComponentTool",    			new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentAdd_x40",      Icon20x20));
			Set("LandscapeEditor.AddComponentTool.Small",    	new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentAdd_x40",      Icon20x20));
			Set("LandscapeEditor.DeleteComponentTool", 			new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentDelete_x40",   Icon20x20));
			Set("LandscapeEditor.DeleteComponentTool.Small", 	new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentDelete_x40",   Icon20x20));
			Set("LandscapeEditor.MoveToLevelTool",     			new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentMove_x40",     Icon20x20));
			Set("LandscapeEditor.MoveToLevelTool.Small",     	new IMAGE_BRUSH("Icons/Landscape/Landscape_ComponentMove_x40",     Icon20x20));

			Set("LandscapeEditor.RegionSelectTool",    			new IMAGE_BRUSH("Icons/Landscape/Landscape_RegionSelect_x40",      Icon20x20));
			Set("LandscapeEditor.RegionSelectTool.Small",    	new IMAGE_BRUSH("Icons/Landscape/Landscape_RegionSelect_x40",      Icon20x20));
			Set("LandscapeEditor.RegionCopyPasteTool",			new IMAGE_BRUSH("Icons/Landscape/Landscape_CopyPasteTool_x40",     Icon20x20));
			Set("LandscapeEditor.RegionCopyPasteTool.Small",	new IMAGE_BRUSH("Icons/Landscape/Landscape_CopyPasteTool_x40",     Icon20x20));

			Set("LandscapeEditor.MirrorTool",       			new IMAGE_BRUSH("Icons/Landscape/Landscape_Mirror_x40",            Icon20x20));
			Set("LandscapeEditor.MirrorTool.Small",       		new IMAGE_BRUSH("Icons/Landscape/Landscape_Mirror_x40",            Icon20x20));

			Set("LandscapeEditor.SplineTool",       			new IMAGE_BRUSH("Icons/Landscape/Landscape_EditSplines_x40",       Icon20x20));
			Set("LandscapeEditor.SplineTool.Small",       		new IMAGE_BRUSH("Icons/Landscape/Landscape_EditSplines_x40",       Icon20x20));

			// Brush Sets
			Set("LandscapeEditor.CircleBrush",        			new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushCircle_x40",       Icon20x20));
			Set("LandscapeEditor.CircleBrush.Small",        	new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushCircle_x40",       Icon20x20));
			Set("LandscapeEditor.AlphaBrush",         			new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushAlpha_x40",        Icon20x20));
			Set("LandscapeEditor.AlphaBrush.Small",         	new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushAlpha_x40",        Icon20x20));
			Set("LandscapeEditor.AlphaBrush_Pattern", 			new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushPattern_x40",      Icon20x20));
			Set("LandscapeEditor.AlphaBrush_Pattern.Small", 	new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushPattern_x40",      Icon20x20));
			Set("LandscapeEditor.ComponentBrush",     			new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushComponent_x40",    Icon20x20));
			Set("LandscapeEditor.ComponentBrush.Small",     	new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushComponent_x40",    Icon20x20));
			Set("LandscapeEditor.GizmoBrush",         			new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushGizmo_x40",        Icon20x20));
			Set("LandscapeEditor.GizmoBrush.Small",         	new IMAGE_BRUSH("Icons/Landscape/Landscape_BrushGizmo_x40",        Icon20x20));

			// Brushes
			Set("LandscapeEditor.CircleBrush_Smooth",   		new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffSmooth_x40",     Icon20x20));
			Set("LandscapeEditor.CircleBrush_Smooth.Small",   	new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffSmooth_x40",     Icon20x20));
			Set("LandscapeEditor.CircleBrush_Linear",   		new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffLinear_x40",     Icon20x20));
			Set("LandscapeEditor.CircleBrush_Linear.Small",   	new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffLinear_x40",     Icon20x20));
			Set("LandscapeEditor.CircleBrush_Spherical",		new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffSpherical_x40",  Icon20x20));
			Set("LandscapeEditor.CircleBrush_Spherical.Small",	new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffSpherical_x40",  Icon20x20));
			Set("LandscapeEditor.CircleBrush_Tip",      		new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffTip_x40",        Icon20x20));
			Set("LandscapeEditor.CircleBrush_Tip.Small",      	new IMAGE_BRUSH("Icons/Landscape/Landscape_FalloffTip_x40",        Icon20x20));

		}

		Set("LandscapeEditor.Brushes.Alpha.UseRChannel", new IMAGE_BRUSH("Icons/icon_Landscape_Brush_Alpha_UseRChannel_20x", Icon20x20));
		Set("LandscapeEditor.Brushes.Alpha.UseGChannel", new IMAGE_BRUSH("Icons/icon_Landscape_Brush_Alpha_UseGChannel_20x", Icon20x20));
		Set("LandscapeEditor.Brushes.Alpha.UseBChannel", new IMAGE_BRUSH("Icons/icon_Landscape_Brush_Alpha_UseBChannel_20x", Icon20x20));
		Set("LandscapeEditor.Brushes.Alpha.UseAChannel", new IMAGE_BRUSH("Icons/icon_Landscape_Brush_Alpha_UseAChannel_20x", Icon20x20));

		Set("LandscapeEditor.Brush.AffectsHeight.Enabled", new IMAGE_BRUSH("Icons/icon_Landscape_Affects_Height_Enabled_16x", Icon16x16));
		Set("LandscapeEditor.Brush.AffectsHeight.Disabled", new IMAGE_BRUSH("Icons/icon_Landscape_Affects_Height_Disabled_16x", Icon16x16));
		Set("LandscapeEditor.Brush.AffectsWeight.Enabled", new IMAGE_BRUSH("Icons/icon_Landscape_Affects_Weight_Enabled_16x", Icon16x16));
		Set("LandscapeEditor.Brush.AffectsWeight.Disabled", new IMAGE_BRUSH("Icons/icon_Landscape_Affects_Weight_Disabled_16x", Icon16x16));

		// Target List
		Set("LandscapeEditor.TargetList.RowBackground",        new FSlateNoResource());
		Set("LandscapeEditor.TargetList.RowBackgroundHovered", new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f)));
		Set("LandscapeEditor.TargetList.RowSelected",          new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor_Pressed));
		Set("LandscapeEditor.TargetList.RowSelectedHovered",   new BOX_BRUSH("Common/RoundedSelection_16x", 4.0f / 16.0f, SelectionColor));

		Set("LandscapeEditor.Target_Heightmap",  new IMAGE_BRUSH("Icons/icon_Landscape_Target_Heightmap_48x",  Icon48x48));
		Set("LandscapeEditor.Target_Visibility", new IMAGE_BRUSH("Icons/icon_Landscape_Target_Visibility_48x", Icon48x48));
		Set("LandscapeEditor.Target_Invalid",    new IMAGE_BRUSH("Icons/icon_Landscape_Target_Invalid_48x",    Icon48x48));

		Set("LandscapeEditor.Target_Create",     new IMAGE_BRUSH("Icons/icon_Landscape_Target_Create_12x", Icon12x12));
		Set("LandscapeEditor.Target_MakePublic", new IMAGE_BRUSH("Icons/assign_right_12x",                 Icon12x12));
		Set("LandscapeEditor.Target_Delete",     new IMAGE_BRUSH("Icons/Cross_12x",                        Icon12x12));

		Set("LandscapeEditor.Target_DisplayOrder.Default", new IMAGE_BRUSH("Icons/icon_landscape_sort_base", Icon16x16));
		Set("LandscapeEditor.Target_DisplayOrder.Alphabetical", new IMAGE_BRUSH("Icons/icon_landscape_sort_alphabetical", Icon16x16));
		Set("LandscapeEditor.Target_DisplayOrder.Custom", new IMAGE_BRUSH("Icons/icon_landscape_sort_custom", Icon16x16));

		Set("LandscapeEditor.TargetList.DropZone.Above", new BOX_BRUSH("Common/VerticalBoxDropZoneIndicator_Above", FMargin(10.0f / 16.0f, 10.0f / 16.0f, 0, 0), SelectionColor_Subdued));
		Set("LandscapeEditor.TargetList.DropZone.Below", new BOX_BRUSH("Common/VerticalBoxDropZoneIndicator_Below", FMargin(10.0f / 16.0f, 0, 0, 10.0f / 16.0f), SelectionColor_Subdued));

		Set("LandscapeEditor.SpinBox", FSpinBoxStyle(GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
			.SetTextPadding(FMargin(0))
			.SetBackgroundBrush(FSlateNoResource())
			.SetHoveredBackgroundBrush(FSlateNoResource())
			.SetInactiveFillBrush(FSlateNoResource())
			.SetActiveFillBrush(FSlateNoResource())
			.SetForegroundColor(FSlateColor::UseForeground())
			.SetArrowsImage(FSlateNoResource())
		);
}

#endif
}

void FStarshipEditorStyle::FStyle::SetupToolkitStyles()
{
#if WITH_EDITOR
	// Project Browser
	{
		Set("ProjectBrowser.VersionOverlayText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 14))
			.SetShadowOffset(FVector2D(0, 1)));


		const FTableRowStyle ProjectBrowserTableRowStyle = FTableRowStyle()
			.SetEvenRowBackgroundBrush(FSlateNoResource())
			.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())
			.SetOddRowBackgroundBrush(FSlateNoResource())
			.SetOddRowBackgroundHoveredBrush(FSlateNoResource())
			.SetSelectorFocusedBrush(FSlateNoResource())
			.SetActiveBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())
			.SetInactiveBrush(FSlateNoResource())
			.SetInactiveHoveredBrush(FSlateNoResource())
			.SetActiveHighlightedBrush(FSlateNoResource())
			.SetInactiveHighlightedBrush(FSlateNoResource())
			.SetTextColor(FStyleColors::Foreground)
			.SetSelectedTextColor(FStyleColors::ForegroundInverted);

		Set("ProjectBrowser.TableRow", ProjectBrowserTableRowStyle);

		Set("ProjectBrowser.MajorCategoryViewBorder", new FSlateRoundedBoxBrush(FStyleColors::Recessed, 4.0f));


		Set("ProjectBrowser.TileViewTooltip.ToolTipBorder", new FSlateColorBrush(FLinearColor::Black));
		Set("ProjectBrowser.TileViewTooltip.NonContentBorder", new BOX_BRUSH("/Docking/TabContentArea", FMargin(4 / 16.0f)));
		Set("ProjectBrowser.TileViewTooltip.ContentBorder", new BOX_BRUSH("Common/GroupBorder", FMargin(4.0f / 16.0f)));
		Set("ProjectBrowser.TileViewTooltip.NameFont", DEFAULT_FONT("Regular", 12));

		Set("ProjectBrowser.ProjectTile.Font", DEFAULT_FONT("Regular", 9));
		Set("ProjectBrowser.ProjectTile.ThumbnailAreaBackground", new FSlateRoundedBoxBrush(COLOR("#474747FF"), FVector4(4.0f,4.0f,0.0f,0.0f)));
		Set("ProjectBrowser.ProjectTile.NameAreaBackground", new FSlateRoundedBoxBrush(EStyleColor::Header, FVector4(0.0f, 0.0f, 4.0f, 4.0f)));
		Set("ProjectBrowser.ProjectTile.DropShadow", new BOX_BRUSH("Starship/ContentBrowser/drop-shadow", FMargin(4.0f / 64.0f)));
		Set("ProjectBrowser.ProjectTile.SelectedBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Primary, 1.0f));
		Set("ProjectBrowser.ProjectTile.SelectedHoverBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::PrimaryHover, 1.0f));
		Set("ProjectBrowser.ProjectTile.HoverBorder", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FStyleColors::Hover, 1.0f));
	}

	// Toolkit Display
	{
		Set("ToolkitDisplay.UnsavedChangeIcon", new IMAGE_BRUSH("Common/UnsavedChange", Icon8x8));
		Set("ToolkitDisplay.MenuDropdown", new IMAGE_BRUSH("Common/ComboArrow", Icon8x8));
		Set("ToolkitDisplay.ColorOverlay", new BOX_BRUSH("/Docking/Tab_ColorOverlay", 4 / 16.0f));

		FComboButtonStyle ComboButton = FComboButtonStyle()
			.SetButtonStyle(Button)
			.SetDownArrowImage(IMAGE_BRUSH("Common/ComboArrow", Icon8x8))
			// Multiboxes draw their own border so we don't want a default content border
			.SetMenuBorderBrush(FSlateNoResource())
			.SetMenuBorderPadding(FMargin(0.0f));
		Set("ToolkitDisplay.ComboButton", ComboButton);
	}

	// Generic Editor
	{
		Set( "GenericEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
	}

	// CurveTable Editor
	{
		Set( "CurveTableEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
	}

	// DataTable Editor
	{
		Set( "DataTableEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );

		Set("DataTableEditor.Copy", new IMAGE_BRUSH("/Icons/Edit/icon_Edit_Copy_40x", Icon32x32));
		Set("DataTableEditor.Paste", new IMAGE_BRUSH("/Icons/Edit/icon_Edit_Paste_40x", Icon32x32));
		Set("DataTableEditor.Duplicate", new IMAGE_BRUSH("Icons/Edit/icon_Edit_Duplicate_40x", Icon32x32));
		Set("DataTableEditor.Save", new IMAGE_BRUSH("Icons/icon_SaveAsset_40x", Icon16x16));
		Set("DataTableEditor.Browse", new IMAGE_BRUSH("Icons/lens_12x", Icon16x16));
		Set("DataTableEditor.Add", new IMAGE_BRUSH("Icons/icon_add_40x", Icon32x32));
		Set("DataTableEditor.Remove", new IMAGE_BRUSH("Icons/Edit/icon_Edit_Delete_40x", Icon32x32));

		Set("DataTableEditor.Copy.Small", new IMAGE_BRUSH("/Icons/Edit/icon_Edit_Copy_40x", Icon16x16));
		Set("DataTableEditor.Paste.Small", new IMAGE_BRUSH("/Icons/Edit/icon_Edit_Paste_40x", Icon16x16));
		Set("DataTableEditor.Duplicate.Small", new IMAGE_BRUSH("Icons/Edit/icon_Edit_Duplicate_40x", Icon16x16));
		Set("DataTableEditor.Add.Small", new IMAGE_BRUSH("Icons/icon_add_40x", Icon16x16));
		Set("DataTableEditor.Remove.Small", new IMAGE_BRUSH("Icons/Edit/icon_Edit_Delete_40x", Icon16x16));



		Set( "DataTableEditor.CellText", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT("Regular", 9 ))
			);

		Set( "DataTableEditor.NameListViewRow", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetEvenRowBackgroundHoveredBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetOddRowBackgroundBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetOddRowBackgroundHoveredBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetSelectorFocusedBrush( FSlateNoResource() )
			.SetActiveBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetActiveHoveredBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetInactiveBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetInactiveHoveredBrush( BOX_BRUSH( "Common/TableViewMajorColumn", 4.f/32.f ) )
			.SetTextColor( DefaultForeground )
			.SetSelectedTextColor( DefaultForeground )
		);

		Set("DataTableEditor.CellListViewRow", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle", FVector2D(16, 16), FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle_Hovered", FVector2D(16, 16), FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetOddRowBackgroundBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle", FVector2D(16, 16), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle_Hovered", FVector2D(16, 16), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetActiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetActiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetInactiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetInactiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetTextColor(DefaultForeground)
			.SetSelectedTextColor(DefaultForeground)
		);

		Set("DataTableEditor.DragDropObject", new BOX_BRUSH("Common/TextBox_Special_Active", FMargin(8.0f / 32.0f)));
		Set("DataTableEditor.DragDropHoveredTarget", new BOX_BRUSH("Common/TextBox_Special_Active", FMargin(8.0f / 32.0f), SelectionColor_Pressed));

	}

	// StringTable Editor
	{
		Set("StringTableEditor.Tabs.Properties", new IMAGE_BRUSH("/Icons/icon_tab_SelectionDetails_16x", Icon16x16));
	}
#endif //#if WITH_EDITOR

	// Material Editor
#if WITH_EDITOR
	{
		Set( "MaterialEditor.Tabs.HLSLCode", new IMAGE_BRUSH( "/Icons/icon_MatEd_HLSL_Code_16x", Icon16x16 ) );

		Set( "MaterialEditor.Layers.EditableFont", DEFAULT_FONT("Regular", 8));
		Set("MaterialEditor.Layers.EditableFontImportant", DEFAULT_FONT("Bold", FStarshipCoreStyle::RegularTextSize));
		Set( "MaterialEditor.NormalFont", DEFAULT_FONT( "Regular", 9 ) );
		Set( "MaterialEditor.BoldFont", DEFAULT_FONT( "Bold", 9 ) );

		Set( "MaterialEditor.Apply", new IMAGE_BRUSH_SVG( "Starship/Common/Apply", Icon20x20 ) );
		Set( "MaterialEditor.LiveUpdate", new IMAGE_BRUSH_SVG("Starship/MaterialEditor/LiveUpdate", Icon20x20));
		Set( "MaterialEditor.Hierarchy", new IMAGE_BRUSH_SVG("Starship/MaterialEditor/Hierarchy", Icon20x20));

		Set( "MaterialEditor.SetCylinderPreview", new IMAGE_BRUSH("Icons/AssetIcons/Cylinder_16x", Icon16x16));
		Set( "MaterialEditor.SetSpherePreview", new IMAGE_BRUSH("Icons/AssetIcons/Sphere_16x", Icon16x16));
		Set( "MaterialEditor.SetPlanePreview", new IMAGE_BRUSH( "Icons/AssetIcons/Plane_16x", Icon16x16) );
		Set( "MaterialEditor.SetCubePreview", new IMAGE_BRUSH( "Icons/AssetIcons/Cube_16x", Icon16x16) );
		Set( "MaterialEditor.SetPreviewMeshFromSelection", new IMAGE_BRUSH_SVG( "Starship/AssetIcons/StaticMesh_64", Icon16x16 ) );
		Set( "MaterialEditor.TogglePreviewGrid", new IMAGE_BRUSH_SVG( "Starship/Common/Grid", Icon16x16 ) );
		Set( "MaterialEditor.ToggleMaterialStats", new IMAGE_BRUSH_SVG( "Starship/Common/MaterialAnalyzer", Icon20x20 ) );
		Set( "MaterialEditor.ToggleMaterialStats.Tab", new IMAGE_BRUSH_SVG("Starship/Common/MaterialAnalyzer", Icon16x16));
		Set( "MaterialEditor.TogglePlatformStats", new IMAGE_BRUSH_SVG( "Starship/MaterialEditor/PlatformStats", Icon20x20 ) );
		Set("MaterialEditor.TogglePlatformStats.Tab", new IMAGE_BRUSH_SVG("Starship/MaterialEditor/PlatformStats", Icon16x16));
		Set( "MaterialEditor.CameraHome", new IMAGE_BRUSH_SVG( "Starship/Common/Home", Icon20x20 ) );
		Set( "MaterialEditor.FindInMaterial", new IMAGE_BRUSH_SVG( "Starship/Common/Search", Icon20x20 ) );


		Set("MaterialEditor.CellListViewRow", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle", FVector2D(16, 16), FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetEvenRowBackgroundHoveredBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle_Hovered", FVector2D(16, 16), FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetOddRowBackgroundBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle", FVector2D(16, 16), FLinearColor(0.35f, 0.35f, 0.35f)))
			.SetOddRowBackgroundHoveredBrush(IMAGE_BRUSH("PropertyView/DetailCategoryMiddle_Hovered", FVector2D(16, 16), FLinearColor(0.35f, 0.35f, 0.35f)))
			.SetActiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetActiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetInactiveBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetInactiveHoveredBrush(IMAGE_BRUSH("Common/Selection", Icon8x8, FLinearColor(0.075f, 0.075f, 0.075f)))
			.SetTextColor(DefaultForeground)
			.SetSelectedTextColor(DefaultForeground)
		);
	}

	// Material Instance Editor
	{
		Set( "MaterialInstanceEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
		Set("MaterialEditor.ShowAllMaterialParameters", new IMAGE_BRUSH_SVG("Starship/GraphEditors/HideUnrelated", Icon20x20));
	}
	
	// Sound Class Editor
	{
		Set( "SoundClassEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );

		Set("SoundClassEditor.ToggleSolo", new IMAGE_BRUSH("Icons/icon_SCueEd_Solo_40x", Icon40x40));
		Set("SoundClassEditor.ToggleSolo.Small", new IMAGE_BRUSH("Icons/icon_SCueEd_Solo_40x", Icon20x20));
		Set("SoundClassEditor.ToggleMute", new IMAGE_BRUSH("Icons/icon_SCueEd_Mute_40x", Icon40x40));
		Set("SoundClassEditor.ToggleMute.Small", new IMAGE_BRUSH("Icons/icon_SCueEd_Mute_40x", Icon20x20));
	}

	// Font Editor
	{
		// Tab icons
		{
			Set( "FontEditor.Tabs.Preview", new IMAGE_BRUSH( "/Icons/icon_Genericfinder_16x", Icon16x16 ) );
			Set( "FontEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
			Set( "FontEditor.Tabs.PageProperties", new IMAGE_BRUSH( "/Icons/properties_16x", Icon16x16 ) );
		}

		Set( "FontEditor.Update", new IMAGE_BRUSH( "Icons/icon_FontEd_Update_40x", Icon40x40 ) );
		Set( "FontEditor.Update.Small", new IMAGE_BRUSH( "Icons/icon_FontEd_Update_40x", Icon20x20 ) );
		Set( "FontEditor.UpdateAll", new IMAGE_BRUSH( "Icons/icon_FontEd_UpdateAll_40x", Icon40x40 ) );
		Set( "FontEditor.UpdateAll.Small", new IMAGE_BRUSH( "Icons/icon_FontEd_UpdateAll_40x", Icon20x20 ) );
		Set( "FontEditor.ExportPage", new IMAGE_BRUSH( "Icons/icon_FontEd_Export_40x", Icon40x40 ) );
		Set( "FontEditor.ExportPage.Small", new IMAGE_BRUSH( "Icons/icon_FontEd_Export_40x", Icon20x20 ) );
		Set( "FontEditor.ExportAllPages", new IMAGE_BRUSH( "Icons/icon_FontEd_ExportAll_40x", Icon40x40 ) );
		Set( "FontEditor.ExportAllPages.Small", new IMAGE_BRUSH( "Icons/icon_FontEd_ExportAll_40x", Icon20x20 ) );

		Set( "FontEditor.FontBackgroundColor", new IMAGE_BRUSH( "Icons/icon_FontEd_Background_40x", Icon40x40 ) );
		Set( "FontEditor.FontBackgroundColor.Small", new IMAGE_BRUSH( "Icons/icon_FontEd_Background_40x", Icon20x20 ) );
		Set( "FontEditor.FontForegroundColor", new IMAGE_BRUSH( "Icons/icon_FontEd_Foreground_40x", Icon40x40 ) );
		Set( "FontEditor.FontForegroundColor.Small", new IMAGE_BRUSH( "Icons/icon_FontEd_Foreground_40x", Icon20x20 ) );

		Set( "FontEditor.Button_Add", new IMAGE_BRUSH( "Icons/PlusSymbol_12x", Icon12x12 ) );
		Set( "FontEditor.Button_Delete", new IMAGE_BRUSH("Icons/Cross_12x", Icon12x12 ) );
	}

	// SoundCueGraph Editor
	{
		Set( "SoundCueGraphEditor.PlayCue", new IMAGE_BRUSH( "Icons/icon_SCueEd_PlayCue_40x", Icon40x40 ) );
		Set( "SoundCueGraphEditor.PlayCue.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_PlayCue_40x", Icon20x20 ) );
		Set( "SoundCueGraphEditor.PlayNode", new IMAGE_BRUSH( "Icons/icon_SCueEd_PlayNode_40x", Icon40x40 ) );
		Set( "SoundCueGraphEditor.PlayNode.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_PlayNode_40x", Icon20x20 ) );
		Set( "SoundCueGraphEditor.StopCueNode", new IMAGE_BRUSH( "Icons/icon_SCueEd_Stop_40x", Icon40x40 ) );
		Set( "SoundCueGraphEditor.StopCueNode.Small", new IMAGE_BRUSH( "Icons/icon_SCueEd_Stop_40x", Icon20x20 ) );

		Set("SoundCueGraphEditor.ToggleSolo", new IMAGE_BRUSH("Icons/icon_SCueEd_Solo_40x", Icon40x40));
		Set("SoundCueGraphEditor.ToggleSolo.Small", new IMAGE_BRUSH("Icons/icon_SCueEd_Solo_40x", Icon20x20));
		Set("SoundCueGraphEditor.ToggleMute", new IMAGE_BRUSH("Icons/icon_SCueEd_Mute_40x", Icon40x40));
		Set("SoundCueGraphEditor.ToggleMute.Small", new IMAGE_BRUSH("Icons/icon_SCueEd_Mute_40x", Icon20x20));
	}

	// Static Mesh Editor
	{
		Set("StaticMeshEditor.Tabs.Properties", new IMAGE_BRUSH_SVG("Starship/Common/Details", Icon16x16));
		Set("StaticMeshEditor.Tabs.SocketManager", new IMAGE_BRUSH_SVG("Starship/StaticMeshEditor/SocketManager", Icon16x16));
		Set("StaticMeshEditor.Tabs.ConvexDecomposition", new IMAGE_BRUSH_SVG("Starship/StaticMeshEditor/ConvexDecomposition", Icon16x16));
		Set("StaticMeshEditor.SetShowWireframe", new IMAGE_BRUSH_SVG("Starship/Common/BrushWireframe", Icon16x16));
		Set("StaticMeshEditor.SetShowVertexColor", new IMAGE_BRUSH_SVG( "Starship/Common/SetShowVertexColors", Icon16x16));
		Set("StaticMeshEditor.SetRealtimePreview", new IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon16x16));
		Set("StaticMeshEditor.ReimportMesh", new IMAGE_BRUSH_SVG("Starship/StaticMeshEditor/ReimportMesh", Icon20x20));
		Set("StaticMeshEditor.SetShowBounds", new IMAGE_BRUSH_SVG("Starship/Common/SetShowBounds", Icon16x16));
		Set("StaticMeshEditor.SetDrawUVs", new IMAGE_BRUSH_SVG("Starship/Common/SetDrawUVs", Icon20x20));
		Set("StaticMeshEditor.SetShowCollision", new IMAGE_BRUSH_SVG("Starship/Common/Collision", Icon20x20));
		Set("StaticMeshEditor.SetShowGrid", new IMAGE_BRUSH_SVG("Starship/Common/Grid", Icon16x16));
		Set("StaticMeshEditor.ResetCamera", new IMAGE_BRUSH_SVG("Starship/Common/ResetCamera", Icon16x16));
		Set("StaticMeshEditor.SetShowPivot", new IMAGE_BRUSH_SVG("Starship/Common/SetShowPivot", Icon16x16));
		Set("StaticMeshEditor.SetShowSockets", new IMAGE_BRUSH_SVG( "Starship/Common/SetShowSockets", Icon16x16));
		Set("StaticMeshEditor.SaveThumbnail", new IMAGE_BRUSH_SVG( "Starship/Common/SaveThumbnail", Icon16x16));
		Set("StaticMeshEditor.SetShowNormals", new IMAGE_BRUSH_SVG( "Starship/Common/SetShowNormals", Icon16x16));
		Set("StaticMeshEditor.SetShowTangents", new IMAGE_BRUSH_SVG("Starship/Common/SetShowTangents", Icon16x16));
		Set("StaticMeshEditor.SetShowBinormals", new IMAGE_BRUSH_SVG("Starship/Common/SetShowBinormals", Icon16x16));
		Set("StaticMeshEditor.SetDrawAdditionalData", new IMAGE_BRUSH_SVG( "Starship/StaticMeshEditor/AdditionalData", Icon16x16));
		Set("StaticMeshEditor.SetShowVertices", new IMAGE_BRUSH_SVG("Starship/Common/SetShowVertices", Icon16x16));
		Set("StaticMeshEditor.ToggleShowPivots", new IMAGE_BRUSH_SVG("Starship/Common/SetShowPivot", Icon16x16));
		Set("StaticMeshEditor.ToggleShowSockets", new IMAGE_BRUSH_SVG("Starship/Common/SetShowSockets", Icon16x16));
		Set("StaticMeshEditor.ToggleShowNormals", new IMAGE_BRUSH_SVG("Starship/Common/SetShowNormals", Icon16x16));
		Set("StaticMeshEditor.ToggleShowTangents", new IMAGE_BRUSH_SVG("Starship/Common/SetShowTangents", Icon16x16));
		Set("StaticMeshEditor.ToggleShowBinormals", new IMAGE_BRUSH_SVG("Starship/Common/SetShowBinormals", Icon16x16));
		Set("StaticMeshEditor.ToggleShowBounds", new IMAGE_BRUSH_SVG("Starship/Common/SetShowBounds", Icon16x16));
		Set("StaticMeshEditor.ToggleShowGrids", new IMAGE_BRUSH_SVG("Starship/Common/Grid", Icon16x16));
		Set("StaticMeshEditor.ToggleShowVertices", new IMAGE_BRUSH_SVG("Starship/Common/SetShowVertices", Icon16x16));
		Set("StaticMeshEditor.ToggleShowWireframes", new IMAGE_BRUSH_SVG("Starship/Common/BrushWireframe", Icon16x16));
		Set("StaticMeshEditor.ToggleShowVertexColors", new IMAGE_BRUSH_SVG("Starship/Common/SetShowVertexColors", Icon16x16));

	}

	// Skeletal Mesh Editor
	{
		Set( "SkeletalMeshEditor.GroupSection", new BOX_BRUSH( "Common/RoundedSelection_16x", FMargin( 4.0f / 16.0f ) ) );
	}

	// Texture Editor
	{
		Set("TextureEditor.Tabs.Properties", new IMAGE_BRUSH("/Icons/icon_tab_SelectionDetails_16x", Icon16x16));
		
		Set("TextureEditor.RedChannel", new IMAGE_BRUSH( "Icons/icon_TextureEd_RedChannel_40x", Icon40x40));
		Set("TextureEditor.RedChannel.Small", new IMAGE_BRUSH( "Icons/icon_TextureEd_RedChannel_40x", Icon20x20));
		Set("TextureEditor.GreenChannel", new IMAGE_BRUSH( "Icons/icon_TextureEd_GreenChannel_40x", Icon40x40));
		Set("TextureEditor.GreenChannel.Small", new IMAGE_BRUSH( "Icons/icon_TextureEd_GreenChannel_40x", Icon20x20));
		Set("TextureEditor.BlueChannel", new IMAGE_BRUSH( "Icons/icon_TextureEd_BlueChannel_40x", Icon40x40));
		Set("TextureEditor.BlueChannel.Small", new IMAGE_BRUSH( "Icons/icon_TextureEd_BlueChannel_40x", Icon20x20));
		Set("TextureEditor.AlphaChannel", new IMAGE_BRUSH( "Icons/icon_TextureEd_AlphaChannel_40x", Icon40x40));
		Set("TextureEditor.AlphaChannel.Small", new IMAGE_BRUSH( "Icons/icon_TextureEd_AlphaChannel_40x", Icon20x20));
		Set("TextureEditor.Saturation", new IMAGE_BRUSH( "Icons/icon_TextureEd_Saturation_40x", Icon40x40));
		Set("TextureEditor.Saturation.Small", new IMAGE_BRUSH( "Icons/icon_TextureEd_Saturation_40x", Icon20x20));

		Set("TextureEditor.CompressNow", new IMAGE_BRUSH_SVG( "Starship/Common/Compress", Icon20x20));
		Set("TextureEditor.Reimport", new CORE_IMAGE_BRUSH_SVG("Starship/Common/import", Icon20x20));

		FButtonStyle MipmapButtonStyle = 
			FButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FButtonStyle>("Button"))
				.SetNormalPadding(FMargin(2, 2, 2, 2))
				.SetPressedPadding(FMargin(2, 3, 2, 1));

		Set("TextureEditor.MipmapButtonStyle", MipmapButtonStyle);

		const FLinearColor White80 = FLinearColor(1, 1, 1, .8f);

		const FCheckBoxStyle ChannelToggleButtonStyle = FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetCheckedImage(FSlateRoundedBoxBrush(White80, CoreStyleConstants::InputFocusRadius))
			.SetCheckedHoveredImage(FSlateRoundedBoxBrush(FLinearColor::White, CoreStyleConstants::InputFocusRadius))
			.SetCheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::White, CoreStyleConstants::InputFocusRadius))
			.SetUncheckedImage(FSlateRoundedBoxBrush(FStyleColors::Dropdown, CoreStyleConstants::InputFocusRadius))
			.SetUncheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::Hover, CoreStyleConstants::InputFocusRadius))
			.SetUncheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::Hover, CoreStyleConstants::InputFocusRadius))
			.SetForegroundColor(FStyleColors::Foreground)
			.SetHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetPressedForegroundColor(FStyleColors::ForegroundHover)
			.SetCheckedForegroundColor(FStyleColors::Foreground)
			.SetCheckedHoveredForegroundColor(FStyleColors::ForegroundHover)
			.SetPadding(FMargin(8.f, 4.f));

		FSlateFontInfo ChannelButtonFont = FStyleFonts::Get().NormalBold;
		ChannelButtonFont.Size = 12;
		Set("TextureEditor.ChannelButtonFont", ChannelButtonFont);

		Set("TextureEditor.ChannelButtonStyle", ChannelToggleButtonStyle);
	}

	// Cascade
	{
		Set( "Cascade.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
		
		Set( "Cascade.RestartSimulation", new IMAGE_BRUSH( "Icons/icon_Cascade_RestartSim_40x", Icon40x40 ) );
		Set( "Cascade.RestartInLevel", new IMAGE_BRUSH( "Icons/icon_Cascade_RestartInLevel_40x", Icon40x40 ) );
		Set( "Cascade.SaveThumbnailImage", new IMAGE_BRUSH( "Icons/icon_Cascade_Thumbnail_40x", Icon40x40 ) );
		Set( "Cascade.Undo", new IMAGE_BRUSH( "Icons/icon_Generic_Undo_40x", Icon40x40 ) );
		Set( "Cascade.Redo", new IMAGE_BRUSH( "Icons/icon_Generic_Redo_40x", Icon40x40 ) );
		Set( "Cascade.ToggleBounds", new IMAGE_BRUSH( "Icons/icon_Cascade_Bounds_40x", Icon40x40 ) );
		Set( "Cascade.ToggleOriginAxis", new IMAGE_BRUSH( "Icons/icon_Cascade_Axis_40x", Icon40x40 ) );
		Set( "Cascade.CascadeBackgroundColor", new IMAGE_BRUSH( "Icons/icon_Cascade_Color_40x", Icon40x40 ) );
		Set( "Cascade.RegenerateLowestLODDuplicatingHighest", new IMAGE_BRUSH( "Icons/icon_Cascade_RegenLOD1_40x", Icon40x40 ) );
		Set( "Cascade.RegenerateLowestLOD", new IMAGE_BRUSH( "Icons/icon_Cascade_RegenLOD2_40x", Icon40x40 ) );
		Set( "Cascade.JumpToHighestLOD", new IMAGE_BRUSH( "Icons/icon_Cascade_HighestLOD_40x", Icon40x40 ) );
		Set( "Cascade.JumpToHigherLOD", new IMAGE_BRUSH( "Icons/icon_Cascade_HigherLOD_40x", Icon40x40 ) );
		Set( "Cascade.AddLODAfterCurrent", new IMAGE_BRUSH( "Icons/icon_Cascade_AddLOD1_40x", Icon40x40 ) );
		Set( "Cascade.AddLODBeforeCurrent", new IMAGE_BRUSH( "Icons/icon_Cascade_AddLOD2_40x", Icon40x40 ) );
		Set( "Cascade.JumpToLowerLOD", new IMAGE_BRUSH( "Icons/icon_Cascade_LowerLOD_40x", Icon40x40 ) );
		Set( "Cascade.JumpToLowestLOD", new IMAGE_BRUSH( "Icons/icon_Cascade_LowestLOD_40x", Icon40x40 ) );
		Set( "Cascade.DeleteLOD", new IMAGE_BRUSH( "Icons/icon_Cascade_DeleteLOD_40x", Icon40x40 ) );
		
		Set( "Cascade.RestartSimulation.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_RestartSim_40x", Icon20x20 ) );
		Set( "Cascade.RestartInLevel.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_RestartInLevel_40x", Icon20x20 ) );
		Set( "Cascade.SaveThumbnailImage.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_Thumbnail_40x", Icon20x20 ) );
		Set( "Cascade.Undo.Small", new IMAGE_BRUSH( "Icons/icon_Generic_Undo_40x", Icon20x20 ) );
		Set( "Cascade.Redo.Small", new IMAGE_BRUSH( "Icons/icon_Generic_Redo_40x", Icon20x20 ) );
		Set( "Cascade.ToggleBounds.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_Bounds_40x", Icon20x20 ) );
		Set( "Cascade.ToggleOriginAxis.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_Axis_40x", Icon20x20 ) );
		Set( "Cascade.CascadeBackgroundColor.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_Color_40x", Icon20x20 ) );
		Set( "Cascade.RegenerateLowestLODDuplicatingHighest.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_RegenLOD1_40x", Icon20x20 ) );
		Set( "Cascade.RegenerateLowestLOD.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_RegenLOD2_40x", Icon20x20 ) );
		Set( "Cascade.JumpToHighestLOD.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_HighestLOD_40x", Icon20x20 ) );
		Set( "Cascade.JumpToHigherLOD.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_HigherLOD_40x", Icon20x20 ) );
		Set( "Cascade.AddLODAfterCurrent.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_AddLOD1_40x", Icon20x20 ) );
		Set( "Cascade.AddLODBeforeCurrent.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_AddLOD2_40x", Icon20x20 ) );
		Set( "Cascade.JumpToLowerLOD.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_LowerLOD_40x", Icon20x20 ) );
		Set( "Cascade.JumpToLowestLOD.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_LowestLOD_40x", Icon20x20 ) );
		Set( "Cascade.DeleteLOD.Small", new IMAGE_BRUSH( "Icons/icon_Cascade_DeleteLOD_40x", Icon20x20 ) );
	}

	// Level Script
	{
		Set( "LevelScript.Delete", new IMAGE_BRUSH( "Icons/icon_delete_16px", Icon16x16 ) );
	}

	// Curve Editor
	{
		Set("CurveAssetEditor.Tabs.Properties", new IMAGE_BRUSH("Icons/AssetIcons/CurveBase_16x", Icon16x16));

		Set("CurveEditor.FitHorizontally", new IMAGE_BRUSH("Icons/icon_CurveEditor_Horizontal_40x", Icon40x40));
		Set("CurveEditor.FitVertically", new IMAGE_BRUSH("Icons/icon_CurveEditor_Vertical_40x", Icon40x40));
		Set("CurveEditor.Fit", new IMAGE_BRUSH("Icons/icon_CurveEditor_ZoomToFit_40x", Icon40x40));
		Set("CurveEditor.PanMode", new IMAGE_BRUSH("Icons/icon_CurveEditor_Pan_40x", Icon40x40));
		Set("CurveEditor.ZoomMode", new IMAGE_BRUSH("Icons/icon_CurveEditor_Zoom_40x", Icon40x40));
		Set("CurveEditor.CurveAuto", new IMAGE_BRUSH("Icons/icon_CurveEditor_Auto_40x", Icon40x40));
		Set("CurveEditor.CurveAutoClamped", new IMAGE_BRUSH("Icons/icon_CurveEditor_AutoClamped_40x", Icon40x40));
		Set("CurveEditor.CurveUser", new IMAGE_BRUSH("Icons/icon_CurveEditor_User_40x", Icon40x40));
		Set("CurveEditor.CurveBreak", new IMAGE_BRUSH("Icons/icon_CurveEditor_Break_40x", Icon40x40));
		Set("CurveEditor.CurveWeight", new IMAGE_BRUSH("Icons/icon_CurveEditor_Break_40x", Icon40x40));
		Set("CurveEditor.CurveNonWeight", new IMAGE_BRUSH("Icons/icon_CurveEditor_Break_40x", Icon40x40));

		Set("CurveEditor.Linear", new IMAGE_BRUSH("Icons/icon_CurveEditor_Linear_40x", Icon40x40));
		Set("CurveEditor.Constant", new IMAGE_BRUSH("Icons/icon_CurveEditor_Constant_40x", Icon40x40));
		Set("CurveEditor.FlattenTangents", new IMAGE_BRUSH("Icons/icon_CurveEditor_Flatten_40x", Icon40x40));
		Set("CurveEditor.StraightenTangents", new IMAGE_BRUSH("Icons/icon_CurveEditor_Straighten_40x", Icon40x40));
		Set("CurveEditor.ShowAllTangents", new IMAGE_BRUSH("Icons/icon_CurveEditor_ShowAll_40x", Icon40x40));
		Set("CurveEditor.CreateTab", new IMAGE_BRUSH("Icons/icon_CurveEditor_Create_40x", Icon40x40));
		Set("CurveEditor.DeleteTab", new IMAGE_BRUSH("Icons/icon_CurveEditor_DeleteTab_40x", Icon40x40));

		Set("CurveEditor.FitHorizontally.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Horizontal_40x", Icon20x20));
		Set("CurveEditor.FitVertically.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Vertical_40x", Icon20x20));
		Set("CurveEditor.Fit.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_ZoomToFit_40x", Icon20x20));
		Set("CurveEditor.PanMode.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Pan_40x", Icon20x20));
		Set("CurveEditor.ZoomMode.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Zoom_40x", Icon20x20));
		Set("CurveEditor.CurveAuto.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Auto_40x", Icon20x20));
		Set("CurveEditor.CurveAutoClamped.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_AutoClamped_40x", Icon20x20));
		Set("CurveEditor.CurveUser.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_User_40x", Icon20x20));
		Set("CurveEditor.CurveBreak.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Break_40x", Icon20x20));
		Set("CurveEditor.CurveWeight.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Break_40x", Icon20x20));
		Set("CurveEditor.CurveNonWeight.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Break_40x", Icon20x20));

		Set("CurveEditor.Linear.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Linear_40x", Icon20x20));
		Set("CurveEditor.Constant.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Constant_40x", Icon20x20));
		Set("CurveEditor.FlattenTangents.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Flatten_40x", Icon20x20));
		Set("CurveEditor.StraightenTangents.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Straighten_40x", Icon20x20));
		Set("CurveEditor.ShowAllTangents.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_ShowAll_40x", Icon20x20));
		Set("CurveEditor.CreateTab.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_Create_40x", Icon20x20));
		Set("CurveEditor.DeleteTab.Small", new IMAGE_BRUSH("Icons/icon_CurveEditor_DeleteTab_40x", Icon20x20));

		Set("CurveEditor.Gradient.HandleDown", new BOX_BRUSH("Sequencer/ScrubHandleDown", FMargin(6.f / 13.f, 5 / 12.f, 6 / 13.f, 8 / 12.f)));
		Set("CurveEditor.Gradient.HandleUp", new BOX_BRUSH("Sequencer/ScrubHandleUp", FMargin(6.f / 13.f, 8 / 12.f, 6 / 13.f, 5 / 12.f)));
	}

	// New Curve Editor 
	{
		// Tab
		Set("GenericCurveEditor.TabIcon", new IMAGE_BRUSH("GenericCurveEditor/Icons/CurveEditor_32x", Icon16x16, FLinearColor(1.f, 1.f, 1.f, 0.8f)));

		// Zoom / Framing
		Set("GenericCurveEditor.ZoomToFit", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingSelected_48x", Icon48x48));
		Set("GenericCurveEditor.ZoomToFit.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingSelected_48x", Icon24x24));

		// Time/Value Snapping
		Set("GenericCurveEditor.ToggleInputSnapping", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeySnapFrame_48x", Icon48x48));
		Set("GenericCurveEditor.ToggleInputSnapping.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeySnapFrame_48x", Icon24x24));
		Set("GenericCurveEditor.ToggleOutputSnapping", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeySnapValue_48x", Icon48x48));
		Set("GenericCurveEditor.ToggleOutputSnapping.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeySnapValue_48x", Icon24x24));

		// Tangent Types
		Set("GenericCurveEditor.InterpolationCubicAuto", new IMAGE_BRUSH("GenericCurveEditor/Icons/CubicTangentAuto_48x", Icon48x48));
		Set("GenericCurveEditor.InterpolationCubicAuto.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/CubicTangentAuto_48x", Icon24x24));
		Set("GenericCurveEditor.InterpolationCubicUser", new IMAGE_BRUSH("GenericCurveEditor/Icons/TangentsCubic_48x", Icon48x48));
		Set("GenericCurveEditor.InterpolationCubicUser.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/TangentsCubic_48x", Icon24x24));
		Set("GenericCurveEditor.InterpolationCubicBreak", new IMAGE_BRUSH("GenericCurveEditor/Icons/CubicTangentBroken_48x", Icon48x48));
		Set("GenericCurveEditor.InterpolationCubicBreak.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/CubicTangentBroken_48x", Icon24x24));
		Set("GenericCurveEditor.InterpolationToggleWeighted", new IMAGE_BRUSH("GenericCurveEditor/Icons/CubicTangentWeighted_48x", Icon48x48));
		Set("GenericCurveEditor.InterpolationToggleWeighted.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/CubicTangentWeighted_48x", Icon24x24));
		Set("GenericCurveEditor.InterpolationLinear", new IMAGE_BRUSH("GenericCurveEditor/Icons/TangentsLinear_48x", Icon48x48));
		Set("GenericCurveEditor.InterpolationLinear.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/TangentsLinear_48x", Icon24x24));
		Set("GenericCurveEditor.InterpolationConstant", new IMAGE_BRUSH("GenericCurveEditor/Icons/TangentsConstant_48x", Icon48x48));
		Set("GenericCurveEditor.InterpolationConstant.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/TangentsConstant_48x", Icon24x24));

		// Tangent Modifications
		Set("GenericCurveEditor.FlattenTangents", new IMAGE_BRUSH("GenericCurveEditor/Icons/MiscFlatten_48x", Icon48x48));
		Set("GenericCurveEditor.FlattenTangents.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/MiscFlatten_48x", Icon24x24));
		Set("GenericCurveEditor.StraightenTangents", new IMAGE_BRUSH("GenericCurveEditor/Icons/MiscStraighten_48x", Icon48x48));
		Set("GenericCurveEditor.StraightenTangents.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/MiscStraighten_48x", Icon24x24));

		// View Modes
		Set("GenericCurveEditor.SetViewModeAbsolute", new IMAGE_BRUSH("GenericCurveEditor/Icons/GraphViewAbsolute_48x", Icon48x48));
		Set("GenericCurveEditor.SetViewModeAbsolute.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/GraphViewAbsolute_48x", Icon24x24));
		Set("GenericCurveEditor.SetViewModeStacked", new IMAGE_BRUSH("GenericCurveEditor/Icons/GraphViewStack_48x", Icon48x48));
		Set("GenericCurveEditor.SetViewModeStacked.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/GraphViewStack_48x", Icon24x24));
		Set("GenericCurveEditor.SetViewModeNormalized", new IMAGE_BRUSH("GenericCurveEditor/Icons/GraphViewNormalized_48x", Icon48x48));
		Set("GenericCurveEditor.SetViewModeNormalized.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/GraphViewNormalized_48x", Icon24x24));

		// Axis Snapping
		Set("GenericCurveEditor.SetAxisSnappingNone", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeyMoveFree_48x", Icon48x48));
		Set("GenericCurveEditor.SetAxisSnappingNone.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeyMoveFree_48x", Icon24x24));
		Set("GenericCurveEditor.SetAxisSnappingHorizontal", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeyLockValue_48x", Icon48x48));
		Set("GenericCurveEditor.SetAxisSnappingHorizontal.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeyLockValue_48x", Icon24x24));
		Set("GenericCurveEditor.SetAxisSnappingVertical", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeyLockFrame_48x", Icon48x48));
		Set("GenericCurveEditor.SetAxisSnappingVertical.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/AdjustKeyLockFrame_48x", Icon24x24));

		// Deactivate Tool
		Set("GenericCurveEditor.DeactivateCurrentTool", new IMAGE_BRUSH("GenericCurveEditor/Icons/GeneralPointer_48x", Icon48x48));
		Set("GenericCurveEditor.DeactivateCurrentTool.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/GeneralPointer_48x", Icon24x24));

		// Filters
		Set("GenericCurveEditor.OpenUserImplementableFilterWindow", new IMAGE_BRUSH("GenericCurveEditor/Icons/MiscFilters_48x", Icon48x48));
		Set("GenericCurveEditor.OpenUserImplementableFilterWindow.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/MiscFilters_48x", Icon24x24));

		// Key Types
		Set("GenericCurveEditor.Key", new IMAGE_BRUSH("GenericCurveEditor/Keys/Diamond_Filled", FVector2D(11.0f, 11.0f))); 
		Set("GenericCurveEditor.ConstantKey", new IMAGE_BRUSH("GenericCurveEditor/Keys/Square_Filled", FVector2D(11.0f, 11.0f))); 
		Set("GenericCurveEditor.LinearKey", new IMAGE_BRUSH("GenericCurveEditor/Keys/Triangle_Filled", FVector2D(11.0f, 11.0f)));
		Set("GenericCurveEditor.CubicKey", new IMAGE_BRUSH("GenericCurveEditor/Keys/Diamond_Filled", FVector2D(11.0f, 11.0f)));
		Set("GenericCurveEditor.TangentHandle", new IMAGE_BRUSH("GenericCurveEditor/Keys/TangentHandle", FVector2D(8.0f, 8.0f)));
		Set("GenericCurveEditor.WeightedTangentCubicKey", new IMAGE_BRUSH("GenericCurveEditor/Keys/Trapezoid_Filled", FVector2D(11.0f, 11.0f)));

		// Pre-Infinity
		Set("GenericCurveEditor.SetPreInfinityExtrapConstant", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityConstant_48x", Icon48x48));
		Set("GenericCurveEditor.SetPreInfinityExtrapConstant.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityConstant_48x", Icon24x24));
		Set("GenericCurveEditor.SetPreInfinityExtrapCycle", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityCycle_48x", Icon48x48));
		Set("GenericCurveEditor.SetPreInfinityExtrapCycle.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityCycle_48x", Icon24x24));
		Set("GenericCurveEditor.SetPreInfinityExtrapCycleWithOffset", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityOffset_48x", Icon48x48));
		Set("GenericCurveEditor.SetPreInfinityExtrapCycleWithOffset.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityOffset_48x", Icon24x24));
		Set("GenericCurveEditor.SetPreInfinityExtrapLinear", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityLinear_48x", Icon48x48));
		Set("GenericCurveEditor.SetPreInfinityExtrapLinear.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityLinear_48x", Icon24x24));
		Set("GenericCurveEditor.SetPreInfinityExtrapOscillate", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityOscillate_48x", Icon48x48));
		Set("GenericCurveEditor.SetPreInfinityExtrapOscillate.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityOscillate_48x", Icon24x24));
		Set("GenericCurveEditor.PreInfinityMixed", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityMultipleSelected_48x", Icon48x48));
		Set("GenericCurveEditor.PreInfinityMixed.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PreInfinityMultipleSelected_48x", Icon24x24));


		// Post-Infinity
		Set("GenericCurveEditor.SetPostInfinityExtrapConstant", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityConstant_48x", Icon48x48));
		Set("GenericCurveEditor.SetPostInfinityExtrapConstant.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityConstant_48x", Icon24x24));
		Set("GenericCurveEditor.SetPostInfinityExtrapCycle", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityCycle_48x", Icon48x48));
		Set("GenericCurveEditor.SetPostInfinityExtrapCycle.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityCycle_48x", Icon24x24));
		Set("GenericCurveEditor.SetPostInfinityExtrapCycleWithOffset", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityOffset_48x", Icon48x48));
		Set("GenericCurveEditor.SetPostInfinityExtrapCycleWithOffset.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityOffset_48x", Icon24x24));
		Set("GenericCurveEditor.SetPostInfinityExtrapLinear", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityLinear_48x", Icon48x48));
		Set("GenericCurveEditor.SetPostInfinityExtrapLinear.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityLinear_48x", Icon24x24));
		Set("GenericCurveEditor.SetPostInfinityExtrapOscillate", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityOscillate_48x", Icon48x48));
		Set("GenericCurveEditor.SetPostInfinityExtrapOscillate.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityOscillate_48x", Icon24x24));
		Set("GenericCurveEditor.PostInfinityMixed", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityMultipleSelected_48x", Icon48x48));
		Set("GenericCurveEditor.PostInfinityMixed.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/PostInfinityMultipleSelected_48x", Icon24x24));

		// Misc
		Set("GenericCurveEditor.VisibilityOptions", new IMAGE_BRUSH("GenericCurveEditor/Icons/SettingsVisibility_48x", Icon48x48));
		Set("GenericCurveEditor.VisibilityOptions.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/SettingsVisibility_48x", Icon24x24));

		Set("GenericCurveEditor.Pin_Active", new IMAGE_BRUSH("Common/PushPin_Down", Icon16x16));
		Set("GenericCurveEditor.Pin_Inactive", new IMAGE_BRUSH("Common/PushPin_Up", Icon16x16));
		
		Set("GenericCurveEditor.Select", new IMAGE_BRUSH("GenericCurveEditor/Icons/SelectButton", Icon16x16));
	}

	// Generic Curve Editor Tools
	{
		Set("CurveEditorTools.SetFocusPlaybackTime", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingPlayback_48x", Icon48x48));
		Set("CurveEditorTools.SetFocusPlaybackTime.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingPlayback_48x", Icon24x24));
		Set("CurveEditorTools.SetFocusPlaybackRange", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingTimeRange_48x", Icon48x48));
		Set("CurveEditorTools.SetFocusPlaybackRange.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingTimeRange_48x", Icon24x24));

		Set("CurveEditorTools.ActivateTransformTool", new IMAGE_BRUSH("GenericCurveEditor/Icons/ToolsTransform_48x", Icon48x48));
		Set("CurveEditorTools.ActivateTransformTool.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/ToolsTransform_48x", Icon24x24));
		Set("CurveEditorTools.ActivateRetimeTool", new IMAGE_BRUSH("GenericCurveEditor/Icons/ToolsRetime_48x", Icon48x48));
		Set("CurveEditorTools.ActivateRetimeTool.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/ToolsRetime_48x", Icon24x24));
		Set("CurveEditorTools.ActivateMultiScaleTool", new IMAGE_BRUSH("GenericCurveEditor/Icons/ToolsMultiScale_48x", Icon48x48));
		Set("CurveEditorTools.ActivateMultiScaleTool.Small", new IMAGE_BRUSH("GenericCurveEditor/Icons/ToolsMultiScale_48x", Icon24x24));
	}

	// General Curve Icons
	{
		Set("Curve.ZoomToFit", new IMAGE_BRUSH_SVG("Starship/Common/ZoomToFit", Icon20x20));
		Set("Curve.ZoomToFitHorizontal", new IMAGE_BRUSH_SVG("Starship/Common/ZoomToFitHorizontal", Icon20x20));
		Set("Curve.ZoomToFitVertical", new IMAGE_BRUSH_SVG("Starship/Common/ZoomToFitVertical", Icon20x20));
	}

	// PhysicsAssetEditor
	{
		Set( "PhysicsAssetEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
		Set( "PhysicsAssetEditor.Tabs.Hierarchy", new IMAGE_BRUSH( "/Icons/levels_16x", Icon16x16 ) );
		Set( "PhysicsAssetEditor.Tabs.Profiles", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_ProfilesTab_16x", Icon16x16 ) );
		Set( "PhysicsAssetEditor.Tabs.Graph", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_GraphTab_16x", Icon16x16 ) );
		Set( "PhysicsAssetEditor.Tabs.Tools", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_ToolsTab_16x", Icon16x16 ) );

		Set( "PhysicsAssetEditor.EditingMode_Body", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_PHatMode_Body_40x", Icon40x40) );
		Set( "PhysicsAssetEditor.EditingMode_Constraint", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_PHatMode_Joint_40x", Icon40x40) );

		Set( "PhysicsAssetEditor.EditingMode_Body.Small", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_PHatMode_Body_40x", Icon20x20) );
		Set( "PhysicsAssetEditor.EditingMode_Constraint.Small", new IMAGE_BRUSH( "/PhysicsAssetEditor/icon_PHatMode_Joint_40x", Icon20x20) );

		Set( "PhysicsAssetEditor.SimulationNoGravity", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_PlaySimNoGravity_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.SelectedSimulation", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_PlaySimSelected_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.SimulationAll", new IMAGE_BRUSH_SVG("Starship/MainToolbar/simulate", Icon20x20));
		Set( "PhysicsAssetEditor.Undo", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Undo", Icon20x20));
		Set( "PhysicsAssetEditor.Redo", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Redo", Icon20x20));
		Set( "PhysicsAssetEditor.ChangeDefaultMesh", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_Mesh_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.ApplyPhysicalMaterial", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationPhysicalMaterial", Icon20x20));
		Set( "PhysicsAssetEditor.CopyJointSettings", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_CopyJoints_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.PlayAnimation", new IMAGE_BRUSH_SVG( "Starship/MainToolbar/play", Icon20x20 ) );
		Set( "PhysicsAssetEditor.PhATTranslationMode", new IMAGE_BRUSH( "Icons/icon_translate_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.PhATRotationMode", new IMAGE_BRUSH( "Icons/icon_rotate_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.PhATScaleMode", new IMAGE_BRUSH( "Icons/icon_scale_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.Snap", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_Snap_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.CopyProperties", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_CopyProperties_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.DisableCollision", new IMAGE_BRUSH_SVG( "Starship/Common/DisableCollision", Icon20x20 ) );
		Set( "PhysicsAssetEditor.EnableCollision", new IMAGE_BRUSH_SVG("Starship/Common/EnableCollision", Icon20x20));
		Set( "PhysicsAssetEditor.DisableCollisionAll", new IMAGE_BRUSH_SVG("Starship/Common/DisableCollision", Icon20x20));
		Set( "PhysicsAssetEditor.EnableCollisionAll", new IMAGE_BRUSH_SVG("Starship/Common/EnableCollision", Icon20x20));
		Set( "PhysicsAssetEditor.WeldToBody", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_Weld_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.AddNewBody", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_NewBody_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.AddSphere", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_Sphere_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.AddSphyl", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_Sphyl_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.AddBox", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_Box_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.AddTaperedCapsule", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_TaperedCapsule_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.DeletePrimitive", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_DeletePrimitive_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.DuplicatePrimitive", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_DupePrim_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.ResetConstraint", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_ResetConstraint_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.SnapConstraint", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_SnapConstraint_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.SnapAllConstraints", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_SnapAll_40x", Icon40x40 ) );
		Set( "PhysicsAssetEditor.ConvertToBallAndSocket", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationToBallAndSocket", Icon20x20));
		Set( "PhysicsAssetEditor.ConvertToHinge", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationToHinge", Icon20x20));
		Set( "PhysicsAssetEditor.ConvertToPrismatic", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationToPrismatic", Icon20x20));
		Set( "PhysicsAssetEditor.ConvertToSkeletal", new IMAGE_BRUSH_SVG("Starship/Persona/AnimationToSkeletal", Icon20x20));
		Set( "PhysicsAssetEditor.DeleteConstraint", new IMAGE_BRUSH( "PhysicsAssetEditor/icon_PhAT_DeleteConstraint_40x", Icon40x40 ) );

		Set("PhysicsAssetEditor.NewPhysicalAnimationProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PhAT_NewBody_40x", Icon20x20));
		Set("PhysicsAssetEditor.DeleteCurrentPhysicalAnimationProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PhAT_DeletePrimitive_40x", Icon20x20));
		Set("PhysicsAssetEditor.AddBodyToPhysicalAnimationProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PhAT_NewBody_40x", Icon20x20));
		Set("PhysicsAssetEditor.RemoveBodyFromPhysicalAnimationProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PhAT_DeletePrimitive_40x", Icon20x20));
		Set("PhysicsAssetEditor.NewConstraintProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PHatMode_Joint_40x", Icon20x20));
		Set("PhysicsAssetEditor.DeleteCurrentConstraintProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PhAT_DeleteConstraint_40x", Icon20x20));
		Set("PhysicsAssetEditor.AddConstraintToCurrentConstraintProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PHatMode_Joint_40x", Icon20x20));
		Set("PhysicsAssetEditor.RemoveConstraintFromCurrentConstraintProfile", new IMAGE_BRUSH("PhysicsAssetEditor/icon_PhAT_DeleteConstraint_40x", Icon20x20));

		Set("PhysicsAssetEditor.Tree.Body", new IMAGE_BRUSH("PhysicsAssetEditor/Body_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.KinematicBody", new IMAGE_BRUSH("PhysicsAssetEditor/KinematicBody_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.EmptyBody", new IMAGE_BRUSH("PhysicsAssetEditor/EmptyBody_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.Bone", new IMAGE_BRUSH("PhysicsAssetEditor/Bone_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.Sphere", new IMAGE_BRUSH("PhysicsAssetEditor/Sphere_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.Sphyl", new IMAGE_BRUSH("PhysicsAssetEditor/Sphyl_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.Box", new IMAGE_BRUSH("PhysicsAssetEditor/Box_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.Convex", new IMAGE_BRUSH("PhysicsAssetEditor/Convex_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.TaperedCapsule", new IMAGE_BRUSH("PhysicsAssetEditor/TaperedCapsule_16x", Icon16x16));
		Set("PhysicsAssetEditor.Tree.Constraint", new IMAGE_BRUSH("PhysicsAssetEditor/Constraint_16x", Icon16x16));

		Set("PhysicsAssetEditor.Tree.Font", DEFAULT_FONT("Regular", 10));

		Set("PhysicsAssetEditor.Graph.TextStyle", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
			.SetFont(DEFAULT_FONT("Regular", 8)));

		Set("PhysicsAssetEditor.Graph.NodeBody", new BOX_BRUSH("PhysicsAssetEditor/NodeBody", FMargin(4.f / 64.f, 4.f / 64.f, 4.f / 64.f, 4.f / 64.f)));
		Set("PhysicsAssetEditor.Graph.NodeIcon", new IMAGE_BRUSH("PhysicsAssetEditor/Bone_16x", Icon16x16));
		Set("PhysicsAssetEditor.Graph.Pin.Background", new IMAGE_BRUSH("PhysicsAssetEditor/NodePin", Icon10x10));
		Set("PhysicsAssetEditor.Graph.Pin.BackgroundHovered", new IMAGE_BRUSH("PhysicsAssetEditor/NodePinHoverCue", Icon10x10));
		Set("PhysicsAssetEditor.Graph.Node.ShadowSelected", new BOX_BRUSH( "PhysicsAssetEditor/PhysicsNode_shadow_selected", FMargin(18.0f/64.0f) ) );
		Set("PhysicsAssetEditor.Graph.Node.Shadow", new BOX_BRUSH( "Graph/RegularNode_shadow", FMargin(18.0f/64.0f) ) );

		FEditableTextBoxStyle EditableTextBlock = FEditableTextBoxStyle()
			.SetFont(NormalText.Font)
			.SetBackgroundImageNormal(BOX_BRUSH("Common/TextBox", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageHovered(BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageFocused(BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageReadOnly(BOX_BRUSH("Common/TextBox_ReadOnly", FMargin(4.0f / 16.0f)));

		Set("PhysicsAssetEditor.Profiles.EditableTextBoxStyle", EditableTextBlock);

		Set("PhysicsAssetEditor.Profiles.Font", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 11))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		Set("PhysicsAssetEditor.Tools.Font", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 11))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		FLinearColor Red = FLinearColor::Red;
		FLinearColor Red_Selected = FLinearColor::Red.Desaturate(0.75f);
		FLinearColor Red_Pressed = FLinearColor::Red.Desaturate(0.5f);

		const FCheckBoxStyle RedRadioButtonStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, Red ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, Red_Selected ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red_Pressed ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red_Selected ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Red_Pressed ) );

		Set( "PhysicsAssetEditor.RadioButtons.Red", RedRadioButtonStyle );

		FLinearColor Green = FLinearColor::Green;
		FLinearColor Green_Selected = FLinearColor::Green.Desaturate(0.75f);
		FLinearColor Green_Pressed = FLinearColor::Green.Desaturate(0.5f);

		const FCheckBoxStyle GreenRadioButtonStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, Green ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, Green_Selected ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green_Pressed ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green_Selected ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Green_Pressed ) );

		Set( "PhysicsAssetEditor.RadioButtons.Green", GreenRadioButtonStyle );

		FLinearColor Blue = FLinearColor::Blue;
		FLinearColor Blue_Selected = FLinearColor::Blue.Desaturate(0.75f);
		FLinearColor Blue_Pressed = FLinearColor::Blue.Desaturate(0.5f);

		const FCheckBoxStyle BlueRadioButtonStyle = FCheckBoxStyle()
			.SetUncheckedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue ) )
			.SetUncheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue ) )
			.SetUncheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue ) )
			.SetCheckedImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, Blue ) )
			.SetCheckedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Selected_16x", Icon16x16, Blue_Selected ) )
			.SetCheckedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue_Pressed ) )
			.SetUndeterminedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue ) )
			.SetUndeterminedHoveredImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue_Selected ) )
			.SetUndeterminedPressedImage( IMAGE_BRUSH( "Common/RadioButton_Unselected_16x", Icon16x16, Blue_Pressed ) );

		Set( "PhysicsAssetEditor.RadioButtons.Blue", BlueRadioButtonStyle );
	}
#endif // WITH_EDITOR
}

void FStarshipEditorStyle::FStyle::SetupMatineeStyle()
{
	//Matinee
#if WITH_EDITOR
	{
		Set( "Matinee.Tabs.RecordingViewport", new IMAGE_BRUSH( "/Icons/icon_Matinee_RecordingViewport_16x", Icon16x16 ) );
		Set( "Matinee.Tabs.CurveEditor", new IMAGE_BRUSH( "/Icons/icon_Matinee_Curve_Editor_16x", Icon16x16 ) );
		Set( "Matinee.Tabs.Tracks", new IMAGE_BRUSH( "/Icons/icon_Matinee_Tracks_16x", Icon16x16 ) );

		Set("Matinee.Filters.Text", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 9))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		Set( "Matinee.AddKey", new IMAGE_BRUSH( "Icons/icon_Matinee_AddKey_40x", Icon40x40 ) );
		Set( "Matinee.CreateMovie", new IMAGE_BRUSH( "Icons/icon_Matinee_CreateMovie_40x", Icon40x40 ) );
		Set( "Matinee.Play", new IMAGE_BRUSH( "Icons/icon_Matinee_Play_40x", Icon40x40 ) );
		Set( "Matinee.PlayLoop", new IMAGE_BRUSH( "Icons/icon_Matinee_PlayLoopSection_40x", Icon40x40 ) );
		Set( "Matinee.Stop", new IMAGE_BRUSH( "Icons/icon_Matinee_Stop_40x", Icon40x40 ) );
		Set( "Matinee.PlayReverse", new IMAGE_BRUSH( "Icons/icon_Matinee_PlayReverse_40x", Icon40x40 ) );
		Set( "Matinee.ToggleSnap", new IMAGE_BRUSH( "Icons/icon_Matinee_ToggleSnap_40x", Icon40x40 ) );
		Set( "Matinee.FitSequence", new IMAGE_BRUSH( "Icons/icon_Matinee_FitSequence_40x", Icon40x40 ) );
		Set( "Matinee.FitViewToSelected", new IMAGE_BRUSH( "Icons/icon_Matinee_FitSelected_40x", Icon40x40 ) );
		Set( "Matinee.FitLoop", new IMAGE_BRUSH( "Icons/icon_Matinee_FitLoop_40x", Icon40x40 ) );
		Set( "Matinee.FitLoopSequence", new IMAGE_BRUSH( "Icons/icon_Matinee_FitLoopSequnce_40x", Icon40x40 ) );
		Set( "Matinee.ViewEndofTrack", new IMAGE_BRUSH( "Icons/icon_Matinee_EndOfTrack_40x", Icon40x40 ) );
		Set( "Matinee.ToggleSnapTimeToFrames", new IMAGE_BRUSH( "Icons/icon_Matinee_SnapTimeToFrames_40x", Icon40x40 ) );
		Set( "Matinee.FixedTimeStepPlayback", new IMAGE_BRUSH( "Icons/icon_Matinee_FixedTimeStepPlayback_40x", Icon40x40 ) );
		Set( "Matinee.ToggleGorePreview", new IMAGE_BRUSH( "Icons/icon_Matinee_GorePreview_40x", Icon40x40 ) );
		Set( "Matinee.CreateCameraActor", new IMAGE_BRUSH( "Icons/icon_Matinee_CreateCameraActor_40x", Icon40x40 ) );
		Set( "Matinee.LaunchRecordWindow", new IMAGE_BRUSH( "Icons/icon_Matinee_LaunchRecorderWindow_40x", Icon40x40 ) );
		Set( "Matinee.ToggleCurveEditor", new IMAGE_BRUSH("Icons/icon_MatineeCurveView_40px", Icon40x40) );
		Set( "Matinee.ToggleDirectorTimeline", new IMAGE_BRUSH("Icons/icon_MatineeDirectorView_40px", Icon40x40) );

		Set( "Matinee.AddKey.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_AddKey_40x", Icon20x20 ) );
		Set( "Matinee.CreateMovie.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_CreateMovie_40x", Icon20x20 ) );
		Set( "Matinee.Play.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_Play_40x", Icon20x20 ) );
		Set( "Matinee.PlayLoop.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_PlayLoopSection_40x", Icon20x20 ) );
		Set( "Matinee.Stop.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_Stop_40x", Icon20x20 ) );
		Set( "Matinee.PlayReverse.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_PlayReverse_40x", Icon20x20 ) );
		Set( "Matinee.ToggleSnap.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_ToggleSnap_40x", Icon20x20 ) );
		Set( "Matinee.FitSequence.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_FitSequence_40x", Icon20x20 ) );
		Set( "Matinee.FitViewToSelected.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_FitSelected_40x", Icon20x20 ) );
		Set( "Matinee.FitLoop.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_FitLoop_40x", Icon20x20 ) );
		Set( "Matinee.FitLoopSequence.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_FitLoopSequnce_40x", Icon20x20 ) );
		Set( "Matinee.ViewEndofTrack.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_EndOfTrack_40x", Icon20x20 ) );
		Set( "Matinee.ToggleSnapTimeToFrames.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_SnapTimeToFrames_40x", Icon20x20 ) );
		Set( "Matinee.FixedTimeStepPlayback.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_FixedTimeStepPlayback_40x", Icon20x20 ) );
		Set( "Matinee.ToggleGorePreview.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_GorePreview_40x", Icon20x20 ) );
		Set( "Matinee.CreateCameraActor.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_CreateCameraActor_40x", Icon20x20 ) );
		Set( "Matinee.LaunchRecordWindow.Small", new IMAGE_BRUSH( "Icons/icon_Matinee_LaunchRecorderWindow_40x", Icon20x20 ) );
		Set( "Matinee.ToggleCurveEditor.Small", new IMAGE_BRUSH("Icons/icon_MatineeCurveView_40px", Icon20x20) );
		Set( "Matinee.ToggleDirectorTimeline.Small", new IMAGE_BRUSH("Icons/icon_MatineeDirectorView_40px", Icon20x20) );
	}
#endif // WITH_EDITOR
	}

void FStarshipEditorStyle::FStyle::SetupSourceControlStyles()
{
	//Source Control
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set("SourceControl.StatusIcon.On", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check-circle", Icon16x16, FStyleColors::AccentGreen));
		Set("SourceControl.StatusIcon.Error", new CORE_IMAGE_BRUSH_SVG("Starship/Common/alert-circle", Icon16x16, FStyleColors::AccentYellow));
		Set("SourceControl.StatusIcon.Off", new CORE_IMAGE_BRUSH_SVG("Starship/Common/reject", Icon16x16, FStyleColors::Foreground));
		Set("SourceControl.StatusIcon.Unknown", new CORE_IMAGE_BRUSH_SVG("Starship/Common/help", Icon16x16, FStyleColors::AccentYellow));

		Set("SourceControl.ChangelistsTab", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check-circle", Icon16x16));
		Set("SourceControl.Changelist", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_Changelist", Icon16x16, FStyleColors::AccentRed));
		Set("SourceControl.ShelvedChangelist", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_Changelist", Icon16x16, FStyleColors::AccentBlue));

		Set("SourceControl.Add", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_ContentAdd",Icon16x16));
		Set("SourceControl.Edit", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_CheckedOut", Icon16x16));
		Set("SourceControl.Delete", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_MarkedForDelete", Icon16x16));
		Set("SourceControl.Branch", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_Branched", Icon16x16));
		Set("SourceControl.Integrate", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_Action_Integrate", Icon16x16));

		Set("SourceControl.LockOverlay", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_Lock", Icon16x16));

		Set("SourceControl.Settings.StatusBorder", new BOX_BRUSH( "Common/GroupBorder", FMargin(4.0f/16.0f), FLinearColor(0.5f,0.5f,0.5f,1.0f)  ) );
		Set("SourceControl.Settings.StatusFont", FTextBlockStyle(NormalText).SetFont(DEFAULT_FONT( "Bold", 12 ) ));


		Set("SourceControl.ProgressWindow.Warning", new IMAGE_BRUSH( "Icons/alert", Icon32x32) );

		// Menu commands
		Set("SourceControl.Actions.Sync", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/arrow-right", Icon16x16));
		Set("SourceControl.Actions.Submit", new CORE_IMAGE_BRUSH_SVG( "Starship/Common/arrow-left", Icon16x16));
		Set("SourceControl.Actions.Diff", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/SCC_Action_Diff", Icon16x16));
		Set("SourceControl.Actions.Revert", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/icon_SCC_Revert", Icon16x16));
		Set("SourceControl.Actions.Connect", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/icon_SCC_Connect", Icon16x16));
		Set("SourceControl.Actions.History", new CORE_IMAGE_BRUSH_SVG( "Starship/SourceControl/icon_SCC_History", Icon16x16));
		Set("SourceControl.Actions.Add", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16));
		Set("SourceControl.Actions.ChangeSettings", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/icon_SCC_Change_Source_Control_Settings", Icon16x16));
		Set("SourceControl.Actions.CheckOut", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16));
		Set("SourceControl.Actions.Refresh", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Undo", Icon16x16));
		
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// Perforce
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set("Perforce.CheckedOut", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16, FStyleColors::AccentRed));
		Set("Perforce.OpenForAdd", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ContentAdd", Icon16x16, FStyleColors::AccentRed));
		Set("Perforce.CheckedOutByOtherUser", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16, FStyleColors::AccentYellow));
		Set("Perforce.CheckedOutByOtherUserOtherBranch", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16, FStyleColors::AccentBlue));
		Set("Perforce.ModifiedOtherBranch", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ModifiedOtherBranch", Icon16x16, FStyleColors::AccentRed));
		Set("Perforce.MarkedForDelete", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_MarkedForDelete", Icon16x16, FStyleColors::AccentRed));
		Set("Perforce.NotAtHeadRevision", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ModifiedOtherBranch", Icon16x16, FStyleColors::AccentYellow));
		Set("Perforce.NotInDepot", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_NotInDepot", Icon16x16, FStyleColors::AccentYellow));
		Set("Perforce.Branched", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_Branched", Icon16x16, FStyleColors::AccentGreen));

	}
	// Subversion
	{
		Set("Subversion.CheckedOut", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16, FStyleColors::AccentRed));
		Set("Subversion.OpenForAdd", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ContentAdd", Icon16x16, FStyleColors::AccentRed));
		Set("Subversion.CheckedOutByOtherUser", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16, FStyleColors::AccentYellow));
		Set("Subversion.CheckedOutByOtherUserOtherBranch", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", Icon16x16, FStyleColors::AccentBlue));
		Set("Subversion.ModifiedOtherBranch", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ModifiedOtherBranch", Icon16x16, FStyleColors::AccentRed));
		Set("Subversion.MarkedForDelete", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_MarkedForDelete", Icon16x16, FStyleColors::AccentRed));
		Set("Subversion.NotAtHeadRevision", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ModifiedOtherBranch", Icon16x16, FStyleColors::AccentYellow));
		Set("Subversion.NotInDepot", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_NotInDepot", Icon16x16, FStyleColors::AccentYellow));
		Set("Subversion.Branched", new CORE_IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_Branched", Icon16x16, FStyleColors::AccentGreen));
	}
#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
}

void FStarshipEditorStyle::FStyle::SetupAutomationStyles()
{
	//Automation
#if WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)
	{
		Set( "Automation.Header" , FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Mono", 12 ) )
			.SetColorAndOpacity(FLinearColor(FColor(0xffffffff))) );

		Set( "Automation.Normal" , FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Mono", 9 ) )
			.SetColorAndOpacity(FLinearColor(FColor(0xffaaaaaa))) );

		Set( "Automation.Warning", FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Mono", 9 ) )
			.SetColorAndOpacity(FLinearColor(FColor(0xffbbbb44))) );

		Set( "Automation.Error"  , FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Mono", 9 ) )
			.SetColorAndOpacity(FLinearColor(FColor(0xffff0000))) );

		Set( "Automation.ReportHeader" , FTextBlockStyle(NormalText)
			.SetFont( DEFAULT_FONT( "Mono", 10 ) )
			.SetColorAndOpacity(FLinearColor(FColor(0xffffffff))) );
		
		//state of individual tests
		Set( "Automation.Success", new IMAGE_BRUSH( "Automation/Success", Icon16x16 ) );
		Set( "Automation.Warning", new IMAGE_BRUSH( "Automation/Warning", Icon16x16 ) );
		Set( "Automation.Fail", new IMAGE_BRUSH( "Automation/Fail", Icon16x16 ) );
		Set( "Automation.InProcess", new IMAGE_BRUSH( "Automation/InProcess", Icon16x16 ) );
		Set( "Automation.NotRun", new IMAGE_BRUSH( "Automation/NotRun", Icon16x16, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f) ) );
		Set( "Automation.NotEnoughParticipants", new IMAGE_BRUSH( "Automation/NotEnoughParticipants", Icon16x16 ) );
		Set( "Automation.ParticipantsWarning", new IMAGE_BRUSH( "Automation/ParticipantsWarning", Icon16x16 ) );
		Set( "Automation.Participant", new IMAGE_BRUSH( "Automation/Participant", Icon16x16 ) );
		
		//status as a regression test or not
		Set( "Automation.SmokeTest", new IMAGE_BRUSH( "Automation/SmokeTest", Icon16x16 ) );
		Set( "Automation.SmokeTestParent", new IMAGE_BRUSH( "Automation/SmokeTestParent", Icon16x16 ) );

		//run icons
		Set( "AutomationWindow.RunTests", new IMAGE_BRUSH( "Automation/RunTests", Icon40x40) );
		Set( "AutomationWindow.RefreshTests", new IMAGE_BRUSH( "Automation/RefreshTests", Icon40x40) );
		Set( "AutomationWindow.FindWorkers", new IMAGE_BRUSH( "Automation/RefreshWorkers", Icon40x40) );
		Set( "AutomationWindow.StopTests", new IMAGE_BRUSH( "Automation/StopTests", Icon40x40 ) );
		Set( "AutomationWindow.RunTests.Small", new IMAGE_BRUSH( "Automation/RunTests", Icon20x20) );
		Set( "AutomationWindow.RefreshTests.Small", new IMAGE_BRUSH( "Automation/RefreshTests", Icon20x20) );
		Set( "AutomationWindow.FindWorkers.Small", new IMAGE_BRUSH( "Automation/RefreshWorkers", Icon20x20) );
		Set( "AutomationWindow.StopTests.Small", new IMAGE_BRUSH( "Automation/StopTests", Icon20x20 ) );

		//filter icons
		Set( "AutomationWindow.ErrorFilter", new IMAGE_BRUSH( "Automation/ErrorFilter", Icon40x40) );
		Set( "AutomationWindow.WarningFilter", new IMAGE_BRUSH( "Automation/WarningFilter", Icon40x40) );
		Set( "AutomationWindow.SmokeTestFilter", new IMAGE_BRUSH( "Automation/SmokeTestFilter", Icon40x40) );
		Set( "AutomationWindow.DeveloperDirectoryContent", new IMAGE_BRUSH( "Automation/DeveloperDirectoryContent", Icon40x40) );
		Set( "AutomationWindow.ErrorFilter.Small", new IMAGE_BRUSH( "Automation/ErrorFilter", Icon20x20) );
		Set( "AutomationWindow.WarningFilter.Small", new IMAGE_BRUSH( "Automation/WarningFilter", Icon20x20) );
		Set( "AutomationWindow.SmokeTestFilter.Small", new IMAGE_BRUSH( "Automation/SmokeTestFilter", Icon20x20) );
		Set( "AutomationWindow.DeveloperDirectoryContent.Small", new IMAGE_BRUSH( "Automation/DeveloperDirectoryContent", Icon20x20) );
		Set( "AutomationWindow.TrackHistory", new IMAGE_BRUSH( "Automation/TrackTestHistory", Icon40x40) );

		//device group settings
		Set( "AutomationWindow.GroupSettings", new IMAGE_BRUSH( "Automation/Groups", Icon40x40) );
		Set( "AutomationWindow.GroupSettings.Small", new IMAGE_BRUSH( "Automation/Groups", Icon20x20) );

		//test preset icons
		Set( "AutomationWindow.PresetNew", new IMAGE_BRUSH( "Icons/icon_add_40x", Icon16x16 ) );
		Set( "AutomationWindow.PresetSave", new IMAGE_BRUSH_SVG("Starship/Common/SaveCurrent", Icon16x16));
		Set( "AutomationWindow.PresetRemove", new IMAGE_BRUSH( "Icons/icon_Cascade_DeleteLOD_40x", Icon16x16 ) );

		//test backgrounds
		Set( "AutomationWindow.GameGroupBorder", new BOX_BRUSH( "Automation/GameGroupBorder", FMargin(4.0f/16.0f) ) );
		Set( "AutomationWindow.EditorGroupBorder", new BOX_BRUSH( "Automation/EditorGroupBorder", FMargin(4.0f/16.0f) ) );
	}

	// Launcher
	{
		Set( "Launcher.Run", new IMAGE_BRUSH("Launcher/Launcher_Run", Icon40x40) );
		Set( "Launcher.EditSettings", new IMAGE_BRUSH("Launcher/Launcher_EditSettings", Icon40x40) );
		Set( "Launcher.Back", new IMAGE_BRUSH("Launcher/Launcher_Back", Icon32x32) );
		Set( "Launcher.Back.Small", new IMAGE_BRUSH("Launcher/Launcher_Back", Icon32x32));
		Set( "Launcher.Delete", new IMAGE_BRUSH("Launcher/Launcher_Delete", Icon32x32) );

		Set( "Launcher.Instance_Commandlet", new IMAGE_BRUSH( "Launcher/Instance_Commandlet", Icon25x25 ) );
		Set( "Launcher.Instance_Editor", new IMAGE_BRUSH( "Launcher/Instance_Editor", Icon25x25 ) );
		Set( "Launcher.Instance_Game", new IMAGE_BRUSH( "Launcher/Instance_Game", Icon25x25 ) );
		Set( "Launcher.Instance_Other", new IMAGE_BRUSH( "Launcher/Instance_Other", Icon25x25 ) );
		Set( "Launcher.Instance_Server", new IMAGE_BRUSH( "Launcher/Instance_Server", Icon25x25 ) );
		Set( "Launcher.Instance_Unknown", new IMAGE_BRUSH( "Launcher/Instance_Unknown", Icon25x25 ) );
		Set( "LauncherCommand.DeployBuild", new IMAGE_BRUSH( "Launcher/Launcher_Deploy", Icon40x40 ) );
		Set( "LauncherCommand.QuickLaunch", new IMAGE_BRUSH( "Launcher/Launcher_Launch", Icon40x40 ) );
		Set( "LauncherCommand.CreateBuild", new IMAGE_BRUSH( "Launcher/Launcher_Build", Icon40x40 ) );
		Set( "LauncherCommand.AdvancedBuild", new IMAGE_BRUSH( "Launcher/Launcher_Advanced", Icon40x40 ) );
		Set( "LauncherCommand.AdvancedBuild.Medium", new IMAGE_BRUSH("Launcher/Launcher_Advanced", Icon25x25) );
		Set( "LauncherCommand.AdvancedBuild.Small", new IMAGE_BRUSH("Launcher/Launcher_Advanced", Icon20x20) );

		Set("Launcher.Filters.Text", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 9))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		Set( "Launcher.Platform.Warning", new IMAGE_BRUSH( "Icons/alert", Icon24x24) );

#if DDPI_HAS_EXTENDED_PLATFORMINFO_DATA

		Set( "Launcher.Platform.AllPlatforms", new IMAGE_BRUSH( "Launcher/All_Platforms_24x", Icon24x24) );
		Set( "Launcher.Platform.AllPlatforms.Large", new IMAGE_BRUSH( "Launcher/All_Platforms_128x", Icon64x64) );
		Set( "Launcher.Platform.AllPlatforms.XLarge", new IMAGE_BRUSH( "Launcher/All_Platforms_128x", Icon128x128) );
		for (auto Pair : FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos())
		{
			const FDataDrivenPlatformInfo& PlatformInfo = Pair.Value;

			// some platforms may specify a "rooted" path in the platform extensions directory, so look for that case here, and use a different path for the brush
			FString NormalIconPath = PlatformInfo.GetIconPath(EPlatformIconSize::Normal);
			if(!NormalIconPath.IsEmpty())
			{
				if (NormalIconPath.StartsWith(TEXT("/Platforms/")))
				{
#define PLATFORM_IMAGE_BRUSH( PlatformPath, ... ) FSlateImageBrush( PlatformPath.Replace(TEXT("/Platforms/"), *FPaths::EnginePlatformExtensionsDir()) + TEXT(".png") , __VA_ARGS__ )
					Set(PlatformInfo.GetIconStyleName(EPlatformIconSize::Normal), new PLATFORM_IMAGE_BRUSH(NormalIconPath, Icon24x24));
					Set(PlatformInfo.GetIconStyleName(EPlatformIconSize::Large), new PLATFORM_IMAGE_BRUSH(PlatformInfo.GetIconPath(EPlatformIconSize::Large), Icon64x64));
					Set(PlatformInfo.GetIconStyleName(EPlatformIconSize::XLarge), new PLATFORM_IMAGE_BRUSH(PlatformInfo.GetIconPath(EPlatformIconSize::XLarge), Icon128x128));
				}
				else
				{
					Set(PlatformInfo.GetIconStyleName(EPlatformIconSize::Normal), new IMAGE_BRUSH(*NormalIconPath, Icon24x24));
					Set(PlatformInfo.GetIconStyleName(EPlatformIconSize::Large), new IMAGE_BRUSH(*PlatformInfo.GetIconPath(EPlatformIconSize::Large), Icon64x64));
					Set(PlatformInfo.GetIconStyleName(EPlatformIconSize::XLarge), new IMAGE_BRUSH(*PlatformInfo.GetIconPath(EPlatformIconSize::XLarge), Icon128x128));
				}
			}
		}
#endif

		Set("Launcher.NoHoverTableRow", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundHoveredBrush(FSlateNoResource())
			.SetOddRowBackgroundHoveredBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())
			.SetInactiveHoveredBrush(FSlateNoResource())
			);
	}

	// Device Manager
	{
		Set( "DeviceDetails.Claim", new IMAGE_BRUSH( "Icons/icon_DeviceClaim_40x", Icon40x40 ) );
		Set( "DeviceDetails.Claim.Small", new IMAGE_BRUSH( "Icons/icon_DeviceClaim_40x", Icon20x20 ) );
		Set( "DeviceDetails.Release", new IMAGE_BRUSH( "Icons/icon_DeviceRelease_40x", Icon40x40 ) );
		Set( "DeviceDetails.Release.Small", new IMAGE_BRUSH( "Icons/icon_DeviceRelease_40x", Icon20x20 ) );
		Set( "DeviceDetails.Remove", new IMAGE_BRUSH( "Icons/icon_DeviceRemove_40x", Icon40x40 ) );
		Set( "DeviceDetails.Remove.Small", new IMAGE_BRUSH( "Icons/icon_DeviceRemove_40x", Icon20x20 ) );
		Set( "DeviceDetails.Share", new IMAGE_BRUSH( "Icons/icon_DeviceShare_40x", Icon40x40 ) );
		Set( "DeviceDetails.Share.Small", new IMAGE_BRUSH( "Icons/icon_DeviceShare_40x", Icon20x20 ) );

		Set( "DeviceDetails.Connect", new IMAGE_BRUSH( "Icons/icon_DeviceConnect_40x", Icon40x40 ) );
		Set( "DeviceDetails.Connect.Small", new IMAGE_BRUSH( "Icons/icon_DeviceConnect_40x", Icon20x20 ) );
		Set( "DeviceDetails.Disconnect", new IMAGE_BRUSH( "Icons/icon_DeviceDisconnect_40x", Icon40x40 ) );
		Set( "DeviceDetails.Disconnect.Small", new IMAGE_BRUSH( "Icons/icon_DeviceDisconnect_40x", Icon20x20 ) );

		Set( "DeviceDetails.PowerOn", new IMAGE_BRUSH( "Icons/icon_DevicePowerOn_40x", Icon40x40 ) );
		Set( "DeviceDetails.PowerOn.Small", new IMAGE_BRUSH( "Icons/icon_DevicePowerOn_40x", Icon20x20 ) );
		Set( "DeviceDetails.PowerOff", new IMAGE_BRUSH( "Icons/icon_DevicePowerOff_40x", Icon40x40 ) );
		Set( "DeviceDetails.PowerOff.Small", new IMAGE_BRUSH( "Icons/icon_DevicePowerOff_40x", Icon20x20 ) );
		Set( "DeviceDetails.PowerOffForce", new IMAGE_BRUSH( "Icons/icon_DevicePowerOff_40x", Icon40x40 ) );
		Set( "DeviceDetails.PowerOffForce.Small", new IMAGE_BRUSH( "Icons/icon_DevicePowerOff_40x", Icon20x20 ) );
		Set( "DeviceDetails.Reboot", new IMAGE_BRUSH( "Icons/icon_DeviceReboot_40x", Icon40x40 ) );
		Set( "DeviceDetails.Reboot.Small", new IMAGE_BRUSH( "Icons/icon_DeviceReboot_40x", Icon20x20 ) );

		Set( "DeviceDetails.TabIcon", new IMAGE_BRUSH_SVG( "Starship/Common/DeviceManager", Icon16x16 ) );
		Set( "DeviceDetails.Tabs.Tools", new IMAGE_BRUSH( "/Icons/icon_tab_Tools_16x", Icon16x16 ) );
		Set( "DeviceDetails.Tabs.ProfileEditor", new IMAGE_BRUSH_SVG( "Starship/Common/DeviceProfiles", Icon16x16 ) );
		Set( "DeviceDetails.Tabs.ProfileEditorSingleProfile", new IMAGE_BRUSH( "/Icons/icon_tab_DeviceProfileEditor_16x", Icon16x16 ) );
	}

	// Settings Editor
	{
		Set( "SettingsEditor.Collision_Engine", new IMAGE_BRUSH("Icons/icon_Cascade_RestartSim_40x", Icon16x16));
		Set( "SettingsEditor.Collision_Game", new IMAGE_BRUSH_SVG("Starship/Common/Realtime", Icon16x16));

		// Settings editor
		Set("SettingsEditor.GoodIcon", new IMAGE_BRUSH("Settings/Settings_Good", Icon40x40));
		Set("SettingsEditor.WarningIcon", new IMAGE_BRUSH("Settings/Settings_Warning", Icon40x40));

		Set("SettingsEditor.CheckoutWarningBorder", new BOX_BRUSH( "Common/GroupBorderLight", FMargin(4.0f/16.0f) ) );

		Set("SettingsEditor.CatgoryAndSectionFont", DEFAULT_FONT("Regular", 18));
		Set("SettingsEditor.TopLevelObjectFontStyle", DEFAULT_FONT("Bold", 12));
	}

	{
		// Navigation defaults
		const FLinearColor NavHyperlinkColor(0.03847f, 0.33446f, 1.0f);
		const FTextBlockStyle NavigationHyperlinkText = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 12))
			.SetColorAndOpacity(NavHyperlinkColor);

		const FButtonStyle NavigationHyperlinkButton = FButtonStyle()
			.SetNormal(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0, 0, 0, 3 / 16.0f), NavHyperlinkColor))
			.SetPressed(FSlateNoResource())
			.SetHovered(BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0, 0, 0, 3 / 16.0f), NavHyperlinkColor));

		FHyperlinkStyle NavigationHyperlink = FHyperlinkStyle()
			.SetUnderlineStyle(NavigationHyperlinkButton)
			.SetTextStyle(NavigationHyperlinkText)
			.SetPadding(FMargin(0.0f));

		Set("NavigationHyperlink", NavigationHyperlink);
	}

#endif // WITH_EDITOR || (IS_PROGRAM && WITH_UNREAL_DEVELOPER_TOOLS)

	// External image picker
	{
		Set("ExternalImagePicker.BlankImage", new IMAGE_BRUSH( "Icons/BlankIcon", Icon16x16 ) );
		Set("ExternalImagePicker.ThumbnailShadow", new BOX_BRUSH( "ContentBrowser/ThumbnailShadow" , FMargin( 4.0f / 64.0f ) ) );
		Set("ExternalImagePicker.PickImageButton", new IMAGE_BRUSH( "Icons/ellipsis_12x", Icon12x12 ) );
	}


	{

		Set("FBXIcon.StaticMesh", new IMAGE_BRUSH("Icons/FBX/StaticMesh_16x", Icon16x16));
		Set("FBXIcon.SkeletalMesh", new IMAGE_BRUSH("Icons/FBX/SkeletalMesh_16x", Icon16x16));
		Set("FBXIcon.Animation", new IMAGE_BRUSH( "Icons/FBX/Animation_16px", Icon16x16 ) );
		Set("FBXIcon.ImportOptionsOverride", new IMAGE_BRUSH("Icons/FBX/FbxImportOptionsOverride_7x16px", Icon7x16));
		Set("FBXIcon.ImportOptionsDefault", new IMAGE_BRUSH("Icons/FBX/FbxImportOptionsDefault_7x16px", Icon7x16));

		Set("FBXIcon.ReimportAdded", new IMAGE_BRUSH("Icons/FBX/FbxReimportAdded_16x16px", Icon16x16));
		Set("FBXIcon.ReimportRemoved", new IMAGE_BRUSH("Icons/FBX/FbxReimportRemoved_16x16px", Icon16x16));
		Set("FBXIcon.ReimportSame", new IMAGE_BRUSH("Icons/FBX/FbxReimportSame_16x16px", Icon16x16));
		Set("FBXIcon.ReimportAddedContent", new IMAGE_BRUSH("Icons/FBX/FbxReimportAddedContent_16x16px", Icon16x16));
		Set("FBXIcon.ReimportRemovedContent", new IMAGE_BRUSH("Icons/FBX/FbxReimportRemovedContent_16x16px", Icon16x16));
		Set("FBXIcon.ReimportSameContent", new IMAGE_BRUSH("Icons/FBX/FbxReimportSameContent_16x16px", Icon16x16));
		Set("FBXIcon.ReimportError", new IMAGE_BRUSH("Icons/FBX/FbxReimportError_16x16px", Icon16x16));

		Set("FBXIcon.ReimportCompareAdd", new IMAGE_BRUSH("Icons/FBX/FbxReimportCompare-Add_16x16px", Icon16x16));
		Set("FBXIcon.ReimportCompareRemoved", new IMAGE_BRUSH("Icons/FBX/FbxReimportCompare-Remove_16x16px", Icon16x16));

		const FTextBlockStyle FBXLargeFont =
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 12))
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor::Black);

		Set("FBXLargeFont", FBXLargeFont);

		const FTextBlockStyle FBXMediumFont =
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 11))
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor::Black);

		Set("FBXMediumFont", FBXMediumFont);

		const FTextBlockStyle FBXSmallFont =
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor::Black);

		Set("FBXSmallFont", FBXSmallFont);
	}

	// Asset Dialog
	{
		Set("AssetDialog.ErrorLabelBorder", new FSlateColorBrush(FStyleColors::AccentRed));
	}
}

void FStarshipEditorStyle::FStyle::SetupUMGEditorStyles()
{
	Set("WidgetDesigner.LayoutTransform", new IMAGE_BRUSH("Icons/UMG/Layout_TransformMode_16x", Icon16x16));
	Set("WidgetDesigner.LayoutTransform.Small", new IMAGE_BRUSH("Icons/UMG/Layout_TransformMode_16x", Icon16x16));
	Set("WidgetDesigner.RenderTransform", new IMAGE_BRUSH("Icons/UMG/Render_TransformMode_16x", Icon16x16));
	Set("WidgetDesigner.RenderTransform.Small", new IMAGE_BRUSH("Icons/UMG/Render_TransformMode_16x", Icon16x16));
	Set("WidgetDesigner.ToggleOutlines", new IMAGE_BRUSH("Icons/UMG/ToggleOutlines.Small", Icon16x16));
	Set("WidgetDesigner.ToggleRespectLocks", new CORE_IMAGE_BRUSH_SVG("Starship/Common/lock", Icon16x16));

	Set("WidgetDesigner.ToggleLocalizationPreview", new IMAGE_BRUSH("Icons/icon_localization_white_16x", Icon16x16));

	Set("WidgetDesigner.LocationGridSnap", new IMAGE_BRUSH_SVG("Starship/EditorViewport/grid", Icon16x16));
	Set("WidgetDesigner.RotationGridSnap", new IMAGE_BRUSH("Old/LevelEditor/RotationGridSnap", Icon16x16));

	Set("WidgetDesigner.WidgetVisible", new IMAGE_BRUSH("/Icons/icon_layer_visible", Icon16x16));
	Set("WidgetDesigner.WidgetHidden", new IMAGE_BRUSH("/Icons/icon_layer_not_visible", Icon16x16));

	Set("UMGEditor.ZoomToFit", new IMAGE_BRUSH("GenericCurveEditor/Icons/FramingSelected_48x", Icon16x16));

	Set("UMGEditor.ScreenOutline", new BOX_BRUSH(TEXT("Icons/UMG/ScreenOutline"), FMargin(0.25f) ));

	Set("UMGEditor.TransformHandle", new IMAGE_BRUSH("Icons/UMG/TransformHandle", Icon8x8));
	Set("UMGEditor.ResizeAreaHandle", new IMAGE_BRUSH("Icons/UMG/ResizeAreaHandle", Icon20x20));

	Set("UMGEditor.AnchorGizmo.Center", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/center", Icon16x16));
	Set("UMGEditor.AnchorGizmo.Center.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/center", Icon16x16, FLinearColor(0, 1, 0)));
	
	Set("UMGEditor.AnchorGizmo.Left", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/left", FVector2D(32, 16)));
	Set("UMGEditor.AnchorGizmo.Left.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/left", FVector2D(32, 16), FLinearColor(0, 1, 0)));
	Set("UMGEditor.AnchorGizmo.Right", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/right", FVector2D(32, 16)));
	Set("UMGEditor.AnchorGizmo.Right.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/right", FVector2D(32, 16), FLinearColor(0, 1, 0)));
	
	Set("UMGEditor.AnchorGizmo.Top", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/top", FVector2D(16, 32)));
	Set("UMGEditor.AnchorGizmo.Top.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/top", FVector2D(16, 32), FLinearColor(0, 1, 0)));
	Set("UMGEditor.AnchorGizmo.Bottom", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/bottom", FVector2D(16, 32)));
	Set("UMGEditor.AnchorGizmo.Bottom.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/bottom", FVector2D(16, 32), FLinearColor(0, 1, 0)));

	Set("UMGEditor.AnchorGizmo.TopLeft", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/topleft", FVector2D(24, 24)));
	Set("UMGEditor.AnchorGizmo.TopLeft.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/topleft", FVector2D(24, 24), FLinearColor(0, 1, 0)));

	Set("UMGEditor.AnchorGizmo.TopRight", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/topright", FVector2D(24, 24)));
	Set("UMGEditor.AnchorGizmo.TopRight.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/topright", FVector2D(24, 24), FLinearColor(0, 1, 0)));

	Set("UMGEditor.AnchorGizmo.BottomLeft", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/bottomleft", FVector2D(24, 24)));
	Set("UMGEditor.AnchorGizmo.BottomLeft.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/bottomleft", FVector2D(24, 24), FLinearColor(0, 1, 0)));

	Set("UMGEditor.AnchorGizmo.BottomRight", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/bottomright", FVector2D(24, 24)));
	Set("UMGEditor.AnchorGizmo.BottomRight.Hovered", new IMAGE_BRUSH("Icons/UMG/AnchorGizmo/bottomright", FVector2D(24, 24), FLinearColor(0, 1, 0)));


	Set("UMGEditor.AnchoredWidget", new BOX_BRUSH("Common/Button", FVector2D(32, 32), 8.0f / 32.0f));
	Set("UMGEditor.AnchoredWidgetAlignment", new IMAGE_BRUSH("Icons/icon_tab_DeviceManager_16x", Icon8x8));
	

	Set("UMGEditor.PaletteHeader", FTableRowStyle()
		.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Header))
		.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Header))
		.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Header))
		.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Header))
		.SetSelectorFocusedBrush(FSlateNoResource())
		.SetActiveBrush(FSlateNoResource())
		.SetActiveHoveredBrush(FSlateNoResource())
		.SetInactiveBrush(FSlateNoResource())
		.SetInactiveHoveredBrush(FSlateNoResource())
		.SetTextColor(DefaultForeground)
		.SetSelectedTextColor(InvertedForeground)
	);

	// Style of the favorite toggle
	const FCheckBoxStyle UMGEditorFavoriteToggleStyle = FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
		.SetUncheckedImage(IMAGE_BRUSH("Icons/EmptyStar_16x", Icon10x10, FLinearColor(0.8f, 0.8f, 0.8f, 1.f)))
		.SetUncheckedHoveredImage(IMAGE_BRUSH("Icons/EmptyStar_16x", Icon10x10, FLinearColor(2.5f, 2.5f, 2.5f, 1.f)))
		.SetUncheckedPressedImage(IMAGE_BRUSH("Icons/EmptyStar_16x", Icon10x10, FLinearColor(0.8f, 0.8f, 0.8f, 1.f)))
		.SetCheckedImage(IMAGE_BRUSH("Icons/Star_16x", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)))
		.SetCheckedHoveredImage(IMAGE_BRUSH("Icons/Star_16x", Icon10x10, FLinearColor(0.4f, 0.4f, 0.4f, 1.f)))
		.SetCheckedPressedImage(IMAGE_BRUSH("Icons/Star_16x", Icon10x10, FLinearColor(0.2f, 0.2f, 0.2f, 1.f)));
	Set("UMGEditor.Palette.FavoriteToggleStyle", UMGEditorFavoriteToggleStyle);

	Set("HorizontalAlignment_Left", new IMAGE_BRUSH("Icons/UMG/Alignment/Horizontal_Left", Icon16x16));
	Set("HorizontalAlignment_Center", new IMAGE_BRUSH("Icons/UMG/Alignment/Horizontal_Center", Icon16x16));
	Set("HorizontalAlignment_Right", new IMAGE_BRUSH("Icons/UMG/Alignment/Horizontal_Right", Icon16x16));
	Set("HorizontalAlignment_Fill", new IMAGE_BRUSH("Icons/UMG/Alignment/Horizontal_Fill", Icon16x16));

	Set("VerticalAlignment_Top", new IMAGE_BRUSH("Icons/UMG/Alignment/Vertical_Top", Icon16x16));
	Set("VerticalAlignment_Center", new IMAGE_BRUSH("Icons/UMG/Alignment/Vertical_Center", Icon16x16));
	Set("VerticalAlignment_Bottom", new IMAGE_BRUSH("Icons/UMG/Alignment/Vertical_Bottom", Icon16x16));
	Set("VerticalAlignment_Fill", new IMAGE_BRUSH("Icons/UMG/Alignment/Vertical_Fill", Icon16x16));

	const FTextBlockStyle NoAnimationFont =
		FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 18))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D(1, 1))
		.SetShadowColorAndOpacity(FLinearColor::Black);


	Set("UMGEditor.AddAnimationIcon", new IMAGE_BRUSH("Icons/PlusSymbol_12x", Icon12x12, FLinearColor(.05,.05,.05) ) );
	Set("UMGEditor.NoAnimationFont", NoAnimationFont);

	Set("UMGEditor.SwitchToDesigner", new IMAGE_BRUSH("UMG/Designer_40x", Icon20x20));

	Set("UMGEditor.AnchorGrid", new IMAGE_BRUSH("Icons/UMG/AnchorGrid", Icon10x10, FLinearColor(.1f, .1f, .1f, 0.5f), ESlateBrushTileType::Both ));

	Set("UMGEditor.DPISettings", new IMAGE_BRUSH("Icons/UMG/SettingsButton", Icon16x16));

	Set("UMGEditor.DesignerMessageBorder", new BOX_BRUSH("/UMG/MessageRoundedBorder", FMargin(18.0f / 64.0f)));

	Set("UMGEditor.OrientLandscape", new IMAGE_BRUSH("Icons/UMG/Icon_Landscape_v2", Icon16x16));
	Set("UMGEditor.OrientPortrait", new IMAGE_BRUSH("Icons/UMG/Icon_Portrait_v2", Icon16x16));
	Set("UMGEditor.Mirror", new IMAGE_BRUSH("Icons/UMG/Icon_Mirror_v3", Icon16x16));

	Set("UMGEditor.ResizeResolutionFont", DEFAULT_FONT("Bold", 10));
	Set("UMGEditor.CategoryIcon", new IMAGE_BRUSH("Icons/hiererchy_16x", Icon16x16));
}

void FStarshipEditorStyle::FStyle::SetupTranslationEditorStyles()
{
	Set("TranslationEditor.Export", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_40x", Icon40x40));
	Set("TranslationEditor.PreviewInEditor", new IMAGE_BRUSH("Icons/icon_levels_visible_40x", Icon40x40));
	Set("TranslationEditor.Import", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("TranslationEditor.Search", new IMAGE_BRUSH("Icons/icon_Blueprint_Find_40px", Icon40x40));
	Set("TranslationEditor.TranslationPicker", new IMAGE_BRUSH("Icons/icon_StaticMeshEd_VertColor_40x", Icon40x40));
	Set("TranslationEditor.ImportLatestFromLocalizationService", new IMAGE_BRUSH("Icons/icon_worldscript_40x", Icon40x40));
}


void FStarshipEditorStyle::FStyle::SetupLocalizationDashboardStyles()
{
	Set("LocalizationDashboard.MenuIcon", new IMAGE_BRUSH_SVG("Starship/Common/LocalizationDashboard", Icon16x16));

	Set("LocalizationDashboard.GatherTextAllTargets", new IMAGE_BRUSH("Icons/Icon_Localisation_Gather_All_40x", Icon40x40));
	Set("LocalizationDashboard.ImportTextAllTargetsAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("LocalizationDashboard.ExportTextAllTargetsAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_40x", Icon40x40));
	Set("LocalizationDashboard.ImportDialogueAllTargetsAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("LocalizationDashboard.ImportDialogueScriptAllTargetsAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("LocalizationDashboard.ExportDialogueScriptAllTargetsAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_40x", Icon40x40));
	Set("LocalizationDashboard.CountWordsForAllTargets", new IMAGE_BRUSH("Icons/Icon_Localisation_Refresh_Word_Counts_40x", Icon40x40));
	Set("LocalizationDashboard.CompileTextAllTargetsAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Compile_Translations_40x", Icon40x40));

	Set("LocalizationDashboard.GatherTextAllTargets.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Gather_All_16x", Icon16x16));
	Set("LocalizationDashboard.ImportTextAllTargetsAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationDashboard.ExportTextAllTargetsAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationDashboard.ImportDialogueAllTargetsAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationDashboard.ImportDialogueScriptAllTargetsAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationDashboard.ExportDialogueScriptAllTargetsAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationDashboard.CountWordsForAllTargets.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Refresh_Word_Counts_16x", Icon16x16));
	Set("LocalizationDashboard.CompileTextAllTargetsAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Compile_Translations_16x", Icon16x16));

	Set("LocalizationDashboard.GatherTextTarget", new IMAGE_BRUSH("Icons/Icon_Localisation_Gather_All_16x", Icon16x16));
	Set("LocalizationDashboard.ImportTextAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationDashboard.ExportTextAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationDashboard.ImportDialogueAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationDashboard.ImportDialogueScriptAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationDashboard.ExportDialogueScriptAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationDashboard.CountWordsForTarget", new IMAGE_BRUSH("Icons/Icon_Localisation_Refresh_Word_Counts_16x", Icon16x16));
	Set("LocalizationDashboard.CompileTextAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Compile_Translations_16x", Icon16x16));
	Set("LocalizationDashboard.DeleteTarget", new IMAGE_BRUSH("Icons/Cross_12x", Icon12x12 ) );

	Set("LocalizationTargetEditor.GatherText", new IMAGE_BRUSH("Icons/Icon_Localisation_Gather_All_40x", Icon40x40));
	Set("LocalizationTargetEditor.ImportTextAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("LocalizationTargetEditor.ExportTextAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_40x", Icon40x40));
	Set("LocalizationTargetEditor.ImportDialogueAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("LocalizationTargetEditor.ImportDialogueScriptAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_40x", Icon40x40));
	Set("LocalizationTargetEditor.ExportDialogueScriptAllCultures", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_40x", Icon40x40));
	Set("LocalizationTargetEditor.CountWords", new IMAGE_BRUSH("Icons/Icon_Localisation_Refresh_Word_Counts_40x", Icon40x40));
	Set("LocalizationTargetEditor.CompileTextAllCultures", new IMAGE_BRUSH( "Icons/Icon_Localisation_Compile_Translations_40x", Icon40x40));

	Set("LocalizationTargetEditor.GatherText.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Gather_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ImportTextAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ExportTextAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ImportDialogueAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ImportDialogueScriptAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ExportDialogueScriptAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.CountWords.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Refresh_Word_Counts_16x", Icon16x16));
	Set("LocalizationTargetEditor.CompileTextAllCultures.Small", new IMAGE_BRUSH("Icons/Icon_Localisation_Compile_Translations_16x", Icon16x16));

	Set("LocalizationTargetEditor.DirectoryPicker", new IMAGE_BRUSH( "Icons/ellipsis_12x", Icon12x12 ));
	Set("LocalizationTargetEditor.GatherSettingsIcon_Valid", new IMAGE_BRUSH("Settings/Settings_Good", Icon16x16));
	Set("LocalizationTargetEditor.GatherSettingsIcon_Warning", new IMAGE_BRUSH("Settings/Settings_Warning", Icon16x16));

	Set("LocalizationTargetEditor.NativeCulture", new IMAGE_BRUSH( "Icons/Star_16x", Icon16x16 ) );

	Set("LocalizationTargetEditor.EditTranslations", new IMAGE_BRUSH("Icons/icon_file_open_16px", Icon16x16));
	Set("LocalizationTargetEditor.ImportTextCulture", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ExportTextCulture", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ImportDialogueScriptCulture", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ExportDialogueScriptCulture", new IMAGE_BRUSH("Icons/Icon_Localisation_Export_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.ImportDialogueCulture", new IMAGE_BRUSH("Icons/Icon_Localisation_Import_All_16x", Icon16x16));
	Set("LocalizationTargetEditor.CompileTextCulture", new IMAGE_BRUSH("Icons/Icon_Localisation_Compile_Translations_16x", Icon16x16));
	Set("LocalizationTargetEditor.DeleteCulture", new IMAGE_BRUSH("Icons/Cross_12x", Icon12x12 ) );

	Set("LocalizationTargetEditor.GatherSettings.AddMetaDataTextKeyPatternArgument", new IMAGE_BRUSH("Icons/icon_Blueprint_AddVariable_40px", Icon16x16 ) );

	Set( "LocalizationDashboard.CommandletLog.Text", FTextBlockStyle(NormalText)
		.SetFont( DEFAULT_FONT( "Regular", 8 ) )
		.SetShadowOffset( FVector2D::ZeroVector )
		);
}

void FStarshipEditorStyle::FStyle::SetupMyBlueprintStyles()
{
	Set("MyBlueprint.DeleteEntry", new IMAGE_BRUSH("Icons/Edit/icon_Edit_Delete_40x", Icon16x16));
}

void FStarshipEditorStyle::FStyle::SetupStatusBarStyle()
{
	const FButtonStyle StatusBarButton = FButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.SetNormalForeground(FStyleColors::Foreground)
		.SetNormalPadding(FMargin(2, 2, 2, 2))
		.SetPressedPadding(FMargin(2, 3, 2, 1));

	const FComboButtonStyle StatusBarComboButton = FComboButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
		.SetDownArrowImage(FSlateNoResource())
		.SetButtonStyle(StatusBarButton);
		
	Set("StatusBar.StatusBarButton", StatusBarButton);
	Set("StatusBar.StatusBarComboButton", StatusBarComboButton);

	Set("StatusBar.ContentBrowserUp", new CORE_IMAGE_BRUSH_SVG("Starship/Common/chevron-up", Icon16x16));
	Set("StatusBar.ContentBrowserDown", new CORE_IMAGE_BRUSH_SVG("Starship/Common/chevron-down", Icon16x16));
	Set("StatusBar.DrawerShadow", new BOX_BRUSH("Starship/StatusBar/drawer-shadow-bottom", FMargin(10/64.0f, 20/64.f, 10/64.f, 0), FLinearColor(0,0,0,1)));
	Set("StatusBar.DrawerBackground", new FSlateRoundedBoxBrush(FStyleColors::Panel, 5.0f));

	Set("StatusBar.Height", 32.0f);
	Set("StatusBar.Background", new FSlateColorBrush(FStyleColors::Panel));
	Set("StatusBar.ProgressOverlay", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 2.0f, FStyleColors::Panel, 1.0f, FVector2D(20, 8)));

	Set("StatusBar.HelpIcon", new CORE_IMAGE_BRUSH_SVG("Starship/Common/help", Icon16x16, FStyleColors::Foreground));

	FToolBarStyle StatusBarToolBarStyle = FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FToolBarStyle>("SlimToolBar");

	StatusBarToolBarStyle.SetBackground(FSlateNoResource());
	StatusBarToolBarStyle.SetLabelPadding(FMargin(5, 5, 0, 5));
	StatusBarToolBarStyle.SetIconSize(Icon16x16);
	Set("StatusBarToolBar", StatusBarToolBarStyle);

	Set("StatusBar.Message.MessageText",
		FTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetColorAndOpacity(FStyleColors::Foreground));

	Set("StatusBar.Message.HintText",
		FTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetColorAndOpacity(FStyleColors::Primary));
}

#undef DEFAULT_FONT
#undef ICON_FONT
#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
