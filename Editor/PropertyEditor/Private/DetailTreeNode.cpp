// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailTreeNode.h"
#include "DetailWidgetRow.h"
#include "PropertyEditorHelpers.h"

FNodeWidgets FDetailTreeNode::CreateNodeWidgets() const
{
	FDetailWidgetRow Row;
	GenerateStandaloneWidget(Row);

	FNodeWidgets Widgets;

	if (Row.HasAnyContent())
	{
		if (Row.HasColumns())
		{
			Widgets.NameWidget = Row.NameWidget.Widget;
			Widgets.NameWidgetLayoutData = FNodeWidgetLayoutData(
				Row.NameWidget.HorizontalAlignment, Row.NameWidget.VerticalAlignment, Row.NameWidget.MinWidth, Row.NameWidget.MaxWidth);
			Widgets.ValueWidget = Row.ValueWidget.Widget;
			Widgets.ValueWidgetLayoutData = FNodeWidgetLayoutData(
				Row.ValueWidget.HorizontalAlignment, Row.ValueWidget.VerticalAlignment, Row.ValueWidget.MinWidth, Row.ValueWidget.MaxWidth);
		}
		else
		{
			Widgets.WholeRowWidget = Row.WholeRowWidget.Widget;
			Widgets.WholeRowWidgetLayoutData = FNodeWidgetLayoutData(
				Row.WholeRowWidget.HorizontalAlignment, Row.WholeRowWidget.VerticalAlignment, Row.WholeRowWidget.MinWidth, Row.WholeRowWidget.MaxWidth);
		}

		Widgets.EditConditionWidget = SNew(SEditConditionWidget)
			.EditConditionValue(Row.EditConditionValue)
			.OnEditConditionValueChanged(Row.OnEditConditionValueChanged);
	}

	Widgets.Actions.CopyMenuAction = Row.CopyMenuAction;
	Widgets.Actions.PasteMenuAction = Row.PasteMenuAction;
	for (const FDetailWidgetRow::FCustomMenuData& CustomMenuItem : Row.CustomMenuItems)
	{
		Widgets.Actions.CustomMenuItems.Add(FNodeWidgetActionsCustomMenuData(CustomMenuItem.Action, CustomMenuItem.Name, CustomMenuItem.Tooltip, CustomMenuItem.SlateIcon));
	}

	return Widgets;
}

void FDetailTreeNode::GetChildren(TArray<TSharedRef<IDetailTreeNode>>& OutChildren)
{
	FDetailNodeList Children;
	GetChildren(Children);

	OutChildren.Reset(Children.Num());

	OutChildren.Append(Children);
}
