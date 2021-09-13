// Copyright Epic Games, Inc. All Rights Reserved.

#include "SInViewportDetails.h"
#include "Widgets/SBoxPanel.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Selection.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SSCSEditor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "LevelEditorGenericDetails.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "Widgets/Docking/SDockTab.h"
#include "Subsystems/PanelExtensionSubsystem.h"
#include "DetailsViewObjectFilter.h"
#include "IDetailRootObjectCustomization.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "PropertyCustomizationHelpers.h"
#include "Input/DragAndDrop.h"
#include "SEditorViewport.h"
#include "SResetToDefaultPropertyEditor.h"
#include "ToolMenus.h"
#include "Editor/EditorEngine.h"
#include "LevelEditorMenuContext.h"

#define LOCTEXT_NAMESPACE "InViewportDetails"
void FInViewportUIDragOperation::OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent)
{
	check(CursorDecoratorWindow.IsValid());

	// Destroy the CursorDecoratorWindow by calling the base class implementation because we are relocating the content into a more permanent home.
	FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);

	UIBeingDragged.Reset();
}

void FInViewportUIDragOperation::OnDragged(const FDragDropEvent& DragDropEvent)
{
	// The tab is being dragged. Move the the decorator window to match the cursor position.
	FVector2D TargetPosition = DragDropEvent.GetScreenSpacePosition() - GetDecoratorOffsetFromCursor();
	CursorDecoratorWindow->UpdateMorphTargetShape(FSlateRect(TargetPosition.X, TargetPosition.Y, TargetPosition.X + LastContentSize.X, TargetPosition.Y + LastContentSize.Y));
	CursorDecoratorWindow->MoveWindowTo(TargetPosition);
}

TSharedRef<FInViewportUIDragOperation> FInViewportUIDragOperation::New(const TSharedRef<class SInViewportDetails>& InUIToBeDragged, const FVector2D InTabGrabOffset, const FVector2D& OwnerAreaSize)
{
	const TSharedRef<FInViewportUIDragOperation> Operation = MakeShareable(new FInViewportUIDragOperation(InUIToBeDragged, InTabGrabOffset, OwnerAreaSize));
	return Operation;
}

FVector2D FInViewportUIDragOperation::GetTabGrabOffsetFraction() const
{
	return TabGrabOffsetFraction;
}

FInViewportUIDragOperation::~FInViewportUIDragOperation()
{

}

FInViewportUIDragOperation::FInViewportUIDragOperation(const TSharedRef<class SInViewportDetails>& InUIToBeDragged, const FVector2D InTabGrabOffsetFraction, const FVector2D& OwnerAreaSize)
	: UIBeingDragged(InUIToBeDragged)
	, TabGrabOffsetFraction(InTabGrabOffsetFraction)
	, LastContentSize(OwnerAreaSize)
{
	// Create the decorator window that we will use during this drag and drop to make the user feel like
	// they are actually dragging a piece of UI.

	// Start the window off hidden.
	const bool bShowImmediately = true;
	CursorDecoratorWindow = FSlateApplication::Get().AddWindow(SWindow::MakeCursorDecorator(), bShowImmediately);
	// Usually cursor decorators figure out their size automatically from content, but we will drive it
	// here because the window will reshape itself to better reflect what will happen when the user drops the Tab.
	CursorDecoratorWindow->SetSizingRule(ESizingRule::Autosized);
	CursorDecoratorWindow->SetOpacity(0.45f);
	CursorDecoratorWindow->SetContent
	(
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("Docking.Background"))
		[
			InUIToBeDragged
		]
	);

}

const FVector2D FInViewportUIDragOperation::GetDecoratorOffsetFromCursor()
{
	const FVector2D TabDesiredSize = UIBeingDragged->GetDesiredSize();
	return TabGrabOffsetFraction * TabDesiredSize;
}

class SInViewportDetailsRow : public STableRow< TSharedPtr<IDetailTreeNode> >
{
public:

	SLATE_BEGIN_ARGS(SInViewportDetailsRow)
	{}

