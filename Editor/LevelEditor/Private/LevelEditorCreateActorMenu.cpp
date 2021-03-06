// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditorCreateActorMenu.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "ToolMenus.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBoxVolume.h"
#include "ActorFactories/ActorFactoryCameraActor.h"
#include "ActorFactories/ActorFactoryCylinderVolume.h"
#include "ActorFactories/ActorFactoryDirectionalLight.h"
#include "ActorFactories/ActorFactoryPlayerStart.h"
#include "ActorFactories/ActorFactoryPointLight.h"
#include "ActorFactories/ActorFactorySpotLight.h"
#include "ActorFactories/ActorFactoryRectLight.h"
#include "ActorFactories/ActorFactorySphereVolume.h"
#include "ActorFactories/ActorFactoryTriggerBox.h"
#include "ActorFactories/ActorFactoryTriggerCapsule.h"
#include "ActorFactories/ActorFactoryTriggerSphere.h"
#include "GameFramework/Volume.h"
#include "Engine/BlockingVolume.h"
#include "AssetData.h"
#include "AssetThumbnail.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "AssetSelection.h"
#include "AssetRegistryModule.h"
#include "Styling/SlateIconFinder.h"
#include "ClassIconFinder.h"
#include "LevelEditorActions.h"
#include "ActorPlacementInfo.h"
#include "IPlacementModeModule.h"
#include "Engine/TriggerBase.h"
#include "LevelEditorMenuContext.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Engine/Selection.h"
#include "ActorEditorUtils.h"

class SMenuThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMenuThumbnail ) 
		: _Width(32)
		, _Height(32)
	{}
		SLATE_ARGUMENT( uint32, Width )
		SLATE_ARGUMENT( uint32, Height )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const FAssetData& InAsset )
	{
		Asset = InAsset;

		Thumbnail = MakeShareable( new FAssetThumbnail(Asset, InArgs._Width, InArgs._Height, UThumbnailManager::Get().GetSharedThumbnailPool()));

		ChildSlot
		[
			Thumbnail->MakeThumbnailWidget()
		];
	}

private:

	FAssetData Asset;
	TSharedPtr< FAssetThumbnail > Thumbnail;
};

class SAssetMenuEntry : public SCompoundWidget
{
	public:

	SLATE_BEGIN_ARGS( SAssetMenuEntry ){}
		SLATE_ARGUMENT( FText, LabelOverride )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The layer the row widget is supposed to represent
	 * @param	InOwnerTableView	The owner of the row widget
	 */
	void Construct( const FArguments& InArgs, const FAssetData& Asset, const TArray< FActorFactoryAssetProxy::FMenuItem >& AssetMenuOptions )
	{
		TSharedPtr< SHorizontalBox > ActorType = SNew(SHorizontalBox);

		const bool IsClass = Asset.GetClass() == UClass::StaticClass();
		const bool IsVolume = IsClass ? Cast<UClass>( Asset.GetAsset() )->IsChildOf( AVolume::StaticClass() ) : false;

		FText AssetDisplayName = FText::FromName( Asset.AssetName );
		if ( IsClass )
		{
			AssetDisplayName = FText::FromString( FName::NameToDisplayString( Asset.AssetName.ToString(), false ) );
		}

		FText ActorTypeDisplayName;
		if ( AssetMenuOptions.Num() == 1 )
		{
			const FActorFactoryAssetProxy::FMenuItem& MenuItem = AssetMenuOptions[0];

			AActor* DefaultActor = NULL;
			if ( IsClass && Cast<UClass>( MenuItem.AssetData.GetAsset() )->IsChildOf( AActor::StaticClass() ) )
			{
				DefaultActor = Cast<AActor>( Cast<UClass>( MenuItem.AssetData.GetAsset() )->ClassDefaultObject );
				ActorTypeDisplayName = FText::FromString( FName::NameToDisplayString( DefaultActor->GetClass()->GetName(), false ) );
			}

			const FSlateBrush* IconBrush = NULL;
			if ( MenuItem.FactoryToUse != NULL )
			{
				DefaultActor = MenuItem.FactoryToUse->GetDefaultActor( MenuItem.AssetData );

				// Prefer the class type name set above over the factory's display name
				if (ActorTypeDisplayName.IsEmpty())
				{
					ActorTypeDisplayName = MenuItem.FactoryToUse->GetDisplayName();
				}

				IconBrush = FSlateIconFinder::FindIconBrushForClass(MenuItem.FactoryToUse->GetClass());
			}

			if ( DefaultActor != NULL && ( MenuItem.FactoryToUse != NULL || !IsClass ) )
			{
				if ( !IconBrush )
				{
					IconBrush = FClassIconFinder::FindIconForActor( DefaultActor );
				}

				if ( !IsClass || IsVolume )
				{
					ActorType->AddSlot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding( 2, 0 )
					.AutoWidth()
					[
						SNew( STextBlock )
						.Text( ActorTypeDisplayName )
						.Font( FEditorStyle::GetFontStyle("LevelViewportContextMenu.ActorType.Text.Font") )
						.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
					];

					ActorType->AddSlot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew( SImage )
						.Image( IconBrush )
						.ToolTipText( ActorTypeDisplayName )
					];
				}
			}
		}

