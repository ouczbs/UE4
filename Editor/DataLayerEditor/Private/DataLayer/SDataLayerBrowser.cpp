// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDataLayerBrowser.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "SDataLayerOutliner.h"
#include "DataLayerActorTreeItem.h"
#include "DataLayerTreeItem.h"
#include "DataLayerMode.h"
#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "DataLayerOutlinerIsDynamicallyLoadedColumn.h"
#include "DataLayerOutlinerDeleteButtonColumn.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "DataLayer"

void SDataLayerBrowser::Construct(const FArguments& InArgs)
{
	Mode = GetDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetShowDataLayerContent() ? EDataLayerBrowserMode::DataLayerContents : EDataLayerBrowserMode::DataLayers;

	auto ToggleDataLayerContents = [this]()
	{
		const EDataLayerBrowserMode NewMode = Mode == EDataLayerBrowserMode::DataLayers ? EDataLayerBrowserMode::DataLayerContents : EDataLayerBrowserMode::DataLayers;
		SetupDataLayerMode(NewMode);
		return FReply::Handled();
	};

	auto GetToggleModeButtonImageBrush = [this]()
	{
		static const FName ExploreDataLayerContents("DataLayerBrowser.ExploreDataLayerContents");
		static const FName ReturnToDataLayersList("DataLayerBrowser.ReturnToDataLayersList");
		return (Mode == EDataLayerBrowserMode::DataLayers) ? FEditorStyle::GetBrush(ExploreDataLayerContents) : FEditorStyle::GetBrush(ReturnToDataLayersList);
	};

	auto GetToggleModeButtonText = [this]()
	{
		return (Mode == EDataLayerBrowserMode::DataLayers) ? LOCTEXT("SeeContentsLabel", "See Contents") : LOCTEXT("HideContentsLabel", "Hide Contents");
	};

	auto GetInvertedForegroundIfHovered = [this]()
	{
		static const FName InvertedForegroundName("InvertedForeground");
		return (ToggleModeButton.IsValid() && (ToggleModeButton->IsHovered() || ToggleModeButton->IsPressed())) ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FSlateColor::UseForeground();
	};

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bAllowSearch = false;
	Args.bHideSelectionTip = true;
	Args.bShowObjectLabel = false;
	DetailsWidget = PropertyModule.CreateDetailView(Args);
	DetailsWidget->SetVisibility(EVisibility::Visible);

	//////////////////////////////////////////////////////////////////////////
	//	DataLayer Contents Header
	SAssignNew(DataLayerContentsHeader, SBorder)
	.BorderImage(FEditorStyle::GetBrush("DataLayerBrowser.DataLayerContentsQuickbarBackground"))
	.Visibility(EVisibility::Visible)
	.Content()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(0, 0, 2, 0))
		[
			SAssignNew(ToggleModeButton, SButton)
			.ContentPadding(FMargin(2, 0, 2, 0))
			.ButtonStyle(FEditorStyle::Get(), "DataLayerBrowserButton")
			.OnClicked_Lambda(ToggleDataLayerContents)
			.ForegroundColor(FSlateColor::UseForeground())
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(0, 1, 3, 1)
				[
					SNew(SImage)
					.Image_Lambda(GetToggleModeButtonImageBrush)
					.ColorAndOpacity_Lambda(GetInvertedForegroundIfHovered)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda(GetToggleModeButtonText)
					.ColorAndOpacity_Lambda(GetInvertedForegroundIfHovered)
				]
			]
		]
	];

	//////////////////////////////////////////////////////////////////////////
	//	DataLayer Contents Section
	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = false;
	InitOptions.bShowParentTree = true;
	InitOptions.bShowCreateNewFolder = false;
	InitOptions.ModeFactory = FCreateSceneOutlinerMode::CreateLambda([this](SSceneOutliner* Outliner) { return new FDataLayerMode(FDataLayerModeParams(Outliner, this, nullptr)); });
	InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
	InitOptions.ColumnMap.Add(FDataLayerOutlinerIsDynamicallyLoadedColumn::GetID(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 1, FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& InSceneOutliner) { return MakeShareable(new FDataLayerOutlinerIsDynamicallyLoadedColumn(InSceneOutliner)); })));
	InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 2));
	InitOptions.ColumnMap.Add(FDataLayerOutlinerDeleteButtonColumn::GetID(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn::CreateLambda([](ISceneOutliner& InSceneOutliner) { return MakeShareable(new FDataLayerOutlinerDeleteButtonColumn(InSceneOutliner)); })));
	DataLayerOutliner = SNew(SDataLayerOutliner, InitOptions).IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());

	SAssignNew(DataLayerContentsSection, SBorder)
	.Padding(5)
	.BorderImage(FEditorStyle::GetBrush("NoBrush"))
	.Content()
	[
		// Data Layer Outliner
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		.Style(FEditorStyle::Get(), "FoliageEditMode.Splitter")
		+ SSplitter::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				DataLayerOutliner.ToSharedRef()
			]
		]
		// Details
		+SSplitter::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				DetailsWidget.ToSharedRef()
			]
		]
	];

	//////////////////////////////////////////////////////////////////////////
	//	DataLayer Browser
	ChildSlot
	[
		SAssignNew(ContentAreaBox, SVerticalBox)
		.IsEnabled_Lambda([]() { return GWorld ? UWorld::HasSubsystem<UWorldPartitionSubsystem>(GWorld) : false; })
	];

	SetupDataLayerMode(Mode);
}

void SDataLayerBrowser::OnSelectionChanged(TSet<TWeakObjectPtr<const UDataLayer>>& InSelectedDataLayersSet)
{
	SelectedDataLayersSet = InSelectedDataLayersSet;
	TArray<UObject*> SelectedDataLayers;
	for (const auto& WeakDataLayer : SelectedDataLayersSet)
	{
		if (WeakDataLayer.IsValid())
		{
			UDataLayer* DataLayer = const_cast<UDataLayer*>(WeakDataLayer.Get());
			if (!DataLayer->IsLocked())
			{
				SelectedDataLayers.Add(DataLayer);
			}
		}
	}
	DetailsWidget->SetObjects(SelectedDataLayers, /*bForceRefresh*/ true);
}


void SDataLayerBrowser::SetupDataLayerMode(EDataLayerBrowserMode InNewMode)
{
	ContentAreaBox->ClearChildren();
	ContentAreaBox->AddSlot()
	.AutoHeight()
	.FillHeight(1.0f)
	[
		DataLayerContentsSection.ToSharedRef()
	];

	ContentAreaBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Bottom)
	.MaxHeight(23)
	[
		DataLayerContentsHeader.ToSharedRef()
	];

	Mode = InNewMode;
	
	GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->SetShowDataLayerContent(Mode == EDataLayerBrowserMode::DataLayerContents);

	ModeChanged.Broadcast(Mode);
}

#undef LOCTEXT_NAMESPACE