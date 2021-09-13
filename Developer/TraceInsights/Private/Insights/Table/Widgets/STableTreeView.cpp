// Copyright Epic Games, Inc. All Rights Reserved.

#include "STableTreeView.h"

#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "TraceServices/AnalysisService.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Views/STableViewBase.h"

// Insights
#include "Insights/InsightsManager.h"
#include "Insights/Log.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"
#include "Insights/Table/ViewModels/TreeNodeSorting.h"
#include "Insights/Table/ViewModels/UntypedTable.h"
#include "Insights/Table/Widgets/STableTreeViewTooltip.h"
#include "Insights/Table/Widgets/STableTreeViewRow.h"
#include "Insights/TimingProfilerCommon.h"
#include "Insights/ViewModels/FilterConfigurator.h"
#include "Insights/Widgets/SAsyncOperationStatus.h"

#include <limits>

#define LOCTEXT_NAMESPACE "STableTreeView"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTableTreeViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

class FTableTreeViewCommands : public TCommands<FTableTreeViewCommands>
{
public:
	FTableTreeViewCommands()
		: TCommands<FTableTreeViewCommands>(TEXT("FTableTreeViewCommands"), NSLOCTEXT("FTableTreeViewCommands", "Table Tree View Commands", "Table Tree View Commands"), NAME_None, FEditorStyle::Get().GetStyleSetName())
	{
	}

	virtual ~FTableTreeViewCommands()
	{
	}