		if ( !InArgs._LabelOverride.IsEmpty() )
		{
			AssetDisplayName = InArgs._LabelOverride;
		}

		ChildSlot
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.Padding( 4, 0, 0, 0 )
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( 35 )
				.HeightOverride( 35 )
				[
					SNew( SMenuThumbnail, Asset )
				]
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 4, 0)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.Padding(0, 0, 0, 1)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("LevelViewportContextMenu.AssetLabel.Text.Font") )
					.Text( ( IsClass && !IsVolume && !ActorTypeDisplayName.IsEmpty() ) ? ActorTypeDisplayName : AssetDisplayName )
				]

				+SVerticalBox::Slot()
				.Padding(0, 1, 0, 0)
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					ActorType.ToSharedRef()
				]
			]
		];
	}
};

static bool CanReplaceActors()
{
	bool bCanReplace = false;

	for (FSelectionIterator SelectionIter = GEditor->GetSelectedActorIterator(); SelectionIter; ++SelectionIter)
	{
		if (AActor* Actor = Cast<AActor>(*SelectionIter))
		{ 
			bCanReplace = true;
			if(!Actor->IsUserManaged() || FActorEditorUtils::IsABuilderBrush(Actor))
			{
				bCanReplace = false;
				break;
			}
		}
	}

	return bCanReplace;
}

/**
 * Helper function to get menu options for selected asset data
 * @param	TargetAssetData		Asset data to be used
 * @param	AssetMenuOptions	Output menu options
 */
static void GetContentBrowserSelectionFactoryMenuEntries( FAssetData& TargetAssetData, TArray< FActorFactoryAssetProxy::FMenuItem >& AssetMenuOptions )
{
	TArray<FAssetData> SelectedAssets;
	AssetSelectionUtils::GetSelectedAssets( SelectedAssets );

	bool bPlaceable = true;

	if ( SelectedAssets.Num() > 0 )
	{
		TargetAssetData = SelectedAssets.Top();
	}

	if ( TargetAssetData.GetClass() == UClass::StaticClass() )
	{
		UClass* Class = Cast<UClass>( TargetAssetData.GetAsset() );

		bPlaceable = AssetSelectionUtils::IsClassPlaceable( Class );
	}
	else if (TargetAssetData.GetClass()->IsChildOf<UBlueprint>())
	{
		// For blueprints, attempt to determine placeability from its tag information

		FString TagValue;

		if ( TargetAssetData.GetTagValue( FBlueprintTags::NativeParentClassPath, TagValue ) && !TagValue.IsEmpty() )
		{
			// If the native parent class can't be placed, neither can the blueprint

			UObject* Outer = nullptr;
			ResolveName( Outer, TagValue, false, false );
			UClass* NativeParentClass = FindObject<UClass>( ANY_PACKAGE, *TagValue );

			bPlaceable = AssetSelectionUtils::IsChildBlueprintPlaceable( NativeParentClass );
		}
		
		if ( bPlaceable && TargetAssetData.GetTagValue( FBlueprintTags::ClassFlags, TagValue ) && !TagValue.IsEmpty() )
		{
			// Check to see if this class is placeable from its class flags

			const int32 NotPlaceableFlags = CLASS_NotPlaceable | CLASS_Deprecated | CLASS_Abstract;
			uint32 ClassFlags = FCString::Atoi( *TagValue );

			bPlaceable = ( ClassFlags & NotPlaceableFlags ) == CLASS_None;
		}
	}

	if ( bPlaceable )
	{
		FActorFactoryAssetProxy::GenerateActorFactoryMenuItems( TargetAssetData, &AssetMenuOptions, true );
	}
}


