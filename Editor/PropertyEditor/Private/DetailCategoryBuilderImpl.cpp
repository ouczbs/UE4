// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailCategoryBuilderImpl.h"
#include "ObjectPropertyNode.h"
#include "Misc/ConfigCacheIni.h"

#include "DetailCategoryGroupNode.h"
#include "DetailItemNode.h"
#include "DetailAdvancedDropdownNode.h"
#include "DetailPropertyRow.h"
#include "SDetailSingleItemRow.h"
#include "DetailGroup.h"
#include "StructurePropertyNode.h"
#include "ItemPropertyNode.h"
#include "IPropertyGenerationUtilities.h"
#include "DetailBuilderTypes.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/StyleColors.h"

namespace DetailLayoutConstants
{
	// Padding for each layout row
	const FMargin RowPadding(10.0f, 2.0f);
}

void FDetailLayout::AddCustomLayout(const FDetailLayoutCustomization& Layout, bool bAdvanced)
{
	AddLayoutInternal(Layout, bAdvanced ? CustomAdvancedLayouts : CustomSimpleLayouts);
}

void FDetailLayout::AddDefaultLayout(const FDetailLayoutCustomization& Layout, bool bAdvanced)
{
	AddLayoutInternal(Layout, bAdvanced ? DefaultAdvancedLayouts : DefaultSimpleLayouts);
}

FDetailLayoutCustomization* FDetailLayout::GetDefaultLayout(const TSharedRef<FPropertyNode>& PropertyNode)
{
	FDetailLayoutCustomization* Customization = 
		DefaultSimpleLayouts.FindByPredicate([&PropertyNode](const FDetailLayoutCustomization& TestCustomization)
		{
			return TestCustomization.GetPropertyNode() == PropertyNode;
		});

	// Didn't find it in the simple layouts, look in advanced layouts
	if (!Customization)
	{
		Customization = 
			DefaultAdvancedLayouts.FindByPredicate([&PropertyNode](const FDetailLayoutCustomization& TestCustomization)
			{
				return TestCustomization.GetPropertyNode() == PropertyNode;
			});
	}

	return Customization;
}

void FDetailLayout::AddLayoutInternal(const FDetailLayoutCustomization& Layout, FCustomizationList& ListToUse)
{
	ListToUse.Add(Layout);
}

FDetailLayoutCustomization::FDetailLayoutCustomization()
	: PropertyRow(nullptr)
	, WidgetDecl(nullptr)
	, CustomBuilderRow(nullptr)
{

}

bool FDetailLayoutCustomization::HasExternalPropertyRow() const
{
	return HasPropertyNode() && PropertyRow->HasExternalProperty();
}

bool FDetailLayoutCustomization::IsHidden() const
{
	return !IsValidCustomization()
		|| (HasCustomWidget() && WidgetDecl->VisibilityAttr.Get() != EVisibility::Visible)
		|| (HasPropertyNode() && PropertyRow->GetPropertyVisibility() != EVisibility::Visible);
}


TSharedPtr<FPropertyNode> FDetailLayoutCustomization::GetPropertyNode() const
{
	return PropertyRow.IsValid() ? PropertyRow->GetPropertyNode() : nullptr;
}

FDetailWidgetRow FDetailLayoutCustomization::GetWidgetRow() const
{
	if (HasCustomWidget())
	{
		return *WidgetDecl;
	}
	else if (HasCustomBuilder())
	{
		return CustomBuilderRow->GetWidgetRow();
	}
	else if (HasPropertyNode())
	{
		return PropertyRow->GetWidgetRow();
	}
	else
	{
		return DetailGroup->GetWidgetRow();
	}
}

FDetailCategoryImpl::FDetailCategoryImpl(FName InCategoryName, TSharedRef<FDetailLayoutBuilderImpl> InDetailLayout)
	: HeaderContentWidget(nullptr)
	, DetailLayoutBuilder(InDetailLayout)
	, CategoryName(InCategoryName)
	, SortOrder(0)
	, bRestoreExpansionState(!ContainsOnlyAdvanced())
	, bShouldBeInitiallyCollapsed(false)
	, bUserShowAdvanced(false)
	, bForceAdvanced(false)
	, bHasFilterStrings(false)
	, bHasVisibleDetails(true)
	, bIsCategoryVisible(true)
	, bFavoriteCategory(false)
	, bShowOnlyChildren(false)
{
	const UStruct* BaseStruct = InDetailLayout->GetRootNode()->GetBaseStructure();

	static const FName NoCategoryName = TEXT("NoCategory");
	bShowOnlyChildren = (InDetailLayout->IsLayoutForExternalRoot() && !InDetailLayout->GetRootNode()->HasNodeFlags(EPropertyNodeFlags::ShowCategories)) || CategoryName == NoCategoryName;

	// Use the base class name if there is one otherwise this is a generic category not specific to a class
	FName BaseStructName = BaseStruct ? BaseStruct->GetFName() : FName("Generic");

	//Path is separate by '.' so convert category delimiter from '|' to '.'
	FString CategoryDelimiterString;
	CategoryDelimiterString.AppendChar(FPropertyNodeConstants::CategoryDelimiterChar);
	FString CategoryPathDelimiterString;
	CategoryPathDelimiterString.AppendChar(TCHAR('.'));
	CategoryPathName = BaseStructName.ToString() + TEXT(".") + CategoryName.ToString().Replace(*CategoryDelimiterString, *CategoryPathDelimiterString);
	bool bUserShowAdvancedConfigValue = false;
	GConfig->GetBool(TEXT("DetailCategoriesAdvanced"), *CategoryPathName, bUserShowAdvancedConfigValue, GEditorPerProjectIni);

	bUserShowAdvanced = bUserShowAdvancedConfigValue;
}

