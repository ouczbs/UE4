// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimationBlendSpaceGridWidget.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/BlendSpace1D.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "IDetailsView.h"
#include "UObject/StructOnScope.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"

#include "Customization/BlendSampleDetails.h"
#include "AssetData.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Classes/EditorStyleSettings.h"

#include "Widgets/Input/SButton.h"
#include "Fonts/FontMeasure.h"
#include "Modules/ModuleManager.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "SAnimationBlendSpaceGridWidget"

void SBlendSpaceGridWidget::Construct(const FArguments& InArgs)
{
	BlendSpaceBase = InArgs._BlendSpaceBase;
	PreviousBlendSpaceBase = BlendSpaceBase.Get();
	Position = InArgs._Position;
	FilteredPosition = InArgs._FilteredPosition;
	NotifyHook = InArgs._NotifyHook;
	OnSampleAdded = InArgs._OnSampleAdded;
	OnSampleMoved = InArgs._OnSampleMoved;
	OnSampleRemoved = InArgs._OnSampleRemoved;
	OnSampleReplaced = InArgs._OnSampleReplaced;
	OnSampleDoubleClicked = InArgs._OnSampleDoubleClicked;
	OnGetBlendSpaceSampleName = InArgs._OnGetBlendSpaceSampleName;
	OnExtendSampleTooltip = InArgs._OnExtendSampleTooltip;
	bReadOnly = InArgs._ReadOnly;
	bShowAxisLabels = InArgs._ShowAxisLabels;
	bShowSettingsButtons = InArgs._ShowSettingsButtons;
	StatusBarName = InArgs._StatusBarName;

	GridType = (BlendSpaceBase.Get() != nullptr && BlendSpaceBase.Get()->IsA<UBlendSpace1D>()) ? EGridType::SingleAxis : EGridType::TwoAxis;
	BlendParametersToDraw = (GridType == EGridType::SingleAxis) ? 1 : 2;
	
	HighlightedSampleIndex = SelectedSampleIndex = DraggedSampleIndex = ToolTipSampleIndex = INDEX_NONE;
	DragState = EDragState::None;
	// Initialize flags 
	bPreviewPositionSet = true;
	bHighlightPreviewPin = false;
	// Initialize preview value to center or the grid
	PreviewPosition.X = BlendSpaceBase.Get() != nullptr ? (BlendSpaceBase.Get()->GetBlendParameter(0).GetRange() * .5f) + BlendSpaceBase.Get()->GetBlendParameter(0).Min : 0.0f;
	PreviewPosition.Y = BlendSpaceBase.Get() != nullptr ? (GridType == EGridType::TwoAxis ? (BlendSpaceBase.Get()->GetBlendParameter(1).GetRange() * .5f) + BlendSpaceBase.Get()->GetBlendParameter(1).Min : 0.0f) : 0.0f;
	PreviewPosition.Z = 0.0f;

	PreviewFilteredPosition = PreviewPosition;

	bShowTriangulation = false;
	bMouseIsOverGeometry = false;
	bRefreshCachedData = true;
	bStretchToFit = true;
	bShowAnimationNames = false;

	InvalidSamplePositionDragDropText = FText::FromString(TEXT("Invalid Sample Position"));

	// Retrieve UI color values
	KeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Regular");
	HighlightKeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Highlight");
	SelectKeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Pressed");
	PreDragKeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Pressed");
	DragKeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Drag");
	InvalidColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Invalid");
	DropKeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Drop");
	PreviewKeyColor = FEditorStyle::GetSlateColor("BlendSpaceKey.Preview");
	UnSnappedColor = FEditorStyle::GetSlateColor("BlendSpaceKey.UnSnapped");
	GridLinesColor = GetDefault<UEditorStyleSettings>()->RegularColor;
	GridOutlineColor = GetDefault<UEditorStyleSettings>()->RuleColor;
	TriangulationColor = FSlateColor(EStyleColor::Foreground);
	
	// Retrieve background and sample key brushes 
	BackgroundImage = FEditorStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
	KeyBrush = FEditorStyle::GetBrush("CurveEd.CurveKey");
	PreviewBrush = FEditorStyle::GetBrush("BlendSpaceEditor.PreviewIcon");
	ArrowBrushes[(uint8)EArrowDirection::Up] = FEditorStyle::GetBrush("BlendSpaceEditor.ArrowUp");
	ArrowBrushes[(uint8)EArrowDirection::Down] = FEditorStyle::GetBrush("BlendSpaceEditor.ArrowDown");
	ArrowBrushes[(uint8)EArrowDirection::Right] = FEditorStyle::GetBrush("BlendSpaceEditor.ArrowRight");
	ArrowBrushes[(uint8)EArrowDirection::Left] = FEditorStyle::GetBrush("BlendSpaceEditor.ArrowLeft");
	LabelBrush = FEditorStyle::GetBrush(TEXT("BlendSpaceEditor.LabelBackground"));
	
	// Retrieve font data 
	FontInfo = FEditorStyle::GetFontStyle("CurveEd.InfoFont");

	// Initialize UI layout values
	KeySize = FVector2D(11.0f, 11.0f);
	PreviewSize = FVector2D(21.0f, 21.0f);
	DragThreshold = 9.0f;
	ClickAndHighlightThreshold = 12.0f;
	TextMargin = 8.0f;
	GridMargin = bShowAxisLabels ?  FMargin(MaxVerticalAxisTextWidth + (TextMargin * 2.0f), TextMargin, (HorizontalAxisMaxTextWidth *.5f) + TextMargin, MaxHorizontalAxisTextHeight + (TextMargin * 2.0f)) : 
									FMargin(TextMargin, TextMargin, TextMargin, TextMargin);

	bPreviewToolTipHidden = false;

	const bool bShowInputBoxLabel = true;
	// Widget construction
	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()				
				[
					SNew(SBorder)
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.DesiredSizeScale(FVector2D(1.0f, 1.0f))
					.Padding_Lambda([&]() { return FMargin(GridMargin.Left + 6, GridMargin.Top + 6, 0, 0) + GridRatioMargin; })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("NoBorder"))
								.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetTriangulationButtonVisibility)))		
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText(LOCTEXT("ShowTriangulation", "Show Triangulation"))
									.OnClicked(this, &SBlendSpaceGridWidget::ToggleTriangulationVisibility)
									.ButtonColorAndOpacity_Lambda([this]() -> FLinearColor { return bShowTriangulation ? FEditorStyle::GetSlateColor("SelectionColor").GetSpecifiedColor() : FLinearColor::White; })
									.ContentPadding(1)
									[
										SNew(SImage)
										.Image(FEditorStyle::GetBrush("BlendSpaceEditor.ToggleTriangulation"))
										.ColorAndOpacity(FSlateColor::UseForeground())
									]
								]
							]
	
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("NoBorder"))
								.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetAnimationNamesButtonVisibility)))
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText(LOCTEXT("ShowAnimationNames", "Show Sample Names"))
									.OnClicked(this, &SBlendSpaceGridWidget::ToggleShowAnimationNames)
									.ButtonColorAndOpacity_Lambda([this]() -> FLinearColor { return bShowAnimationNames ? FEditorStyle::GetSlateColor("SelectionColor").GetSpecifiedColor() : FLinearColor::White; })
									.ContentPadding(1)
									[
										SNew(SImage)
										.Image(FEditorStyle::GetBrush("BlendSpaceEditor.ToggleLabels"))
										.ColorAndOpacity(FSlateColor::UseForeground())
									]
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("NoBorder"))
								.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetFittingButtonVisibility)))
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ToolTipText(this, &SBlendSpaceGridWidget::GetFittingTypeButtonToolTipText)
									.OnClicked(this, &SBlendSpaceGridWidget::ToggleFittingType)
									.ContentPadding(1)
									.ButtonColorAndOpacity_Lambda([this]() -> FLinearColor { return bStretchToFit ? FEditorStyle::GetSlateColor("SelectionColor").GetSpecifiedColor() : FLinearColor::White; })
									[
										SNew(SImage)
										.Image(FEditorStyle::GetBrush("BlendSpaceEditor.ZoomToFit"))
										.ColorAndOpacity(FSlateColor::UseForeground())
									]
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("NoBorder"))
								.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetInputBoxVisibility, 0)))
								.VAlign(VAlign_Center)
								[
									CreateGridEntryBox(0, bShowInputBoxLabel).ToSharedRef()
								]
							]
	
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("NoBorder"))
								.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetInputBoxVisibility, 1)))
								.VAlign(VAlign_Center)
								[
									CreateGridEntryBox(1, bShowInputBoxLabel).ToSharedRef()
								]
							]
						]
						
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f, 3.0f, 0.0f, 0.0f ))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BlendSpaceSamplesToolTip", "Drag and Drop Animations from the Asset Browser to place Sample Points"))
							.Font(FEditorStyle::GetFontStyle(TEXT("AnimViewport.MessageFont")))
							.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.7f))
							.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetSampleToolTipVisibility)))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f, 3.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BlendspacePreviewToolTip", "Hold Shift to move the Preview Point (Green)" ))
							.Font(FEditorStyle::GetFontStyle(TEXT("AnimViewport.MessageFont")))
							.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.7f))
							.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlendSpaceGridWidget::GetPreviewToolTipVisibility)))
						]
					]
				]
			]
		]
	];

	SAssignNew(ToolTip, SToolTip)
	.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.Background"))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SBlendSpaceGridWidget::GetToolTipAnimationName)
			.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SBlendSpaceGridWidget::GetToolTipSampleValue)
			.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ToolTipExtensionContainer, SBox)
		]
	];

	if(Position.IsSet())
	{
		StartPreviewing();
	}
}

SBlendSpaceGridWidget::~SBlendSpaceGridWidget()
{
	EnableStatusBarMessage(false);
}

