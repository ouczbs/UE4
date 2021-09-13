// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConsoleSlateDebuggerPaint.h"
#include "ConsoleSlateDebugger.h"

#if WITH_SLATE_DEBUGGING

#include "Application/SlateApplicationBase.h"
#include "CoreGlobals.h"
#include "Debugging/SlateDebugging.h"
#include "Layout/WidgetPath.h"
#include "Types/ReflectionMetadata.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "ConsoleSlateDebuggerPaint"

FConsoleSlateDebuggerPaint::FConsoleSlateDebuggerPaint()
	: bEnabled(false)
	, bEnabledCVarValue(false)
	, bDisplayWidgetsNameList(false)
	, bUseWidgetPathAsName(false)
	, bDrawBox(false)
	, bDrawQuad(true)
	, bLogWidgetName(false)
	, bLogWidgetNameOnce(false)
	, bLogWarningIfWidgetIsPaintedMoreThanOnce(true)
	, DrawBoxColor(1.0f, 1.0f, 0.0f, 0.2f)
	, DrawQuadColor(1.0f, 1.0f, 1.0f, 1.0f)
	, DrawWidgetNameColor(FColorList::SpicyPink)
	, MaxNumberOfWidgetInList(20)
	, CacheDuration(2.0f)
	, ShowPaintWidgetCommand(
		TEXT("SlateDebugger.Paint.Start")
		, TEXT("Start the painted widget debug tool. It shows when widgets are painted.")
		, FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebuggerPaint::StartDebugging))
	, HidePaintWidgetCommand(
		TEXT("SlateDebugger.Paint.Stop")
		, TEXT("Stop the painted widget debug tool.")
		, FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebuggerPaint::StopDebugging))
	, EnabledRefCVar(
		TEXT("SlateDebugger.Paint.Enable")
		, bEnabledCVarValue
		, TEXT("Start/Stop the painted widget debug tool. It shows when widgets are painted.")
		, FConsoleVariableDelegate::CreateRaw(this, &FConsoleSlateDebuggerPaint::HandleEnabled))
	, LogPaintedWidgetOnceCommand(
		TEXT("SlateDebugger.Paint.LogOnce")
		, TEXT("Log the widgets that has been painted during the last update once")
		, FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebuggerPaint::HandleLogOnce))
	, DisplayWidgetsNameListCommand(
		TEXT("SlateDebugger.Paint.ToggleWidgetNameList"),
		TEXT("Option to display the name of the widgets that have been painted."),
		FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebuggerPaint::HandleToggleWidgetNameList))
	, MaxNumberOfWidgetInListtRefCVar(
		TEXT("SlateDebugger.Paint.MaxNumberOfWidgetDisplayedInList")
		, MaxNumberOfWidgetInList
		, TEXT("The max number of widget that will be displayed when DisplayWidgetNameList is active."))
	, LogWarningIfWidgetIsPaintedMoreThanOnceRefCVar(
		TEXT("SlateDebugger.Paint.LogWarningIfWidgetIsPaintedMoreThanOnce")
		, bLogWarningIfWidgetIsPaintedMoreThanOnce
		, TEXT("Option to log a warning if a widget is painted more than once in the same frame."))
{
	GConfig->GetBool(TEXT("SlateDebugger.Paint"), TEXT("bDisplayWidgetsNameList"), bDisplayWidgetsNameList, *GEditorPerProjectIni);
	GConfig->GetBool(TEXT("SlateDebugger.Paint"), TEXT("bUseWidgetPathAsName"), bUseWidgetPathAsName, *GEditorPerProjectIni);
	GConfig->GetBool(TEXT("SlateDebugger.Paint"), TEXT("bDrawBox"), bDrawBox, *GEditorPerProjectIni);
	GConfig->GetBool(TEXT("SlateDebugger.Paint"), TEXT("bDrawQuad"), bDrawQuad, *GEditorPerProjectIni);
	GConfig->GetBool(TEXT("SlateDebugger.Paint"), TEXT("bLogWidgetName"), bLogWidgetName, *GEditorPerProjectIni);
	GConfig->GetBool(TEXT("SlateDebugger.Paint"), TEXT("bLogWarningIfWidgetIsPaintedMoreThanOnce"), bLogWarningIfWidgetIsPaintedMoreThanOnce, *GEditorPerProjectIni);
	FColor TmpColor;
	if (GConfig->GetColor(TEXT("SlateDebugger.Paint"), TEXT("DrawBoxColor"), TmpColor, *GEditorPerProjectIni))
	{
		DrawBoxColor = TmpColor;
	}
	if (GConfig->GetColor(TEXT("SlateDebugger.Paint"), TEXT("DrawQuadColor"), TmpColor, *GEditorPerProjectIni))
	{
		DrawQuadColor = TmpColor;
	}
	if (GConfig->GetColor(TEXT("SlateDebugger.Paint"), TEXT("DrawWidgetNameColor"), TmpColor, *GEditorPerProjectIni))
	{
		DrawWidgetNameColor = TmpColor;
	}
	GConfig->GetInt(TEXT("SlateDebugger.Paint"), TEXT("MaxNumberOfWidgetInList"), MaxNumberOfWidgetInList, *GEditorPerProjectIni);
	GConfig->GetFloat(TEXT("SlateDebugger.Paint"), TEXT("CacheDuration"), CacheDuration, *GEditorPerProjectIni);
}