FDetailCategoryImpl::~FDetailCategoryImpl()
{
}


FDetailWidgetRow& FDetailCategoryImpl::AddCustomRow(const FText& FilterString, bool bForAdvanced)
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.WidgetDecl = MakeShareable(new FDetailWidgetRow);
	NewCustomization.WidgetDecl->FilterString(FilterString);

	IDetailsViewPrivate* DetailsView = GetDetailsView();
	if (DetailsView && DetailsView->IsCustomRowVisibilityFiltered() && !GetDetailsView()->IsCustomRowVisible(FName(*FilterString.ToString()), FName(*DisplayName.ToString())))
	{
		NewCustomization.WidgetDecl->Visibility(TAttribute<EVisibility>(EVisibility::Collapsed));
	}

	AddCustomLayout(NewCustomization, bForAdvanced);

	return *NewCustomization.WidgetDecl;
}


void FDetailCategoryImpl::AddCustomBuilder(TSharedRef<IDetailCustomNodeBuilder> InCustomBuilder, bool bForAdvanced)
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.CustomBuilderRow = MakeShareable(new FDetailCustomBuilderRow(InCustomBuilder));

	AddCustomLayout(NewCustomization, bForAdvanced);
}

IDetailGroup& FDetailCategoryImpl::AddGroup(FName GroupName, const FText& LocalizedDisplayName, bool bForAdvanced, bool bStartExpanded)
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.DetailGroup = MakeShareable(new FDetailGroup(GroupName, AsShared(), LocalizedDisplayName, bStartExpanded));

	AddCustomLayout(NewCustomization, bForAdvanced);

	return *NewCustomization.DetailGroup;
}

int32 FDetailCategoryImpl::GetNumCustomizations() const
{
	int32 NumCustomizations = 0;

	for (int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex)
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		NumCustomizations += Layout.GetDefaultSimpleLayouts().Num();
		NumCustomizations += Layout.GetDefaultAdvancedLayouts().Num();
		NumCustomizations += Layout.GetCustomSimpleLayouts().Num();
		NumCustomizations += Layout.GetCustomAdvancedLayouts().Num();
	}

	return NumCustomizations;
}

void FDetailCategoryImpl::GetDefaultProperties(TArray<TSharedRef<IPropertyHandle> >& OutDefaultProperties, bool bSimpleProperties, bool bAdvancedProperties)
{
	FDetailLayoutBuilderImpl& DetailLayoutBuilderRef = GetParentLayoutImpl();
	for (int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex)
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		if (bSimpleProperties)
		{
			const FCustomizationList& CustomizationList = Layout.GetDefaultSimpleLayouts();

			for (int32 CustomizationIndex = 0; CustomizationIndex < CustomizationList.Num(); ++CustomizationIndex)
			{
				if (CustomizationList[CustomizationIndex].HasPropertyNode())
				{
					const TSharedPtr<FPropertyNode>& Node = CustomizationList[CustomizationIndex].GetPropertyNode();

					TSharedRef<IPropertyHandle> PropertyHandle = DetailLayoutBuilderRef.GetPropertyHandle(Node);

					if (PropertyHandle->IsValidHandle())
					{
						OutDefaultProperties.Add(PropertyHandle);
					}
				}
			}
		}

		if (bAdvancedProperties)
		{
			const FCustomizationList& CustomizationList = Layout.GetDefaultAdvancedLayouts();

			for (int32 CustomizationIndex = 0; CustomizationIndex < CustomizationList.Num(); ++CustomizationIndex)
			{
				if (CustomizationList[CustomizationIndex].HasPropertyNode())
				{
					const TSharedPtr<FPropertyNode>& Node = CustomizationList[CustomizationIndex].GetPropertyNode();

					OutDefaultProperties.Add(DetailLayoutBuilderRef.GetPropertyHandle(Node));
				}
			}
		}
	}
}

void FDetailCategoryImpl::SetCategoryVisibility(bool bIsVisible)
{
	if (bIsVisible != bIsCategoryVisible)
	{
		bIsCategoryVisible = bIsVisible;

		if (GetDetailsView())
		{
			GetDetailsView()->RerunCurrentFilter();
		}
		if (DetailLayoutBuilder.IsValid())
		{
			DetailLayoutBuilder.Pin()->NotifyNodeVisibilityChanged();
		}
	}
}

