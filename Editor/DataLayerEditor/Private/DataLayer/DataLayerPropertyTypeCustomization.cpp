// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerPropertyTypeCustomization.h"
#include "DataLayer/DataLayerPropertyTypeCustomizationHelper.h"
#include "DataLayer/DataLayerDragDropOp.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "DragAndDrop/CompositeDragDropOp.h"
#include "Algo/Accumulate.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "SDropTarget.h"
#include "Editor.h"
#include "EditorFontGlyphs.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

#define LOCTEXT_NAMESPACE "DataLayer"

void FDataLayerPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = StructPropertyHandle->GetChildHandle("Name");

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SDropTarget)
		.OnDrop(this, &FDataLayerPropertyTypeCustomization::OnDrop)
		.OnAllowDrop(this, &FDataLayerPropertyTypeCustomization::OnVerifyDrag)
		.OnIsRecognized(this, &FDataLayerPropertyTypeCustomization::OnVerifyDrag)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(TEXT("DataLayer.Icon16x")))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(3.0f, 0.0f)
			.FillWidth(1.0f)
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("ComboButtonTip", "Drag and drop a Data Layer onto this property, or choose one from the drop down."))
				.OnGetMenuContent(this, &FDataLayerPropertyTypeCustomization::OnGetDataLayerMenu)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ForegroundColor(FSlateColor::UseForeground())
				.ContentPadding(FMargin(0))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FDataLayerPropertyTypeCustomization::GetDataLayerText)
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("SelectTip", "Select all actors in this Data Layer"))
				.OnClicked(this, &FDataLayerPropertyTypeCustomization::OnSelectDataLayer)
				.Visibility(this, &FDataLayerPropertyTypeCustomization::GetSelectDataLayerVisibility)
				.ForegroundColor(FSlateColor::UseForeground())
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
					.Text(FEditorFontGlyphs::Sign_In)
				]
			]
		]
	];
	HeaderRow.IsEnabled(TAttribute<bool>(StructPropertyHandle, &IPropertyHandle::IsEditable));
}

UDataLayer* FDataLayerPropertyTypeCustomization::GetDataLayerFromPropertyHandle(FPropertyAccess::Result* OutPropertyAccessResult) const
{
	FName DataLayerName;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(DataLayerName);
	if (OutPropertyAccessResult)
	{
		*OutPropertyAccessResult = Result;
	}
	if (Result == FPropertyAccess::Success)
	{
		UDataLayer* DataLayer = UDataLayerEditorSubsystem::Get()->GetDataLayerFromName(DataLayerName);
		return DataLayer;
	}
	return nullptr;
}

FText FDataLayerPropertyTypeCustomization::GetDataLayerText() const
{
	FPropertyAccess::Result PropertyAccessResult;
	const UDataLayer* DataLayer = GetDataLayerFromPropertyHandle(&PropertyAccessResult);
	if (PropertyAccessResult == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}
	return UDataLayer::GetDataLayerText(DataLayer);
}

TSharedRef<SWidget> FDataLayerPropertyTypeCustomization::OnGetDataLayerMenu()
{
	return FDataLayerPropertyTypeCustomizationHelper::CreateDataLayerMenu([this](const UDataLayer* DataLayer) { AssignDataLayer(DataLayer); });
}

EVisibility FDataLayerPropertyTypeCustomization::GetSelectDataLayerVisibility() const
{
	const UDataLayer* DataLayer = GetDataLayerFromPropertyHandle();
	return DataLayer ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FDataLayerPropertyTypeCustomization::OnSelectDataLayer()
{
	if (UDataLayer* DataLayer = GetDataLayerFromPropertyHandle())
	{
		GEditor->SelectNone(true, true);
		UDataLayerEditorSubsystem::Get()->SelectActorsInDataLayer(DataLayer, true, true);
	}
	return FReply::Handled();
}

void FDataLayerPropertyTypeCustomization::AssignDataLayer(const UDataLayer* InDataLayer)
{
	if (GetDataLayerFromPropertyHandle() != InDataLayer)
	{
		PropertyHandle->SetValue(InDataLayer ? InDataLayer->GetFName() : NAME_None);
		UDataLayerEditorSubsystem::Get()->OnDataLayerChanged().Broadcast(EDataLayerAction::Reset, NULL, NAME_None);
	}
}

FReply FDataLayerPropertyTypeCustomization::OnDrop(TSharedPtr<FDragDropOperation> InDragDrop)
{
	TSharedPtr<const FDataLayerDragDropOp> DataLayerDragDropOp = GetDataLayerDragDropOp(InDragDrop);
	if (DataLayerDragDropOp.IsValid())
	{
		const TArray<FName>& DataLayerLabels = DataLayerDragDropOp->DataLayerLabels;
		if (ensure(DataLayerLabels.Num() == 1))
		{
			if (const UDataLayer* DataLayerPtr = UDataLayerEditorSubsystem::Get()->GetDataLayerFromLabel(DataLayerLabels[0]))
			{
				AssignDataLayer(DataLayerPtr);
			}
		}
	}
	return FReply::Handled();
}

bool FDataLayerPropertyTypeCustomization::OnVerifyDrag(TSharedPtr<FDragDropOperation> InDragDrop)
{
	TSharedPtr<const FDataLayerDragDropOp> DataLayerDragDropOp = GetDataLayerDragDropOp(InDragDrop);
	return DataLayerDragDropOp.IsValid() && DataLayerDragDropOp->DataLayerLabels.Num() == 1;
}

TSharedPtr<const FDataLayerDragDropOp> FDataLayerPropertyTypeCustomization::GetDataLayerDragDropOp(TSharedPtr<FDragDropOperation> InDragDrop)
{
	TSharedPtr<const FDataLayerDragDropOp> DataLayerDragDropOp;
	if (InDragDrop.IsValid())
	{
		if (InDragDrop->IsOfType<FCompositeDragDropOp>())
		{
			TSharedPtr<const FCompositeDragDropOp> CompositeDragDropOp = StaticCastSharedPtr<const FCompositeDragDropOp>(InDragDrop);
			DataLayerDragDropOp = CompositeDragDropOp->GetSubOp<FDataLayerDragDropOp>();
		}
		else if (InDragDrop->IsOfType<FDataLayerDragDropOp>())
		{
			DataLayerDragDropOp = StaticCastSharedPtr<const FDataLayerDragDropOp>(InDragDrop);
		}
	}
	return DataLayerDragDropOp;
}

#undef LOCTEXT_NAMESPACE