	// UI_COMMAND takes long for the compiler to optimize
	PRAGMA_DISABLE_OPTIMIZATION
	virtual void RegisterCommands() override
	{
		UI_COMMAND(Command_CopyToClipboard, "Copy To Clipboard", "Copies selection to clipboard.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
		UI_COMMAND(Command_CopyColumnToClipboard, "Copy Column Value To Clipboard", "Copies the value of hovered column to clipboard.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::C));
		UI_COMMAND(Command_CopyColumnTooltipToClipboard, "Copy Column Tooltip To Clipboard", "Copies the value of hovered column's tooltip to clipboard.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::C));
		UI_COMMAND(Command_ExpandSubtree, "Expand Subtree", "Expand the subtree that starts from the selected group node.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::E));
		UI_COMMAND(Command_ExpandCriticalPath, "Expand Critical Path", "Expand the first group child node recursively until a leaf nodes in reached.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::R));
		UI_COMMAND(Command_CollapseSubtree, "Collapse Subtree", "Collapse the subtree that starts from the selected group node.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::C));
	}
	PRAGMA_ENABLE_OPTIMIZATION

	TSharedPtr<FUICommandInfo> Command_CopyToClipboard;
	TSharedPtr<FUICommandInfo> Command_CopyColumnToClipboard;
	TSharedPtr<FUICommandInfo> Command_CopyColumnTooltipToClipboard;
	TSharedPtr<FUICommandInfo> Command_ExpandSubtree;
	TSharedPtr<FUICommandInfo> Command_ExpandCriticalPath;
	TSharedPtr<FUICommandInfo> Command_CollapseSubtree;
};

const FName STableTreeView::RootNodeName(TEXT("Root"));

////////////////////////////////////////////////////////////////////////////////////////////////////

STableTreeView::STableTreeView()
	: Table()
	, Session(FInsightsManager::Get()->GetSession())
	, TreeView(nullptr)
	, TreeViewHeaderRow(nullptr)
	, ExternalScrollbar(nullptr)
	, HoveredColumnId()
	, HoveredNodePtr(nullptr)
	, HighlightedNodeName()
	, Root(MakeShared<FTableTreeNode>(RootNodeName, Table))
	, TableTreeNodes()
	, FilteredGroupNodes()
	, ExpandedNodes()
	, bExpansionSaved(false)
	, SearchBox(nullptr)
	, TextFilter(nullptr)
	, Filters(nullptr)
	, AvailableGroupings()
	, CurrentGroupings()
	, GroupingBreadcrumbTrail(nullptr)
	, AvailableSorters()
	, CurrentSorter(nullptr)
	, ColumnBeingSorted(GetDefaultColumnBeingSorted())
	, ColumnSortMode(GetDefaultColumnSortMode())
	, StatsStartTime(0.0)
	, StatsEndTime(0.0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

STableTreeView::~STableTreeView()
{
	// Remove ourselves from the Insights manager.
	if (FInsightsManager::Get().IsValid())
	{
		FInsightsManager::Get()->GetSessionChangedEvent().RemoveAll(this);
	}

	if (CurrentAsyncOpFilterConfigurator)
	{
		delete CurrentAsyncOpFilterConfigurator;
		CurrentAsyncOpFilterConfigurator = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::Construct(const FArguments& InArgs, TSharedPtr<FTable> InTablePtr)
{
	ConstructWidget(InTablePtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STableTreeView::ConstructWidget(TSharedPtr<FTable> InTablePtr)
{
	check(InTablePtr.IsValid());
	Table = InTablePtr;

	SAssignNew(ExternalScrollbar, SScrollBar)
	.AlwaysShowScrollbar(true);

	TSharedPtr<SHorizontalBox> ToolbarBox;

	auto WidgetContent = 
	SNew(SVerticalBox)

	+ SVerticalBox::Slot()
	.VAlign(VAlign_Center)
	.AutoHeight()
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(2.0f)
		[
			SNew(SVerticalBox)

			// Search box
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0)
				.VAlign(VAlign_Center)
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchBox_Hint", "Search"))
					.OnTextChanged(this, &STableTreeView::SearchBox_OnTextChanged)
					.IsEnabled(this, &STableTreeView::SearchBox_IsEnabled)
					.ToolTipText(this, &STableTreeView::SearchBox_GetTooltipText)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("AdvancedFiltersBtn_Text", "Advanced Filters"))
					.ToolTipText(this, &STableTreeView::AdvancedFilters_GetTooltipText)
					.OnClicked(this, &STableTreeView::OnAdvancedFiltersClicked)
					.IsEnabled(this, &STableTreeView::AdvancedFilters_ShouldBeEnabled)
				]
			]

			// Group by
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2.0f)
			.AutoHeight()
			[
				SAssignNew(ToolbarBox, SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GroupByText", "Hierarchy:"))
					.Margin(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(GroupingBreadcrumbTrail, SBreadcrumbTrail<TSharedPtr<FTreeNodeGrouping>>)
					.ButtonContentPadding(FMargin(1.0f, 1.0f))
					//.DelimiterImage(FEditorStyle::GetBrush("SlateFileDialogs.PathDelimiter"))
					//.TextStyle(FEditorStyle::Get(), "Tutorials.Browser.PathText")
					//.ShowLeadingDelimiter(true)
					//.PersistentBreadcrumbs(true)
					.OnCrumbClicked(this, &STableTreeView::OnGroupingCrumbClicked)
					.GetCrumbMenuContent(this, &STableTreeView::GetGroupingCrumbMenuContent)
				]
			]
		]
	]

	// Tree view
	+ SVerticalBox::Slot()
	.FillHeight(1.0f)
	.Padding(0.0f, 6.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f)
		[
			//SNew(SScrollBox)
			//.Orientation(Orient_Horizontal)

			//+ SScrollBox::Slot()
			//[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						SAssignNew(TreeView, STreeView<FTableTreeNodePtr>)
						//.ExternalScrollbar(ExternalScrollbar)
						.SelectionMode(ESelectionMode::Multi)
						.TreeItemsSource(&FilteredGroupNodes)
						.OnGetChildren(this, &STableTreeView::TreeView_OnGetChildren)
						.OnGenerateRow(this, &STableTreeView::TreeView_OnGenerateRow)
						.OnSelectionChanged(this, &STableTreeView::TreeView_OnSelectionChanged)
						.OnMouseButtonDoubleClick(this, &STableTreeView::TreeView_OnMouseButtonDoubleClick)
						.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &STableTreeView::TreeView_GetMenuContent))
						.ItemHeight(12.0f)
						.HeaderRow
						(
							SAssignNew(TreeViewHeaderRow, SHeaderRow)
							.Visibility(EVisibility::Visible)
						)
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(16.0f)
				[
					SAssignNew(AsyncOperationStatus, Insights::SAsyncOperationStatus, SharedThis(this))
				]
			//]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f)
		[
			SNew(SBox)
			.WidthOverride(FOptionalSize(13.0f))
			[
				ExternalScrollbar.ToSharedRef()
			]
		]
	];

	TSharedPtr<SWidget> Toolbar = ConstructToolbar();
	if (Toolbar.IsValid())
	{
		ToolbarBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.Padding(0.0f)
			[
				Toolbar.ToSharedRef()
			];
	}

	TSharedPtr<SWidget> Footer = ConstructFooter();
	if (Footer.IsValid())
	{
		WidgetContent->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			Footer.ToSharedRef()
		];
	}

	ChildSlot
	[
		WidgetContent
	];

	// Create the search filters: text based, type based etc.
	TextFilter = MakeShared<FTableTreeNodeTextFilter>(FTableTreeNodeTextFilter::FItemToStringArray::CreateStatic(&STableTreeView::HandleItemToStringArray));
	Filters = MakeShared<FTableTreeNodeFilterCollection>();
	if (bRunInAsyncMode)
	{
		CurrentAsyncOpTextFilter = MakeShared<FTableTreeNodeTextFilter>(FTableTreeNodeTextFilter::FItemToStringArray::CreateStatic(&STableTreeView::HandleItemToStringArray));
		Filters->Add(CurrentAsyncOpTextFilter);
	}
	else
	{
		Filters->Add(TextFilter);
	}

	InitializeAndShowHeaderColumns();
	CreateGroupings();
	CreateSortings();

	InitCommandList();

	// Register ourselves with the Insights manager.
	FInsightsManager::Get()->GetSessionChangedEvent().AddSP(this, &STableTreeView::InsightsManager_OnSessionChanged);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::InitCommandList()
{
	FTableTreeViewCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(FTableTreeViewCommands::Get().Command_CopyToClipboard, FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CopySelectedToClipboard_Execute), FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CopySelectedToClipboard_CanExecute));
	CommandList->MapAction(FTableTreeViewCommands::Get().Command_CopyColumnToClipboard, FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CopyColumnToClipboard_Execute), FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CopyColumnToClipboard_CanExecute));
	CommandList->MapAction(FTableTreeViewCommands::Get().Command_CopyColumnTooltipToClipboard, FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CopyColumnTooltipToClipboard_Execute), FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CopyColumnTooltipToClipboard_CanExecute));
	CommandList->MapAction(FTableTreeViewCommands::Get().Command_ExpandSubtree, FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ExpandSubtree_Execute), FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ExpandSubtree_CanExecute));
	CommandList->MapAction(FTableTreeViewCommands::Get().Command_ExpandCriticalPath, FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ExpandCriticalPath_Execute), FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ExpandCriticalPath_CanExecute));
	CommandList->MapAction(FTableTreeViewCommands::Get().Command_CollapseSubtree, FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CollapseSubtree_Execute), FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_CollapseSubtree_CanExecute));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> STableTreeView::TreeView_GetMenuContent()
{
	const TArray<FTableTreeNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	const int32 NumSelectedNodes = SelectedNodes.Num();
	FTableTreeNodePtr SelectedNode = NumSelectedNodes ? SelectedNodes[0] : nullptr;

	const TSharedPtr<FTableColumn> HoveredColumnPtr = Table->FindColumn(HoveredColumnId);

	FText SelectionStr;
	FText PropertyName;
	FText PropertyValue;

	if (NumSelectedNodes == 0)
	{
		SelectionStr = LOCTEXT("NothingSelected", "Nothing selected");
	}
	else if (NumSelectedNodes == 1)
	{
		if (HoveredColumnPtr != nullptr)
		{
			PropertyName = HoveredColumnPtr->GetShortName();
			PropertyValue = HoveredColumnPtr->GetValueAsTooltipText(*SelectedNode);
		}
		FString ItemName = SelectedNode->GetName().ToString();
		const int32 MaxStringLen = 64;
		if (ItemName.Len() > MaxStringLen)
		{
			ItemName = ItemName.Left(MaxStringLen) + TEXT("...");
		}
		SelectionStr = FText::FromString(ItemName);
	}
	else
	{
		SelectionStr = LOCTEXT("MultipleSelection", "Multiple selection");
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList.ToSharedRef());

	// Selection menu
	MenuBuilder.BeginSection("Selection", LOCTEXT("ContextMenu_Header_Selection", "Selection"));
	{
		struct FLocal
		{
			static bool ReturnFalse()
			{
				return false;
			}
		};

		FUIAction DummyUIAction;
		DummyUIAction.CanExecuteAction = FCanExecuteAction::CreateStatic(&FLocal::ReturnFalse);
		MenuBuilder.AddMenuEntry
		(
			SelectionStr,
			LOCTEXT("ContextMenu_Selection", "Currently selected items"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "@missing.icon"), DummyUIAction, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Node", LOCTEXT("ContextMenu_Header_Node", "Node"));
	{
		MenuBuilder.AddMenuEntry
		(
			FTableTreeViewCommands::Get().Command_ExpandSubtree,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandSelection")
		);

		MenuBuilder.AddMenuEntry
		(
			FTableTreeViewCommands::Get().Command_ExpandCriticalPath,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandSelection")
		);

		MenuBuilder.AddMenuEntry
		(
			FTableTreeViewCommands::Get().Command_CollapseSubtree,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.CollapseSelection")
		);
	}

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Misc", LOCTEXT("ContextMenu_Header_Misc", "Miscellaneous"));
	{
		MenuBuilder.AddMenuEntry
		(
			FTableTreeViewCommands::Get().Command_CopyToClipboard,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.CopyToClipboard")
		);

		MenuBuilder.AddMenuEntry
		(
			FTableTreeViewCommands::Get().Command_CopyColumnToClipboard,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.CopyToClipboard")
		);

		MenuBuilder.AddMenuEntry
		(
			FTableTreeViewCommands::Get().Command_CopyColumnTooltipToClipboard,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.CopyToClipboard")
		);

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_Header_Misc_Sort", "Sort By"),
			LOCTEXT("ContextMenu_Header_Misc_Sort_Desc", "Sort by column"),
			FNewMenuDelegate::CreateSP(this, &STableTreeView::TreeView_BuildSortByMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortBy")
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Columns", LOCTEXT("ContextMenu_Header_Columns", "Columns"));
	{
		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_Header_Columns_View", "View Column"),
			LOCTEXT("ContextMenu_Header_Columns_View_Desc", "Hides or shows columns"),
			FNewMenuDelegate::CreateSP(this, &STableTreeView::TreeView_BuildViewColumnMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ViewColumn")
		);

		FUIAction Action_ShowAllColumns
		(
			FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ShowAllColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ShowAllColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Header_Columns_ShowAllColumns", "Show All Columns"),
			LOCTEXT("ContextMenu_Header_Columns_ShowAllColumns_Desc", "Resets tree view to show all columns"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_ShowAllColumns, NAME_None, EUserInterfaceActionType::Button
		);

		FUIAction Action_ResetColumns
		(
			FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ResetColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_ResetColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Header_Columns_ResetColumns", "Reset Columns to Default"),
			LOCTEXT("ContextMenu_Header_Columns_ResetColumns_Desc", "Resets columns to default"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_ResetColumns, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TreeView_BuildSortByMenu(FMenuBuilder& MenuBuilder)
{
	// TODO: Refactor later @see TSharedPtr<SWidget> SCascadePreviewViewportToolBar::GenerateViewMenu() const

	MenuBuilder.BeginSection("ColumnName", LOCTEXT("ContextMenu_Header_Misc_ColumnName", "Column Name"));

	//TODO: for (Sorting : AvailableSortings)
	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const FTableColumn& Column = ColumnRef.Get();
		if (Column.IsVisible() && Column.CanBeSorted())
		{
			FUIAction Action_SortByColumn
			(
				FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_SortByColumn_Execute, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_SortByColumn_CanExecute, Column.GetId()),
				FIsActionChecked::CreateSP(this, &STableTreeView::ContextMenu_SortByColumn_IsChecked, Column.GetId())
			);
			MenuBuilder.AddMenuEntry
			(
				Column.GetTitleName(),
				Column.GetDescription(),
				FSlateIcon(), Action_SortByColumn, NAME_None, EUserInterfaceActionType::RadioButton
			);
		}
	}

	MenuBuilder.EndSection();

	//-----------------------------------------------------------------------------

	MenuBuilder.BeginSection("SortMode", LOCTEXT("ContextMenu_Header_Misc_Sort_SortMode", "Sort Mode"));
	{
		FUIAction Action_SortAscending
		(
			FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_SortMode_Execute, EColumnSortMode::Ascending),
			FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Ascending),
			FIsActionChecked::CreateSP(this, &STableTreeView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Ascending)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending", "Sort Ascending"),
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending_Desc", "Sorts ascending"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortAscending"), Action_SortAscending, NAME_None, EUserInterfaceActionType::RadioButton
		);

		FUIAction Action_SortDescending
		(
			FExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_SortMode_Execute, EColumnSortMode::Descending),
			FCanExecuteAction::CreateSP(this, &STableTreeView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Descending),
			FIsActionChecked::CreateSP(this, &STableTreeView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Descending)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending", "Sort Descending"),
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending_Desc", "Sorts descending"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortDescending"), Action_SortDescending, NAME_None, EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TreeView_BuildViewColumnMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("ViewColumn", LOCTEXT("ContextMenu_Header_Columns_View", "View Column"));

	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const FTableColumn& Column = ColumnRef.Get();
		FUIAction Action_ToggleColumn
		(
			FExecuteAction::CreateSP(this, &STableTreeView::ToggleColumnVisibility, Column.GetId()),
			FCanExecuteAction::CreateSP(this, &STableTreeView::CanToggleColumnVisibility, Column.GetId()),
			FIsActionChecked::CreateSP(this, &STableTreeView::IsColumnVisible, Column.GetId())
		);
		MenuBuilder.AddMenuEntry
		(
			Column.GetTitleName(),
			Column.GetDescription(),
			FSlateIcon(), Action_ToggleColumn, NAME_None, EUserInterfaceActionType::ToggleButton
		);
	}

	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::InitializeAndShowHeaderColumns()
{
	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		FTableColumn& Column = ColumnRef.Get();
		if (Column.ShouldBeVisible())
		{
			ShowColumn(Column);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::RebuildColumns()
{
	TreeViewHeaderRow->ClearColumns();
	InitializeAndShowHeaderColumns();

	PreChangeGroupings();
	CreateGroupings();
	PostChangeGroupings();

	CreateSortings();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STableTreeView::GetColumnHeaderText(const FName ColumnId) const
{
	const FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.GetShortName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STableTreeView::TreeViewHeaderRow_GenerateColumnMenu(const FTableColumn& Column)
{
	bool bIsMenuVisible = false;

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);
	{
		if (Column.CanBeHidden())
		{
			MenuBuilder.BeginSection("Column", LOCTEXT("TreeViewHeaderRow_Header_Column", "Column"));

			FUIAction Action_HideColumn
			(
				FExecuteAction::CreateSP(this, &STableTreeView::HideColumn, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &STableTreeView::CanHideColumn, Column.GetId())
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("TreeViewHeaderRow_HideColumn", "Hide"),
				LOCTEXT("TreeViewHeaderRow_HideColumn_Desc", "Hides the selected column"),
				FSlateIcon(), Action_HideColumn, NAME_None, EUserInterfaceActionType::Button
			);
			bIsMenuVisible = true;

			MenuBuilder.EndSection();
		}

		if (Column.CanBeSorted())
		{
			MenuBuilder.BeginSection("SortMode", LOCTEXT("ContextMenu_Header_Misc_Sort_SortMode", "Sort Mode"));

			FUIAction Action_SortAscending
			(
				FExecuteAction::CreateSP(this, &STableTreeView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Ascending),
				FCanExecuteAction::CreateSP(this, &STableTreeView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Ascending),
				FIsActionChecked::CreateSP(this, &STableTreeView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Ascending)
			);
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending", "Sort Ascending"),
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending_Desc", "Sorts ascending"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortAscending"), Action_SortAscending, NAME_None, EUserInterfaceActionType::RadioButton
			);

			FUIAction Action_SortDescending
			(
				FExecuteAction::CreateSP(this, &STableTreeView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Descending),
				FCanExecuteAction::CreateSP(this, &STableTreeView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Descending),
				FIsActionChecked::CreateSP(this, &STableTreeView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Descending)
			);
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending", "Sort Descending"),
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending_Desc", "Sorts descending"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortDescending"), Action_SortDescending, NAME_None, EUserInterfaceActionType::RadioButton
			);
			bIsMenuVisible = true;

			MenuBuilder.EndSection();
		}

		//if (Column.CanBeFiltered())
		//{
		//	MenuBuilder.BeginSection("FilterMode", LOCTEXT("ContextMenu_Header_Misc_Filter_FilterMode", "Filter Mode"));
		//	bIsMenuVisible = true;
		//	MenuBuilder.EndSection();
		//}
	}

	return bIsMenuVisible ? MenuBuilder.MakeWidget() : (TSharedRef<SWidget>)SNullWidget::NullWidget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::InsightsManager_OnSessionChanged()
{
	TSharedPtr<const TraceServices::IAnalysisSession> NewSession = FInsightsManager::Get()->GetSession();

	if (NewSession != Session)
	{
		Session = NewSession;
		Reset();
	}
	else
	{
		UpdateTree();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bRunInAsyncMode && bIsUpdateRunning && !bIsCloseScheduled)
	{
		if (DispatchEvent.IsValid() && !DispatchEvent->IsComplete())
		{
			// We wait for the TreeView to be refreshed before dispatching the tasks.
			// This should make the TreeView release all of it's shared pointers to nodes to prevent 
			// the TreeView (MainThread) and the tasks from accesing the non-thread safe shared pointers at the same time.
			if (!TreeView->IsPendingRefresh())
			{
				DispatchEvent->DispatchSubsequents();
			}
		}
		else
		{
			check(InProgressAsyncOperationEvent.IsValid());
			if (InProgressAsyncOperationEvent->IsComplete())
			{
				OnPostAsyncUpdate();
				StartPendingAsyncOperations();
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::UpdateTree()
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	if (bRunInAsyncMode)
	{
		if (!bIsUpdateRunning)
		{
			OnPreAsyncUpdate();

			FGraphEventRef CompletedEvent = StartCreateGroupsTask();
			CompletedEvent = StartSortTreeNodesTask(CompletedEvent);
			InProgressAsyncOperationEvent = StartApplyFiltersTask(CompletedEvent);
		}
		else
		{
			CancelCurrentAsyncOp();
		}
	}
	else
	{
		CreateGroups(CurrentGroupings);
		if (CurrentSorter.IsValid())
		{
			SortTreeNodes(CurrentSorter.Get(), ColumnSortMode);
		}
		ApplyFiltering();
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TraceInsights, Log, TEXT("[Tree - %s] Tree updated (grouping + sorting + filtering) in %.3fs."), *Table->GetDisplayName().ToString(), TotalTime);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ApplyFiltering()
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	// Apply filter to all groups and its children.
	if (FilterConfigurator_HasFilters())
	{
		ApplyAdvancedFiltersForNode(Root);
	}
	else
	{
		const bool bFilterIsEmpty = bRunInAsyncMode ? CurrentAsyncOpTextFilter->GetRawFilterText().IsEmpty() : TextFilter->GetRawFilterText().IsEmpty();
		ApplyHierarchicalFilterForNode(Root, bFilterIsEmpty);
	}

	FilteredGroupNodes.Reset();
	const TArray<FBaseTreeNodePtr>& RootChildren = Root->GetFilteredChildren();
	const int32 NumRootChildren = RootChildren.Num();
	for (int32 Cx = 0; Cx < NumRootChildren; ++Cx)
	{
		// Add a child.
		const FTableTreeNodePtr& ChildNodePtr = StaticCastSharedPtr<FTableTreeNode>(RootChildren[Cx]);
		if (ChildNodePtr->IsGroup())
		{
			FilteredGroupNodes.Add(ChildNodePtr);
		}
	}

	UpdateAggregatedValues(*Root);

	// Cannot call TreeView functions from other threads than MainThread and SlateThread. 
	if (!bRunInAsyncMode)
	{
		// Only expand nodes if we have a text filter.
		const bool bNonEmptyTextFilter = !TextFilter->GetRawFilterText().IsEmpty();
		if (bNonEmptyTextFilter)
		{
			if (!bExpansionSaved)
			{
				ExpandedNodes.Empty();
				TreeView->GetExpandedItems(ExpandedNodes);
				bExpansionSaved = true;
			}

			for (int32 Fx = 0; Fx < FilteredGroupNodes.Num(); Fx++)
			{
				const FTableTreeNodePtr& GroupPtr = FilteredGroupNodes[Fx];
				TreeView->SetItemExpansion(GroupPtr, GroupPtr->IsExpanded());
			}
		}
		else
		{
			if (bExpansionSaved)
			{
				// Restore previously expanded nodes when the text filter is disabled.
				TreeView->ClearExpandedItems();
				for (auto It = ExpandedNodes.CreateConstIterator(); It; ++It)
				{
					TreeView->SetItemExpansion(*It, true);
				}
				bExpansionSaved = false;
			}
		}

		// Request tree refresh
		TreeView->RequestTreeRefresh();
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TraceInsights, Log, TEXT("[Tree - %s] Filtering completed in %.3fs."), *Table->GetDisplayName().ToString(), TotalTime);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ApplyHierarchicalFilterForNode(FTableTreeNodePtr NodePtr, bool bFilterIsEmpty)
{
	bool bIsNodeVisible = bFilterIsEmpty || Filters->PassesAllFilters(NodePtr);

	if (NodePtr->IsGroup())
	{
		// If a group node passes the filter, all child nodes will be shown
		if (bIsNodeVisible)
		{
			MakeSubtreeVisible(NodePtr, bFilterIsEmpty);
			return true;
		}

		NodePtr->ClearFilteredChildren();

		const TArray<FBaseTreeNodePtr>& GroupChildren = NodePtr->GetChildren();
		const int32 NumChildren = GroupChildren.Num();
		int32 NumVisibleChildren = 0;
		for (int32 Cx = 0; Cx < NumChildren; ++Cx)
		{
			if (bCancelCurrentAsyncOp)
			{
				break;
			}
			// Add a child.
			const FTableTreeNodePtr& ChildNodePtr = StaticCastSharedPtr<FTableTreeNode>(GroupChildren[Cx]);
			if (ApplyHierarchicalFilterForNode(ChildNodePtr, bFilterIsEmpty))
			{
				NodePtr->AddFilteredChild(ChildNodePtr);
				NumVisibleChildren++;
			}
		}

		const bool bIsGroupNodeVisible = bIsNodeVisible || NumVisibleChildren > 0;
		if (!bFilterIsEmpty && bIsGroupNodeVisible )
		{
			if (bRunInAsyncMode)
			{
				NodesToExpand.Add(NodePtr);
			}
			else
			{
				TreeView->SetItemExpansion(NodePtr, true);
			}
		}
		return bIsGroupNodeVisible;
	}
	else
	{
		NodePtr->SetIsFiltered(!bIsNodeVisible);
		return bIsNodeVisible;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ApplyAdvancedFiltersForNode(FTableTreeNodePtr NodePtr)
{
	if (NodePtr->IsGroup())
	{
		NodePtr->ClearFilteredChildren();

		const TArray<FBaseTreeNodePtr>& GroupChildren = NodePtr->GetChildren();
		const int32 NumChildren = GroupChildren.Num();
		int32 NumVisibleChildren = 0;
		for (int32 Cx = 0; Cx < NumChildren; ++Cx)
		{
			if (bCancelCurrentAsyncOp)
			{
				break;
			}
			// Add a child.
			const FTableTreeNodePtr& ChildNodePtr = StaticCastSharedPtr<FTableTreeNode>(GroupChildren[Cx]);
			if (ApplyAdvancedFiltersForNode(ChildNodePtr))
			{
				NodePtr->AddFilteredChild(ChildNodePtr);
				NumVisibleChildren++;
			}
		}

		const bool bIsGroupNodeVisible = NumVisibleChildren > 0;

		if (bIsGroupNodeVisible)
		{
			// Add a group.
			NodePtr->SetExpansion(true);
		}
		else
		{
			NodePtr->SetExpansion(false);
		}

		return bIsGroupNodeVisible;
	}
	else
	{
		bool bIsNodeVisible = ApplyAdvancedFilters(NodePtr);
		NodePtr->SetIsFiltered(!bIsNodeVisible);
		return bIsNodeVisible;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::MakeSubtreeVisible(FTableTreeNodePtr NodePtr, bool bFilterIsEmpty)
{
	bool bPassesNonEmptyFilter = !bFilterIsEmpty && Filters->PassesAllFilters(NodePtr);
	if (NodePtr->IsGroup())
	{
		NodePtr->ClearFilteredChildren();

		const TArray<FBaseTreeNodePtr>& GroupChildren = NodePtr->GetChildren();
		const int32 NumChildren = GroupChildren.Num();
		int32 NumVisibleChildren = 0;
		bool bShouldExpand = false;
		for (int32 Cx = 0; Cx < NumChildren; ++Cx)
		{
			if (bCancelCurrentAsyncOp)
			{
				break;
			}

			const FTableTreeNodePtr& ChildNodePtr = StaticCastSharedPtr<FTableTreeNode>(GroupChildren[Cx]);
			bShouldExpand |= MakeSubtreeVisible(ChildNodePtr, bFilterIsEmpty);
			NodePtr->AddFilteredChild(ChildNodePtr);
			NodePtr->SetExpansion(true);

			if (bShouldExpand)
			{
				if (bRunInAsyncMode)
				{
					NodesToExpand.Add(NodePtr);
				}
				else
				{
					TreeView->SetItemExpansion(NodePtr, true);
				}
			}
		}

		return bShouldExpand || bPassesNonEmptyFilter;
	}

	NodePtr->SetIsFiltered(false);
	return bPassesNonEmptyFilter;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::HandleItemToStringArray(const FTableTreeNodePtr& FTableTreeNodePtr, TArray<FString>& OutSearchStrings)
{
	OutSearchStrings.Add(FTableTreeNodePtr->GetName().GetPlainNameString());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeView
////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TreeView_Refresh()
{
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TreeView_OnSelectionChanged(FTableTreeNodePtr SelectedItem, ESelectInfo::Type SelectInfo)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TreeView_OnGetChildren(FTableTreeNodePtr InParent, TArray<FTableTreeNodePtr>& OutChildren)
{
	const TArray<FBaseTreeNodePtr>& FilteredChildren = InParent->GetFilteredChildren();
	for (const FBaseTreeNodePtr& NodePtr : FilteredChildren)
	{
		OutChildren.Add(StaticCastSharedPtr<FTableTreeNode>(NodePtr));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TreeView_OnMouseButtonDoubleClick(FTableTreeNodePtr NodePtr)
{
	if (NodePtr->IsGroup())
	{
		const bool bIsGroupExpanded = TreeView->IsItemExpanded(NodePtr);
		TreeView->SetItemExpansion(NodePtr, !bIsGroupExpanded);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tree View's Table Row
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> STableTreeView::TreeView_OnGenerateRow(FTableTreeNodePtr NodePtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> TableRow =
		SNew(STableTreeViewRow, OwnerTable)
		.OnShouldBeEnabled(this, &STableTreeView::TableRow_ShouldBeEnabled)
		.OnIsColumnVisible(this, &STableTreeView::IsColumnVisible)
		.OnSetHoveredCell(this, &STableTreeView::TableRow_SetHoveredCell)
		.OnGetColumnOutlineHAlignmentDelegate(this, &STableTreeView::TableRow_GetColumnOutlineHAlignment)
		.HighlightText(this, &STableTreeView::TableRow_GetHighlightText)
		.HighlightedNodeName(this, &STableTreeView::TableRow_GetHighlightedNodeName)
		.TablePtr(Table)
		.TableTreeNodePtr(NodePtr);

	return TableRow;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::TableRow_ShouldBeEnabled(FTableTreeNodePtr NodePtr) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::TableRow_SetHoveredCell(TSharedPtr<FTable> InTablePtr, TSharedPtr<FTableColumn> InColumnPtr, FTableTreeNodePtr InNodePtr)
{
	const bool bIsAnyMenusVisible = FSlateApplication::Get().AnyMenusVisible();
	if (!HasMouseCapture() && !bIsAnyMenusVisible)
	{
		HoveredColumnId = InColumnPtr ? InColumnPtr->GetId() : FName();
		HoveredNodePtr = InNodePtr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EHorizontalAlignment STableTreeView::TableRow_GetColumnOutlineHAlignment(const FName ColumnId) const
{
	const TIndirectArray<SHeaderRow::FColumn>& Columns = TreeViewHeaderRow->GetColumns();
	const int32 LastColumnIdx = Columns.Num() - 1;

	// First column
	if (Columns[0].ColumnId == ColumnId)
	{
		return HAlign_Left;
	}
	// Last column
	else if (Columns[LastColumnIdx].ColumnId == ColumnId)
	{
		return HAlign_Right;
	}
	// Middle columns
	{
		return HAlign_Center;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STableTreeView::TableRow_GetHighlightText() const
{
	return SearchBox->GetText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName STableTreeView::TableRow_GetHighlightedNodeName() const
{
	return HighlightedNodeName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SearchBox
////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SearchBox_OnTextChanged(const FText& InFilterText)
{
	TextFilter->SetRawFilterText(InFilterText);
	SearchBox->SetError(TextFilter->GetFilterErrorText());

	if (bRunInAsyncMode)
	{
		if (!bIsUpdateRunning)
		{
			OnPreAsyncUpdate();

			InProgressAsyncOperationEvent = StartApplyFiltersTask();
		}
		else
		{
			CancelCurrentAsyncOp();
		}
	}
	else
	{
		ApplyFiltering();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::SearchBox_IsEnabled() const
{
	return !FilterConfigurator_HasFilters();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STableTreeView::SearchBox_GetTooltipText() const
{
	if (SearchBox_IsEnabled())
	{
		return LOCTEXT("SearchBox_ToolTip", "Type here to search the tree hierarchy by item or group name.");
	}

	return LOCTEXT("SearchBox_Disabled_ToolTip", "Searching the tree hierarchy is disabled when advanced filters are set.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Grouping
////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::CreateGroups(const TArray<TSharedPtr<FTreeNodeGrouping>>& Groupings)
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	GroupNodesRec(TableTreeNodes, *Root, 0, Groupings);

	Stopwatch.Update();
	const double Time1 = Stopwatch.GetAccumulatedTime();

	UpdateAggregatedValues(*Root);

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TraceInsights, Log, TEXT("[Tree - %s] Grouping completed in %.3fs (%.3fs + %.3fs)."), *Table->GetDisplayName().ToString(), TotalTime, Time1, TotalTime - Time1);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupNodesRec(const TArray<FTableTreeNodePtr>& Nodes, FTableTreeNode& ParentGroup, int32 GroupingDepth, const TArray<TSharedPtr<FTreeNodeGrouping>>& Groupings)
{
	if (bIsUpdateRunning && bCancelCurrentAsyncOp)
	{
		return;
	}

	ensure(Groupings.Num() > 0);

	FTreeNodeGrouping& Grouping = *Groupings[GroupingDepth];

	Grouping.GroupNodes(Nodes, ParentGroup, Table, bCancelCurrentAsyncOp);

	for (FBaseTreeNodePtr GroupPtr : ParentGroup.GetChildren())
	{
		ensure(GroupPtr->IsGroup());
		FTableTreeNodePtr TableTreeGroupPtr = StaticCastSharedPtr<FTableTreeNode>(GroupPtr);
		if (!bRunInAsyncMode)
		{
			TreeView->SetItemExpansion(TableTreeGroupPtr, TableTreeGroupPtr->IsExpanded());
		}
	}

	if (GroupingDepth < Groupings.Num() - 1)
	{
		TArray<FTableTreeNodePtr> ChildNodes;

		for (FBaseTreeNodePtr GroupPtr : ParentGroup.GetChildren())
		{
			ensure(GroupPtr->IsGroup());
			FTableTreeNode& Group = *StaticCastSharedPtr<FTableTreeNode>(GroupPtr);

			// Make a copy of the child nodes.
			ChildNodes.Reset();
			for (FBaseTreeNodePtr ChildPtr : Group.GetChildren())
			{
				ChildNodes.Add(StaticCastSharedPtr<FTableTreeNode>(ChildPtr));
			}

			GroupNodesRec(ChildNodes, Group, GroupingDepth + 1, Groupings);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::UpdateAggregatedValues(FTableTreeNode& GroupNode)
{
	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		FTableColumn& Column = ColumnRef.Get();
		switch (Column.GetAggregation())
		{
			case ETableColumnAggregation::Sum:
				STableTreeView::UpdateAggregationRec<int64>(Column, GroupNode, 0, true, [](int64 InValue, TOptional<FTableCellValue> InTableCellValue)
				{
					return InValue + InTableCellValue->AsInt64();
				});
				break;

			case ETableColumnAggregation::Min:
				STableTreeView::UpdateAggregationRec<double>(Column, GroupNode, std::numeric_limits<double>::max(), false, [](double InValue, TOptional<FTableCellValue> InTableCellValue)
				{
					return FMath::Min(InValue, InTableCellValue->AsDouble());
				});
				break;

			case ETableColumnAggregation::Max:
				STableTreeView::UpdateAggregationRec<double>(Column, GroupNode, std::numeric_limits<double>::lowest(), false, [](double InValue, TOptional<FTableCellValue> InTableCellValue)
				{
					return FMath::Max(InValue, InTableCellValue->AsDouble());
				});
				break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::CreateGroupings()
{
	AvailableGroupings.Reset();
	CurrentGroupings.Reset();

	InternalCreateGroupings();

	if (CurrentGroupings.IsEmpty() && AvailableGroupings.Num() > 0)
	{
		CurrentGroupings.Add(AvailableGroupings[0]);
	}

	RebuildGroupingCrumbs();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::InternalCreateGroupings()
{
	AvailableGroupings.Add(MakeShared<FTreeNodeGroupingFlat>());
	//AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByNameFirstLetter>());
	//AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByType>());

	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const FTableColumn& Column = ColumnRef.Get();
		if (!Column.IsHierarchy())
		{
			switch (Column.GetDataType())
			{
				case ETableCellDataType::Bool:
					AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByUniqueValueBool>(ColumnRef));
					break;

				case ETableCellDataType::Int64:
					AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByUniqueValueInt64>(ColumnRef));
					break;

				case ETableCellDataType::Float:
					AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByUniqueValueFloat>(ColumnRef));
					break;

				case ETableCellDataType::Double:
					AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByUniqueValueDouble>(ColumnRef));
					break;

				case ETableCellDataType::CString:
					AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByUniqueValueCString>(ColumnRef));
					break;

				default:
					AvailableGroupings.Add(MakeShared<FTreeNodeGroupingByUniqueValue>(ColumnRef));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::PreChangeGroupings()
{
	for (TSharedPtr<FTreeNodeGrouping>& GroupingPtr : CurrentGroupings)
	{
		const FName& ColumnId = GroupingPtr->GetColumnId();
		if (ColumnId != NAME_None)
		{
			// Show columns used in previous groupings.
			ShowColumn(ColumnId);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::PostChangeGroupings()
{
	constexpr bool bAdjustHierarchyColumnWidth = false;
	constexpr bool bAdjustHierarchyColumnName = false;

	constexpr float HierarchyMinWidth = 60.0f;
	constexpr float HierarchyIndentation = 10.0f;
	constexpr float DefaultHierarchyColumnWidth = 90.0f;

	float HierarchyColumnWidth = DefaultHierarchyColumnWidth;
	FString GroupingStr;

	int32 GroupingDepth = 0;
	for (TSharedPtr<FTreeNodeGrouping>& GroupingPtr : CurrentGroupings)
	{
		const FName& ColumnId = GroupingPtr->GetColumnId();

		if (ColumnId != NAME_None)
		{
			if (bAdjustHierarchyColumnWidth)
			{
				// Compute width for Hierarchy column based on column used in grouping and its indentation.
				const int32 NumColumns = TreeViewHeaderRow->GetColumns().Num();
				for (int32 ColumnIndex = 0; ColumnIndex < NumColumns; ++ColumnIndex)
				{
					const SHeaderRow::FColumn& CurrentColumn = TreeViewHeaderRow->GetColumns()[ColumnIndex];
					if (CurrentColumn.ColumnId == ColumnId)
					{
						const float Width = HierarchyMinWidth + GroupingDepth * HierarchyIndentation + CurrentColumn.GetWidth();
						if (Width > HierarchyColumnWidth)
						{
							HierarchyColumnWidth = Width;
						}
						break;
					}
				}
			}

			// Hide columns used in groupings.
			HideColumn(ColumnId);
		}

		if (bAdjustHierarchyColumnName)
		{
			// Compute name of the Hierarchy column.
			if (!GroupingStr.IsEmpty())
			{
				GroupingStr.Append(TEXT(" / "));
			}
			GroupingStr.Append(GroupingPtr->GetShortName().ToString());
		}

		++GroupingDepth;
	}

	//////////////////////////////////////////////////

	if (TreeViewHeaderRow->GetColumns().Num() > 0)
	{
		if (bAdjustHierarchyColumnWidth)
		{
			// Set width for the Hierarchy column.
			SHeaderRow::FColumn& HierarchyColumn = const_cast<SHeaderRow::FColumn&>(TreeViewHeaderRow->GetColumns()[0]);
			HierarchyColumn.SetWidth(HierarchyColumnWidth);
		}

		if (bAdjustHierarchyColumnName)
		{
			// Set name for the Hierarchy column.
			SHeaderRow::FColumn& HierarchyColumn = const_cast<SHeaderRow::FColumn&>(TreeViewHeaderRow->GetColumns()[0]);
			FTableColumn& HierarchyTableColumn = *Table->FindColumnChecked(HierarchyColumn.ColumnId);
			if (!GroupingStr.IsEmpty())
			{
				const FText HierarchyColumnName = FText::Format(LOCTEXT("HierarchyShortNameFmt", "Hierarchy ({0})"), FText::FromString(GroupingStr));
				HierarchyTableColumn.SetShortName(HierarchyColumnName);
			}
			else
			{
				const FText HierarchyColumnName(LOCTEXT("HierarchyShortName", "Hierarchy"));
				HierarchyTableColumn.SetShortName(HierarchyColumnName);
			}
		}
	}

	//////////////////////////////////////////////////

	TreeViewHeaderRow->RefreshColumns();

	if (bRunInAsyncMode)
	{
		if (!bIsUpdateRunning)
		{
			OnPreAsyncUpdate();

			FGraphEventRef CompletedEvent = StartCreateGroupsTask();
			CompletedEvent = StartSortTreeNodesTask(CompletedEvent);
			InProgressAsyncOperationEvent = StartApplyFiltersTask(CompletedEvent);
		}
		else
		{
			CancelCurrentAsyncOp();
		}
	}
	else
	{
		CreateGroups(CurrentGroupings);
		if (CurrentSorter.IsValid())
		{
			SortTreeNodes(CurrentSorter.Get(), ColumnSortMode);
		}
		ApplyFiltering();
	}

	RebuildGroupingCrumbs();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::RebuildGroupingCrumbs()
{
	GroupingBreadcrumbTrail->ClearCrumbs();

	for (const TSharedPtr<FTreeNodeGrouping>& Grouping : CurrentGroupings)
	{
		GroupingBreadcrumbTrail->PushCrumb(Grouping->GetShortName(), Grouping);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 STableTreeView::GetGroupingDepth(const TSharedPtr<FTreeNodeGrouping>& Grouping) const
{
	for (int32 GroupingDepth = CurrentGroupings.Num() - 1; GroupingDepth >= 0; --GroupingDepth)
	{
		if (Grouping == CurrentGroupings[GroupingDepth])
		{
			return GroupingDepth;
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::OnGroupingCrumbClicked(const TSharedPtr<FTreeNodeGrouping>& CrumbGrouping)
{
	const int32 CrumbGroupingDepth = GetGroupingDepth(CrumbGrouping);
	if (CrumbGroupingDepth >= 0 && CrumbGroupingDepth < CurrentGroupings.Num() - 1)
	{
		PreChangeGroupings();

		CurrentGroupings.RemoveAt(CrumbGroupingDepth + 1, CurrentGroupings.Num() - CrumbGroupingDepth - 1);

		PostChangeGroupings();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::BuildGroupingSubMenu_Change(FMenuBuilder& MenuBuilder, const TSharedPtr<FTreeNodeGrouping> CrumbGrouping)
{
	MenuBuilder.BeginSection("ChangeGrouping");
	{
		for (const TSharedPtr<FTreeNodeGrouping>& Grouping : AvailableGroupings)
		{
			FUIAction Action_Change
			(
				FExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_Change_Execute, CrumbGrouping, Grouping),
				FCanExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_Change_CanExecute, CrumbGrouping, Grouping)
			);
			MenuBuilder.AddMenuEntry
			(
				Grouping->GetTitleName(),
				Grouping->GetDescription(),
				FSlateIcon(), Action_Change, NAME_None, EUserInterfaceActionType::Button
			);
		}
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::BuildGroupingSubMenu_Add(FMenuBuilder& MenuBuilder, const TSharedPtr<FTreeNodeGrouping> CrumbGrouping)
{
	MenuBuilder.BeginSection("AddGrouping");
	{
		for (const TSharedPtr<FTreeNodeGrouping>& Grouping : AvailableGroupings)
		{
			FUIAction Action_Add
			(
				FExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_Add_Execute, Grouping, CrumbGrouping),
				FCanExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_Add_CanExecute, Grouping, CrumbGrouping)
			);
			MenuBuilder.AddMenuEntry
			(
				Grouping->GetTitleName(),
				Grouping->GetDescription(),
				FSlateIcon(), Action_Add, NAME_None, EUserInterfaceActionType::Button
			);
		}
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STableTreeView::GetGroupingCrumbMenuContent(const TSharedPtr<FTreeNodeGrouping>& CrumbGrouping)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	const int32 CrumbGroupingDepth = GetGroupingDepth(CrumbGrouping);

	MenuBuilder.BeginSection("InsertOrAdd");
	{
		const FText AddGroupingText = (CrumbGroupingDepth == CurrentGroupings.Num() - 1) ? // after last one
			LOCTEXT("GroupingMenu_Add", "Add Grouping...") :
			LOCTEXT("GroupingMenu_Insert", "Insert Grouping...");
		MenuBuilder.AddSubMenu
		(
			AddGroupingText,
			LOCTEXT("GroupingMenu_AddOrInsert_Desc", "Add or insert new grouping."),
			FNewMenuDelegate::CreateSP(this, &STableTreeView::BuildGroupingSubMenu_Add, CrumbGrouping),
			false,
			FSlateIcon()
		);
	}
	MenuBuilder.EndSection();

	auto CanExecute = []()
	{
		return true;
	};

	if (CrumbGroupingDepth >= 0)
	{
		MenuBuilder.BeginSection("CrumbGrouping", CrumbGrouping->GetTitleName());
		{
			MenuBuilder.AddSubMenu
			(
				LOCTEXT("GroupingMenu_Change", "Change To..."),
				LOCTEXT("GroupingMenu_Change_Desc", "Change selected grouping."),
				FNewMenuDelegate::CreateSP(this, &STableTreeView::BuildGroupingSubMenu_Change, CrumbGrouping),
				false,
				FSlateIcon()
			);

			if (CrumbGroupingDepth > 0)
			{
				FUIAction Action_MoveLeft
				(
					FExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_MoveLeft_Execute, CrumbGrouping),
					FCanExecuteAction::CreateLambda(CanExecute)
				);
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("GroupingMenu_MoveLeft", "Move Left"),
					LOCTEXT("GroupingMenu_MoveLeft_Desc", "Move selected grouping to the left."),
					FSlateIcon(), Action_MoveLeft, NAME_None, EUserInterfaceActionType::Button
				);
			}

			if (CrumbGroupingDepth < CurrentGroupings.Num() - 1)
			{
				FUIAction Action_MoveRight
				(
					FExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_MoveRight_Execute, CrumbGrouping),
					FCanExecuteAction::CreateLambda(CanExecute)
				);
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("GroupingMenu_MoveRight", "Move Right"),
					LOCTEXT("GroupingMenu_MoveRight_Desc", "Move selected grouping to the right."),
					FSlateIcon(), Action_MoveRight, NAME_None, EUserInterfaceActionType::Button
				);
			}

			if (CurrentGroupings.Num() > 1)
			{
				FUIAction Action_Remove
				(
					FExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_Remove_Execute, CrumbGrouping),
					FCanExecuteAction::CreateLambda(CanExecute)
				);
				MenuBuilder.AddMenuEntry
				(
					LOCTEXT("GroupingMenu_Remove", "Remove"),
					LOCTEXT("GroupingMenu_Remove_Desc", "Remove selected grouping."),
					FSlateIcon(), Action_Remove, NAME_None, EUserInterfaceActionType::Button
				);
			}
		}
		MenuBuilder.EndSection();
	}

	if (CurrentGroupings.Num() > 1 || CurrentGroupings[0] != AvailableGroupings[0])
	{
		MenuBuilder.BeginSection("ResetGroupings");
		{
			FUIAction Action_Reset
			(
				FExecuteAction::CreateSP(this, &STableTreeView::GroupingCrumbMenu_Reset_Execute),
				FCanExecuteAction::CreateLambda(CanExecute)
			);
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("GroupingMenu_Reset", "Reset"),
				LOCTEXT("GroupingMenu_Reset_Desc", "Reset groupings to default."),
				FSlateIcon(), Action_Reset, NAME_None, EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupingCrumbMenu_Reset_Execute()
{
	PreChangeGroupings();

	CurrentGroupings.Reset();
	CurrentGroupings.Add(AvailableGroupings[0]);

	PostChangeGroupings();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupingCrumbMenu_Remove_Execute(const TSharedPtr<FTreeNodeGrouping> Grouping)
{
	const int32 GroupingDepth = GetGroupingDepth(Grouping);
	if (GroupingDepth >= 0)
	{
		PreChangeGroupings();

		CurrentGroupings.RemoveAt(GroupingDepth);

		PostChangeGroupings();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupingCrumbMenu_MoveLeft_Execute(const TSharedPtr<FTreeNodeGrouping> Grouping)
{
	const int32 GroupingDepth = GetGroupingDepth(Grouping);
	if (GroupingDepth > 0)
	{
		PreChangeGroupings();

		CurrentGroupings[GroupingDepth] = CurrentGroupings[GroupingDepth - 1];
		CurrentGroupings[GroupingDepth - 1] = Grouping;

		PostChangeGroupings();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupingCrumbMenu_MoveRight_Execute(const TSharedPtr<FTreeNodeGrouping> Grouping)
{
	const int32 GroupingDepth = GetGroupingDepth(Grouping);
	if (GroupingDepth < CurrentGroupings.Num() - 1)
	{
		PreChangeGroupings();

		CurrentGroupings[GroupingDepth] = CurrentGroupings[GroupingDepth + 1];
		CurrentGroupings[GroupingDepth + 1] = Grouping;

		PostChangeGroupings();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupingCrumbMenu_Change_Execute(const TSharedPtr<FTreeNodeGrouping> OldGrouping, const TSharedPtr<FTreeNodeGrouping> NewGrouping)
{
	const int32 OldGroupingDepth = GetGroupingDepth(OldGrouping);
	if (OldGroupingDepth >= 0)
	{
		PreChangeGroupings();

		const int32 NewGroupingDepth = GetGroupingDepth(NewGrouping);

		if (NewGroupingDepth >= 0 && NewGroupingDepth != OldGroupingDepth) // NewGrouping already exists
		{
			CurrentGroupings.RemoveAt(NewGroupingDepth);

			if (NewGroupingDepth < OldGroupingDepth)
			{
				CurrentGroupings[OldGroupingDepth - 1] = NewGrouping;
			}
			else
			{
				CurrentGroupings[OldGroupingDepth] = NewGrouping;
			}
		}
		else
		{
			CurrentGroupings[OldGroupingDepth] = NewGrouping;
		}

		PostChangeGroupings();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::GroupingCrumbMenu_Change_CanExecute(const TSharedPtr<FTreeNodeGrouping> OldGrouping, const TSharedPtr<FTreeNodeGrouping> NewGrouping) const
{
	return NewGrouping != OldGrouping;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::GroupingCrumbMenu_Add_Execute(const TSharedPtr<FTreeNodeGrouping> Grouping, const TSharedPtr<FTreeNodeGrouping> AfterGrouping)
{
	PreChangeGroupings();

	if (AfterGrouping.IsValid())
	{
		const int32 AfterGroupingDepth = GetGroupingDepth(AfterGrouping);
		ensure(AfterGroupingDepth >= 0);

		const int32 GroupingDepth = GetGroupingDepth(Grouping);

		if (GroupingDepth >= 0) // Grouping already exists
		{
			CurrentGroupings.RemoveAt(GroupingDepth);

			if (GroupingDepth <= AfterGroupingDepth)
			{
				CurrentGroupings.Insert(Grouping, AfterGroupingDepth);
			}
			else
			{
				CurrentGroupings.Insert(Grouping, AfterGroupingDepth + 1);
			}
		}
		else
		{
			CurrentGroupings.Insert(Grouping, AfterGroupingDepth + 1);
		}
	}
	else
	{
		CurrentGroupings.Remove(Grouping);
		CurrentGroupings.Add(Grouping);
	}

	PostChangeGroupings();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::GroupingCrumbMenu_Add_CanExecute(const TSharedPtr<FTreeNodeGrouping> Grouping, const TSharedPtr<FTreeNodeGrouping> AfterGrouping) const
{
	if (AfterGrouping.IsValid())
	{
		const int32 AfterGroupingDepth = GetGroupingDepth(AfterGrouping);
		ensure(AfterGroupingDepth >= 0);

		const int32 GroupingDepth = GetGroupingDepth(Grouping);

		return GroupingDepth < AfterGroupingDepth || GroupingDepth > AfterGroupingDepth + 1;
	}
	else
	{
		return Grouping != CurrentGroupings.Last();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorting
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////

const EColumnSortMode::Type STableTreeView::GetDefaultColumnSortMode()
{
	return EColumnSortMode::Descending;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FName STableTreeView::GetDefaultColumnBeingSorted()
{
	return NAME_None;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::CreateSortings()
{
	AvailableSorters.Reset();
	CurrentSorter = nullptr;

	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const FTableColumn& Column = ColumnRef.Get();
		if (Column.CanBeSorted())
		{
			TSharedPtr<Insights::ITableCellValueSorter> SorterPtr = Column.GetValueSorter();
			if (ensure(SorterPtr.IsValid()))
			{
				AvailableSorters.Add(SorterPtr);
			}
		}
	}

	UpdateCurrentSortingByColumn();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::UpdateCurrentSortingByColumn()
{
	TSharedPtr<Insights::FTableColumn> ColumnPtr = Table->FindColumn(ColumnBeingSorted);
	CurrentSorter = ColumnPtr.IsValid() ? ColumnPtr->GetValueSorter() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SortTreeNodes(ITableCellValueSorter* InSorter, EColumnSortMode::Type InColumnSortMode)
{
	FStopwatch Stopwatch;
	Stopwatch.Start();

	SortTreeNodesRec(*Root, *InSorter, InColumnSortMode);

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	if (TotalTime > 0.1)
	{
		UE_LOG(TraceInsights, Log, TEXT("[Tree - %s] Sorting completed in %.3fs."), *Table->GetDisplayName().ToString(), TotalTime);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SortTreeNodesRec(FTableTreeNode& GroupNode, const ITableCellValueSorter& Sorter, EColumnSortMode::Type InColumnSortMode)
{
	if (InColumnSortMode == EColumnSortMode::Type::Descending)
	{
		GroupNode.SortChildrenDescending(Sorter);
	}
	else // if (ColumnSortMode == EColumnSortMode::Type::Ascending)
	{
		GroupNode.SortChildrenAscending(Sorter);
	}

	for (FBaseTreeNodePtr ChildPtr : GroupNode.GetChildren())
	{
		if (bCancelCurrentAsyncOp)
		{
			break;
		}
		if (ChildPtr->IsGroup())
		{
			SortTreeNodesRec(*StaticCastSharedPtr<FTableTreeNode>(ChildPtr), Sorter, InColumnSortMode);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EColumnSortMode::Type STableTreeView::GetSortModeForColumn(const FName ColumnId) const
{
	if (ColumnBeingSorted != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return ColumnSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SetSortModeForColumn(const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	ColumnBeingSorted = ColumnId;
	ColumnSortMode = SortMode;
	UpdateCurrentSortingByColumn();

	if (bRunInAsyncMode)
	{
		if (!bIsUpdateRunning)
		{
			OnPreAsyncUpdate();

			FGraphEventRef CompletedEvent = StartSortTreeNodesTask();
			InProgressAsyncOperationEvent = StartApplyFiltersTask(CompletedEvent);
		}
		else
		{
			CancelCurrentAsyncOp();
		}
	}
	else
	{
		if (CurrentSorter.IsValid())
		{
			SortTreeNodes(CurrentSorter.Get(), ColumnSortMode);
		}
		ApplyFiltering();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	SetSortModeForColumn(ColumnId, SortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (HeaderMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::HeaderMenu_SortMode_IsChecked(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	return ColumnBeingSorted == ColumnId && ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::HeaderMenu_SortMode_CanExecute(const FName ColumnId, const EColumnSortMode::Type InSortMode) const
{
	const FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeSorted();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::HeaderMenu_SortMode_Execute(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnId, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_SortMode_IsChecked(const EColumnSortMode::Type InSortMode)
{
	return ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_SortMode_CanExecute(const EColumnSortMode::Type InSortMode) const
{
	return true; //ColumnSortMode != InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_SortMode_Execute(const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnBeingSorted, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortByColumn action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_SortByColumn_IsChecked(const FName ColumnId)
{
	return ColumnId == ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_SortByColumn_CanExecute(const FName ColumnId) const
{
	return true; //ColumnId != ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_SortByColumn_Execute(const FName ColumnId)
{
	SetSortModeForColumn(ColumnId, EColumnSortMode::Descending);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::CanShowColumn(const FName ColumnId) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ShowColumn(const FName ColumnId)
{
	FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	ShowColumn(Column);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ShowColumn(FTableColumn& Column)
{
	if (!Column.IsVisible())
	{
		Column.Show();

		SHeaderRow::FColumn::FArguments ColumnArgs;
		ColumnArgs
			.ColumnId(Column.GetId())
			.DefaultLabel(Column.GetShortName())
			.HAlignHeader(HAlign_Fill)
			.VAlignHeader(VAlign_Fill)
			.HeaderContentPadding(FMargin(2.0f))
			.HAlignCell(HAlign_Fill)
			.VAlignCell(VAlign_Fill)
			.SortMode(this, &STableTreeView::GetSortModeForColumn, Column.GetId())
			.OnSort(this, &STableTreeView::OnSortModeChanged)
			//.ManualWidth(Column.GetInitialWidth())
			.FillWidth(Column.GetInitialWidth())
			//.FixedWidth(Column.IsFixedWidth() ? Column.GetInitialWidth() : TOptional<float>())
			.HeaderContent()
			[
				SNew(SBox)
				.ToolTip(STableTreeViewTooltip::GetColumnTooltip(Column))
				.HAlign(Column.GetHorizontalAlignment())
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &STableTreeView::GetColumnHeaderText, Column.GetId())
				]
			]
			.MenuContent()
			[
				TreeViewHeaderRow_GenerateColumnMenu(Column)
			];

		int32 ColumnIndex = 0;
		const int32 NewColumnPosition = Table->GetColumnPositionIndex(Column.GetId());
		const int32 NumColumns = TreeViewHeaderRow->GetColumns().Num();
		for (; ColumnIndex < NumColumns; ColumnIndex++)
		{
			const SHeaderRow::FColumn& CurrentColumn = TreeViewHeaderRow->GetColumns()[ColumnIndex];
			const int32 CurrentColumnPosition = Table->GetColumnPositionIndex(CurrentColumn.ColumnId);
			if (NewColumnPosition < CurrentColumnPosition)
			{
				break;
			}
		}

		TreeViewHeaderRow->InsertColumn(ColumnArgs, ColumnIndex);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// HideColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::CanHideColumn(const FName ColumnId) const
{
	if (bIsUpdateRunning)
	{
		return false;
	}

	const FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::HideColumn(const FName ColumnId)
{
	FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	HideColumn(Column);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::HideColumn(FTableColumn& Column)
{
	if (Column.IsVisible())
	{
		Column.Hide();
		TreeViewHeaderRow->RemoveColumn(Column.GetId());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ToggleColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::IsColumnVisible(const FName ColumnId)
{
	const FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::CanToggleColumnVisibility(const FName ColumnId) const
{
	if (bIsUpdateRunning)
	{
		return false;
	}

	const FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return !Column.IsVisible() || Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ToggleColumnVisibility(const FName ColumnId)
{
	FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	if (Column.IsVisible())
	{
		HideColumn(Column);
	}
	else
	{
		ShowColumn(Column);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// "Show All Columns" action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_ShowAllColumns_CanExecute() const
{
	if (bIsUpdateRunning)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_ShowAllColumns_Execute()
{
	ColumnBeingSorted = GetDefaultColumnBeingSorted();
	ColumnSortMode = GetDefaultColumnSortMode();
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		FTableColumn& Column = ColumnRef.Get();
		if (!Column.IsVisible())
		{
			ShowColumn(Column);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ResetColumns action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_ResetColumns_CanExecute() const
{
	if (bIsUpdateRunning)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_ResetColumns_Execute()
{
	ColumnBeingSorted = GetDefaultColumnBeingSorted();
	ColumnSortMode = GetDefaultColumnSortMode();
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<FTableColumn>& ColumnRef : Table->GetColumns())
	{
		FTableColumn& Column = ColumnRef.Get();
		if (Column.ShouldBeVisible() && !Column.IsVisible())
		{
			ShowColumn(Column);
		}
		else if (!Column.ShouldBeVisible() && Column.IsVisible())
		{
			HideColumn(Column);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::Reset()
{
	StatsStartTime = 0.0;
	StatsEndTime = 0.0;

	RebuildTree(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::RebuildTree(bool bResync)
{
	unimplemented();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTableTreeNodePtr STableTreeView::GetNodeByTableRowIndex(int32 RowIndex) const
{
	return (RowIndex >= 0 && RowIndex < TableTreeNodes.Num()) ? TableTreeNodes[RowIndex] : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SelectNodeByTableRowIndex(int32 RowIndex)
{
	if (RowIndex >= 0 && RowIndex < TableTreeNodes.Num())
	{
		FTableTreeNodePtr NodePtr = TableTreeNodes[RowIndex];
		if (ensure(NodePtr))
		{
			TreeView->SetSelection(NodePtr);
			TreeView->RequestScrollIntoView(NodePtr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::OnPreAsyncUpdate()
{
	check(!bIsUpdateRunning);

	ClearInProgressAsyncOperations();

	AsyncUpdateStopwatch.Restart();
	bIsUpdateRunning = true;

	ExpandedNodes.Empty();
	TreeView->GetExpandedItems(ExpandedNodes);

	TreeView->SetTreeItemsSource(&DummyGroupNodes);
	TreeView_Refresh();

	DispatchEvent = FGraphEvent::CreateGraphEvent();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::OnPostAsyncUpdate()
{
	check(bIsUpdateRunning);

	bIsUpdateRunning = false;

	if (!bCancelCurrentAsyncOp)
	{
		TreeView->SetTreeItemsSource(&FilteredGroupNodes);

		TreeView->ClearExpandedItems();

		// Grouping can result in old group nodes no longer existing in the tree, so we don't keep the expanded list.
		if (!HasInProgressAsyncOperation(EAsyncOperationType::GroupingOp))
		{
			for (auto It = ExpandedNodes.CreateConstIterator(); It; ++It)
			{
				TreeView->SetItemExpansion(*It, true);
			}
		}

		for (FTableTreeNodePtr& Node : NodesToExpand)
		{
			TreeView->SetItemExpansion(Node, true);
		}
		NodesToExpand.Empty();

		// Expand each group node on the first few depths (if it doesn't have too many children).
		SetExpandValueForChildGroups(Root.Get(), 1000, 4, true);

		ClearInProgressAsyncOperations();
		TreeView_Refresh();
	}

	bCancelCurrentAsyncOp = false;
	AsyncUpdateStopwatch.Stop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SetExpandValueForChildGroups(FBaseTreeNode* InRoot, int32 InMaxExpandedNodes, int32 InMaxDepthToExpand, bool InValue)
{
	TArray<int32> NumNodesPerDepth;
	NumNodesPerDepth.AddDefaulted(InMaxDepthToExpand + 1);
	CountNumNodesPerDepthRec(InRoot, NumNodesPerDepth, 0, InMaxDepthToExpand, InMaxExpandedNodes);

	int32 MaxDepth = 0;
	for (int32 Depth = 0; Depth <= InMaxDepthToExpand; ++Depth)
	{
		if (Depth > 0)
		{
			NumNodesPerDepth[Depth] += NumNodesPerDepth[Depth - 1];
		}
		if (NumNodesPerDepth[Depth] > InMaxExpandedNodes)
		{
			break;
		}
		MaxDepth = Depth;
	}

	if (MaxDepth > 0)
	{
		SetExpandValueForChildGroupsRec(InRoot, 1, MaxDepth, InValue);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::CountNumNodesPerDepthRec(FBaseTreeNode* InRoot, TArray<int32>& InOutNumNodesPerDepth, int32 InDepth, int32 InMaxDepth, int32 InMaxNodes) const
{
	InOutNumNodesPerDepth[InDepth] += InRoot->GetChildren().Num();

	if (InDepth < InMaxDepth && InOutNumNodesPerDepth[InDepth] < InMaxNodes)
	{
		for (const FBaseTreeNodePtr& Node : InRoot->GetChildren())
		{
			if (Node->IsGroup())
			{
				CountNumNodesPerDepthRec(Node.Get(), InOutNumNodesPerDepth, InDepth + 1, InMaxDepth, InMaxNodes);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::SetExpandValueForChildGroupsRec(FBaseTreeNode* InRoot, int32 InDepth, int32 InMaxDepth, bool InValue)
{
	for (const FBaseTreeNodePtr& Node : InRoot->GetChildren())
	{
		if (Node->IsGroup())
		{
			Node->SetExpansion(InValue);
			TreeView->SetItemExpansion(StaticCastSharedPtr<FTableTreeNode>(Node), InValue);

			if (InDepth < InMaxDepth)
			{
				SetExpandValueForChildGroupsRec(Node.Get(), InDepth + 1, InMaxDepth, InValue);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FGraphEventRef STableTreeView::StartSortTreeNodesTask(FGraphEventRef Prerequisite)
{
	if (!CurrentSorter.IsValid())
	{
		return Prerequisite;
	}

	AddInProgressAsyncOperation(EAsyncOperationType::SortingOp);

	CurrentAsyncOpSorter = CurrentSorter.Get();
	CurrentAsyncOpColumnSortMode = ColumnSortMode;

	FGraphEventArray Prerequisites;
	if (Prerequisite.IsValid())
	{
		Prerequisites.Add(Prerequisite);
	}
	else
	{
		Prerequisites.Add(DispatchEvent);
	}

	return TGraphTask<FTableTreeViewSortAsyncTask>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(this, CurrentAsyncOpSorter, CurrentAsyncOpColumnSortMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FGraphEventRef STableTreeView::StartCreateGroupsTask(FGraphEventRef Prerequisite)
{
	AddInProgressAsyncOperation(EAsyncOperationType::GroupingOp);

	CurrentAsyncOpGroupings.Empty();
	CurrentAsyncOpGroupings.Insert(CurrentGroupings, 0);

	FGraphEventArray Prerequisites;
	if (Prerequisite.IsValid())
	{
		Prerequisites.Add(Prerequisite);
	}
	else
	{
		Prerequisites.Add(DispatchEvent);
	}

	return TGraphTask<FTableTreeViewGroupAsyncTask>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(this, &CurrentAsyncOpGroupings);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FGraphEventRef STableTreeView::StartApplyFiltersTask(FGraphEventRef Prerequisite)
{
	AddInProgressAsyncOperation(EAsyncOperationType::FilteringOp);

	CurrentAsyncOpTextFilter->SetRawFilterText(TextFilter->GetRawFilterText());
	if (FilterConfigurator.IsValid() && CurrentAsyncOpFilterConfigurator)
	{
		*CurrentAsyncOpFilterConfigurator = *FilterConfigurator;
	}

	FGraphEventArray Prerequisites;
	if (Prerequisite.IsValid())
	{
		Prerequisites.Add(Prerequisite);
	}
	else
	{
		Prerequisites.Add(DispatchEvent);
	}

	return TGraphTask<FTableTreeViewFilterAsyncTask>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::OnClose()
{
	if (bIsUpdateRunning && InProgressAsyncOperationEvent.IsValid() && !InProgressAsyncOperationEvent->IsComplete())
	{
		bIsCloseScheduled = true;
		CancelCurrentAsyncOp();

		FGraphEventArray Prerequisites;
		Prerequisites.Add(InProgressAsyncOperationEvent);
		TGraphTask<FTableTreeViewAsyncCompleteTask>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(SharedThis(this));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STableTreeView::GetCurrentOperationName() const
{
	return LOCTEXT("CurrentOperationName", "Updating Tree");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double STableTreeView::GetAllOperationsDuration()
{ 
	AsyncUpdateStopwatch.Update();  
	return AsyncUpdateStopwatch.GetAccumulatedTime(); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::StartPendingAsyncOperations()
{
	// Check if grouping settings have changed. If they did, a full refresh (Grouping, Sorting and Filtering) is scheduled.
	bool bGroupingsHaveChanged = HasInProgressAsyncOperation(EAsyncOperationType::GroupingOp);
	bGroupingsHaveChanged |= CurrentGroupings.Num() != CurrentAsyncOpGroupings.Num();

	if (!bGroupingsHaveChanged)
	{
		for (int Index = 0; Index < CurrentGroupings.Num(); ++Index)
		{
			if (CurrentGroupings[Index] != CurrentAsyncOpGroupings[Index])
			{
				bGroupingsHaveChanged = true;
				break;
			}
		}
	}

	if (bGroupingsHaveChanged)
	{
		OnPreAsyncUpdate();

		FGraphEventRef CompletedEvent = StartCreateGroupsTask();
		CompletedEvent = StartSortTreeNodesTask(CompletedEvent);
		InProgressAsyncOperationEvent = StartApplyFiltersTask(CompletedEvent);

		return;
	}

	// Check if sorting settings have changed. If they did, a Sorting and Filtering Refresh is scheduled.
	bool bSortingHasChanged = HasInProgressAsyncOperation(EAsyncOperationType::SortingOp);
	bSortingHasChanged |= (CurrentSorter.IsValid() && CurrentAsyncOpSorter == nullptr) || (!CurrentSorter.IsValid() && CurrentAsyncOpSorter != nullptr);
	if (!bSortingHasChanged && CurrentSorter.IsValid())
	{
		bSortingHasChanged = CurrentSorter.Get() != CurrentAsyncOpSorter || ColumnSortMode != CurrentAsyncOpColumnSortMode;
	}

	if (bSortingHasChanged)
	{
		OnPreAsyncUpdate();

		FGraphEventRef CompletedEvent = StartSortTreeNodesTask();
		InProgressAsyncOperationEvent = StartApplyFiltersTask(CompletedEvent);

		return;
	}

	// Check if the text filter has changed. If it has, schedule a new Filtering Refresh.
	bool bFiltersHaveChanged = HasInProgressAsyncOperation(EAsyncOperationType::FilteringOp);
	bFiltersHaveChanged |= TextFilter->GetRawFilterText().CompareTo(CurrentAsyncOpTextFilter->GetRawFilterText()) != 0;
	if (FilterConfigurator.IsValid() && CurrentAsyncOpFilterConfigurator && !bFiltersHaveChanged)
	{
		bFiltersHaveChanged |= *FilterConfigurator != *CurrentAsyncOpFilterConfigurator;
	}

	if (bFiltersHaveChanged)
	{
		OnPreAsyncUpdate();

		InProgressAsyncOperationEvent = StartApplyFiltersTask();

		return;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::CancelCurrentAsyncOp()
{
	if (bIsUpdateRunning)
	{
		bCancelCurrentAsyncOp = true;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply STableTreeView::OnAdvancedFiltersClicked()
{
	if (!FilterConfigurator.IsValid())
	{
		FilterConfigurator = MakeShared<FFilterConfigurator>();
		TSharedPtr<TArray<TSharedPtr<struct FFilter>>>& AvailableFilters = FilterConfigurator->GetAvailableFilters();

		for (const TSharedRef<FTableColumn>& Column : Table->GetColumns())
		{
			switch (Column->GetDataType())
			{
				case ETableCellDataType::Int64:
				{
					AvailableFilters->Add(MakeShared<FFilter>(Column->GetIndex(), Column->GetTitleName(), Column->GetDescription(), EFilterDataType::Int64, FFilterService::Get()->GetIntegerOperators()));
					Context.AddFilterData<int64>(Column->GetIndex(), 0);
					break;
				}
				case ETableCellDataType::Double:
				{
					AvailableFilters->Add(MakeShared<FFilter>(Column->GetIndex(), Column->GetTitleName(), Column->GetDescription(), EFilterDataType::Double, FFilterService::Get()->GetDoubleOperators()));
					Context.AddFilterData<double>(Column->GetIndex(), 0.0);
					break;
				}
				case ETableCellDataType::CString:
				case ETableCellDataType::Text:
				case ETableCellDataType::Custom:
				{
					if (!Column->IsHierarchy())
					{
						AvailableFilters->Add(MakeShared<FFilter>(Column->GetIndex(), Column->GetTitleName(), Column->GetDescription(), EFilterDataType::String, FFilterService::Get()->GetStringOperators()));
						Context.AddFilterData<FString>(Column->GetIndex(), FString());
					}
					break;
				}
			}
		}

		AddCustomAdvancedFilters();

		CurrentAsyncOpFilterConfigurator = new FFilterConfigurator(*FilterConfigurator);
		OnFilterChangesCommitedHandle = FilterConfigurator->GetOnChangesCommitedEvent().AddSP(this, &STableTreeView::OnAdvancedFiltersChangesCommited);
	}

	FFilterService::Get()->CreateFilterConfiguratorWidget(FilterConfigurator);

	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ApplyAdvancedFilters(const FTableTreeNodePtr& NodePtr)
{
	FFilterConfigurator* FilterConfiguratorToUse = bRunInAsyncMode ? CurrentAsyncOpFilterConfigurator : FilterConfigurator.Get();

	if (FilterConfiguratorToUse == nullptr)
	{
		return true;
	}

	if (FilterConfiguratorToUse->GetRootNode()->GetChildren().Num() == 0)
	{
		return true;
	}

	for (const TSharedRef<FTableColumn>& Column : Table->GetColumns())
	{
		switch (Column->GetDataType())
		{
			case ETableCellDataType::Int64:
			{
				Context.SetFilterData<int64>(Column->GetIndex(), Column->GetValue(*NodePtr)->AsInt64());
				break;
			}
			case ETableCellDataType::Double:
			{
				Context.SetFilterData<double>(Column->GetIndex(), Column->GetValue(*NodePtr)->AsDouble());
				break;
			}
			case ETableCellDataType::CString:
			case ETableCellDataType::Text:
			case ETableCellDataType::Custom:
			{
				if (!Column->IsHierarchy())
				{
					Context.SetFilterData<FString>(Column->GetIndex(), Column->GetValue(*NodePtr)->AsString());
				}
				break;
			}
		}
	}

	return ApplyCustomAdvancedFilters(NodePtr) && FilterConfiguratorToUse->ApplyFilters(Context);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::OnAdvancedFiltersChangesCommited()
{
	if (bRunInAsyncMode)
	{
		if (!bIsUpdateRunning)
		{
			OnPreAsyncUpdate();
			InProgressAsyncOperationEvent = StartApplyFiltersTask();
		}
		else
		{
			CancelCurrentAsyncOp();
		}
	}
	else
	{
		ApplyFiltering();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::AdvancedFilters_ShouldBeEnabled() const
{
	return TextFilter->GetRawFilterText().IsEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STableTreeView::AdvancedFilters_GetTooltipText() const
{
	if (AdvancedFilters_ShouldBeEnabled())
	{
		return LOCTEXT("AdvancedFiltersBtn_ToolTip", "Opens the filter configurator window.");
	}
	
	return LOCTEXT("AdvancedFiltersBtn_Disabled_ToolTip", "Advanced filters cannot be added when filters are already applied using the search box.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::FilterConfigurator_HasFilters() const
{
	return FilterConfigurator.IsValid() && FilterConfigurator->GetRootNode()->GetChildren().Num() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> STableTreeView::ConstructToolbar()
{
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> STableTreeView::ConstructFooter()
{
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply STableTreeView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return CommandList->ProcessCommandBindings(InKeyEvent) == true ? FReply::Handled() : FReply::Unhandled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_CopySelectedToClipboard_CanExecute() const
{
	return TreeView->GetNumItemsSelected() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_CopySelectedToClipboard_Execute()
{
	if (!Table->IsValid())
	{
		return;
	}

	TArray<Insights::FBaseTreeNodePtr> SelectedNodes;
	for (FTableTreeNodePtr TimerPtr : TreeView->GetSelectedItems())
	{
		SelectedNodes.Add(TimerPtr);
	}

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	FString ClipboardText;

	if (CurrentSorter.IsValid())
	{
		CurrentSorter->Sort(SelectedNodes, ColumnSortMode == EColumnSortMode::Ascending ? Insights::ESortMode::Ascending : Insights::ESortMode::Descending);
	}

	Table->GetVisibleColumnsData(SelectedNodes, GetLogListingName(), ClipboardText);

	if (ClipboardText.Len() > 0)
	{
		FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_CopyColumnToClipboard_CanExecute() const
{
	const TSharedPtr<FTableColumn> HoveredColumnPtr = Table->FindColumn(HoveredColumnId);

	if (HoveredColumnPtr.IsValid() && TreeView->GetNumItemsSelected() == 1)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_CopyColumnToClipboard_Execute()
{
	if (TreeView->GetNumItemsSelected() > 0)
	{
		FTableTreeNodePtr SelectedNode = TreeView->GetSelectedItems()[0];
		const TSharedPtr<FTableColumn> HoveredColumnPtr = Table->FindColumn(HoveredColumnId);
		if (HoveredColumnPtr.IsValid())
		{
			FString Text = HoveredColumnPtr->GetValueAsText(*SelectedNode).ToString();
			FPlatformApplicationMisc::ClipboardCopy(*Text);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_CopyColumnTooltipToClipboard_CanExecute() const
{
	const TSharedPtr<FTableColumn> HoveredColumnPtr = Table->FindColumn(HoveredColumnId);

	if (HoveredColumnPtr.IsValid() && TreeView->GetNumItemsSelected() == 1)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_CopyColumnTooltipToClipboard_Execute()
{
	if (TreeView->GetNumItemsSelected() > 0)
	{
		FTableTreeNodePtr SelectedNode = TreeView->GetSelectedItems()[0];
		const TSharedPtr<FTableColumn> HoveredColumnPtr = Table->FindColumn(HoveredColumnId);
		if (HoveredColumnPtr.IsValid())
		{
			FString Text = HoveredColumnPtr->GetValueAsTooltipText(*SelectedNode).ToString();
			FPlatformApplicationMisc::ClipboardCopy(*Text);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_ExpandSubtree_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_ExpandSubtree_Execute()
{
	const TArray<FTableTreeNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	for (const FTableTreeNodePtr& Node : SelectedNodes)
	{
		if (Node->IsGroup())
		{
			Node->SetExpansion(true);
			TreeView->SetItemExpansion(Node, true);
			SetExpandValueForChildGroups((FBaseTreeNode*)Node.Get(), MAX_NUMBER_OF_NODES_TO_EXPAND, MAX_DEPTH_TO_EXPAND, true);
		}
	}

	TreeView->RequestTreeRefresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_CollapseSubtree_CanExecute() const
{
	const TArray<FTableTreeNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	for (const FTableTreeNodePtr& Node : SelectedNodes)
	{
		if (Node->IsGroup() && Node->GetFilteredChildren().Num() > 0 && TreeView->IsItemExpanded(Node))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_CollapseSubtree_Execute()
{
	const TArray<FTableTreeNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	for (const FTableTreeNodePtr& Node : SelectedNodes)
	{
		if (Node->IsGroup() && TreeView->IsItemExpanded(Node))
		{
			Node->SetExpansion(false);
			TreeView->SetItemExpansion(Node, false);
			SetExpandValueForChildGroups((FBaseTreeNode*)Node.Get(), MAX_NUMBER_OF_NODES_TO_EXPAND, MAX_DEPTH_TO_EXPAND, false);
		}
	}

	TreeView->RequestTreeRefresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeView::ContextMenu_ExpandCriticalPath_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STableTreeView::ContextMenu_ExpandCriticalPath_Execute()
{
	const TArray<FTableTreeNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	for (const FTableTreeNodePtr& Node : SelectedNodes)
	{
		FTableTreeNodePtr CurrentNode = Node;
		while (CurrentNode->IsGroup())
		{
			check(CurrentNode.IsValid());
			if (!TreeView->IsItemExpanded(CurrentNode))
			{
				CurrentNode->SetExpansion(true);
				TreeView->SetItemExpansion(CurrentNode, true);
			}

			if (CurrentNode->GetFilteredChildren().Num() > 0)
			{
				CurrentNode = StaticCastSharedPtr<FTableTreeNode>(CurrentNode->GetFilteredChildren()[0]);
			}
			else
			{
				break;
			}
		}
	}

	TreeView->RequestTreeRefresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
