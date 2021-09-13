// Copyright Epic Games, Inc. All Rights Reserved.

#include "TableCellValueFormatter.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#include "Insights/Common/TimeUtils.h"
#include "Insights/Table/ViewModels/TableCellValueGetter.h"
#include "Insights/Table/ViewModels/TableColumn.h"

#define LOCTEXT_NAMESPACE "TableCellValueFormatter"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableCellValueFormatter::FormatValue(const FTableColumn& Column, const FBaseTreeNode& Node) const
{
	return FormatValue(Column.GetValue(Node));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FTableCellValueFormatter::FormatValueForTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const
{
	return FormatValueForTooltip(Column.GetValue(Node));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<IToolTip> FTableCellValueFormatter::GetCustomTooltip(const FTableColumn& Column, const FBaseTreeNode& Node) const
{
	return SNew(SToolTip)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FTableCellValueFormatter::GetTooltipVisibility)))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(FormatValueForTooltip(Column.GetValueGetter()->GetValue(Column, Node)))
			]
		];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EVisibility FTableCellValueFormatter::GetTooltipVisibility()
{
	return FSlateApplication::Get().AnyMenusVisible() ? EVisibility::Collapsed : EVisibility::Visible;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FBoolValueFormatterAsTrueFalse::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		return FText::FromString(InValue.GetValue().Bool ? TEXT("True") : TEXT("False"));
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FBoolValueFormatterAsOnOff::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		return FText::FromString(InValue.GetValue().Bool ? TEXT("On") : TEXT("Off"));
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FInt64ValueFormatterAsMemory::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const int64 Value = InValue.GetValue().Int64;
		if (Value > 0)
		{
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 1;
			return FText::AsMemory(Value, &FormattingOptions);
		}
		else if (Value == 0)
		{
			return LOCTEXT("AsMemory_ZeroValue", "0");
		}
		else // Value < 0
		{
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 1;
			return FText::Format(LOCTEXT("AsMemory_NegativeValue_Fmt1", "-{0}"), FText::AsMemory(-Value, &FormattingOptions));
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FInt64ValueFormatterAsMemory::FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const int64 Value = InValue.GetValue().Int64;
		if (Value > 0)
		{
			if (Value < 1024)
			{
				return FText::Format(LOCTEXT("AsMemory_PositiveValue_TooltipFmt1", "{0} bytes"), FText::AsNumber(Value));
			}
			else
			{
				FNumberFormattingOptions FormattingOptions;
				FormattingOptions.MaximumFractionalDigits = 2;
				return FText::Format(LOCTEXT("AsMemory_PositiveValue_TooltipFmt2", "{0} ({1} bytes)"), FText::AsMemory(Value, &FormattingOptions), FText::AsNumber(Value));
			}
		}
		else if (Value == 0)
		{
			return LOCTEXT("AsMemory_ZeroValue", "0");
		}
		else // Value < 0
		{
			if (-Value < 1024)
			{
				return FText::Format(LOCTEXT("AsMemory_NegativeValue_TooltipFmt1", "-{0} bytes"), FText::AsNumber(-Value));
			}
			else
			{
				FNumberFormattingOptions FormattingOptions;
				FormattingOptions.MaximumFractionalDigits = 2;
				return FText::Format(LOCTEXT("AsMemory_NegativeValue_TooltipFmt2", "-{0} (-{1} bytes)"), FText::AsMemory(-Value, &FormattingOptions), FText::AsNumber(-Value));
			}
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FFloatValueFormatterAsNumber::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const float Value = InValue.GetValue().Float;
		if (Value == 0.0f)
		{
			return FText::FromString(TEXT("0"));
		}
		else
		{
			return FText::FromString(FString::Printf(TEXT("%f"), Value));
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FFloatValueFormatterAsTimeAuto::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const float Value = InValue.GetValue().Float;
		return FText::FromString(TimeUtils::FormatTimeAuto(static_cast<double>(Value)));
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FFloatValueFormatterAsTimeAuto::FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const float Value = InValue.GetValue().Float;
		if (Value == 0.0f)
		{
			return FText::FromString(TEXT("0"));
		}
		else
		{
			return FText::FromString(FString::Printf(TEXT("%f (%s)"), Value, *TimeUtils::FormatTimeAuto(static_cast<double>(Value))));
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FDoubleValueFormatterAsNumber::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const double Value = InValue.GetValue().Double;
		if (Value == 0.0)
		{
			return FText::FromString(TEXT("0"));
		}
		else
		{
			return FText::FromString(FString::Printf(TEXT("%f"), Value));
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FDoubleValueFormatterAsTimeAuto::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const double Value = InValue.GetValue().Double;
		return FText::FromString(TimeUtils::FormatTimeAuto(Value));
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FDoubleValueFormatterAsTimeAuto::FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const double Value = InValue.GetValue().Double;
		if (Value == 0.0)
		{
			return FText::FromString(TEXT("0"));
		}
		else
		{
			return FText::FromString(FString::Printf(TEXT("%f (%s)"), Value, *TimeUtils::FormatTimeAuto(Value)));
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FDoubleValueFormatterAsTimeMs::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const double Value = InValue.GetValue().Double;
		return FText::FromString(TimeUtils::FormatTimeMs(Value));
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FDoubleValueFormatterAsTimeMs::FormatValueForTooltip(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const double Value = InValue.GetValue().Double;
		if (Value == 0.0)
		{
			return FText::FromString(TEXT("0"));
		}
		else
		{
			return FText::FromString(FString::Printf(TEXT("%f (%s)"), Value, *TimeUtils::FormatTimeMs(Value)));
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FCStringValueFormatterAsText::FormatValue(const TOptional<FTableCellValue>& InValue) const
{
	if (InValue.IsSet())
	{
		const TCHAR* Value = InValue.GetValue().CString;
		if (Value)
		{
			return FText::FromString(Value);
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