/**
 * Helper function for FillAddReplaceActorMenu & FillAddReplaceContextMenuSections. Builds a menu for an asset & options.
 * @param	MenuBuilder			The menu builder used to generate the context menu
 * @param	Asset				Asset data to use
 * @param	AssetMenuOptions	Menu options to use
 * @param	CreateMode			The creation mode to use
 */
static void FillAssetAddReplaceActorMenu(UToolMenu* Menu, const FAssetData Asset, const TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions, EActorCreateMode::Type CreateMode)
{
	FToolMenuSection& Section = Menu->AddSection("Section");
	for( int32 ItemIndex = 0; ItemIndex < AssetMenuOptions.Num(); ++ItemIndex )
	{
		const FActorFactoryAssetProxy::FMenuItem& MenuItem = AssetMenuOptions[ItemIndex];
		AActor* DefaultActor = MenuItem.FactoryToUse->GetDefaultActor( MenuItem.AssetData );

		FText Label = MenuItem.FactoryToUse->DisplayName;
		FText ToolTip = MenuItem.FactoryToUse->DisplayName;

		FSlateIcon Icon = FSlateIconFinder::FindIcon(*FString::Printf(TEXT("ClassIcon.%s"), *MenuItem.FactoryToUse->GetClass()->GetName()));
		if ( !Icon.IsSet() )
		{
			Icon = FClassIconFinder::FindSlateIconForActor(DefaultActor);
		}

		FUIAction Action;
		if ( CreateMode == EActorCreateMode::Replace )
		{
			Action = FUIAction( FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ReplaceActors_Clicked, MenuItem.FactoryToUse,  MenuItem.AssetData ) );
		}
		else
		{
			Action = FUIAction( FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AddActor_Clicked, MenuItem.FactoryToUse,  MenuItem.AssetData) );
		}

		Section.AddMenuEntry(NAME_None, Label, ToolTip, Icon, Action);
	}
}

/**
 * Helper function for FillAddReplaceActorMenu & FillAddReplaceContextMenuSections. Builds a single menu option.
 * @param	MenuBuilder			The menu builder used to generate the context menu
 * @param	Asset				Asset data to use
 * @param	AssetMenuOptions	Menu options to use
 * @param	CreateMode			The creation mode to use
 * @param	LabelOverride		The lable to use, if any.
 */
static void BuildSingleAssetAddReplaceActorMenu(FToolMenuSection& Section, const FAssetData& Asset, const TArray< FActorFactoryAssetProxy::FMenuItem >& AssetMenuOptions, EActorCreateMode::Type CreateMode, const FText& LabelOverride = FText::GetEmpty())
{
	if ( !Asset.IsValid() || AssetMenuOptions.Num() == 0 )
	{
		return;
	}

	if ( AssetMenuOptions.Num() == 1 )
	{
		const FActorFactoryAssetProxy::FMenuItem& MenuItem = AssetMenuOptions[0];

		FUIAction Action;
		if ( CreateMode == EActorCreateMode::Replace )
		{
			Action = FUIAction( FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ReplaceActors_Clicked, MenuItem.FactoryToUse,  MenuItem.AssetData ) );
		}
		else
		{
			Action = FUIAction( FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AddActor_Clicked, MenuItem.FactoryToUse,  MenuItem.AssetData) );
		}

		Section.AddEntry(FToolMenuEntry::InitMenuEntry(NAME_None, Action, SNew(SAssetMenuEntry, Asset, AssetMenuOptions).LabelOverride(LabelOverride)));
	}
	else
	{
		FToolMenuEntry Entry = FToolMenuEntry::InitWidget(NAME_None, SNew( SAssetMenuEntry, Asset, AssetMenuOptions ).LabelOverride( LabelOverride ), FText());
		Entry.Type = EMultiBlockType::MenuEntry;
		Entry.SubMenuData.bIsSubMenu = true;
		Entry.SubMenuData.ConstructMenu = FNewToolMenuDelegate::CreateStatic(&FillAssetAddReplaceActorMenu, Asset, AssetMenuOptions, CreateMode);
		Section.AddEntry(Entry);
	}
}