IDetailCategoryBuilder& FDetailCategoryImpl::InitiallyCollapsed(bool bInShouldBeInitiallyCollapsed)
{
	this->bShouldBeInitiallyCollapsed = bInShouldBeInitiallyCollapsed;
	return *this;
}

IDetailCategoryBuilder& FDetailCategoryImpl::OnExpansionChanged(FOnBooleanValueChanged InOnExpansionChanged)
{
	this->OnExpansionChangedDelegate = InOnExpansionChanged;
	return *this;
}

IDetailCategoryBuilder& FDetailCategoryImpl::RestoreExpansionState(bool bRestore)
{
	this->bRestoreExpansionState = bRestore;
	return *this;
}

IDetailCategoryBuilder& FDetailCategoryImpl::HeaderContent(TSharedRef<SWidget> InHeaderContent)
{
	ensureMsgf(!this->HeaderContentWidget.IsValid(), TEXT("Category already has a header content widget defined!"));
	this->HeaderContentWidget = InHeaderContent;
	return *this;
}

IDetailPropertyRow& FDetailCategoryImpl::AddProperty(FName PropertyPath, UClass* ClassOutermost, FName InstanceName, EPropertyLocation::Type Location)
{
	FDetailLayoutCustomization NewCustomization;
	TSharedPtr<FPropertyNode> PropertyNode = GetParentLayoutImpl().GetPropertyNode(PropertyPath, ClassOutermost, InstanceName);
	if (PropertyNode.IsValid())
	{
		GetParentLayoutImpl().SetCustomProperty(PropertyNode);
	}

	NewCustomization.PropertyRow = MakeShareable(new FDetailPropertyRow(PropertyNode, AsShared()));

	bool bForAdvanced = false;

	if (Location == EPropertyLocation::Default)
	{
		// Get the default location of this property
		bForAdvanced = IsAdvancedLayout(NewCustomization);
	}
	else if (Location == EPropertyLocation::Advanced)
	{
		// Force advanced
		bForAdvanced = true;
	}

	AddCustomLayout(NewCustomization, bForAdvanced);

	return *NewCustomization.PropertyRow;
}

IDetailPropertyRow& FDetailCategoryImpl::AddProperty(TSharedPtr<IPropertyHandle> PropertyHandle, EPropertyLocation::Type Location)
{
	FDetailLayoutCustomization NewCustomization;
	TSharedPtr<FPropertyNode> PropertyNode = GetParentLayoutImpl().GetPropertyNode(PropertyHandle);

	if (PropertyNode.IsValid())
	{
		GetParentLayoutImpl().SetCustomProperty(PropertyNode);
	}

	NewCustomization.PropertyRow = MakeShareable(new FDetailPropertyRow(PropertyNode, AsShared()));

	bool bForAdvanced = false;

	if (Location == EPropertyLocation::Default)
	{
		// Get the default location of this property
		bForAdvanced = IsAdvancedLayout(NewCustomization);
	}
	else if (Location == EPropertyLocation::Advanced)
	{
		// Force advanced
		bForAdvanced = true;
	}

	AddCustomLayout(NewCustomization, bForAdvanced);

	return *NewCustomization.PropertyRow;
}

IDetailPropertyRow* FDetailCategoryImpl::AddExternalObjects(const TArray<UObject*>& Objects, EPropertyLocation::Type Location /*= EPropertyLocation::Default*/, const FAddPropertyParams& Params /*= FAddPropertyParams()*/)
{
	FDetailLayoutCustomization NewCustomization;

	FAddPropertyParams AddPropertyParams = Params;
	AddPropertyParams.AllowChildren(true);

	FDetailPropertyRow::MakeExternalPropertyRowCustomization(Objects, NAME_None, AsShared(), NewCustomization, AddPropertyParams);

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;

	if (NewRow.IsValid())
	{
		bool bForAdvanced = false;
		if (Location == EPropertyLocation::Advanced)
		{
			// Force advanced
			bForAdvanced = true;
		}

		AddCustomLayout(NewCustomization, bForAdvanced);
	}

	return NewRow.Get();
}

IDetailPropertyRow* FDetailCategoryImpl::AddExternalObjectProperty(const TArray<UObject*>& Objects, FName PropertyName, EPropertyLocation::Type Location, const FAddPropertyParams& Params)
{
	FDetailLayoutCustomization NewCustomization;

	FDetailPropertyRow::MakeExternalPropertyRowCustomization(Objects, PropertyName, AsShared(), NewCustomization, Params);

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;

	if (NewRow.IsValid())
	{
		bool bForAdvanced = false;
		if (Location == EPropertyLocation::Advanced)
		{
			// Force advanced
			bForAdvanced = true;
		}

		AddCustomLayout(NewCustomization, bForAdvanced);

	}

	return NewRow.Get();
}

IDetailPropertyRow* FDetailCategoryImpl::AddExternalStructure(TSharedPtr<FStructOnScope> StructData, EPropertyLocation::Type Location /*= EPropertyLocation::Default*/)
{
	return AddExternalStructureProperty(StructData, NAME_None, Location);
}

