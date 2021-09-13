// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailCategoryGroupNode.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorConstants.h"
#include "SDetailExpanderArrow.h"
#include "SDetailRowIndent.h"
#include "Styling/StyleColors.h"

void SDetailCategoryTableRow::Construct( const FArguments& InArgs, TSharedRef<FDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	OwnerTreeNode = InOwnerTreeNode;

	bIsInnerCategory = InArgs._InnerCategory;
	bShowBorder = InArgs._ShowBorder;

	const float VerticalPadding = bIsInnerCategory ? 6 : 8;

	FDetailColumnSizeData& ColumnSizeData = InOwnerTreeNode->GetDetailsView()->GetColumnSizeData();

	TSharedRef<SHorizontalBox> HeaderBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.AutoWidth()
		[
			SNew(SDetailRowIndent, SharedThis(this))
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5,0,0,0)
		.AutoWidth()
		[
			SNew(SDetailExpanderArrow, SharedThis(this))
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(12, VerticalPadding, 0, VerticalPadding)
		.FillWidth(1)
		[
			SNew(STextBlock)
			.TransformPolicy(ETextTransformPolicy::ToUpper)
			.Text(InArgs._DisplayName)
			.Font(FAppStyle::Get().GetFontStyle(bIsInnerCategory ? PropertyEditorConstants::PropertyFontStyle : PropertyEditorConstants::CategoryFontStyle))
			.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
		];

	if (InArgs._HeaderContent.IsValid())
	{
		HeaderBox->AddSlot()
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			InArgs._HeaderContent.ToSharedRef()
		];
	}
	
	TWeakPtr<STableViewBase> OwnerTableViewWeak = InOwnerTableView;
	auto GetScrollbarWellBrush = [this, OwnerTableViewWeak]()
	{
		return SDetailTableRowBase::IsScrollBarVisible(OwnerTableViewWeak) ?
			FAppStyle::Get().GetBrush("DetailsView.GridLine") : 
			this->GetBackgroundImage();
	};

	auto GetScrollbarWellTint = [this, OwnerTableViewWeak]()
	{
		return SDetailTableRowBase::IsScrollBarVisible(OwnerTableViewWeak) ?
			FSlateColor(EStyleColor::White) : 
			this->GetInnerBackgroundColor();
	};

	this->ChildSlot
	[
		SNew( SBorder )
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
		.Padding(FMargin(0, 0, 0, 1))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SBorder)
				.BorderImage(this, &SDetailCategoryTableRow::GetBackgroundImage)
				.BorderBackgroundColor(this, &SDetailCategoryTableRow::GetInnerBackgroundColor)
				.Padding(0)
				[
					HeaderBox
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			.AutoWidth()
			[
				SNew(SBorder)
				.BorderImage_Lambda(GetScrollbarWellBrush)
				.BorderBackgroundColor_Lambda(GetScrollbarWellTint)
				.Padding(FMargin(0, 0, SDetailTableRowBase::ScrollBarPadding, 0))
			]
		]
	];

	STableRow< TSharedPtr< FDetailTreeNode > >::ConstructInternal(
		STableRow::FArguments()
			.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
		InOwnerTableView
	);
}

EVisibility SDetailCategoryTableRow::IsSeparatorVisible() const
{
	return bIsInnerCategory || IsItemExpanded() ? EVisibility::Collapsed : EVisibility::Visible;
}

const FSlateBrush* SDetailCategoryTableRow::GetBackgroundImage() const
{
	if (bShowBorder)
	{
		if (bIsInnerCategory)
		{
			return FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
		}

		if (IsHovered())
		{
			return FAppStyle::Get().GetBrush("DetailsView.CategoryTop_Hovered");
		}
		else
		{
			return FAppStyle::Get().GetBrush("DetailsView.CategoryTop");
		}
	}

	return nullptr;
}

FSlateColor SDetailCategoryTableRow::GetInnerBackgroundColor() const
{
	if (bShowBorder && bIsInnerCategory)
	{
		int32 IndentLevel = -1;
		if (OwnerTablePtr.IsValid())
		{
			IndentLevel = GetIndentLevel();
		}

		IndentLevel = FMath::Max(IndentLevel - 1, 0);

		return PropertyEditorConstants::GetRowBackgroundColor(IndentLevel);
	}

	return FSlateColor(FLinearColor::White);
}

FSlateColor SDetailCategoryTableRow::GetOuterBackgroundColor() const
{
	if (bIsHovered)
	{
		return FAppStyle::Get().GetSlateColor("Colors.Header");
	}

	return FAppStyle::Get().GetSlateColor("Colors.Panel");
}

FReply SDetailCategoryTableRow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ToggleExpansion();
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SDetailCategoryTableRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return OnMouseButtonDown(InMyGeometry, InMouseEvent);
}

FDetailCategoryGroupNode::FDetailCategoryGroupNode( const FDetailNodeList& InChildNodes, FName InGroupName, FDetailCategoryImpl& InParentCategory )
	: ChildNodes( InChildNodes )
	, ParentCategory( InParentCategory )
	, GroupName( InGroupName )
	, bShouldBeVisible( false )
	, bShowBorder(true)
	, bHasSplitter(false)
{
}

TSharedRef< ITableRow > FDetailCategoryGroupNode::GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, bool bAllowFavoriteSystem)
{
	return SNew( SDetailCategoryTableRow, AsShared(), OwnerTable )
		.DisplayName( FText::FromName(GroupName) )
		.InnerCategory( true )
		.ShowBorder( bShowBorder );
}


bool FDetailCategoryGroupNode::GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const
{
	OutRow.NameContent()
	[
		SNew(STextBlock)
		.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
		.Text(FText::FromName(GroupName))
	];

	return true;
}

void FDetailCategoryGroupNode::GetChildren( FDetailNodeList& OutChildren )
{
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = ChildNodes[ChildIndex];
		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			if( Child->ShouldShowOnlyChildren() )
			{
				Child->GetChildren( OutChildren );
			}
			else
			{
				OutChildren.Add( Child );
			}
		}
	}
}

void FDetailCategoryGroupNode::FilterNode( const FDetailFilter& InFilter )
{
	bShouldBeVisible = false;
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = ChildNodes[ChildIndex];

		Child->FilterNode( InFilter );

		if( Child->GetVisibility() == ENodeVisibility::Visible )
		{
			bShouldBeVisible = true;

			ParentCategory.RequestItemExpanded( Child, Child->ShouldBeExpanded() );
		}
	}
}