void LevelEditorCreateActorMenu::FillAddReplaceContextMenuSections(UToolMenu* Menu, ULevelEditorContextMenuContext* LevelEditorMenuContext)
{
	FAssetData TargetAssetData;
	TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
	GetContentBrowserSelectionFactoryMenuEntries( /*OUT*/TargetAssetData, /*OUT*/AssetMenuOptions );

	const bool bCanPlaceActor = LevelEditorMenuContext && (LevelEditorMenuContext->ContextType == ELevelEditorMenuContext::Viewport);
	const bool bCanReplaceActors = CanReplaceActors();

	if ( AssetMenuOptions.Num() == 0 )
	{
		{
			FToolMenuSection& Section = Menu->AddSection("ActorType");

			if (bCanPlaceActor)
			{
				Section.AddSubMenu(
					"AddActor",
					NSLOCTEXT("LevelViewportContextMenu", "AddActorHeading", "Place Actor") , 
					NSLOCTEXT("LevelViewportContextMenu", "AddActorMenu_ToolTip", "Templates for adding a new actor to the world"),
					FNewToolMenuDelegate::CreateStatic(&LevelEditorCreateActorMenu::FillAddReplaceActorMenu, EActorCreateMode::Add));
			}

			if (bCanReplaceActors)
			{
				Section.AddSubMenu(
					"ReplaceActor",
					NSLOCTEXT("LevelViewportContextMenu", "ReplaceActorHeading", "Replace Selected Actors with") , 
					NSLOCTEXT("LevelViewportContextMenu", "ReplaceActorMenu_ToolTip", "Templates for replacing selected with new actors in the world"),
					FNewToolMenuDelegate::CreateStatic(&LevelEditorCreateActorMenu::FillAddReplaceActorMenu, EActorCreateMode::Replace));
			}
		}
	}
	else
	{
		while ( AssetMenuOptions.Num() > 1 )
		{
			AssetMenuOptions.Pop();
		}

		if (bCanPlaceActor)
		{
			FToolMenuSection& Section = Menu->AddSection("AddActor", NSLOCTEXT("LevelViewportContextMenu", "AddActorHeading", "Place Actor"));
			FUIAction Action( FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AddActor_Clicked, AssetMenuOptions[0].FactoryToUse,  AssetMenuOptions[0].AssetData ) );
			TSharedRef<SWidget> Widget = SNew(SAssetMenuEntry, TargetAssetData, AssetMenuOptions);
			Section.AddEntry(FToolMenuEntry::InitSubMenu(
				"AddActor",
				Action,
				Widget,
				FNewToolMenuDelegate::CreateStatic(&LevelEditorCreateActorMenu::FillAddReplaceActorMenu, EActorCreateMode::Add)
			));
		}

		if (bCanReplaceActors)
		{
			{
				FToolMenuSection& Section = Menu->AddSection("ReplaceActor", NSLOCTEXT("LevelViewportContextMenu", "ReplaceActorHeading", "Replace Selected Actors with"));
				FUIAction Action( FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ReplaceActors_Clicked, AssetMenuOptions[0].FactoryToUse,  AssetMenuOptions[0].AssetData ) );
				TSharedRef<SWidget> Widget = SNew(SAssetMenuEntry, TargetAssetData, AssetMenuOptions);
				Section.AddEntry(FToolMenuEntry::InitSubMenu(
					"ReplaceActor",
					Action,
					Widget,
					FNewToolMenuDelegate::CreateStatic(&LevelEditorCreateActorMenu::FillAddReplaceActorMenu, EActorCreateMode::Replace)
				));
			}
		}
	}
}