IDetailPropertyRow* FDetailCategoryImpl::AddExternalStructureProperty(TSharedPtr<FStructOnScope> StructData, FName PropertyName, EPropertyLocation::Type Location/* = EPropertyLocation::Default*/, const FAddPropertyParams& Params)
{
	FDetailLayoutCustomization NewCustomization;

	FDetailPropertyRow::MakeExternalPropertyRowCustomization(StructData, PropertyName, AsShared(), NewCustomization, Params);

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;

	if (NewRow.IsValid())
	{
		TSharedPtr<FPropertyNode> PropertyNode = NewRow->GetPropertyNode();
		TSharedPtr<FComplexPropertyNode> RootNode = StaticCastSharedRef<FComplexPropertyNode>(PropertyNode->FindComplexParent()->AsShared());

		bool bForAdvanced = false;
		if (Location == EPropertyLocation::Advanced)
		{
			// Force advanced
			bForAdvanced = true;
		}

		AddCustomLayout(NewCustomization, bForAdvanced);
	}

	return NewRow.Get();
}

TArray<TSharedPtr<IPropertyHandle>> FDetailCategoryImpl::AddAllExternalStructureProperties(TSharedRef<FStructOnScope> StructData, EPropertyLocation::Type Location)
{
	TSharedPtr<FStructurePropertyNode> RootPropertyNode(new FStructurePropertyNode);
	RootPropertyNode->SetStructure(StructData);

	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = nullptr;
	InitParams.Property = nullptr;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	InitParams.bAllowChildren = false;
	InitParams.bForceHiddenPropertyVisibility = FPropertySettings::Get().ShowHiddenProperties();
	InitParams.bCreateCategoryNodes = false;

	RootPropertyNode->InitNode(InitParams);

	TArray<TSharedPtr<IPropertyHandle>> Handles;

	FDetailLayoutBuilderImpl& DetailLayoutBuilderRef = GetParentLayoutImpl();

	const bool bForAdvanced = Location == EPropertyLocation::Advanced;
	if (RootPropertyNode.IsValid())
	{
		RootPropertyNode->RebuildChildren();
		DetailLayoutBuilder.Pin()->AddExternalRootPropertyNode(RootPropertyNode.ToSharedRef());

		for (int32 ChildIdx = 0; ChildIdx < RootPropertyNode->GetNumChildNodes(); ++ChildIdx)
		{
			TSharedPtr< FPropertyNode > PropertyNode = RootPropertyNode->GetChildNode(ChildIdx);
			if (FProperty* Property = PropertyNode->GetProperty())
			{
				FDetailLayoutCustomization NewCustomization;
				NewCustomization.PropertyRow = MakeShared<FDetailPropertyRow>(PropertyNode, AsShared(), RootPropertyNode);
				AddDefaultLayout(NewCustomization, bForAdvanced, NAME_None);

				Handles.Add(DetailLayoutBuilderRef.GetPropertyHandle(PropertyNode));
			}
		}
	}

	return Handles;
}

void FDetailCategoryImpl::AddPropertyNode(TSharedRef<FPropertyNode> PropertyNode, FName InstanceName)
{
	FDetailLayoutCustomization NewCustomization;

	NewCustomization.PropertyRow = MakeShareable(new FDetailPropertyRow(PropertyNode, AsShared()));
	AddDefaultLayout(NewCustomization, IsAdvancedLayout(NewCustomization), InstanceName);
}

bool FDetailCategoryImpl::IsAdvancedLayout(const FDetailLayoutCustomization& LayoutInfo)
{
	bool bAdvanced = false;
	if (LayoutInfo.PropertyRow.IsValid() && LayoutInfo.GetPropertyNode().IsValid() && LayoutInfo.GetPropertyNode()->HasNodeFlags(EPropertyNodeFlags::IsAdvanced))
	{
		bAdvanced = true;
	}

	return bAdvanced;
}

void FDetailCategoryImpl::AddCustomLayout(const FDetailLayoutCustomization& LayoutInfo, bool bForAdvanced)
{
	GetLayoutForInstance(GetParentLayoutImpl().GetCurrentCustomizationVariableName()).AddCustomLayout(LayoutInfo, bForAdvanced);
}

void FDetailCategoryImpl::AddDefaultLayout(const FDetailLayoutCustomization& LayoutInfo, bool bForAdvanced, FName InstanceName)
{
	GetLayoutForInstance(InstanceName).AddDefaultLayout(LayoutInfo, bForAdvanced);
}

FDetailLayout& FDetailCategoryImpl::GetLayoutForInstance(FName InstanceName)
{
	return LayoutMap.FindOrAdd(InstanceName);
}

void FDetailCategoryImpl::OnAdvancedDropdownClicked()
{
	bUserShowAdvanced = !bUserShowAdvanced;

	GConfig->SetBool(TEXT("DetailCategoriesAdvanced"), *CategoryPathName, bUserShowAdvanced, GEditorPerProjectIni);

	const bool bRefilterCategory = true;
	RefreshTree(bRefilterCategory);
}