FConsoleSlateDebuggerPaint::~FConsoleSlateDebuggerPaint()
{
	StopDebugging();
}

void FConsoleSlateDebuggerPaint::SaveConfig()
{
	GConfig->SetBool(TEXT("SlateDebugger.Paint"), TEXT("bDisplayWidgetsNameList"), bDisplayWidgetsNameList, *GEditorPerProjectIni);
	GConfig->SetBool(TEXT("SlateDebugger.Paint"), TEXT("bUseWidgetPathAsName"), bUseWidgetPathAsName, *GEditorPerProjectIni);
	GConfig->SetBool(TEXT("SlateDebugger.Paint"), TEXT("bDrawBox"), bDrawBox, *GEditorPerProjectIni);
	GConfig->SetBool(TEXT("SlateDebugger.Paint"), TEXT("bDrawQuad"), bDrawQuad, *GEditorPerProjectIni);
	GConfig->SetBool(TEXT("SlateDebugger.Paint"), TEXT("bLogWidgetName"), bLogWidgetName, *GEditorPerProjectIni);
	GConfig->SetBool(TEXT("SlateDebugger.Paint"), TEXT("bLogWarningIfWidgetIsPaintedMoreThanOnce"), bLogWarningIfWidgetIsPaintedMoreThanOnce, *GEditorPerProjectIni);
	FColor TmpColor = DrawBoxColor.ToFColor(true);
	GConfig->SetColor(TEXT("SlateDebugger.Paint"), TEXT("DrawBoxColor"), TmpColor, *GEditorPerProjectIni);
	TmpColor = DrawQuadColor.ToFColor(true);
	GConfig->SetColor(TEXT("SlateDebugger.Paint"), TEXT("DrawQuadColor"), TmpColor, *GEditorPerProjectIni);
	TmpColor = DrawWidgetNameColor.ToFColor(true);
	GConfig->SetColor(TEXT("SlateDebugger.Paint"), TEXT("DrawWidgetNameColor"), TmpColor, *GEditorPerProjectIni);
	GConfig->SetInt(TEXT("SlateDebugger.Paint"), TEXT("MaxNumberOfWidgetInList"), MaxNumberOfWidgetInList, *GEditorPerProjectIni);
	GConfig->SetFloat(TEXT("SlateDebugger.Paint"), TEXT("CacheDuration"), CacheDuration, *GEditorPerProjectIni);
}