	/** The item content. */
	SLATE_ARGUMENT(TSharedPtr<IDetailTreeNode>, InNode)
	SLATE_ARGUMENT(TSharedPtr<SInViewportDetails>, InDetailsView)
	SLATE_END_ARGS()


	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		ParentDetailsView = InArgs._InDetailsView;
		FDetailColumnSizeData& ColumnSizeData = InArgs._InDetailsView->GetColumnSizeData();

		TSharedPtr<IDetailPropertyRow> DetailPropertyRow = InArgs._InNode->GetRow();
		FDetailWidgetRow Row;
		TSharedPtr< SWidget > NameWidget;
		TSharedPtr< SWidget > ValueWidget;
		DetailPropertyRow->GetDefaultWidgets(NameWidget, ValueWidget, Row, true);
		TSharedPtr< SWidget > ResetWidget = SNew(SResetToDefaultPropertyEditor, InArgs._InNode->CreatePropertyHandle());
 		TSharedPtr<SWidget> RowWidget = SNullWidget::NullWidget;
		{
 
			RowWidget = SNew(SSplitter)
 				.Style(FEditorStyle::Get(), "DetailsView.Splitter")
				.PhysicalSplitterHandleSize(1.0f)
 				.HitDetectionSplitterHandleSize(5.0f)
 				+ SSplitter::Slot()
				.SizeRule(SSplitter::ESizeRule::FractionOfParent)
 				.Value(ColumnSizeData.NameColumnWidth)
				.OnSlotResized(ColumnSizeData.OnNameColumnResized)
 				[
					SNew(SBox)
					.HAlign(HAlign_Right)
 					.Padding(2.0f)
 					[
						NameWidget.ToSharedRef()
					]
 				]
 				+ SSplitter::Slot()
				.SizeRule(SSplitter::ESizeRule::FractionOfParent)
 				.Value(ColumnSizeData.ValueColumnWidth)
 				.OnSlotResized(ColumnSizeData.OnValueColumnResized)
 				[
 					SNew(SBox)
 					.Padding(2.0f)
 					[
 						ValueWidget.ToSharedRef()
 					]
				]
				+ SSplitter::Slot()
					.SizeRule(SSplitter::ESizeRule::SizeToContent)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(0.0f)
						[
							ResetWidget.ToSharedRef()
						]
					];
 		}

		ChildSlot
			[
				SNew(SBox)
				.MinDesiredWidth(300.0f)
				[
					RowWidget.ToSharedRef()
				]
			];

		STableRow< TSharedPtr<IDetailTreeNode> >::ConstructInternal(
			STableRow::FArguments()
			.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
			InOwnerTableView
		);
	}

	TWeakPtr<SInViewportDetails> ParentDetailsView;

};

void SInViewportDetails::Construct(const FArguments& InArgs)
{
	OwningViewport = InArgs._InOwningViewport;
	ParentLevelEditor = InArgs._InOwningLevelEditor;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FPropertyRowGeneratorArgs Args;
	Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	Args.NotifyHook = GUnrealEd;
	ColumnSizeData.ValueColumnWidth = 0.5f;
	PropertyRowGenerator = PropertyEditorModule.CreatePropertyRowGenerator(Args);

	USelection::SelectionChangedEvent.AddRaw(this, &SInViewportDetails::OnEditorSelectionChanged);

	GEditor->RegisterForUndo(this);

	GenerateWidget();

}

void SInViewportDetails::GenerateWidget()
{
	if (AActor* SelectedActor = GetSelectedActorInEditor())
	{
		FString NameString;
		if (GEditor->GetSelectedActors()->Num() > 1)
		{
			NameString = LOCTEXT("SelectedObjects", "Selected Objects").ToString();
		}
		else
		{
			NameString = SelectedActor->GetHumanReadableName();
		}
		
		ChildSlot
			[
				SNew(SBackgroundBlur)
				.Visibility(this, &SInViewportDetails::GetHeaderVisibility)
				.BlurStrength(1)
				.BlurRadius(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SInViewportDetailsHeader)
						.Parent(SharedThis(this))
						.Content()
						[					
							SNew(SBorder)
							.BorderImage(FAppStyle::Get().GetBrush("PropertyTable.InViewport.Header"))
							.Padding(5.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(NameString))
								.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SInViewportDetailsToolbar)
						.Parent(SharedThis(this))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeDetailsWidget()
					]
				]
			];
	}
}