TSharedPtr<SWidget> SBlendSpaceGridWidget::CreateGridEntryBox(const int32 BoxIndex, const bool bShowLabel)
{
	return SNew(SNumericEntryBox<float>)
		.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
		.Value(this, &SBlendSpaceGridWidget::GetInputBoxValue, BoxIndex)
		.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
		.OnValueCommitted(this, &SBlendSpaceGridWidget::OnInputBoxValueCommited, BoxIndex)
		.OnValueChanged(this, &SBlendSpaceGridWidget::OnInputBoxValueChanged, BoxIndex, true)
		.LabelVAlign(VAlign_Center)
		.AllowSpin(true)
		.MinValue(this, &SBlendSpaceGridWidget::GetInputBoxMinValue, BoxIndex)
		.MaxValue(this, &SBlendSpaceGridWidget::GetInputBoxMaxValue, BoxIndex)
		.MinSliderValue(this, &SBlendSpaceGridWidget::GetInputBoxMinValue, BoxIndex)
		.MaxSliderValue(this, &SBlendSpaceGridWidget::GetInputBoxMaxValue, BoxIndex)
		.MinDesiredValueWidth(60.0f)
		.Label()
		[
			SNew(STextBlock)
			.Visibility(bShowLabel ? EVisibility::Visible : EVisibility::Collapsed)
			.Text_Lambda([=]() { return (BoxIndex == 0) ? ParameterXName : ParameterYName; })
		];
}

int32 SBlendSpaceGridWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled());
	
	PaintBackgroundAndGrid(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);

	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		if (bShowTriangulation)
		{
			PaintTriangulation(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
		}	
		PaintSampleKeys(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);

		if(bShowAxisLabels)
		{
			PaintAxisText(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
		}

		if (bShowAnimationNames)
		{
			PaintAnimationNames(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
		}
	}

	return LayerId;
}

void SBlendSpaceGridWidget::PaintBackgroundAndGrid(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	// Fill the background
	FSlateDrawElement::MakeBox( OutDrawElements, DrawLayerId + 1, AllottedGeometry.ToPaintGeometry(), BackgroundImage );

	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		// Create the grid
		const FVector2D GridSize = CachedGridRectangle.GetSize();
		const FVector2D GridOffset = CachedGridRectangle.GetTopLeft();
		TArray<FVector2D> LinePoints;

		// Draw grid lines
		LinePoints.SetNumZeroed(2);
		const FVector2D StartVectors[2] = { FVector2D(1.0f, 0.0f), FVector2D(0.0f, 1.0f) };
		const FVector2D OffsetVectors[2] = { FVector2D(0.0f, GridSize.Y), FVector2D(GridSize.X, 0.0f) };
		for (uint32 ParameterIndex = 0; ParameterIndex < BlendParametersToDraw; ++ParameterIndex)
		{
			const FBlendParameter& BlendParameter = BlendSpace->GetBlendParameter(ParameterIndex);
			const float Steps = GridSize[ParameterIndex] / ( BlendParameter.GridNum);

			for (int32 Index = 1; Index < BlendParameter.GridNum; ++Index)
			{			
				// Calculate line points
				LinePoints[0] = ((Index * Steps) * StartVectors[ParameterIndex]) + GridOffset;
				LinePoints[1] = LinePoints[0] + OffsetVectors[ParameterIndex];

				FSlateDrawElement::MakeLines( OutDrawElements, DrawLayerId + 2, AllottedGeometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, GridLinesColor, true);
			}
		}

		// Draw outer grid lines separately (this will avoid missing lines with 1D blend spaces)
		LinePoints.SetNumZeroed(5);

		// Top line
		LinePoints[0] = GridOffset;

		LinePoints[1] = GridOffset;
		LinePoints[1].X += GridSize.X;

		LinePoints[2] = GridOffset;
		LinePoints[2].X += GridSize.X;
		LinePoints[2].Y += GridSize.Y;

		LinePoints[3] = GridOffset;
		LinePoints[3].Y += GridSize.Y;

		LinePoints[4] = GridOffset;

		FSlateDrawElement::MakeLines( OutDrawElements, DrawLayerId + 3, AllottedGeometry.ToPaintGeometry(),	LinePoints, ESlateDrawEffect::None, GridOutlineColor, true, 2.0f);

	}
	
	DrawLayerId += 3;
}

void SBlendSpaceGridWidget::PaintSampleKeys(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	const int32 FilteredPositionLayer = DrawLayerId + 1;
	const int32 PreviewPositionLayer = DrawLayerId + 2;
	const int32 SampleLayer = DrawLayerId + 3;
	
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		// Draw keys
		const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
		for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
		{
			const FBlendSample& Sample = Samples[SampleIndex];
		
			FLinearColor DrawColor = KeyColor.GetSpecifiedColor();
			if (DraggedSampleIndex == SampleIndex)
			{
				DrawColor = (DragState == EDragState::PreDrag) ? PreDragKeyColor.GetSpecifiedColor() : DragKeyColor.GetSpecifiedColor();
			}
			else if (SelectedSampleIndex == SampleIndex)
			{
				DrawColor = SelectKeyColor.GetSpecifiedColor();
			}
			else if (HighlightedSampleIndex == SampleIndex)
			{
				DrawColor = HighlightKeyColor.GetSpecifiedColor();
			}
			else if(!Sample.bIsValid)
			{
				DrawColor = InvalidColor.GetSpecifiedColor();
			}
			else
			{
				DrawColor = Sample.bSnapToGrid ? DrawColor : UnSnappedColor.GetSpecifiedColor();
			}

			const FVector2D GridPosition = SampleValueToGridPosition(Sample.SampleValue) - (KeySize * 0.5f);
			FSlateDrawElement::MakeBox( OutDrawElements, SampleLayer, AllottedGeometry.ToPaintGeometry(GridPosition, KeySize), KeyBrush, ESlateDrawEffect::None, DrawColor );
		}

		// Always draw the filtered position which comes back from whatever is running
		{
			FVector2D GridPosition = SampleValueToGridPosition(PreviewFilteredPosition) - (PreviewSize * .5f);
			FSlateDrawElement::MakeBox(OutDrawElements, FilteredPositionLayer, AllottedGeometry.ToPaintGeometry(GridPosition, PreviewSize), PreviewBrush, ESlateDrawEffect::None, PreviewKeyColor.GetSpecifiedColor() * 0.5);
		}

		if (bPreviewPositionSet)
		{
			FVector2D GridPosition = SampleValueToGridPosition(PreviewPosition) - (PreviewSize * .5f);
			FSlateDrawElement::MakeBox(OutDrawElements, PreviewPositionLayer, AllottedGeometry.ToPaintGeometry(GridPosition, PreviewSize), PreviewBrush, ESlateDrawEffect::None, PreviewKeyColor.GetSpecifiedColor());
		}

		if (DragState == EDragState::DragDrop || DragState == EDragState::InvalidDragDrop)
		{
			const FVector2D GridPoint = SnapToClosestGridPoint(LocalMousePosition) - (KeySize * .5f);
			FSlateDrawElement::MakeBox( OutDrawElements, SampleLayer, AllottedGeometry.ToPaintGeometry(GridPoint, KeySize), KeyBrush, ESlateDrawEffect::None,
				(DragState == EDragState::DragDrop) ? DropKeyColor.GetSpecifiedColor() : InvalidColor.GetSpecifiedColor() );
		}
	}

	DrawLayerId += 3;
}