void FConsoleSlateDebuggerPaint::StartDebugging()
{
	if (!bEnabled)
	{
		bEnabled = true;
		PaintedWidgets.Empty();

		FSlateDebugging::EndWidgetPaint.AddRaw(this, &FConsoleSlateDebuggerPaint::HandleEndWidgetPaint);
		FSlateDebugging::PaintDebugElements.AddRaw(this, &FConsoleSlateDebuggerPaint::HandlePaintDebugInfo);
		FCoreDelegates::OnEndFrame.AddRaw(this, &FConsoleSlateDebuggerPaint::HandleEndFrame);
	}
	bEnabledCVarValue = bEnabled;
}

void FConsoleSlateDebuggerPaint::StopDebugging()
{
	if (bEnabled)
	{
		FCoreDelegates::OnEndFrame.RemoveAll(this);
		FSlateDebugging::PaintDebugElements.RemoveAll(this);
		FSlateDebugging::EndWidgetPaint.RemoveAll(this);

		PaintedWidgets.Empty();
		bEnabled = false;
	}
	bEnabledCVarValue = bEnabled;
}

void FConsoleSlateDebuggerPaint::HandleEnabled(IConsoleVariable* Variable)
{
	if (bEnabledCVarValue)
	{
		StartDebugging();
	}
	else
	{
		StopDebugging();
	}
}

void FConsoleSlateDebuggerPaint::HandleLogOnce()
{
	bLogWidgetNameOnce = true;
}

void FConsoleSlateDebuggerPaint::HandleToggleWidgetNameList()
{
	bDisplayWidgetsNameList = !bDisplayWidgetsNameList;
	SaveConfig();
}

void FConsoleSlateDebuggerPaint::HandleEndFrame()
{
	double LastTime = FSlateApplicationBase::Get().GetCurrentTime() - CacheDuration;
	for (TPaintedWidgetMap::TIterator It(PaintedWidgets); It; ++It)
	{
		It.Value().PaintCount = 0;
		if (It.Value().LastPaint < LastTime)
		{
			It.RemoveCurrent();
		}
	}
}

void FConsoleSlateDebuggerPaint::HandleEndWidgetPaint(const SWidget* Widget, const FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	// Use the Widget pointer for the id.
	//That may introduce bug when a widget is destroyed and the same memory is reused for another widget. We do not care for this debug tool.
	//We do not keep the widget alive or reuse it later, cache all the info that we need.
	const FConsoleSlateDebuggerUtility::TSWidgetId WidgetId = FConsoleSlateDebuggerUtility::GetId(Widget);
	const FConsoleSlateDebuggerUtility::TSWindowId WindowId = FConsoleSlateDebuggerUtility::GetId(OutDrawElements.GetPaintWindow());

	FPaintInfo* FoundItem = PaintedWidgets.Find(WidgetId);
	if (FoundItem == nullptr)
	{
		FoundItem = &PaintedWidgets.Add(WidgetId);
		FoundItem->PaintCount = 0;
		FoundItem->Window = WindowId;
		FoundItem->WidgetName = bUseWidgetPathAsName ? FReflectionMetaData::GetWidgetPath(Widget) : FReflectionMetaData::GetWidgetDebugInfo(Widget);
	}
	else
	{
		ensureAlways(FoundItem->Window == WindowId);
		if (bLogWarningIfWidgetIsPaintedMoreThanOnce && FoundItem->PaintCount != 0)
		{
			UE_LOG(LogSlateDebugger, Warning, TEXT("'%s' got painted more than once."), *(FoundItem->WidgetName));
		}
	}

	if (bLogWidgetName)
	{
		UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *(FoundItem->WidgetName));
	}
	FoundItem->PaintLocation = Widget->GetPersistentState().AllottedGeometry.GetAbsolutePosition();
	FoundItem->PaintSize = Widget->GetPersistentState().AllottedGeometry.GetAbsoluteSize();
	FoundItem->LastPaint = FSlateApplicationBase::Get().GetCurrentTime();
	++FoundItem->PaintCount;
}

