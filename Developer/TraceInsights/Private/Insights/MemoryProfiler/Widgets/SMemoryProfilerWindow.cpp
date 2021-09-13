// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemoryProfilerWindow.h"

#include "EditorStyleSet.h"
#include "Features/IModularFeatures.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SlateOptMacros.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR
	#include "EngineAnalytics.h"
	#include "Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h"
	#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#endif // WITH_EDITOR

// Insights
#include "Insights/Common/InsightsMenuBuilder.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/ITimingViewExtender.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/MemoryProfiler/Widgets/SMemAllocTableTreeView.h"
#include "Insights/MemoryProfiler/Widgets/SMemInvestigationView.h"
#include "Insights/MemoryProfiler/Widgets/SMemoryProfilerToolbar.h"
#include "Insights/MemoryProfiler/Widgets/SMemTagTreeView.h"
#include "Insights/TraceInsightsModule.h"
#include "Insights/Version.h"
#include "Insights/ViewModels/TimeRulerTrack.h"
#include "Insights/Widgets/SInsightsSettings.h"
#include "Insights/Widgets/STimingView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "SMemoryProfilerWindow"

////////////////////////////////////////////////////////////////////////////////////////////////////

const FName FMemoryProfilerTabs::ToolbarID(TEXT("Toolbar"));
const FName FMemoryProfilerTabs::TimingViewID(TEXT("TimingView"));
const FName FMemoryProfilerTabs::MemInvestigationViewID(TEXT("MemInvestigation"));
const FName FMemoryProfilerTabs::MemTagTreeViewID(TEXT("LowLevelMemTags"));
const FName FMemoryProfilerTabs::MemAllocTableTreeViewID(TEXT("MemAllocTableTreeView"));

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemoryProfilerWindow::SMemoryProfilerWindow()
	: TimingView()
	, SharedState(MakeShared<FMemorySharedState>())
	, MemInvestigationView()
	, MemTagTreeView()
	, TabManager()
	, ActiveTimerHandle()
	, DurationActive(0.0f)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemoryProfilerWindow::~SMemoryProfilerWindow()
{
	if (MemTagTreeView)
	{
		HideTab(FMemoryProfilerTabs::MemTagTreeViewID);
		check(MemTagTreeView == nullptr);
	}

	if (MemInvestigationView)
	{
		HideTab(FMemoryProfilerTabs::MemInvestigationViewID);
		check(MemInvestigationView == nullptr);
	}

	if (TimingView)
	{
		HideTab(FMemoryProfilerTabs::TimingViewID);
		check(TimingView == nullptr);
	}

	HideTab(FMemoryProfilerTabs::ToolbarID);

#if WITH_EDITOR
	if (DurationActive > 0.0f && FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Insights.Usage.MemoryProfiler"), FAnalyticsEventAttribute(TEXT("Duration"), DurationActive));
	}
#endif // WITH_EDITOR
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::Reset()
{
	if (TimingView)
	{
		TimingView->Reset();
		ResetTimingViewMarkers();
	}

	if (MemInvestigationView)
	{
		MemInvestigationView->Reset();
	}

	if (MemTagTreeView)
	{
		MemTagTreeView->Reset();
	}

	UpdateMemInvestigationView();
	UpdateTableTreeViews();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::ResetTimingViewMarkers()
{
	TSharedRef<FTimeRulerTrack> TimeRulerTrack = TimingView->GetTimeRulerTrack();

	TimeRulerTrack->RemoveAllTimeMarkers();
	CustomTimeMarkers.Reset();

	constexpr uint32 MaxNumTimeMarkers = 5;

	TCHAR TimeMarkerName[2];
	TimeMarkerName[1] = 0;

	for (uint32 Index = 0; Index < MaxNumTimeMarkers; ++Index)
	{
		// Keep (re-add) the "Default Time Marker" as the first time marker and create new ones for the rest.
		TSharedRef<Insights::FTimeMarker> TimeMarker = (Index == 0) ? TimingView->GetDefaultTimeMarker() : MakeShared<Insights::FTimeMarker>();

		TimeMarkerName[0] = TEXT('A') + Index; // "A", "B", "C", etc.
		TimeMarker->SetName(TimeMarkerName);

		const uint32 HueStep = 256 / MaxNumTimeMarkers;
		const uint8 H = uint8(HueStep * Index);
		const uint8 S = 192;
		const uint8 V = 255;
		const FLinearColor Color = FLinearColor::MakeFromHSV8(H, S, V);
		TimeMarker->SetColor(Color);

		TimeMarker->SetTime(static_cast<float>(Index)); // 0.0f, 1.0f, 2.0f, etc.

		TimeRulerTrack->AddTimeMarker(TimeMarker);
		CustomTimeMarkers.Add(TimeMarker);
	}

	UpdateTimingViewMarkers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::UpdateTimingViewMarkers()
{
	TSharedPtr<Insights::FMemoryRuleSpec> Rule = SharedState->GetCurrentMemoryRule();
	const uint32 NumVisibleTimeMarkers = Rule ? Rule->GetNumTimeMarkers() : 0;

	const uint32 NumTimeMarkers = CustomTimeMarkers.Num();
	ensure(NumVisibleTimeMarkers <= NumTimeMarkers);

	for (uint32 Index = 0; Index < NumTimeMarkers; ++Index)
	{
		TSharedRef<Insights::FTimeMarker>& TimeMarker = CustomTimeMarkers[Index];
		if (Index < NumVisibleTimeMarkers)
		{
			TimeMarker->SetVisibility(true);
		}
		else
		{
			TimeMarker->SetVisibility(false);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::UpdateMemInvestigationView()
{
	if (MemInvestigationView)
	{
		//MemInvestigationView->Update();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::UpdateTableTreeViews()
{
	UpdateMemTagTreeView();

	for (TSharedPtr<Insights::SMemAllocTableTreeView>& MemAllocTableTreeView : MemAllocTableTreeViews)
	{
		UpdateMemAllocTableTreeView(MemAllocTableTreeView);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::UpdateMemTagTreeView()
{
	if (MemTagTreeView)
	{
		/*
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadMemoryProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::IMemoryProfilerProvider& MemoryProfilerProvider = *TraceServices::ReadMemoryProfilerProvider(*Session.Get());

			const double SelectionStartTime = TimingView ? TimingView->GetSelectionStartTime() : 0.0;
			const double SelectionEndTime = TimingView ? TimingView->GetSelectionEndTime() : 0.0;

			TraceServices::ITable<TraceServices::FMemoryProfilerAggregatedStats>* EventAggregationTable = MemoryProfilerProvider.CreateEventAggregation(SelectionStartTime, SelectionEndTime);
			MemTagTreeView->UpdateSourceTable(MakeShareable(EventAggregationTable));
		}
		else
		{
			MemTagTreeView->UpdateSourceTable(nullptr);
		}
		*/
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::UpdateMemAllocTableTreeView(TSharedPtr<Insights::SMemAllocTableTreeView> MemAllocTableTreeView)
{
	if (MemAllocTableTreeView)
	{
		//TODO
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SDockTab> SMemoryProfilerWindow::SpawnTab_Toolbar(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(true)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SMemoryProfilerToolbar)
		];

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SMemoryProfilerWindow::OnToolbarTabClosed));

	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnToolbarTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> SMemoryProfilerWindow::SpawnTab_TimingView(const FSpawnTabArgs& Args)
{
	FMemoryProfilerManager::Get()->SetTimingViewVisible(true);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(TimingView, STimingView)
		];

	SharedState->SetTimingView(TimingView);
	IModularFeatures::Get().RegisterModularFeature(Insights::TimingViewExtenderFeatureName, SharedState.Get());

	TimingView->Reset(true);
	ResetTimingViewMarkers();
	TimingView->HideAllDefaultTracks();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SMemoryProfilerWindow::OnTimingViewTabClosed));

	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnTimingViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	IModularFeatures::Get().UnregisterModularFeature(Insights::TimingViewExtenderFeatureName, SharedState.Get());
	SharedState->SetTimingView(nullptr);

	TimingView = nullptr;

	FMemoryProfilerManager::Get()->SetTimingViewVisible(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> SMemoryProfilerWindow::SpawnTab_MemInvestigationView(const FSpawnTabArgs& Args)
{
	FMemoryProfilerManager::Get()->SetMemInvestigationViewVisible(true);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(MemInvestigationView, SMemInvestigationView, SharedThis(this))
		];

	UpdateMemInvestigationView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SMemoryProfilerWindow::OnMemInvestigationViewTabClosed));

	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnMemInvestigationViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FMemoryProfilerManager::Get()->SetMemInvestigationViewVisible(false);
	MemInvestigationView = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> SMemoryProfilerWindow::SpawnTab_MemTagTreeView(const FSpawnTabArgs& Args)
{
	FMemoryProfilerManager::Get()->SetMemTagTreeViewVisible(true);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(MemTagTreeView, SMemTagTreeView, SharedThis(this))
		];

	UpdateMemTagTreeView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SMemoryProfilerWindow::OnMemTagTreeViewTabClosed));

	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnMemTagTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FMemoryProfilerManager::Get()->SetMemTagTreeViewVisible(false);
	MemTagTreeView = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> SMemoryProfilerWindow::SpawnTab_MemAllocTableTreeView(const FSpawnTabArgs& Args, int32 TabIndex)
{
	//FMemoryProfilerManager::Get()->SetMemAllocTableTreeViewVisible(TabIndex, true);

	TSharedRef<Insights::FMemAllocTable> MemAllocTable = MakeShared<Insights::FMemAllocTable>();
	MemAllocTable->Reset();

	TSharedPtr<Insights::SMemAllocTableTreeView> MemAllocTableTreeView;

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(MemAllocTableTreeView, Insights::SMemAllocTableTreeView, MemAllocTable)
		];

	MemAllocTableTreeView->SetLogListingName(FMemoryProfilerManager::Get()->GetLogListingName());
	MemAllocTableTreeView->SetTabIndex(TabIndex);
	MemAllocTableTreeViews.Add(MemAllocTableTreeView);
	UpdateMemAllocTableTreeView(MemAllocTableTreeView);

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SMemoryProfilerWindow::OnMemAllocTableTreeViewTabClosed));

	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnMemAllocTableTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	TSharedRef<Insights::SMemAllocTableTreeView> MemAllocTableTreeView = StaticCastSharedRef<Insights::SMemAllocTableTreeView>(TabBeingClosed->GetContent());

	FName ClosingTabId = FMemoryProfilerTabs::MemAllocTableTreeViewID;
	ClosingTabId.SetNumber(MemAllocTableTreeView->GetTabIndex());

	TSharedPtr<Insights::FQueryTargetWindowSpec> TargetToDelete;
	const TArray<TSharedPtr<Insights::FQueryTargetWindowSpec>>& Targets = this->GetSharedState().GetQueryTargets();
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		if (Targets[Index]->GetName() == ClosingTabId)
		{
			TargetToDelete = Targets[Index];
			break;
		}
	}

	if (TargetToDelete.IsValid())
	{
		this->GetSharedState().RemoveQueryTarget(TargetToDelete);
	}

	if (Targets.Num() > 0)
	{
		TSharedPtr<Insights::FQueryTargetWindowSpec> NewSelection = Targets[0];
		this->GetSharedState().SetCurrentQueryTarget(NewSelection);
		if (MemInvestigationView.IsValid())
		{
			MemInvestigationView->QueryTarget_OnSelectionChanged(NewSelection, ESelectInfo::Type::Direct);
		}
	}

	TabManager->UnregisterTabSpawner(ClosingTabId);

	MemAllocTableTreeView->OnClose();
	MemAllocTableTreeViews.Remove(MemAllocTableTreeView);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::CloseMemAllocTableTreeTabs()
{
	const TArray<TSharedPtr<Insights::FQueryTargetWindowSpec>>& Targets = this->GetSharedState().GetQueryTargets();
	while(Targets.Num() > 0)
	{
		FName Name = Targets[0]->GetName();
		this->GetSharedState().RemoveQueryTarget(Targets[0]);

		if (Name != Insights::FQueryTargetWindowSpec::NewWindow)
		{
			HideTab(Name);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<Insights::SMemAllocTableTreeView> SMemoryProfilerWindow::ShowMemAllocTableTreeViewTab()
{
	if (SharedState->GetCurrentQueryTarget()->GetName() == Insights::FQueryTargetWindowSpec::NewWindow)
	{
		++LastMemAllocTableTreeViewIndex;
		FName TabId = FMemoryProfilerTabs::MemAllocTableTreeViewID;
		TabId.SetNumber(LastMemAllocTableTreeViewIndex);

		FText MemAllocTableTreeViewTabDisplayName = FText::Format(LOCTEXT("MemoryProfiler.MemAllocTableTreeViewTabTitle", "Allocs Table {0}"), FText::AsNumber(LastMemAllocTableTreeViewIndex));
		TabManager->RegisterTabSpawner(TabId, FOnSpawnTab::CreateRaw(this, &SMemoryProfilerWindow::SpawnTab_MemAllocTableTreeView, LastMemAllocTableTreeViewIndex))
			.SetDisplayName(MemAllocTableTreeViewTabDisplayName)
			.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "MemAllocTableTreeView.Icon.Small"))
			.SetGroup(AppMenuGroup.ToSharedRef());


		TSharedPtr<Insights::FQueryTargetWindowSpec> NewTarget = MakeShared<Insights::FQueryTargetWindowSpec>(TabId, MemAllocTableTreeViewTabDisplayName);
		SharedState->AddQueryTarget(NewTarget);
		SharedState->SetCurrentQueryTarget(NewTarget);
		MemInvestigationView->QueryTarget_OnSelectionChanged(NewTarget, ESelectInfo::Type::Direct);
	}

	FName TabId = SharedState->GetCurrentQueryTarget()->GetName();
	if (TabManager->HasTabSpawner(TabId))
	{
		TSharedPtr<SDockTab> Tab = TabManager->TryInvokeTab(TabId);
		if (Tab)
		{
			TSharedRef<Insights::SMemAllocTableTreeView> MemAllocTableTreeView = StaticCastSharedRef<Insights::SMemAllocTableTreeView>(Tab->GetContent());

			if (SharedState->GetCurrentQueryTarget()->GetName() == Insights::FQueryTargetWindowSpec::NewWindow)
			{
				Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SMemoryProfilerWindow::OnMemAllocTableTreeViewTabClosed));
			}
			return MemAllocTableTreeView;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	// Create & initialize tab manager.
	TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);
	const auto& PersistLayout = [](const TSharedRef<FTabManager::FLayout>& LayoutToSave)
	{
		FLayoutSaveRestore::SaveToConfig(FTraceInsightsModule::GetUnrealInsightsLayoutIni(), LayoutToSave);
	};
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateLambda(PersistLayout));

	AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("MemoryProfilerMenuGroupName", "Memory Insights"));

	TabManager->RegisterTabSpawner(FMemoryProfilerTabs::ToolbarID, FOnSpawnTab::CreateRaw(this, &SMemoryProfilerWindow::SpawnTab_Toolbar))
		.SetDisplayName(LOCTEXT("DeviceToolbarTabTitle", "Toolbar"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Toolbar.Icon.Small"))
		.SetGroup(AppMenuGroup.ToSharedRef());

	TabManager->RegisterTabSpawner(FMemoryProfilerTabs::TimingViewID, FOnSpawnTab::CreateRaw(this, &SMemoryProfilerWindow::SpawnTab_TimingView))
		.SetDisplayName(LOCTEXT("MemoryProfiler.TimingViewTabTitle", "Timing View"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "TimingView.Icon.Small"))
		.SetGroup(AppMenuGroup.ToSharedRef());

	TabManager->RegisterTabSpawner(FMemoryProfilerTabs::MemInvestigationViewID, FOnSpawnTab::CreateRaw(this, &SMemoryProfilerWindow::SpawnTab_MemInvestigationView))
		.SetDisplayName(LOCTEXT("MemoryProfiler.MemInvestigationViewTabTitle", "Investigation"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "MemInvestigationView.Icon.Small"))
		.SetGroup(AppMenuGroup.ToSharedRef());

	TabManager->RegisterTabSpawner(FMemoryProfilerTabs::MemTagTreeViewID, FOnSpawnTab::CreateRaw(this, &SMemoryProfilerWindow::SpawnTab_MemTagTreeView))
		.SetDisplayName(LOCTEXT("MemoryProfiler.MemTagTreeViewTabTitle", "LLM Tags"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "MemTagTreeView.Icon.Small"))
		.SetGroup(AppMenuGroup.ToSharedRef());

	TSharedPtr<FMemoryProfilerManager> MemoryProfilerManager = FMemoryProfilerManager::Get();
	ensure(MemoryProfilerManager.IsValid());

	// Create tab layout.
	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("InsightsMemoryProfilerLayout_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(FMemoryProfilerTabs::ToolbarID, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(1.0f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.77f)
					->SetHideTabWell(true)
					->AddTab(FMemoryProfilerTabs::TimingViewID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.23f)
					->AddTab(FMemoryProfilerTabs::MemInvestigationViewID, ETabState::OpenedTab)
					->AddTab(FMemoryProfilerTabs::MemTagTreeViewID, ETabState::OpenedTab)
					->SetForegroundTab(FMemoryProfilerTabs::MemInvestigationViewID)
				)
			)
		);

	Layout = FLayoutSaveRestore::LoadFromConfig(FTraceInsightsModule::GetUnrealInsightsLayoutIni(), Layout);

	// Create & initialize main menu.
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());

	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("MenuLabel", "Menu"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateStatic(&SMemoryProfilerWindow::FillMenu, TabManager),
		FName(TEXT("Menu"))
	);

	TSharedRef<SWidget> MenuWidget = MenuBarBuilder.MakeWidget();

	ChildSlot
		[
			SNew(SOverlay)

			// Version
			+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(0.0f, -16.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Clipping(EWidgetClipping::ClipToBoundsWithoutIntersecting)
						.Text(LOCTEXT("UnrealInsightsVersion", UNREAL_INSIGHTS_VERSION_STRING_EX))
						.ColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
				]

			// Overlay slot for the main window area
			+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							MenuWidget
						]

					+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							TabManager->RestoreFrom(Layout, ConstructUnderWindow).ToSharedRef()
						]
				]

			// Session hint overlay
			+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
						.Visibility(this, &SMemoryProfilerWindow::IsSessionOverlayVisible)
						.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
						.Padding(8.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("SelectTraceOverlayText", "Please select a trace."))
						]
				]
		];

	// Tell tab-manager about the global menu bar.
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuWidget);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::FillMenu(FMenuBuilder& MenuBuilder, const TSharedPtr<FTabManager> TabManager)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	FInsightsManager::Get()->GetInsightsMenuBuilder()->PopulateMenu(MenuBuilder);

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::ShowTab(const FName& TabID)
{
	if (TabManager->HasTabSpawner(TabID))
	{
		TabManager->TryInvokeTab(TabID);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::HideTab(const FName& TabID)
{
	TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(TabID);
	if (Tab.IsValid())
	{
		Tab->RequestCloseTab();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EVisibility SMemoryProfilerWindow::IsSessionOverlayVisible() const
{
	if (FInsightsManager::Get()->GetSession().IsValid())
	{
		return EVisibility::Hidden;
	}
	else
	{
		return EVisibility::Visible;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemoryProfilerWindow::IsProfilerEnabled() const
{
	return FInsightsManager::Get()->GetSession().IsValid();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EActiveTimerReturnType SMemoryProfilerWindow::UpdateActiveDuration(double InCurrentTime, float InDeltaTime)
{
	DurationActive += InDeltaTime;

	// The profiler window will explicitly unregister this active timer when the mouse leaves.
	return EActiveTimerReturnType::Continue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

	if (!ActiveTimerHandle.IsValid())
	{
		ActiveTimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SMemoryProfilerWindow::UpdateActiveDuration));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	auto PinnedActiveTimerHandle = ActiveTimerHandle.Pin();
	if (PinnedActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PinnedActiveTimerHandle.ToSharedRef());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemoryProfilerWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return FMemoryProfilerManager::Get()->GetCommandList()->ProcessCommandBindings(InKeyEvent) ? FReply::Handled() : FReply::Unhandled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemoryProfilerWindow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (DragDropOp.IsValid())
	{
		if (DragDropOp->HasFiles())
		{
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".utrace"))
				{
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnDragOver(MyGeometry,DragDropEvent);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemoryProfilerWindow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (DragDropOp.IsValid())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".utrace"))
				{
					// Enqueue load operation.
					FInsightsManager::Get()->LoadTraceFile(Files[0]);
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnDrop(MyGeometry,DragDropEvent);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