void SBlendSpaceGridWidget::PaintAxisText(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D GridCenter = CachedGridRectangle.GetCenter();

	// X axis
	FString Text = ParameterXName.ToString();
	FVector2D TextSize = FontMeasure->Measure(Text, FontInfo);

	// arrow left
	
	FVector2D ArrowSize = ArrowBrushes[(uint8)EArrowDirection::Left]->GetImageSize();
	FVector2D TextPosition = FVector2D(GridCenter.X - (TextSize.X * .5f), CachedGridRectangle.Bottom + TextMargin + (ArrowSize.Y * .25f));
	FVector2D ArrowPosition = FVector2D(TextPosition.X - ArrowSize.X - 10.f/* give padding*/, TextPosition.Y);
	FSlateDrawElement::MakeBox(OutDrawElements, DrawLayerId + 1, AllottedGeometry.ToPaintGeometry(ArrowPosition, ArrowSize), ArrowBrushes[(uint8)EArrowDirection::Left], ESlateDrawEffect::None, FLinearColor::White);

	// Label
	FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(TextPosition, FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Text, FontInfo, ESlateDrawEffect::None, FLinearColor::White);

	// arrow right
	ArrowSize = ArrowBrushes[(uint8)EArrowDirection::Right]->GetImageSize();
	ArrowPosition = FVector2D(TextPosition.X + TextSize.X + 10.f/* give padding*/, TextPosition.Y);
	FSlateDrawElement::MakeBox(OutDrawElements, DrawLayerId + 1, AllottedGeometry.ToPaintGeometry(ArrowPosition, ArrowSize), ArrowBrushes[(uint8)EArrowDirection::Right], ESlateDrawEffect::None, FLinearColor::White);

	Text = FString::SanitizeFloat(SampleValueMin.X);
	TextSize = FontMeasure->Measure(Text, FontInfo);

	// Minimum value
	FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(FVector2D(CachedGridRectangle.Left - (TextSize.X * .5f), CachedGridRectangle.Bottom + TextMargin + (TextSize.Y * .25f)), FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Text, FontInfo, ESlateDrawEffect::None, FLinearColor::White);

	Text = FString::SanitizeFloat(SampleValueMax.X);
	TextSize = FontMeasure->Measure(Text, FontInfo);

	// Maximum value
	FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(FVector2D(CachedGridRectangle.Right - (TextSize.X * .5f), CachedGridRectangle.Bottom + TextMargin + (TextSize.Y * .25f)), FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Text, FontInfo, ESlateDrawEffect::None, FLinearColor::White);

	// Only draw Y axis labels if this is a 2D grid
	if (GridType == EGridType::TwoAxis)
	{
		// Y axis
		Text = ParameterYName.ToString();
		TextSize = FontMeasure->Measure(Text, FontInfo);

		// arrow up
		ArrowSize = ArrowBrushes[(uint8)EArrowDirection::Up]->GetImageSize();
		TextPosition = FVector2D(((GridMargin.Left - TextSize.X) * 0.5f - (ArrowSize.X * .25f)) + GridRatioMargin.Left, GridCenter.Y - (TextSize.Y * .5f));
		ArrowPosition = FVector2D(TextPosition.X + TextSize.X * 0.5f - ArrowSize.X * 0.5f, TextPosition.Y - ArrowSize.Y - 10.f/* give padding*/);
		FSlateDrawElement::MakeBox(OutDrawElements, DrawLayerId + 1, AllottedGeometry.ToPaintGeometry(ArrowPosition, ArrowSize), ArrowBrushes[(uint8)EArrowDirection::Up], ESlateDrawEffect::None, FLinearColor::White);

		// Label
		FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(TextPosition, FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Text,	FontInfo, ESlateDrawEffect::None, FLinearColor::White);

		// arrow down
		ArrowSize = ArrowBrushes[(uint8)EArrowDirection::Down]->GetImageSize();
		ArrowPosition = FVector2D(TextPosition.X + TextSize.X * 0.5f - ArrowSize.X * 0.5f, TextPosition.Y + TextSize.Y + 10.f/* give padding*/);
		FSlateDrawElement::MakeBox(OutDrawElements, DrawLayerId + 1, AllottedGeometry.ToPaintGeometry(ArrowPosition, ArrowSize), ArrowBrushes[(uint8)EArrowDirection::Down], ESlateDrawEffect::None, FLinearColor::White);

		Text = FString::SanitizeFloat(SampleValueMin.Y);
		TextSize = FontMeasure->Measure(Text, FontInfo);

		// Minimum value
		FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(FVector2D(((GridMargin.Left - TextSize.X) * 0.5f - (TextSize.X * .25f)) + GridRatioMargin.Left, CachedGridRectangle.Bottom - (TextSize.Y * .5f)), FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Text, FontInfo, ESlateDrawEffect::None, FLinearColor::White);

		Text = FString::SanitizeFloat(SampleValueMax.Y);
		TextSize = FontMeasure->Measure(Text, FontInfo);

		// Maximum value
		FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(FVector2D(((GridMargin.Left - TextSize.X) * 0.5f - (TextSize.X * .25f) ) + GridRatioMargin.Left, ( GridMargin.Top + GridRatioMargin.Top) - (TextSize.Y * .5f)), FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Text, FontInfo, ESlateDrawEffect::None, FLinearColor::White);
	}

	DrawLayerId += 1;
}

void SBlendSpaceGridWidget::PaintTriangulation(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
		const TArray<FEditorElement>& EditorElements = BlendSpace->GetGridSamples();
		
		for (const FEditorElement& Element : EditorElements)
		{
			for (int32 SourceIndex = 0; SourceIndex < 3; ++SourceIndex)
			{
				if (Element.Indices[SourceIndex] != INDEX_NONE)
				{
					const FBlendSample& SourceSample = Samples[Element.Indices[SourceIndex]];
					for (int32 TargetIndex = 0; TargetIndex < 3; ++TargetIndex)
					{
						if (Element.Indices[TargetIndex] != INDEX_NONE)
						{
							if (TargetIndex != SourceIndex)
							{
								const FBlendSample& TargetSample = Samples[Element.Indices[TargetIndex]];
								TArray<FVector2D> Points;

								Points.Add(SampleValueToGridPosition(SourceSample.SampleValue));
								Points.Add(SampleValueToGridPosition(TargetSample.SampleValue));

								// Draw line from and to element
								FSlateDrawElement::MakeLines(OutDrawElements, DrawLayerId + 1, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, TriangulationColor.GetSpecifiedColor(), true, 0.5f);
							}
						}
					}
				}
			}
		}
	}

	DrawLayerId += 1;
}

FText SBlendSpaceGridWidget::GetSampleName(const FBlendSample& InBlendSample, int32 InSampleIndex) const
{
	if(OnGetBlendSpaceSampleName.IsBound())
	{
		return FText::FromName(OnGetBlendSpaceSampleName.Execute(InSampleIndex));
	}
	else
	{
		if(InBlendSample.Animation != nullptr)
		{
			return FText::FromString(InBlendSample.Animation->GetName());
		}
	}
		
	return LOCTEXT("NoAnimationSetTooltipText", "No Animation Set");
}

void SBlendSpaceGridWidget::PaintAnimationNames(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D GridCenter = CachedGridRectangle.GetCenter();
		const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
		for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
		{
			const FBlendSample& Sample = Samples[SampleIndex];

			const FText Name = FText::Format(LOCTEXT("SampleNameFormat", "{0} ({1})"), GetSampleName(Sample, SampleIndex), FText::AsNumber(SampleIndex));
			const FVector2D TextSize = FontMeasure->Measure(Name, FontInfo);

			FVector2D GridPosition = SampleValueToGridPosition(Sample.SampleValue);
			// Check on which side of the sample the text should be positioned so that we don't run out of geometry space
			if ((GridPosition + TextSize).X > AllottedGeometry.GetLocalSize().X)
			{
				GridPosition -= FVector2D(TextSize.X + KeySize.X, KeySize.X * .5f);
			}
			else
			{
				GridPosition += FVector2D(KeySize.X, -KeySize.X * .5f);
			}

			FSlateDrawElement::MakeBox(OutDrawElements, DrawLayerId + 1, AllottedGeometry.MakeChild(FVector2D(GridPosition.X - 6, GridPosition.Y - 2), TextSize + FVector2D(8.0f, 4.0f)).ToPaintGeometry(), LabelBrush, ESlateDrawEffect::None, FLinearColor::Black);
			FSlateDrawElement::MakeText(OutDrawElements, DrawLayerId + 2, AllottedGeometry.MakeChild(FVector2D(GridPosition.X, GridPosition.Y), FVector2D(1.0f, 1.0f)).ToPaintGeometry(), Name, FontInfo, ESlateDrawEffect::None, FLinearColor::White);
		}
	}

	DrawLayerId += 2;
}

FReply SBlendSpaceGridWidget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			// Check if we are in dropping state and if so snap to the grid and try to add the sample
			if (DragState == EDragState::DragDrop || DragState == EDragState::InvalidDragDrop || DragState == EDragState::DragDropOverride)
			{
				if (DragState == EDragState::DragDrop)
				{
					const FVector SampleValue = SnapToClosestSamplePoint(LocalMousePosition);
					TSharedPtr<FAssetDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
					if (DragDropOperation.IsValid())
					{
						UAnimSequence* Animation = FAssetData::GetFirstAsset<UAnimSequence>(DragDropOperation->GetAssets());
						OnSampleAdded.ExecuteIfBound(Animation, SampleValue);
					}	
				}
				else if (DragState == EDragState::DragDropOverride)
				{
					TSharedPtr<FAssetDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
					if (DragDropOperation.IsValid())
					{
						UAnimSequence* Animation = FAssetData::GetFirstAsset<UAnimSequence>(DragDropOperation->GetAssets());
						int32 DroppedSampleIndex = GetClosestSamplePointIndexToMouse();
						OnSampleReplaced.ExecuteIfBound(DroppedSampleIndex, Animation);
					}
				}

				DragState = EDragState::None;
			}

			DragDropAnimationSequence = nullptr;
			DragDropAnimationName = FText::GetEmpty();
			HoveredAnimationName = FText::GetEmpty();
		}
	}

	return FReply::Unhandled();
}

void SBlendSpaceGridWidget::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (DragDropEvent.GetOperationAs<FAssetDragDropOp>().IsValid())
			{
				DragState = IsValidDragDropOperation(DragDropEvent, InvalidDragDropText) ? EDragState::DragDrop : EDragState::InvalidDragDrop;
			}
		}
	}
}

FReply SBlendSpaceGridWidget::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (DragState == EDragState::DragDrop || DragState == EDragState::InvalidDragDrop || DragState == EDragState::DragDropOverride)
			{		
				LocalMousePosition = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());				
		
				// Always update the tool tip, in case it became invalid
				TSharedPtr<FAssetDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
				if (DragDropOperation.IsValid())
				{
					DragDropOperation->SetToolTip(GetToolTipSampleValue(), DragDropOperation->GetIcon());
				}		

				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

void SBlendSpaceGridWidget::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (DragState == EDragState::DragDrop || DragState == EDragState::InvalidDragDrop || DragState == EDragState::DragDropOverride)
			{
				DragState = EDragState::None;
				DragDropAnimationSequence = nullptr;
				DragDropAnimationName = FText::GetEmpty();
				HoveredAnimationName = FText::GetEmpty();
			}
		}
	}
}