EVisibility SInViewportDetails::GetHeaderVisibility() const
{
	return Nodes.Num() ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SInViewportDetails::MakeDetailsWidget()
{
	TSharedRef<SWidget> DetailWidget = SNullWidget::NullWidget;
	static const FName Name_ShouldShowInViewport("ShouldShowInViewport");
	if (PropertyRowGenerator.IsValid())
	{
		Nodes.Empty();
		
		for (TSharedRef<IDetailTreeNode> RootNode : PropertyRowGenerator->GetRootTreeNodes())
		{
			TArray<TSharedRef<IDetailTreeNode>> Children;
			RootNode->GetChildren(Children);
			
			for (TSharedRef<IDetailTreeNode> Child : Children)
			{
				bool bShowChild = false;
				{
					TSharedPtr<IPropertyHandle> NodePropertyHandle = Child->CreatePropertyHandle();
					if (NodePropertyHandle.IsValid() && NodePropertyHandle->GetProperty()->HasAllPropertyFlags(CPF_DisableEditOnInstance))
					{
						continue;
					}
					if (NodePropertyHandle.IsValid() && NodePropertyHandle->GetProperty()->GetBoolMetaData(Name_ShouldShowInViewport))
					{
						bShowChild = true;
					}
					if (bShowChild)
					{
						Nodes.Add(Child);
					}	
				}

			}
			
		}
	}
	if (Nodes.Num())
	{
		NodeList = SNew(SListView< TSharedPtr<IDetailTreeNode> >)
			.ItemHeight(24)
			.ListItemsSource(&Nodes)
			.OnGenerateRow(this, &SInViewportDetails::GenerateListRow);

		
		DetailWidget = SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("PropertyTable.InViewport.Background"))
			[
				NodeList.ToSharedRef()
			];
	}
	return DetailWidget;
}

SInViewportDetails::~SInViewportDetails()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
	USelection::SelectionChangedEvent.RemoveAll(this);
}


void SInViewportDetails::SetObjects(const TArray<UObject*>& InObjects, bool bForceRefresh)
{
	if (PropertyRowGenerator)
	{
		PropertyRowGenerator->SetObjects(InObjects);
		GenerateWidget();
		if (!Nodes.Num())
		{
			// Do this on a delay so that we are are not caught in a loop of creating and hiding the menu
			GEditor->GetTimerManager()->SetTimerForNextTick([this]()
				{
					OwningViewport.Pin()->HideInViewportContextMenu();
				});
		}
	}
}

void SInViewportDetails::PostUndo(bool bSuccess)
{

}

void SInViewportDetails::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}



void SInViewportDetails::OnEditorSelectionChanged(UObject* Object)
{
	TArray<UObject*> SelectedActors;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* Actor = static_cast<AActor*>(*It);
		checkSlow(Actor->IsA(AActor::StaticClass()));

		if (!Actor->IsPendingKill())
		{
			SelectedActors.Add(Actor);
		}
	}
	SetObjects(SelectedActors);
}

AActor* SInViewportDetails::GetSelectedActorInEditor() const
{
	//@todo this doesn't work w/ multi-select
	return GEditor->GetSelectedActors()->GetTop<AActor>();
}

UToolMenu* SInViewportDetails::GetGeneratedToolbarMenu() const
{
	return GeneratedToolbarMenu.IsValid() ? GeneratedToolbarMenu.Get() : nullptr;
}

AActor* SInViewportDetails::GetActorContext() const
{
	AActor* SelectedActorInEditor = GetSelectedActorInEditor();
	
	return SelectedActorInEditor;
}