FDetailLayoutCustomization* FDetailCategoryImpl::GetDefaultCustomization(TSharedRef<FPropertyNode> PropertyNode)
{
	FDetailLayout& Layout = GetLayoutForInstance(GetParentLayoutImpl().GetCurrentCustomizationVariableName());
	
	FDetailLayoutCustomization* Customization = Layout.GetDefaultLayout(PropertyNode);
	if (Customization)
	{
		return Customization;
	}
	return nullptr;
}

bool FDetailCategoryImpl::ShouldShowAdvanced() const
{
	return bUserShowAdvanced || bForceAdvanced;
}

void FDetailCategoryImpl::SetShowAdvanced(bool bShowAdvanced)
{
	bUserShowAdvanced = bShowAdvanced;
}

int32 FDetailCategoryImpl::GetSortOrder() const
{
	return SortOrder;
}

void FDetailCategoryImpl::SetSortOrder(int32 InSortOrder)
{
	SortOrder = InSortOrder;
}

bool FDetailCategoryImpl::IsAdvancedDropdownEnabled() const
{
	return !bForceAdvanced;
}

void FDetailCategoryImpl::RequestItemExpanded(TSharedRef<FDetailTreeNode> TreeNode, bool bShouldBeExpanded)
{
	if (GetDetailsView())
	{		
		GetDetailsView()->RequestItemExpanded(TreeNode, bShouldBeExpanded);
	}
}

void FDetailCategoryImpl::RefreshTree(bool bRefilterCategory)
{
	if (bRefilterCategory)
	{
		TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
		if (DetailLayoutBuilderPtr.IsValid())
		{
			FilterNode(DetailLayoutBuilderPtr->GetCurrentFilter());
			DetailLayoutBuilderPtr->GetPropertyGenerationUtilities().RebuildTreeNodes();
		}
	}
	else
	{
		if (GetDetailsView())
		{
			GetDetailsView()->RefreshTree();
		}
	}
}

void FDetailCategoryImpl::AddTickableNode(FDetailTreeNode& TickableNode)
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if (DetailLayoutBuilderPtr.IsValid())
	{
		DetailLayoutBuilderPtr->AddTickableNode(TickableNode);
	}
}

void FDetailCategoryImpl::RemoveTickableNode(FDetailTreeNode& TickableNode)
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if (DetailLayoutBuilderPtr.IsValid())
	{
		DetailLayoutBuilderPtr->RemoveTickableNode(TickableNode);
	}
}

void FDetailCategoryImpl::SaveExpansionState(FDetailTreeNode& InTreeNode)
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if (DetailLayoutBuilderPtr.IsValid())
	{
		bool bIsExpanded = InTreeNode.ShouldBeExpanded();

		FString Key = CategoryPathName;
		Key += TEXT(".");
		Key += InTreeNode.GetNodeName().ToString();

		DetailLayoutBuilderPtr->SaveExpansionState(Key, bIsExpanded);
	}
}

bool FDetailCategoryImpl::GetSavedExpansionState(FDetailTreeNode& InTreeNode) const
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if (DetailLayoutBuilderPtr.IsValid())
	{
		FString Key = CategoryPathName;
		Key += TEXT(".");
		Key += InTreeNode.GetNodeName().ToString();

		return DetailLayoutBuilderPtr->GetSavedExpansionState(Key);
	}

	return false;
}

bool FDetailCategoryImpl::ContainsOnlyAdvanced() const
{ 
	return !bFavoriteCategory && SimpleChildNodes.Num() == 0 && AdvancedChildNodes.Num() > 0;
}

void FDetailCategoryImpl::SetDisplayName(FName InCategoryName, const FText& LocalizedNameOverride)
{
	if (!LocalizedNameOverride.IsEmpty())
	{
		DisplayName = LocalizedNameOverride;
	}
	else if (InCategoryName != NAME_None)
	{
		static const FTextKey CategoryLocalizationNamespace = TEXT("UObjectCategory");
		static const FName CategoryMetaDataKey = TEXT("Category");

		DisplayName = FText();

		const FString NativeCategory = InCategoryName.ToString();
		if (FText::FindText(CategoryLocalizationNamespace, NativeCategory, /*OUT*/DisplayName, &NativeCategory))
		{
			// Category names in English are typically gathered in their non-pretty form (eg "UserInterface" rather than "User Interface"), so skip 
			// applying the localized variant if the text matches the raw category name, as in this case the pretty printer will do a better job
			if (NativeCategory.Equals(DisplayName.ToString(), ESearchCase::CaseSensitive))
			{
				DisplayName = FText();
			}
		}
		
		if (DisplayName.IsEmpty())
		{
			DisplayName = FText::AsCultureInvariant(FName::NameToDisplayString(NativeCategory, false));
		}
	}
	else
	{
		// Use the base class name if there is one otherwise this is a generic category not specific to a class
		const UStruct* BaseStruct = GetParentLayoutImpl().GetRootNode()->GetBaseStructure();
		if (BaseStruct)
		{
			DisplayName = BaseStruct->GetDisplayNameText();
		}
		else
		{
			DisplayName = NSLOCTEXT("DetailCategory", "GenericCategory", "Generic");
		}
	}
}