FReply SBlendSpaceGridWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (this->HasMouseCapture())
			{
				if (DragState == EDragState::None || DragState == EDragState::PreDrag)
				{
					ProcessClick(MyGeometry, MouseEvent);
				}
				else if (DragState == EDragState::DragSample)
				{
					// Process drag ending			
					ResetToolTip();
				}

				// Reset drag state and index
				DragState = EDragState::None;
				DraggedSampleIndex = INDEX_NONE;

				return FReply::Handled().ReleaseMouseCapture();
			}
			else
			{
				return ProcessClick(MyGeometry, MouseEvent);
			}
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceGridWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				// If we are over a sample, make it our currently (dragged) sample
				if (HighlightedSampleIndex != INDEX_NONE)
				{
					DraggedSampleIndex = SelectedSampleIndex = HighlightedSampleIndex;
					HighlightedSampleIndex = INDEX_NONE;
					ResetToolTip();
					DragState = EDragState::PreDrag;
					MouseDownPosition = LocalMousePosition;

					// Start mouse capture
					return FReply::Handled().CaptureMouse(SharedThis(this));
				}		
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceGridWidget::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				if (SelectedSampleIndex != INDEX_NONE)
				{
					OnSampleDoubleClicked.ExecuteIfBound(SelectedSampleIndex);
				}

				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceGridWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		if(!bReadOnly)
		{
			EnableStatusBarMessage(true);
		}

		// Cache the mouse position in local and screen space
		LocalMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		LastMousePosition = MouseEvent.GetScreenSpacePosition();

		if(!bReadOnly)
		{
			if (this->HasMouseCapture())
			{
				if (DragState == EDragState::None)
				{
					if (HighlightedSampleIndex != INDEX_NONE)
					{
						DragState = EDragState::DragSample;
						DraggedSampleIndex = HighlightedSampleIndex;
						HighlightedSampleIndex = INDEX_NONE;
						return FReply::Handled();
					}
				}
				else if (DragState == EDragState::PreDrag)
				{
					// Actually start dragging
					if ((LocalMousePosition - MouseDownPosition).SizeSquared() > DragThreshold)
					{
						DragState = EDragState::DragSample;
						HighlightedSampleIndex = INDEX_NONE;
						ShowToolTip();
						return FReply::Handled();
					}
				}
			}
			else if (IsHovered() && bMouseIsOverGeometry)
			{
				if (MouseEvent.IsLeftShiftDown() || MouseEvent.IsRightShiftDown())
				{
					StartPreviewing();
					DragState = EDragState::Preview;
					// Make tool tip visible (this will display the current preview sample value)
					ShowToolTip();			

					// Set flag for showing advanced preview info in tooltip
					bAdvancedPreview = MouseEvent.IsLeftControlDown() || MouseEvent.IsRightControlDown();
					return FReply::Handled();
				}
				else if(Position.IsSet())
				{
					StartPreviewing();
					DragState = EDragState::None;
					ShowToolTip();

					// Set flag for showing advanced preview info in tooltip
					bAdvancedPreview = MouseEvent.IsLeftControlDown() || MouseEvent.IsRightControlDown();
					return FReply::Handled();
				}
				else if (bSamplePreviewing)
				{
					StopPreviewing();
					DragState = EDragState::None;
					ResetToolTip();
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceGridWidget::ProcessClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				SelectedSampleIndex = INDEX_NONE;

				if (HighlightedSampleIndex == INDEX_NONE)
				{
					// If there isn't any sample currently being highlighted, retrieve all of them and see if we are over one
					SelectedSampleIndex = GetClosestSamplePointIndexToMouse();
				}
				else
				{
					// If we are over a sample, make it the selected sample index
					SelectedSampleIndex = HighlightedSampleIndex;
					HighlightedSampleIndex = INDEX_NONE;			
				}
			}
			else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
				auto PushMenu = [this, &MouseEvent](TSharedPtr<SWidget> InMenuContent)
				{
					if (InMenuContent.IsValid())
					{
						const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
						const FVector2D MousePosition = MouseEvent.GetScreenSpacePosition();
						// This is of a fixed size atm since MenuContent->GetDesiredSize() will not take the detail customization into account and return an incorrect (small) size
						const FVector2D ExpectedSize(300, 100);				
						const FVector2D MenuPosition = FSlateApplication::Get().CalculatePopupWindowPosition(FSlateRect(MousePosition.X, MousePosition.Y, MousePosition.X, MousePosition.Y), ExpectedSize, false);

						FSlateApplication::Get().PushMenu(
							AsShared(),
							WidgetPath,
							InMenuContent.ToSharedRef(),
							MenuPosition,
							FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
							);
					}
				};

				// If we are over a sample open a context menu for editing its data
				if (HighlightedSampleIndex != INDEX_NONE)
				{	
					SelectedSampleIndex = HighlightedSampleIndex;

					// Create context menu
					TSharedPtr<SWidget> MenuContent = CreateBlendSampleContextMenu();
			
					// Reset highlight sample index
					HighlightedSampleIndex = INDEX_NONE;

					PushMenu(MenuContent);

					return FReply::Handled().SetUserFocus(MenuContent.ToSharedRef(), EFocusCause::SetDirectly).ReleaseMouseCapture();
				}
				else
				{
					TSharedPtr<SWidget> MenuContent = CreateNewBlendSampleContextMenu(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
				
					PushMenu(MenuContent);
				
					return FReply::Handled().SetUserFocus(MenuContent.ToSharedRef(), EFocusCause::SetDirectly).ReleaseMouseCapture();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceGridWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			// Start previewing when either one of the shift keys is pressed
			if (IsHovered() && bMouseIsOverGeometry)
			{
				if ((InKeyEvent.GetKey() == EKeys::LeftShift) || (InKeyEvent.GetKey() == EKeys::RightShift))
				{
					StartPreviewing();
					DragState = EDragState::Preview;
					// Make tool tip visible (this will display the current preview sample value)
					ShowToolTip();
					return FReply::Handled();
				}
		
				// Set flag for showing advanced preview info in tooltip
				if ((InKeyEvent.GetKey() == EKeys::LeftControl) || (InKeyEvent.GetKey() == EKeys::RightControl))
				{
					bAdvancedPreview = true;
					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SBlendSpaceGridWidget::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{	
	if(!bReadOnly)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			// Stop previewing when shift keys are released 
			if ((InKeyEvent.GetKey() == EKeys::LeftShift) || (InKeyEvent.GetKey() == EKeys::RightShift))
			{
				StopPreviewing();
				DragState = EDragState::None;
				ResetToolTip();
				return FReply::Handled();
			}

			if((InKeyEvent.GetKey() == EKeys::LeftControl) || (InKeyEvent.GetKey() == EKeys::RightControl))
			{
				bAdvancedPreview = false;
				return FReply::Handled();
			}

			// If delete is pressed and we currently have a sample selected remove it from the blendspace
			if (InKeyEvent.GetKey() == EKeys::Delete)
			{
				if (SelectedSampleIndex != INDEX_NONE)
				{
					OnSampleRemoved.ExecuteIfBound(SelectedSampleIndex);
			
					if (SelectedSampleIndex == HighlightedSampleIndex)
					{
						HighlightedSampleIndex = INDEX_NONE;
						ResetToolTip();
					}

					SelectedSampleIndex = INDEX_NONE;
				}
			}

			// Pressing esc will remove the current key selection
			if( InKeyEvent.GetKey() == EKeys::Escape)
			{
				SelectedSampleIndex = INDEX_NONE;
			}
		}
	}

	return FReply::Unhandled();
}

void SBlendSpaceGridWidget::MakeViewContextMenuEntries(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.BeginSection("ViewOptions", LOCTEXT("ViewOptionsMenuHeader", "View Options"));
	{
		InMenuBuilder.AddMenuEntry(
			LOCTEXT("ShowTriangulation", "Show Triangulation"),
			LOCTEXT("ShowTriangulationToolTip", "Show the Delaunay triangulation for all blend space samples"),
			FSlateIcon("EditorStyle", "BlendSpaceEditor.ToggleTriangulation"),
			FUIAction(
				FExecuteAction::CreateLambda([this](){ bShowTriangulation = !bShowTriangulation; }),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){ return bShowTriangulation ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		InMenuBuilder.AddMenuEntry(
			LOCTEXT("ShowAnimationNames", "Show Sample Names"),
			LOCTEXT("ShowAnimationNamesToolTip", "Show the names of each of the samples"),
			FSlateIcon("EditorStyle", "BlendSpaceEditor.ToggleLabels"),
			FUIAction(
				FExecuteAction::CreateLambda([this](){ bShowAnimationNames = !bShowAnimationNames; }),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){ return bShowAnimationNames ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		InMenuBuilder.AddMenuEntry(
			LOCTEXT("StretchFittingText", "Stretch Grid to Fit"),
			LOCTEXT("StretchFittingTextToolTip", "Whether to stretch the grid to fit or to fit the grid to the largest axis"),
			FSlateIcon("EditorStyle", "BlendSpaceEditor.ZoomToFit"),
			FUIAction(
				FExecuteAction::CreateLambda([this](){ bStretchToFit = !bStretchToFit; }),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){ return bStretchToFit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	InMenuBuilder.EndSection();
}

TSharedPtr<SWidget> SBlendSpaceGridWidget::CreateBlendSampleContextMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);
	
	TSharedPtr<IStructureDetailsView> StructureDetailsView;
	// Initialize details view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NotifyHook = NotifyHook;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;		
	}
	
	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = true;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = true;
	}
	
	StructureDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
		.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr, LOCTEXT("SampleData", "Blend Sample"));
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			const FBlendSample& Sample = BlendSpace->GetBlendSample(HighlightedSampleIndex);		
			StructureDetailsView->GetDetailsView()->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FBlendSampleDetails::MakeInstance, BlendSpace, this, HighlightedSampleIndex));

			FStructOnScope* Struct = new FStructOnScope(FBlendSample::StaticStruct(), (uint8*)&Sample);
			Struct->SetPackage(BlendSpace->GetOutermost());
			StructureDetailsView->SetStructureData(MakeShareable(Struct));
		}
	}
	
	MenuBuilder.BeginSection("Sample", LOCTEXT("SampleMenuHeader", "Sample"));
	{
		MenuBuilder.AddWidget(StructureDetailsView->GetWidget().ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	MakeViewContextMenuEntries(MenuBuilder);

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SBlendSpaceGridWidget::CreateNewBlendSampleContextMenu(const FVector2D& InMousePosition)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FVector NewSampleValue;
	if(FSlateApplication::Get().GetModifierKeys().IsAltDown())
	{
		const FVector2D GridPosition(FMath::Clamp(InMousePosition.X, CachedGridRectangle.Left, CachedGridRectangle.Right),
									 FMath::Clamp(InMousePosition.Y, CachedGridRectangle.Top, CachedGridRectangle.Bottom));
		NewSampleValue = GridPositionToSampleValue(GridPosition, false);
	}
	else
	{
		NewSampleValue = GridPositionToSampleValue(SnapToClosestGridPoint(InMousePosition), false);
	}

	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		MenuBuilder.BeginSection("Sample", LOCTEXT("SampleMenuHeader", "Sample"));
		{
			if(!BlendSpace->IsAsset())
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AddNewSample", "Add New Sample"),
					LOCTEXT("AddNewSampleTooltip", "Add a new sample to this blendspace at this location"),
						FSlateIcon("EditorStyle", "Plus"),
						FUIAction(
							FExecuteAction::CreateLambda([this, NewSampleValue](){ OnSampleAdded.ExecuteIfBound(nullptr, NewSampleValue); })
						)
				);
			}
		}
		MenuBuilder.EndSection();
	}

	MakeViewContextMenuEntries(MenuBuilder);

	return MenuBuilder.MakeWidget();
}

FReply SBlendSpaceGridWidget::ToggleTriangulationVisibility()
{
	bShowTriangulation = !bShowTriangulation;
	return FReply::Handled();
}

void SBlendSpaceGridWidget::CalculateGridPoints()
{
	CachedGridPoints.Empty(SampleGridDivisions.X * SampleGridDivisions.Y);
	CachedSamplePoints.Empty(SampleGridDivisions.X * SampleGridDivisions.Y);
	if (SampleGridDivisions.X <= 0 || (GridType == EGridType::TwoAxis && SampleGridDivisions.Y <= 0))
	{
		return;
	}
	for (int32 GridY = 0; GridY < ((GridType == EGridType::TwoAxis) ? SampleGridDivisions.Y + 1 : 1); ++GridY)
	{
		for (int32 GridX = 0; GridX < SampleGridDivisions.X + 1; ++GridX)
		{
			// Calculate grid point in 0-1 form
			FVector2D GridPoint = FVector2D(GridX * (1.0f / SampleGridDivisions.X), (GridType == EGridType::TwoAxis) ? GridY * (1.0f /
				SampleGridDivisions.Y) : 0.5f);

			// Multiply with size and offset according to the grid layout
			GridPoint *= CachedGridRectangle.GetSize();
			GridPoint += CachedGridRectangle.GetTopLeft();
			CachedGridPoints.Add(GridPoint);

			CachedSamplePoints.Add(FVector(SampleValueMin.X + (GridX * (SampleValueRange.X / SampleGridDivisions.X)),
				(GridType == EGridType::TwoAxis) ? SampleValueMax.Y - (GridY * (SampleValueRange.Y / SampleGridDivisions.Y)) : 0.0f, 0.0f));
		}
	}
}

const FVector2D SBlendSpaceGridWidget::SnapToClosestGridPoint(const FVector2D& InPosition) const
{
	const int32 GridPointIndex = FindClosestGridPointIndex(InPosition);
	return CachedGridPoints[GridPointIndex];
}

const FVector SBlendSpaceGridWidget::SnapToClosestSamplePoint(const FVector2D& InPosition) const
{
	const int32 GridPointIndex = FindClosestGridPointIndex(InPosition);
	return CachedSamplePoints[GridPointIndex];
}

int32 SBlendSpaceGridWidget::FindClosestGridPointIndex(const FVector2D& InPosition) const
{
	// Clamp the screen position to the grid
	const FVector2D GridPosition(FMath::Clamp(InPosition.X, CachedGridRectangle.Left, CachedGridRectangle.Right),
								  FMath::Clamp(InPosition.Y, CachedGridRectangle.Top, CachedGridRectangle.Bottom));
	// Find the closest grid point
	float Distance = FLT_MAX;
	int32 GridPointIndex = INDEX_NONE;
	for (int32 Index = 0; Index < CachedGridPoints.Num(); ++Index)
	{
		const FVector2D& GridPoint = CachedGridPoints[Index];
		const float DistanceToGrid = FVector2D::DistSquared(GridPosition, GridPoint);
		if (DistanceToGrid < Distance)
		{
			Distance = DistanceToGrid;
			GridPointIndex = Index;
		}
	}

	checkf(GridPointIndex != INDEX_NONE, TEXT("Unable to find gridpoint"));

	return GridPointIndex;
}
const FVector2D SBlendSpaceGridWidget::SampleValueToGridPosition(const FVector& SampleValue) const
{
	const FVector2D GridSize = CachedGridRectangle.GetSize();	
	const FVector2D GridCenter = GridSize * 0.5f;
		
	FVector2D SamplePosition2D;
	// Convert the sample value to -1 to 1 form
	SamplePosition2D.X = (((SampleValue.X - SampleValueMin.X) / SampleValueRange.X) * 2.0f) - 1.0f;
	SamplePosition2D.Y = (GridType == EGridType::TwoAxis) ? (((SampleValueMax.Y - SampleValue.Y) / SampleValueRange.Y) * 2.0f) - 1.0f : 0.0f;

	// Multiply by half of the grid size and offset is using the grid center position
	SamplePosition2D *= CachedGridRectangle.GetSize()* 0.5f;
	SamplePosition2D += CachedGridRectangle.GetCenter();

	return SamplePosition2D;	
}

const FVector SBlendSpaceGridWidget::GridPositionToSampleValue(const FVector2D& GridPosition, bool bClamp) const
{
	FVector2D LocalGridPosition = GridPosition;
	// Move to center of grid and convert to 0 - 1 form
	LocalGridPosition -= CachedGridRectangle.GetCenter();
	LocalGridPosition /= (CachedGridRectangle.GetSize() * 0.5f);
	LocalGridPosition += FVector2D::UnitVector;
	LocalGridPosition *= 0.5f;

	// Calculate the sample value by mapping it to the blend parameter range
	FVector SampleValue
	(		
		(LocalGridPosition.X * SampleValueRange.X) + SampleValueMin.X,
		(GridType == EGridType::TwoAxis) ? SampleValueMax.Y - (LocalGridPosition.Y * SampleValueRange.Y) : 0.0f,
		0.f	
	);
	if (bClamp)
	{
		SampleValue.X = FMath::Clamp(SampleValue.X, SampleValueMin.X, SampleValueMax.X);
		SampleValue.Y = FMath::Clamp(SampleValue.Y, SampleValueMin.Y, SampleValueMax.Y);
	}
	return SampleValue;
}

const FSlateRect SBlendSpaceGridWidget::GetGridRectangleFromGeometry(const FGeometry& MyGeometry)
{
	FSlateRect WindowRect = FSlateRect(0, 0, MyGeometry.GetLocalSize().X, MyGeometry.GetLocalSize().Y);
	if (!bStretchToFit)
	{
		UpdateGridRatioMargin(WindowRect.GetSize());
	}

	return WindowRect.InsetBy(GridMargin + GridRatioMargin);
}

bool SBlendSpaceGridWidget::IsSampleValueWithinMouseRange(const FVector& SampleValue, float& OutDistance) const
{
	const FVector2D GridPosition = SampleValueToGridPosition(SampleValue);
	OutDistance = FVector2D::Distance(LocalMousePosition, GridPosition);
	return (OutDistance < ClickAndHighlightThreshold);
}

int32 SBlendSpaceGridWidget::GetClosestSamplePointIndexToMouse() const
{
	float BestDistance = FLT_MAX;
	int32 BestIndex = INDEX_NONE;

	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
		for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
		{
			const FBlendSample& Sample = Samples[SampleIndex];
			float Distance;
			if (IsSampleValueWithinMouseRange(Sample.SampleValue, Distance))
			{
				if(Distance < BestDistance)
				{
					BestDistance = Distance;
					BestIndex = SampleIndex;
				}
			}
		}
	}

	return BestIndex;
}