TSharedRef<ITableRow> SInViewportDetails::GenerateListRow(TSharedPtr<IDetailTreeNode> InItem, const TSharedRef<STableViewBase>& InOwningTable)
{
	return SNew(SInViewportDetailsRow, InOwningTable)
		.InNode(InItem)
		.InDetailsView(SharedThis(this));

}

FReply SInViewportDetails::StartDraggingDetails(FVector2D InTabGrabOffsetFraction, const FPointerEvent& MouseEvent)
{
	// Start dragging.
	TSharedRef<FInViewportUIDragOperation> DragDropOperation =
		FInViewportUIDragOperation::New(
			SharedThis(this),
			InTabGrabOffsetFraction,
			GetDesiredSize()
		);
	if (OwningViewport.IsValid())
	{
		OwningViewport.Pin()->ToggleInViewportContextMenu();
	}
	return FReply::Handled().BeginDragDrop(DragDropOperation);
}

void SInViewportDetailsHeader::Construct(const FArguments& InArgs)
{
	ParentPtr = InArgs._Parent;
	ChildSlot
		[
			InArgs._Content.Widget
		];
}

FReply SInViewportDetailsHeader::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Need to remember where within a tab we grabbed
	const FVector2D TabGrabOffset = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D TabSize = MyGeometry.GetLocalSize();
	const FVector2D TabGrabOffsetFraction = FVector2D(
		FMath::Clamp(TabGrabOffset.X / TabSize.X, 0.0f, 1.0f),
		FMath::Clamp(TabGrabOffset.Y / TabSize.Y, 0.0f, 1.0f));

	TSharedPtr<SInViewportDetails> PinnedParent = ParentPtr.Pin();
	if (PinnedParent.IsValid())
	{
		return PinnedParent->StartDraggingDetails(TabGrabOffsetFraction, MouseEvent);
	}

	return FReply::Unhandled();
}

TSharedPtr<class FDragDropOperation> SInViewportDetailsHeader::CreateDragDropOperation()
{
	TSharedPtr<FDragDropOperation> Operation = MakeShareable(new FDragDropOperation());

	return Operation;
}

void SInViewportDetailsToolbar::Construct(const FArguments& InArgs)
{
	TSharedPtr<SInViewportDetails> Parent = InArgs._Parent;
	if (!Parent.IsValid())
	{
		return;
	}
	const FName ToolBarName = GetQuickActionMenuName(Parent->GetSelectedActorInEditor()->GetClass());
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* FoundMenu = ToolMenus->FindMenu(ToolBarName);

	if (!FoundMenu || !FoundMenu->IsRegistered())
	{
		FoundMenu = ToolMenus->RegisterMenu(ToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
	}

	FToolMenuContext MenuContext;

	UQuickActionMenuContext* ToolbarMenuContext = NewObject<UQuickActionMenuContext>(FoundMenu);
	TWeakPtr<ILevelEditor> ParentLevelEditor = Parent->ParentLevelEditor;
	if (ParentLevelEditor.IsValid())
	{
		ToolbarMenuContext->CurrentSelection = ParentLevelEditor.Pin()->GetElementSelectionSet();
	}
	MenuContext.AddObject(ToolbarMenuContext);
	Parent->GeneratedToolbarMenu = ToolMenus->GenerateMenu(ToolBarName, MenuContext);

	// Move this to the GenerateMenu API
	Parent->GeneratedToolbarMenu->StyleName = "InViewportToolbar";
	Parent->GeneratedToolbarMenu->bToolBarIsFocusable = false;
	Parent->GeneratedToolbarMenu->bToolBarForceSmallIcons = true;
	TSharedRef< class SWidget > ToolBarWidget = ToolMenus->GenerateWidget(Parent->GeneratedToolbarMenu.Get());

	ChildSlot
	[
		ToolBarWidget
	];
}

FName SInViewportDetailsToolbar::GetQuickActionMenuName(UClass* InClass)
{
	return FName("LevelEditor.InViewportPanel");
}

#undef LOCTEXT_NAMESPACE