IDetailsViewPrivate* FDetailCategoryImpl::GetDetailsView() const
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayoutBuilderPtr = DetailLayoutBuilder.Pin();
	if (DetailLayoutBuilderPtr.IsValid())
	{
		return DetailLayoutBuilderPtr->GetDetailsView();
	}

	return nullptr;
}

TSharedRef<ITableRow> FDetailCategoryImpl::GenerateWidgetForTableView(const TSharedRef<STableViewBase>& OwnerTable, bool bAllowFavoriteSystem)
{
	TSharedPtr<SWidget> HeaderContent = HeaderContentWidget;
	if (InlinePropertyNode.IsValid())
	{
		FDetailWidgetRow Row;
		InlinePropertyNode->GenerateStandaloneWidget(Row);
		HeaderContent = Row.ValueWidget.Widget;
	}

	return SNew(SDetailCategoryTableRow, AsShared(), OwnerTable)
		.InnerCategory(DetailLayoutBuilder.IsValid() ? DetailLayoutBuilder.Pin()->IsLayoutForExternalRoot() : false)
		.DisplayName(GetDisplayName())
		.HeaderContent(HeaderContent);
}


bool FDetailCategoryImpl::GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const
{
	TSharedPtr<SWidget> HeaderContent = HeaderContentWidget;
	if (InlinePropertyNode.IsValid())
	{
		FDetailWidgetRow Row;
		InlinePropertyNode->GenerateStandaloneWidget(Row);
		HeaderContent = Row.ValueWidget.Widget;
	}

	const bool bIsInnerCategory = DetailLayoutBuilder.Pin()->IsLayoutForExternalRoot();
	FTextBlockStyle NameStyle = bIsInnerCategory ? FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText") : FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("DetailsView.CategoryTextStyleUpdate");
	OutRow.NameContent()
	[
		SNew(STextBlock)
		.Text(GetDisplayName())
		.TextStyle(&NameStyle)
		.ShadowOffset(FVector2D::ZeroVector)
	];

	if(HeaderContentWidget.IsValid())
	{
		OutRow
			.ValueContent()
			.HAlign(HAlign_Fill)
			[
				HeaderContent.ToSharedRef()
			];
	}

	return true;
}

void FDetailCategoryImpl::GetFilterStrings(TArray<FString>& OutFilterStrings) const
{
	OutFilterStrings.Add(GetDisplayName().ToString());
}

void FDetailCategoryImpl::OnItemExpansionChanged(bool bIsExpanded, bool bShouldSaveState)
{
	if (bRestoreExpansionState && bShouldSaveState)
	{
		// Save the collapsed state of this section
		GConfig->SetBool(TEXT("DetailCategories"), *CategoryPathName, bIsExpanded, GEditorPerProjectIni);
	}

	OnExpansionChangedDelegate.ExecuteIfBound(bIsExpanded);
}

bool FDetailCategoryImpl::ShouldBeExpanded() const
{
	if (bHasFilterStrings)
	{
		return true;
	}
	else if (bRestoreExpansionState)
	{
		// Collapse by default if there are no simple child nodes
		bool bShouldBeExpanded = !ContainsOnlyAdvanced() && !bShouldBeInitiallyCollapsed;
		// Save the collapsed state of this section
		GConfig->GetBool(TEXT("DetailCategories"), *CategoryPathName, bShouldBeExpanded, GEditorPerProjectIni);
		return bShouldBeExpanded;
	}
	else
	{
		return !bShouldBeInitiallyCollapsed;
	}
}

ENodeVisibility FDetailCategoryImpl::GetVisibility() const
{
	return bHasVisibleDetails && bIsCategoryVisible ? 
		ENodeVisibility::Visible : ENodeVisibility::ForcedHidden;
}

static bool IsCustomProperty(const TSharedPtr<FPropertyNode>& PropertyNode)
{
	// The property node is custom if it has a custom layout or if its a struct and any of its children have a custom layout
	bool bIsCustom = !PropertyNode.IsValid() || PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsCustomized) != 0;

	return bIsCustom;
}