void SBlendSpaceGridWidget::StartPreviewing()
{
	bSamplePreviewing = true;
	LastPreviewingMousePosition = LocalMousePosition;
	FModifierKeysState ModifierKeyState = FSlateApplication::Get().GetModifierKeys();
	bool bIsManualPreviewing = !bReadOnly && IsHovered() && bMouseIsOverGeometry && (ModifierKeyState.IsLeftShiftDown() || ModifierKeyState.IsRightShiftDown());
	PreviewPosition = Position.IsSet() && !bIsManualPreviewing ? Position.Get() : GridPositionToSampleValue(LastPreviewingMousePosition, false);
	PreviewFilteredPosition = FilteredPosition.IsSet() ? FilteredPosition.Get() : PreviewPosition;
	bPreviewPositionSet = true;	
	bPreviewToolTipHidden = true;
}

void SBlendSpaceGridWidget::StopPreviewing()
{
	bSamplePreviewing = false;
}

FText SBlendSpaceGridWidget::GetToolTipAnimationName() const
{
	FText ToolTipText = FText::GetEmpty();
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		const FText PreviewValue = LOCTEXT("PreviewValueTooltip", "Preview Value");

		if(bReadOnly)
		{
			ToolTipText = PreviewValue;
		}
		else
		{
			switch (DragState)
			{
				// If we are not dragging, but over a valid blend sample return its animation asset name
				case EDragState::None:
				{		
					if (bHighlightPreviewPin)
					{
						ToolTipText = PreviewValue;
					}
					else if (HighlightedSampleIndex != INDEX_NONE && BlendSpace->IsValidBlendSampleIndex(HighlightedSampleIndex))
					{
						const FBlendSample& BlendSample = BlendSpace->GetBlendSample(HighlightedSampleIndex);
						ToolTipText = GetSampleName(BlendSample, HighlightedSampleIndex);
					}
					else if(Position.IsSet())
					{
						ToolTipText = PreviewValue;
					}
					break;
				}

				case EDragState::PreDrag:
				{
					break;
				}

				// If we are dragging a sample return the dragged sample's animation asset name
				case EDragState::DragSample:
				{
					if (BlendSpace->IsValidBlendSampleIndex(DraggedSampleIndex))
					{
						const FBlendSample& BlendSample = BlendSpace->GetBlendSample(DraggedSampleIndex);
						ToolTipText = GetSampleName(BlendSample, DraggedSampleIndex);
					}
			
					break;
				}

				// If we are performing a drag/drop operation return the cached operation animation name
				case EDragState::DragDrop:
				{
					ToolTipText = DragDropAnimationName;
					break;
				}

				case EDragState::DragDropOverride:
				{
					ToolTipText = DragDropAnimationName;
					break;
				}

				case EDragState::InvalidDragDrop:
				{
					break;
				}
		
				// If we are previewing return a descriptive label
				case EDragState::Preview:
				{
					ToolTipText = PreviewValue;
					break;
				}
				default:
					check(false);
			}
		}
	}

	return ToolTipText;
}