void FConsoleSlateDebuggerPaint::HandlePaintDebugInfo(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, FSlateWindowElementList& InOutDrawElements, int32& InOutLayerId)
{
	++InOutLayerId;

	FConsoleSlateDebuggerUtility::TSWindowId PaintWindow = FConsoleSlateDebuggerUtility::GetId(InOutDrawElements.GetPaintWindow());

	int32 NumberOfWidget = 0;
	const float TextElementY = 36.f;
	const FSlateBrush* BoxBrush = bDrawBox ? FCoreStyle::Get().GetBrush("WhiteBrush") : nullptr;
	const FSlateBrush* QuadBrush = FCoreStyle::Get().GetBrush(TEXT("FocusRectangle"));
	FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("SmallFont");
	FontInfo.OutlineSettings.OutlineSize = 1;

	CacheDuration = FMath::Max(CacheDuration, 0.01f);
	const double SlateApplicationCurrentTime = FSlateApplicationBase::Get().GetCurrentTime();

	auto MakeText = [&](const FString& Text, const FVector2D& Location, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(
			InOutDrawElements
			, InOutLayerId
			, InAllottedGeometry.ToPaintGeometry(Location, FVector2D(1.f, 1.f))
			, Text
			, FontInfo
			, ESlateDrawEffect::None
			, Color);
	};

	for (const auto& Itt : PaintedWidgets)
	{
		if (Itt.Value.Window == PaintWindow)
		{
			const float LerpValue = FMath::Clamp((float)(SlateApplicationCurrentTime - Itt.Value.LastPaint) / CacheDuration, 0.0f, 1.0f);
			const FGeometry Geometry = FGeometry::MakeRoot(Itt.Value.PaintSize, FSlateLayoutTransform(1.f, Itt.Value.PaintLocation));
			const FPaintGeometry PaintGeometry = Geometry.ToPaintGeometry();

			if (BoxBrush)
			{
				FSlateDrawElement::MakeBox(
					InOutDrawElements,
					InOutLayerId,
					PaintGeometry,
					BoxBrush,
					ESlateDrawEffect::None,
					DrawBoxColor.CopyWithNewOpacity(FMath::InterpExpoOut(1.0f, 0.0f, LerpValue)));
			}

			if (bDrawQuad)
			{
				FSlateDrawElement::MakeDebugQuad(
					InOutDrawElements,
					InOutLayerId,
					PaintGeometry,
					DrawQuadColor);

				FSlateDrawElement::MakeBox(
					InOutDrawElements,
					InOutLayerId,
					PaintGeometry,
					QuadBrush,
					ESlateDrawEffect::None,
					DrawQuadColor.CopyWithNewOpacity(FMath::InterpExpoOut(1.0f, 0.0f, LerpValue)));
			}

			if (bLogWidgetNameOnce)
			{
				UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *(Itt.Value.WidgetName));
			}

			if (bDisplayWidgetsNameList)
			{
				if (NumberOfWidget < MaxNumberOfWidgetInList)
				{
					MakeText(Itt.Value.WidgetName, FVector2D(0, (12 * NumberOfWidget) + TextElementY), DrawWidgetNameColor);
				}
			}
			++NumberOfWidget;
		}
	}
	bLogWidgetNameOnce = false;

	{
		FString NumberOfWidgetDrawn = FString::Printf(TEXT("Number of Widget Painted: %d"), NumberOfWidget);
		MakeText(NumberOfWidgetDrawn, FVector2D(10, 10), DrawWidgetNameColor);
	}

	if (bDisplayWidgetsNameList && NumberOfWidget > MaxNumberOfWidgetInList)
	{
		FString WidgetDisplayName = FString::Printf(TEXT("   %d more invalidations"), NumberOfWidget - MaxNumberOfWidgetInList);
		MakeText(WidgetDisplayName, FVector2D(0.f, (12.f * NumberOfWidget) + TextElementY), FLinearColor::White);
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_SLATE_DEBUGGING
