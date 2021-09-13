// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styling/WidgetReflectorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"

// This is to fix the issue that SlateStyleMacros like IMAGE_BRUSH look for RootToContentDir but StyleSet->RootToContentDir is how this style is set up
#define RootToContentDir StyleSet->RootToContentDir

TSharedPtr< FSlateStyleSet > FWidgetReflectorStyle::StyleInstance = nullptr;

void FWidgetReflectorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FWidgetReflectorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FWidgetReflectorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("WidgetReflectorStyleStyle"));
	return StyleSetName;
}

TSharedRef< FSlateStyleSet > FWidgetReflectorStyle::Create()
{
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);

	TSharedRef<FSlateStyleSet> StyleSet = MakeShareable(new FSlateStyleSet(FWidgetReflectorStyle::GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

#if WITH_EDITOR
	{
		FButtonStyle Button = FButtonStyle()
			.SetNormal(FSlateBoxBrush(RootToContentDir("Common/ButtonHoverHint.png"), FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.15f)))
			.SetHovered(FSlateBoxBrush(RootToContentDir("Common/ButtonHoverHint.png"), FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.25f)))
			.SetPressed(FSlateBoxBrush(RootToContentDir("Common/ButtonHoverHint.png"), FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.30f)))
			.SetNormalPadding(FMargin(0, 0, 0, 1))
			.SetPressedPadding(FMargin(0, 1, 0, 0));
		StyleSet->Set("Button", Button);

		FCheckBoxStyle CustomCheckBoxStyle = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox");
		CustomCheckBoxStyle.SetUncheckedImage(CustomCheckBoxStyle.UncheckedHoveredImage);
		CustomCheckBoxStyle.UncheckedImage.TintColor = FLinearColor(1, 1, 1, 0.1f);	
		StyleSet->Set("CheckBox", CustomCheckBoxStyle);

		CustomCheckBoxStyle.SetUncheckedHoveredImage(Button.Hovered);
		StyleSet->Set("CheckBoxNoHover", CustomCheckBoxStyle);

		FComboButtonStyle ComboButton = FComboButtonStyle()
			.SetButtonStyle(Button.SetNormal(FSlateNoResource()))
			.SetDownArrowImage(FSlateImageBrush(StyleSet->RootToCoreContentDir(TEXT("Common/ComboArrow.png")), Icon8x8))
			.SetMenuBorderBrush(FSlateBoxBrush(StyleSet->RootToCoreContentDir(TEXT("Old/Menu_Background.png")), FMargin(8.0f / 64.0f)))
			.SetMenuBorderPadding(FMargin(0.0f));
		StyleSet->Set("ComboButton", ComboButton);

		StyleSet->Set("SplitterDark", FSplitterStyle()
			.SetHandleNormalBrush(FSlateColorBrush(FLinearColor(FColor(32, 32, 32))))
			.SetHandleHighlightBrush(FSlateColorBrush(FLinearColor(FColor(96, 96, 96))))
		);
	}
	
	{
		StyleSet->Set("Icon.FocusPicking", new FSlateImageBrush(RootToContentDir("Icons/SlateReflector/FocusPicking_24x.png"), Icon24x24));
		StyleSet->Set("Icon.HitTestPicking", new FSlateImageBrush(RootToContentDir("Icons/SlateReflector/HitTestPicking_24x.png"), Icon24x24));
		StyleSet->Set("Icon.VisualPicking", new FSlateImageBrush(RootToContentDir("Icons/SlateReflector/VisualPicking_24x.png"), Icon24x24));

		StyleSet->Set("Symbols.LeftArrow", new FSlateImageBrush(RootToContentDir("Common/LeftArrow.png"), Icon24x24));
		StyleSet->Set("Symbols.RightArrow", new FSlateImageBrush(RootToContentDir("Common/SubmenuArrow.png"), Icon24x24));
		StyleSet->Set("Symbols.UpArrow", new FSlateImageBrush(RootToContentDir("Common/UpArrow.png"), Icon24x24));
		StyleSet->Set("Symbols.DownArrow", new FSlateImageBrush(RootToContentDir("Common/DownArrow.png"), Icon24x24));

		StyleSet->Set("WidgetReflector.TabIcon", new IMAGE_BRUSH_SVG("Starship/Common/Widget", Icon16x16));
	}
#endif

	return StyleSet;
}

const ISlateStyle& FWidgetReflectorStyle::Get()
{
	return *StyleInstance;
}