FText SBlendSpaceGridWidget::GetToolTipSampleValue() const
{
	FText ToolTipText = FText::GetEmpty();

	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		static const FTextFormat OneAxisFormat = LOCTEXT("OneAxisFormat", "{0}: {1}");
		static const FTextFormat TwoAxisFormat = LOCTEXT("TwoAxisFormat", "{0}: {1} - {2}: {3}");
		const FTextFormat& ValueFormattingText = (GridType == EGridType::TwoAxis) ? TwoAxisFormat : OneAxisFormat;

		auto AddAdvancedPreview = [this, &ToolTipText, BlendSpace]()
		{
			FTextBuilder TextBuilder;
			TextBuilder.AppendLine(ToolTipText);

			if (bAdvancedPreview)
			{				
				for (const FBlendSampleData& SampleData : PreviewedSamples)
				{
					if(BlendSpace->IsValidBlendSampleIndex(SampleData.SampleDataIndex))
					{
						const FBlendSample& BlendSample = BlendSpace->GetBlendSample(SampleData.SampleDataIndex);
						static const FTextFormat SampleFormat = LOCTEXT("SampleFormat", "{0}: {1}");
						TextBuilder.AppendLine(FText::Format(SampleFormat, GetSampleName(BlendSample, SampleData.SampleDataIndex), FText::AsNumber(SampleData.TotalWeight)));
					}
				}
			}

			ToolTipText = TextBuilder.ToText();
		};

		if(bReadOnly)
		{
			ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(PreviewPosition.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(PreviewPosition.Y)));

			AddAdvancedPreview();
		}
		else
		{
			switch (DragState)
			{
				// If we are over a sample return its sample value if valid and otherwise show an error message as to why the sample is invalid
				case EDragState::None:
				{		
					if (bHighlightPreviewPin)
					{
						ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(PreviewPosition.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(PreviewPosition.Y)));

						AddAdvancedPreview();
					}
					else if (HighlightedSampleIndex != INDEX_NONE && BlendSpace->IsValidBlendSampleIndex(HighlightedSampleIndex))
					{
						const FBlendSample& BlendSample = BlendSpace->GetBlendSample(HighlightedSampleIndex);

						// Check if the sample is valid
						if (BlendSample.bIsValid)
						{
							ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(BlendSample.SampleValue.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(BlendSample.SampleValue.Y)));
						}
						else
						{
							ToolTipText = GetSampleErrorMessage(BlendSample);
						}
					}
					else if(Position.IsSet())
					{
						ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(PreviewPosition.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(PreviewPosition.Y)));

						AddAdvancedPreview();
					}
					break;
				}

				case EDragState::PreDrag:
				{
					break;
				}

				// If we are dragging a sample return the current sample value it is hovered at
				case EDragState::DragSample:
				{
					if (DraggedSampleIndex != INDEX_NONE)
					{
						const FBlendSample& BlendSample = BlendSpace->GetBlendSample(DraggedSampleIndex);
						ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(BlendSample.SampleValue.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(BlendSample.SampleValue.Y)));
					}
					break;
				}

				// If we are performing a drag and drop operation return the current sample value it is hovered at
				case EDragState::DragDrop:
				{
					const FVector SampleValue = SnapToClosestSamplePoint(LocalMousePosition);

					ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(SampleValue.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(SampleValue.Y)));

					break;
				}

				case EDragState::DragDropOverride:
				{
					if(HoveredAnimationName.IsEmpty())
					{
						static const FTextFormat OverrideAnimationFormat = LOCTEXT("InvalidSampleChangingFormat", "Changing sample to {0}");
						ToolTipText = FText::Format(OverrideAnimationFormat, DragDropAnimationName);
					}
					else
					{
						static const FTextFormat OverrideAnimationFormat = LOCTEXT("ValidSampleChangingFormat", "Changing sample from {0} to {1}");
						ToolTipText = FText::Format(OverrideAnimationFormat, HoveredAnimationName, DragDropAnimationName);
					}
					break;
				}
				// If the drag and drop operation is invalid return the cached error message as to why it is invalid
				case EDragState::InvalidDragDrop:
				{
					ToolTipText = InvalidDragDropText;
					break;
				}

				// If we are setting the preview value return the current preview sample value
				case EDragState::Preview:
				{
					ToolTipText = FText::Format(ValueFormattingText, ParameterXName, FText::FromString(FString::SanitizeFloat(PreviewPosition.X)), ParameterYName, FText::FromString(FString::SanitizeFloat(PreviewPosition.Y)));

					AddAdvancedPreview();
					break;
				}
				default:
					check(false);
			}
		}
	}

	return ToolTipText;
}

void SBlendSpaceGridWidget::EnableStatusBarMessage(bool bEnable)
{
	if(!bReadOnly)
	{
		if(bEnable)
		{
			if (!StatusBarMessageHandle.IsValid())
			{
				if(UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>())
				{
					StatusBarMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarName, MakeAttributeLambda([]()
					{
						return LOCTEXT("StatusBarMssage", "Hold Ctrl for weight details, hold Shift to move preview value");
					}));
				}
			}
		}
		else
		{
			if (StatusBarMessageHandle.IsValid())
			{
				if(UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>())
				{
					StatusBarSubsystem->PopStatusBarMessage(StatusBarName, StatusBarMessageHandle);
					StatusBarMessageHandle.Reset();
				}
			}
		}
	}
}

FText SBlendSpaceGridWidget::GetSampleErrorMessage(const FBlendSample &BlendSample) const
{
	const FVector2D GridPosition = SampleValueToGridPosition(BlendSample.SampleValue);
	// Either an invalid animation asset set
	if (BlendSample.Animation == nullptr)
	{
		static const FText NoAnimationErrorText = LOCTEXT("NoAnimationErrorText", "Invalid Animation for Sample");
		return NoAnimationErrorText;
	}
	// Or not aligned on the grid (which means that it does not match one of the cached grid points, == for FVector2D fails to compare though :/)
	else if (!CachedGridPoints.FindByPredicate([&](const FVector2D& Other) { return FMath::IsNearlyEqual(GridPosition.X, Other.X) && FMath::IsNearlyEqual(GridPosition.Y, Other.Y);}))
	{
		static const FText SampleNotAtGridPoint = LOCTEXT("SampleNotAtGridPointErrorText", "Sample is not on a valid Grid Point");
		return SampleNotAtGridPoint;
	}

	static const FText UnknownError = LOCTEXT("UnknownErrorText", "Sample is invalid for an Unknown Reason");
	return UnknownError;
}

void SBlendSpaceGridWidget::ShowToolTip()
{
	if(HighlightedSampleIndex != INDEX_NONE && ToolTipSampleIndex != HighlightedSampleIndex)
	{
		ToolTipSampleIndex = HighlightedSampleIndex;
		if(OnExtendSampleTooltip.IsBound())
		{
			ToolTipExtensionContainer->SetContent(OnExtendSampleTooltip.Execute(HighlightedSampleIndex));
		}
	}
	
	SetToolTip(ToolTip);
}

void SBlendSpaceGridWidget::ResetToolTip()
{
	ToolTipSampleIndex = INDEX_NONE;
	ToolTipExtensionContainer->SetContent(SNullWidget::NullWidget);
	SetToolTip(nullptr);
}

EVisibility SBlendSpaceGridWidget::GetInputBoxVisibility(const int32 ParameterIndex) const
{
	bool bVisible = !bReadOnly;
	// Only show input boxes when a sample is selected (hide it when one is being dragged since we have the tooltip information as well)
	bVisible &= (SelectedSampleIndex != INDEX_NONE && DraggedSampleIndex == INDEX_NONE);
	if ( ParameterIndex == 1 )
	{ 
		bVisible &= (GridType == EGridType::TwoAxis);
	}

	return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SBlendSpaceGridWidget::GetInputBoxValue(const int32 ParameterIndex) const
{
	checkf(ParameterIndex < 3, TEXT("Invalid parameter index, suppose to be within FVector array range"));
	float ReturnValue = 0.0f;
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		if (SelectedSampleIndex != INDEX_NONE && SelectedSampleIndex < BlendSpace->GetNumberOfBlendSamples())
		{
			const FBlendSample& BlendSample = BlendSpace->GetBlendSample(SelectedSampleIndex);
			ReturnValue = BlendSample.SampleValue[ParameterIndex];
		}
	}
	return ReturnValue;
}

TOptional<float> SBlendSpaceGridWidget::GetInputBoxMinValue(const int32 ParameterIndex) const
{
	checkf(ParameterIndex < 3, TEXT("Invalid parameter index, suppose to be within FVector array range"));
	return SampleValueMin[ParameterIndex];
}