static bool ShouldBeInlineNode(const TSharedRef<FDetailItemNode>& Node)
{
	TSharedPtr<FPropertyNode> PropertyNode = Node->GetPropertyNode();
	if (PropertyNode.IsValid())
	{
		const FProperty* Property = PropertyNode->GetProperty();
		if (Property != nullptr)
		{
			const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
			const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
			const FByteProperty* ByteProperty = CastField<FByteProperty>(Property);

			// Only allow bools and enums as inline nodes.
			if (BoolProperty != nullptr || EnumProperty != nullptr ||
				(ByteProperty != nullptr && ByteProperty->IsEnum()))
			{
				static const FName Name_InlineCategoryProperty("InlineCategoryProperty");
				if (Property->HasMetaData(Name_InlineCategoryProperty))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FDetailCategoryImpl::GenerateNodesFromCustomizations(const FCustomizationList& InCustomizationList, bool bDefaultLayouts, FDetailNodeList& OutNodeList, bool &bOutLastItemHasMultipleColumns)
{
	TAttribute<bool> IsParentEnabled(this, &FDetailCategoryImpl::IsParentEnabled);

	bOutLastItemHasMultipleColumns = false;
	for (int32 CustomizationIndex = 0; CustomizationIndex < InCustomizationList.Num(); ++CustomizationIndex)
	{
		const FDetailLayoutCustomization& Customization = InCustomizationList[CustomizationIndex];
		// When building default layouts cull default properties which have been customized
		if (bFavoriteCategory || (Customization.IsValidCustomization() && (!bDefaultLayouts || !IsCustomProperty(Customization.GetPropertyNode()))))
		{
			TSharedRef<FDetailItemNode> NewNode = MakeShareable(new FDetailItemNode(Customization, AsShared(), IsParentEnabled));
			NewNode->Initialize();

			if (ShouldBeInlineNode(NewNode))
			{
				ensureMsgf(!InlinePropertyNode.IsValid(), TEXT("Multiple properties marked InlineCategoryProperty detected in category %s."), *DisplayName.ToString());
				InlinePropertyNode = NewNode;
				continue;
			}

			// Add the node unless only its children should be visible or it didnt generate any children or if it is a custom builder which can generate children at any point
			if (!NewNode->ShouldShowOnlyChildren() || NewNode->HasGeneratedChildren() || Customization.HasCustomBuilder())
			{
				if (CustomizationIndex == InCustomizationList.Num() - 1)
				{
					bOutLastItemHasMultipleColumns = NewNode->HasMultiColumnWidget();
				}

				OutNodeList.Add(NewNode);
			}
		}
	}
}


bool FDetailCategoryImpl::GenerateChildrenForSingleLayout(const FName RequiredGroupName, bool bDefaultLayout, bool bNeedsGroup, const FCustomizationList& LayoutList, FDetailNodeList& OutChildren, bool& bOutLastItemHasMultipleColumns)
{
	bool bGeneratedAnyChildren = false;
	if (LayoutList.Num() > 0)
	{
		FDetailNodeList GeneratedChildren;
		GenerateNodesFromCustomizations(LayoutList, bDefaultLayout, GeneratedChildren, bOutLastItemHasMultipleColumns);

		if (GeneratedChildren.Num() > 0)
		{
			bGeneratedAnyChildren = true;
			if (bNeedsGroup)
			{
				TSharedRef<FDetailTreeNode> GroupNode = MakeShareable(new FDetailCategoryGroupNode(GeneratedChildren, RequiredGroupName, *this));
				OutChildren.Add(GroupNode);
			}
			else
			{
				OutChildren.Append(GeneratedChildren);
			}
		}
	}

	return bGeneratedAnyChildren;
}

void FDetailCategoryImpl::GenerateChildrenForLayouts()
{
	bool bHasAdvancedLayouts = false;
	bool bGeneratedAnyChildren = false;

	bool bLastItemHasMultipleColumns = false;
	// @todo Details each loop can be a function
	for (int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex)
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		const FName RequiredGroupName = Layout.GetInstanceName();

		bHasAdvancedLayouts |= Layout.HasAdvancedLayouts();

		const bool bDefaultLayout = false;
		const bool bShouldShowGroup = LayoutMap.ShouldShowGroup(RequiredGroupName);
		bGeneratedAnyChildren |= GenerateChildrenForSingleLayout(RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetCustomSimpleLayouts(), SimpleChildNodes, bLastItemHasMultipleColumns);
	}

	for (int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex)
	{
		const FDetailLayout& Layout = LayoutMap[LayoutIndex];

		const FName RequiredGroupName = Layout.GetInstanceName();

		const bool bDefaultLayout = true;
		const bool bShouldShowGroup = LayoutMap.ShouldShowGroup(RequiredGroupName);
		bGeneratedAnyChildren |= GenerateChildrenForSingleLayout(RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetDefaultSimpleLayouts(), SimpleChildNodes, bLastItemHasMultipleColumns);
	}

	TAttribute<bool> ShowAdvanced(this, &FDetailCategoryImpl::ShouldShowAdvanced);
	TAttribute<bool> IsEnabled(this, &FDetailCategoryImpl::IsAdvancedDropdownEnabled);
	if (bHasAdvancedLayouts)
	{
		for (int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex)
		{
			const FDetailLayout& Layout = LayoutMap[LayoutIndex];

			const FName RequiredGroupName = Layout.GetInstanceName();

			const bool bDefaultLayout = false;
			const bool bShouldShowGroup = LayoutMap.ShouldShowGroup(RequiredGroupName);
			bGeneratedAnyChildren |= GenerateChildrenForSingleLayout(RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetCustomAdvancedLayouts(), AdvancedChildNodes, bLastItemHasMultipleColumns);
		}

		for (int32 LayoutIndex = 0; LayoutIndex < LayoutMap.Num(); ++LayoutIndex)
		{
			const FDetailLayout& Layout = LayoutMap[LayoutIndex];

			const FName RequiredGroupName = Layout.GetInstanceName();

			const bool bDefaultLayout = true;
			const bool bShouldShowGroup = LayoutMap.ShouldShowGroup(RequiredGroupName);
			bGeneratedAnyChildren |= GenerateChildrenForSingleLayout(RequiredGroupName, bDefaultLayout, bShouldShowGroup, Layout.GetDefaultAdvancedLayouts(), AdvancedChildNodes, bLastItemHasMultipleColumns);
		}
	}

	// Generate nodes for advanced dropdowns
	{
		if (AdvancedChildNodes.Num() > 0)
		{
			AdvancedDropdownNodeTop = MakeShareable(new FAdvancedDropdownNode(*this, true));
		}

		AdvancedDropdownNodeBottom = MakeShareable(new FAdvancedDropdownNode(*this, ShowAdvanced, IsEnabled, AdvancedChildNodes.Num() > 0, SimpleChildNodes.Num() == 0));
	}
}


void FDetailCategoryImpl::GetChildren(FDetailNodeList& OutChildren)
{
	GetGeneratedChildren(OutChildren, false, false);
}

void FDetailCategoryImpl::GetGeneratedChildren(FDetailNodeList& OutChildren, bool bIgnoreVisibility, bool bIgnoreAdvancedDropdown)
{
	for (TSharedRef<FDetailTreeNode>& Child : SimpleChildNodes)
	{
		if (bIgnoreVisibility || Child->GetVisibility() == ENodeVisibility::Visible)
		{
			if (Child->ShouldShowOnlyChildren())
			{
				Child->GetChildren(OutChildren);
			}
			else
			{
				OutChildren.Add(Child);
			}
		}
	}

	if (!bIgnoreAdvancedDropdown && ShouldShowAdvanced())
	{
		if (AdvancedDropdownNodeTop.IsValid())
		{
			OutChildren.Add(AdvancedDropdownNodeTop.ToSharedRef());
		}

		for (TSharedRef<FDetailTreeNode>& Child : AdvancedChildNodes)
		{
			if (bIgnoreVisibility || Child->GetVisibility() == ENodeVisibility::Visible)
			{
				if (Child->ShouldShowOnlyChildren())
				{
					Child->GetChildren(OutChildren);
				}
				else
				{
					OutChildren.Add(Child);
				}
			}
		}
	}

	if (!bIgnoreAdvancedDropdown && AdvancedDropdownNodeBottom.IsValid())
	{
		OutChildren.Add(AdvancedDropdownNodeBottom.ToSharedRef());
	}
}

void FDetailCategoryImpl::FilterNode(const FDetailFilter& InFilter)
{
	bHasFilterStrings = InFilter.FilterStrings.Num() > 0;
	bForceAdvanced = bFavoriteCategory || bHasFilterStrings || InFilter.bShowAllAdvanced == true || ContainsOnlyAdvanced();

	bHasVisibleDetails = false;

	if (bFavoriteCategory && !InFilter.bShowFavoritesCategory)
	{
		return;
	}

	if (InlinePropertyNode.IsValid())
	{
		bHasVisibleDetails = true;
	}

	for (TSharedRef<FDetailTreeNode>& Child : SimpleChildNodes)
	{
		Child->FilterNode(InFilter);

		if (Child->GetVisibility() == ENodeVisibility::Visible)
		{
			bHasVisibleDetails = true;
			RequestItemExpanded(Child, Child->ShouldBeExpanded());
		}
	}

	for (TSharedRef<FDetailTreeNode>& Child : AdvancedChildNodes)
	{
		Child->FilterNode(InFilter);

		if (Child->GetVisibility() == ENodeVisibility::Visible)
		{
			bHasVisibleDetails = true;
			RequestItemExpanded(Child, Child->ShouldBeExpanded());
		}
	}
}

FCustomPropertyTypeLayoutMap FDetailCategoryImpl::GetCustomPropertyTypeLayoutMap() const
{
	if (DetailLayoutBuilder.IsValid())
	{
		return DetailLayoutBuilder.Pin()->GetInstancedPropertyTypeLayoutMap();
	}

	return FCustomPropertyTypeLayoutMap();
}

void FDetailCategoryImpl::GenerateLayout()
{
	// Reset all children
	SimpleChildNodes.Empty();
	AdvancedChildNodes.Empty();
	AdvancedDropdownNodeTop.Reset();
	AdvancedDropdownNodeBottom.Reset();
	InlinePropertyNode.Reset();

	GenerateChildrenForLayouts();

	bHasVisibleDetails = SimpleChildNodes.Num() + AdvancedChildNodes.Num() > 0;
}

bool FDetailCategoryImpl::IsParentEnabled() const
{
	IDetailsViewPrivate* DetailsView = GetDetailsView();
	return !DetailsView || DetailsView->IsPropertyEditingEnabled();
}