void LevelEditorCreateActorMenu::FillAddReplaceActorMenu(UToolMenu* Menu, EActorCreateMode::Type CreateMode)
{
	{
		FToolMenuSection& Section = Menu->AddSection("ContentBrowserActor", NSLOCTEXT("LevelViewportContextMenu", "AssetSelectionSection", "Selection"));
		FAssetData TargetAssetData;
		TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
		GetContentBrowserSelectionFactoryMenuEntries( /*OUT*/TargetAssetData, /*OUT*/AssetMenuOptions );

		BuildSingleAssetAddReplaceActorMenu( Section, TargetAssetData, AssetMenuOptions, CreateMode );
	}

	{
		FToolMenuSection& Section = Menu->AddSection("RecentlyPlaced", NSLOCTEXT("LevelViewportContextMenu", "RecentlyPlacedSection", "Recently Placed"));
		if ( IPlacementModeModule::IsAvailable() )
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			const TArray< FActorPlacementInfo > RecentlyPlaced = IPlacementModeModule::Get().GetRecentlyPlaced();
			for (int Index = 0; Index < RecentlyPlaced.Num() && Index < 3; Index++)
			{
				FAssetData Asset = AssetRegistryModule.Get().GetAssetByObjectPath( *RecentlyPlaced[Index].ObjectPath );

				if ( Asset.IsValid() )
				{
					TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
					UActorFactory* Factory = FindObject<UActorFactory>( NULL, *RecentlyPlaced[Index].Factory );

					if ( Factory != NULL )
					{
						AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, Asset ) );
					}
					else
					{
						FActorFactoryAssetProxy::GenerateActorFactoryMenuItems( Asset, &AssetMenuOptions, true );
						while( AssetMenuOptions.Num() > 1 )
						{
							AssetMenuOptions.Pop();
						}
					}

					BuildSingleAssetAddReplaceActorMenu(Section, Asset, AssetMenuOptions, CreateMode);
				}
			}
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Lights", NSLOCTEXT("LevelViewportContextMenu", "LightsSection", "Lights"));
		TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;

		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryDirectionalLight::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}

		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactorySpotLight::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}

		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryPointLight::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}

		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryRectLight::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Primitives", NSLOCTEXT("LevelViewportContextMenu", "PrimitivesSection", "Primitives"));
		TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryCameraActor::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}

		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryPlayerStart::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}

		{
			const UClass* BlockingVolumeClass = ABlockingVolume::StaticClass();
			FAssetData AssetData = FAssetData( BlockingVolumeClass );

			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClassForActorClass( UActorFactorySphereVolume::StaticClass(), BlockingVolumeClass );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );

			Factory = GEditor->FindActorFactoryByClassForActorClass( UActorFactoryBoxVolume::StaticClass(), BlockingVolumeClass );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );

			Factory = GEditor->FindActorFactoryByClassForActorClass( UActorFactoryCylinderVolume::StaticClass(), BlockingVolumeClass );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );
			BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
		}

		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->FindActorFactoryByClass( UActorFactoryTriggerBox::StaticClass() );
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );

			Factory = GEditor->FindActorFactoryByClass( UActorFactoryTriggerSphere::StaticClass() );
			AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );

			Factory = GEditor->FindActorFactoryByClass( UActorFactoryTriggerCapsule::StaticClass() );
			AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
			AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, AssetData ) );

			BuildSingleAssetAddReplaceActorMenu(Section, FAssetData(ATriggerBase::StaticClass()), AssetMenuOptions, CreateMode, NSLOCTEXT("LevelViewportContextMenu", "TriggersGroup", "Trigger"));
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Custom", NSLOCTEXT("LevelViewportContextMenu", "CustomSection", "Custom Actors"));
		TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
		FText UnusedErrorMessage;
		const FAssetData NoAssetData {};
		for ( int32 FactoryIdx = 0; FactoryIdx < GEditor->ActorFactories.Num(); FactoryIdx++ )
		{
			AssetMenuOptions.Empty();
			UActorFactory* Factory = GEditor->ActorFactories[FactoryIdx];
			FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );

			const bool FactoryWorksWithoutAsset = Factory->CanCreateActorFrom( NoAssetData, UnusedErrorMessage );

			if ( FactoryWorksWithoutAsset && Factory->bShowInEditorQuickMenu )
			{
				AssetMenuOptions.Add( FActorFactoryAssetProxy::FMenuItem( Factory, NoAssetData ) );
				BuildSingleAssetAddReplaceActorMenu(Section, AssetData, AssetMenuOptions, CreateMode);
			}
		}
	}
}