TOptional<float> SBlendSpaceGridWidget::GetInputBoxMaxValue(const int32 ParameterIndex) const
{
	checkf(ParameterIndex < 3, TEXT("Invalid parameter index, suppose to be within FVector array range"));
	return SampleValueMax[ParameterIndex];
}

float SBlendSpaceGridWidget::GetInputBoxDelta(const int32 ParameterIndex) const
{
	checkf(ParameterIndex < 3, TEXT("Invalid parameter index, suppose to be within FVector array range"));
	return SampleGridDelta[ParameterIndex];
}

void SBlendSpaceGridWidget::OnInputBoxValueCommited(const float NewValue, ETextCommit::Type CommitType, const int32 ParameterIndex)
{
	OnInputBoxValueChanged(NewValue, ParameterIndex, false);
}

void SBlendSpaceGridWidget::OnInputBoxValueChanged(const float NewValue, const int32 ParameterIndex, bool bIsInteractive)
{
	checkf(ParameterIndex < 3, TEXT("Invalid parameter index, suppose to be within FVector array range"));

	if (SelectedSampleIndex != INDEX_NONE && BlendSpaceBase.Get() != nullptr)
	{
		// Retrieve current sample value
		const FBlendSample& Sample = BlendSpaceBase.Get()->GetBlendSample(SelectedSampleIndex);
		FVector SampleValue = Sample.SampleValue;

		// Calculate snapped value
		if(Sample.bSnapToGrid)
		{
			const float MinOffset = NewValue - SampleValueMin[ParameterIndex];
			float GridSteps = MinOffset / SampleGridDelta[ParameterIndex];
			int32 FlooredSteps = FMath::FloorToInt(GridSteps);
			GridSteps -= FlooredSteps;
			FlooredSteps = (GridSteps > .5f) ? FlooredSteps + 1 : FlooredSteps;

			// Temporary snap this value to closest point on grid (since the spin box delta does not provide the desired functionality)
			SampleValue[ParameterIndex] = SampleValueMin[ParameterIndex] + (FlooredSteps * SampleGridDelta[ParameterIndex]);
		}
		else
		{
			SampleValue[ParameterIndex] = NewValue;
		}

		OnSampleMoved.ExecuteIfBound(SelectedSampleIndex, SampleValue, bIsInteractive, Sample.bSnapToGrid);
	}
}

EVisibility SBlendSpaceGridWidget::GetSampleToolTipVisibility() const
{
	// Show tool tip when the grid is empty
	return (!bReadOnly && BlendSpaceBase.Get() != nullptr && BlendSpaceBase.Get()->GetNumberOfBlendSamples() == 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBlendSpaceGridWidget::GetPreviewToolTipVisibility() const
{
	// Only show preview tooltip until the user discovers the functionality
	return (!bReadOnly && !bPreviewToolTipHidden) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBlendSpaceGridWidget::GetTriangulationButtonVisibility() const
{
	return (bShowSettingsButtons && (GridType == EGridType::TwoAxis)) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBlendSpaceGridWidget::GetAnimationNamesButtonVisibility() const
{
	return bShowSettingsButtons ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SBlendSpaceGridWidget::ToggleFittingType()
{
	bStretchToFit = !bStretchToFit;

	// If toggle to stretching, reset the margin immediately
	if (bStretchToFit)
	{
		GridRatioMargin.Top = GridRatioMargin.Bottom = GridRatioMargin.Left = GridRatioMargin.Right = 0.0f;
	}

	return FReply::Handled();
}

FReply SBlendSpaceGridWidget::ToggleShowAnimationNames()
{
	bShowAnimationNames = !bShowAnimationNames;
	return FReply::Handled();
}

void SBlendSpaceGridWidget::UpdateGridRatioMargin(const FVector2D& GeometrySize)
{
	if (GridType == EGridType::TwoAxis)
	{
		// Reset values first
		GridRatioMargin.Top = GridRatioMargin.Bottom = GridRatioMargin.Left = GridRatioMargin.Right = 0.0f;

		if (SampleValueRange.X >= SampleValueRange.Y)
		{
			if (GeometrySize.Y > GeometrySize.X)
			{
				const float Difference = GeometrySize.Y - GeometrySize.X;
				GridRatioMargin.Top = GridRatioMargin.Bottom = Difference * 0.5f;
			}
		}
		else if (SampleValueRange.X < SampleValueRange.Y)
		{
			if (GeometrySize.X > GeometrySize.Y)
			{
				const float Difference = GeometrySize.X - GeometrySize.Y;
				GridRatioMargin.Left = GridRatioMargin.Right = Difference * 0.5f;
			}
		}
	}
}

FText SBlendSpaceGridWidget::GetFittingTypeButtonToolTipText() const
{
	static const FText StretchText = LOCTEXT("StretchFittingText", "Stretch Grid to Fit");
	static const FText GridRatioText = LOCTEXT("GridRatioFittingText", "Fit Grid to Largest Axis");
	return (bStretchToFit) ? GridRatioText : StretchText;
}

EVisibility SBlendSpaceGridWidget::GetFittingButtonVisibility() const
{
	return (bShowSettingsButtons && (GridType == EGridType::TwoAxis)) ? EVisibility::Visible : EVisibility::Collapsed;
}

void SBlendSpaceGridWidget::UpdateCachedBlendParameterData()
{
	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		const FBlendParameter& BlendParameterX = BlendSpace->GetBlendParameter(0);
		const FBlendParameter& BlendParameterY = BlendSpace->GetBlendParameter(1);
		SampleValueRange.X = BlendParameterX.Max - BlendParameterX.Min;
		SampleValueRange.Y = BlendParameterY.Max - BlendParameterY.Min;
	
		SampleValueMin.X = BlendParameterX.Min;
		SampleValueMin.Y = BlendParameterY.Min;

		SampleValueMax.X = BlendParameterX.Max;
		SampleValueMax.Y = BlendParameterY.Max;

		SampleGridDelta = SampleValueRange;
		SampleGridDelta.X /= (BlendParameterX.GridNum);
		SampleGridDelta.Y /= (BlendParameterY.GridNum);

		SampleGridDivisions.X = BlendParameterX.GridNum;
		SampleGridDivisions.Y = BlendParameterY.GridNum;

		ParameterXName = FText::FromString(BlendParameterX.DisplayName);
		ParameterYName = FText::FromString(BlendParameterY.DisplayName);
	
		const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		MaxVerticalAxisTextWidth = HorizontalAxisMaxTextWidth = MaxHorizontalAxisTextHeight = 0.0f;
		FVector2D TextSize = FontMeasure->Measure(ParameterYName, FontInfo);	
		MaxVerticalAxisTextWidth = FMath::Max(MaxVerticalAxisTextWidth, TextSize.X);

		TextSize = FontMeasure->Measure(FString::SanitizeFloat(SampleValueMin.Y), FontInfo);
		MaxVerticalAxisTextWidth = FMath::Max(MaxVerticalAxisTextWidth, TextSize.X);

		TextSize = FontMeasure->Measure(FString::SanitizeFloat(SampleValueMax.Y), FontInfo);
		MaxVerticalAxisTextWidth = FMath::Max(MaxVerticalAxisTextWidth, TextSize.X);
	
		TextSize = FontMeasure->Measure(ParameterXName, FontInfo);
		MaxHorizontalAxisTextHeight = FMath::Max(MaxHorizontalAxisTextHeight, TextSize.Y);

		TextSize = FontMeasure->Measure(FString::SanitizeFloat(SampleValueMin.X), FontInfo);
		MaxHorizontalAxisTextHeight = FMath::Max(MaxHorizontalAxisTextHeight, TextSize.Y);

		TextSize = FontMeasure->Measure(FString::SanitizeFloat(SampleValueMax.X), FontInfo);
		MaxHorizontalAxisTextHeight = FMath::Max(MaxHorizontalAxisTextHeight, TextSize.Y);
		HorizontalAxisMaxTextWidth = TextSize.X;
	}
}

void SBlendSpaceGridWidget::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{	
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	bMouseIsOverGeometry = true;
	EnableStatusBarMessage(true);
}

void SBlendSpaceGridWidget::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bMouseIsOverGeometry = false;
	EnableStatusBarMessage(false);
}

void SBlendSpaceGridWidget::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusLost(InFocusEvent);
	HighlightedSampleIndex = DraggedSampleIndex = INDEX_NONE;
	DragState = EDragState::None;
	bSamplePreviewing = false;
	ResetToolTip();
	EnableStatusBarMessage(false);
}

bool SBlendSpaceGridWidget::SupportsKeyboardFocus() const
{
	return true;
}

void SBlendSpaceGridWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const int32 PreviousSampleIndex = HighlightedSampleIndex;
	HighlightedSampleIndex = INDEX_NONE;
	const bool bPreviousHighlightPreviewPin = bHighlightPreviewPin;

	if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		if(PreviousBlendSpaceBase.Get() != BlendSpace)
		{
			PreviousBlendSpaceBase = BlendSpace;
			InvalidateCachedData();
		}

		GridType = BlendSpace->IsA<UBlendSpace1D>() ? EGridType::SingleAxis : EGridType::TwoAxis;
		BlendParametersToDraw = (GridType == EGridType::SingleAxis) ? 1 : 2;

		if(!bReadOnly)
		{
			if (DragState == EDragState::None)
			{
				// Check if we are highlighting preview pin
				float Distance;
				bHighlightPreviewPin = IsSampleValueWithinMouseRange(PreviewPosition, Distance);
				if (bHighlightPreviewPin)
				{
					if (bHighlightPreviewPin != bPreviousHighlightPreviewPin)
					{
						ShowToolTip();
					}
				}
				else if (bPreviousHighlightPreviewPin != bHighlightPreviewPin)
				{
					ResetToolTip();
				}
		
				// Determine highlighted sample
				HighlightedSampleIndex = GetClosestSamplePointIndexToMouse();

				if (!bHighlightPreviewPin)
				{
					// If we started selecting or selected a different sample make sure we show/hide the tooltip
					if (PreviousSampleIndex != HighlightedSampleIndex)
					{
						if (HighlightedSampleIndex != INDEX_NONE)
						{
							ShowToolTip();
						}
						else
						{
							ResetToolTip();
						}
					}
				}
		}
			else if (DragState == EDragState::DragSample)
			{
				// If we are dragging a sample, find out whether or not it has actually moved to a different grid position since the last tick and update the blend space accordingly
				const FBlendSample& BlendSample = BlendSpace->GetBlendSample(DraggedSampleIndex);

				FVector SampleValue;
				bool bSnap;
				if(FSlateApplication::Get().GetModifierKeys().IsAltDown() || !BlendSample.bSnapToGrid)
				{
					const FVector2D GridPosition(FMath::Clamp(LocalMousePosition.X, CachedGridRectangle.Left, CachedGridRectangle.Right),
													FMath::Clamp(LocalMousePosition.Y, CachedGridRectangle.Top, CachedGridRectangle.Bottom));
 
					SampleValue = GridPositionToSampleValue(GridPosition, true);
					bSnap = false;
				}
				else
				{
					SampleValue = SnapToClosestSamplePoint(LocalMousePosition);
					bSnap = true;
				} 

				if (SampleValue != LastDragPosition)
				{
					LastDragPosition = SampleValue;
					OnSampleMoved.ExecuteIfBound(DraggedSampleIndex, SampleValue, false, bSnap);
				}
			}
			else if (DragState == EDragState::DragDrop || DragState == EDragState::InvalidDragDrop || DragState == EDragState::DragDropOverride)
			{
				// Validate that the sample is not overlapping with a current sample when doing a drag/drop operation and that we are dropping a valid animation for the blend space (type)
				const FVector DropSampleValue = SnapToClosestSamplePoint(LocalMousePosition);
				const bool bValidPosition = BlendSpace->IsSampleWithinBounds(DropSampleValue);
				const bool bExistingSample = BlendSpace->IsTooCloseToExistingSamplePoint(DropSampleValue, INDEX_NONE);
				const bool bValidSequence = ValidateAnimationSequence(DragDropAnimationSequence, InvalidDragDropText);
		
				if (!bValidSequence)
				{
					DragState = EDragState::InvalidDragDrop;
				}
				else if (!bValidPosition)
				{			
					InvalidDragDropText = InvalidSamplePositionDragDropText;
					DragState = EDragState::InvalidDragDrop;
				}
				else if (bExistingSample)
				{	
					const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
					for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
					{
						const FBlendSample& Sample = Samples[SampleIndex];
						if (Sample.SampleValue == DropSampleValue)
						{
							HoveredAnimationName = Sample.Animation ? FText::FromString(Sample.Animation->GetName()) : FText::GetEmpty();
							break;
						}
					}

					DragState = EDragState::DragDropOverride;			
				}
				else if (bValidPosition && bValidSequence && !bExistingSample)
				{
					DragState = EDragState::DragDrop;
				}
			}
		}

		// Check if we should update the preview sample value
		if (bSamplePreviewing)
		{
			// Clamping happens later
			LastPreviewingMousePosition.X = LocalMousePosition.X;
			LastPreviewingMousePosition.Y = LocalMousePosition.Y;
			FModifierKeysState ModifierKeyState = FSlateApplication::Get().GetModifierKeys();
			bool bIsManualPreviewing = !bReadOnly && IsHovered() && bMouseIsOverGeometry && (ModifierKeyState.IsLeftShiftDown() || ModifierKeyState.IsRightShiftDown());
			PreviewPosition = Position.IsSet() && !bIsManualPreviewing ? Position.Get() : GridPositionToSampleValue(LastPreviewingMousePosition, false);
			PreviewPosition = BlendSpace->GetClampedAndWrappedBlendInput(PreviewPosition);

			if (FilteredPosition.IsSet())
			{
				PreviewFilteredPosition = BlendSpace->GetClampedAndWrappedBlendInput(FilteredPosition.Get());
			}

			// Retrieve and cache weighted samples
			PreviewedSamples.Empty(4);
			BlendSpace->GetSamplesFromBlendInput(PreviewPosition, PreviewedSamples);
		}
	}

	// Refresh cache blendspace/grid data if needed
	if (bRefreshCachedData)
	{
		UpdateCachedBlendParameterData();
		GridMargin = bShowAxisLabels ?  FMargin(MaxVerticalAxisTextWidth + (TextMargin * 2.0f), TextMargin, (HorizontalAxisMaxTextWidth *.5f) + TextMargin, MaxHorizontalAxisTextHeight + (TextMargin * 2.0f)) : 
										FMargin(TextMargin, TextMargin, TextMargin, TextMargin);
		bRefreshCachedData = false;
	}
	
	// Always need to update the rectangle and grid points according to the geometry (this can differ per tick)
	CachedGridRectangle = GetGridRectangleFromGeometry(AllottedGeometry);
	CalculateGridPoints();
}

const FVector SBlendSpaceGridWidget::GetPreviewPosition() const
{	
	return PreviewPosition;
}

void SBlendSpaceGridWidget::SetPreviewingState(const FVector& InPosition, const FVector& InFilteredPosition)
{
	if (const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
	{
		PreviewFilteredPosition = BlendSpace->GetClampedAndWrappedBlendInput(InFilteredPosition);
		PreviewPosition = BlendSpace->GetClampedAndWrappedBlendInput(InPosition);
	}
	else
	{
		PreviewFilteredPosition = InFilteredPosition;
		PreviewPosition = InPosition;
	}
}

void SBlendSpaceGridWidget::InvalidateCachedData()
{
	bRefreshCachedData = true;	
}

void SBlendSpaceGridWidget::InvalidateState()
{
	if (HighlightedSampleIndex != INDEX_NONE)
	{
		ResetToolTip();
	}

	if (DragState != EDragState::None)
	{
		DragState = EDragState::None;
	}
	
	SelectedSampleIndex = (BlendSpaceBase.Get() != nullptr && BlendSpaceBase.Get()->IsValidBlendSampleIndex(SelectedSampleIndex)) ? SelectedSampleIndex : INDEX_NONE;
	HighlightedSampleIndex = DraggedSampleIndex = INDEX_NONE;
}

const bool SBlendSpaceGridWidget::IsValidDragDropOperation(const FDragDropEvent& DragDropEvent, FText& InvalidOperationText)
{
	bool bResult = false;

	TSharedPtr<FAssetDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();

	if (DragDropOperation.IsValid())
	{
		// Check whether or not this animation is compatible with the blend space
		DragDropAnimationSequence = FAssetData::GetFirstAsset<UAnimSequence>(DragDropOperation->GetAssets());
		if (DragDropAnimationSequence)
		{
			bResult = ValidateAnimationSequence(DragDropAnimationSequence, InvalidOperationText);
		}
		else
		{
			// If is isn't an animation set error message
			bResult = false;
			InvalidOperationText = FText::FromString("Invalid Asset Type");
		}		
	}

	if (!bResult)
	{
		DragDropOperation->SetToolTip(InvalidOperationText, DragDropOperation->GetIcon());
	}
	else
	{
		DragDropAnimationName = FText::FromString(DragDropAnimationSequence->GetName());
	}

	return bResult;
}

bool SBlendSpaceGridWidget::ValidateAnimationSequence(const UAnimSequence* AnimationSequence, FText& InvalidOperationText) const
{	
	if (AnimationSequence != nullptr)
	{
		if(const UBlendSpaceBase* BlendSpace = BlendSpaceBase.Get())
		{
			if(BlendSpace->IsAsset())
			{
				// If there are any existing blend samples check whether or not the the animation should be additive and if so if the additive matches the existing samples
				if ( BlendSpace->GetNumberOfBlendSamples() > 0)
				{
					const bool bIsAdditive = BlendSpace->ShouldAnimationBeAdditive();
					if (AnimationSequence->IsValidAdditive() != bIsAdditive)
					{
						InvalidOperationText = FText::FromString(bIsAdditive ? "Animation should be additive" : "Animation should be non-additive");
						return false;
					}

					// If it is the supported additive type, but does not match existing samples
					if (!BlendSpace->DoesAnimationMatchExistingSamples(AnimationSequence))
					{
						InvalidOperationText = FText::FromString("Additive Animation Type does not match existing Samples");
						return false;
					}
				}

				// Check if the supplied animation is of a different additive animation type 
				if (!BlendSpace->IsAnimationCompatible(AnimationSequence))
				{
					InvalidOperationText = FText::FromString("Invalid Additive Animation Type");
					return false;
				}

				// Check if the supplied animation is compatible with the skeleton
				if (!BlendSpace->IsAnimationCompatibleWithSkeleton(AnimationSequence))
				{
					InvalidOperationText = FText::FromString("Animation is incompatible with the skeleton");
					return false;
				}
			}
		}
	}

	return AnimationSequence != nullptr;
}

const bool SBlendSpaceGridWidget::IsPreviewing() const 
{ 
	FModifierKeysState ModifierKeyState = FSlateApplication::Get().GetModifierKeys();
	bool bIsManualPreviewing = !bReadOnly && IsHovered() && bMouseIsOverGeometry && (ModifierKeyState.IsLeftShiftDown() || ModifierKeyState.IsRightShiftDown());
	return (bSamplePreviewing && !Position.IsSet()) || (Position.IsSet() && bIsManualPreviewing);
}

#undef LOCTEXT_NAMESPACE // "SAnimationBlendSpaceGridWidget"
