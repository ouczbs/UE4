// Copyright Epic Games, Inc. All Rights Reserved.

#include "StatsNodeHelper.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "StatsNode"

////////////////////////////////////////////////////////////////////////////////////////////////////
// StatsNode Type Helper
////////////////////////////////////////////////////////////////////////////////////////////////////

FText StatsNodeTypeHelper::ToText(const EStatsNodeType NodeType)
{
	static_assert(static_cast<int>(EStatsNodeType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case EStatsNodeType::Counter:	return LOCTEXT("Stats_Name_Counter", "Counter");
		case EStatsNodeType::Stat:		return LOCTEXT("Stats_Name_Stat", "Stat");
		case EStatsNodeType::Group:		return LOCTEXT("Stats_Name_Group", "Group");
		default:						return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText StatsNodeTypeHelper::ToDescription(const EStatsNodeType NodeType)
{
	static_assert(static_cast<int>(EStatsNodeType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case EStatsNodeType::Counter:	return LOCTEXT("Stats_Desc_Counter", "Counter node");
		case EStatsNodeType::Stat:		return LOCTEXT("Stats_Desc_Stat", "UE4 stat node");
		case EStatsNodeType::Group:		return LOCTEXT("Stats_Desc_Group", "Group node");
		default:						return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName StatsNodeTypeHelper::ToBrushName(const EStatsNodeType NodeType)
{
	static_assert(static_cast<int>(EStatsNodeType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case EStatsNodeType::Counter:	return TEXT("Profiler.FiltersAndPresets.StatTypeIcon");
		case EStatsNodeType::Stat:		return TEXT("Profiler.FiltersAndPresets.StatTypeIcon");
		case EStatsNodeType::Group:		return TEXT("Profiler.Misc.GenericGroup");
		default:						return NAME_None;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* StatsNodeTypeHelper::GetIconForGroup()
{
	return FEditorStyle::GetBrush(TEXT("Profiler.Misc.GenericGroup")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.GenericGroup"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* StatsNodeTypeHelper::GetIcon(const EStatsNodeType NodeType)
{
	static_assert(static_cast<int>(EStatsNodeType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case EStatsNodeType::Counter:	return FEditorStyle::GetBrush(TEXT("Profiler.FiltersAndPresets.StatTypeIcon"));
		case EStatsNodeType::Stat:		return FEditorStyle::GetBrush(TEXT("Profiler.FiltersAndPresets.StatTypeIcon"));
		case EStatsNodeType::Group:		return FEditorStyle::GetBrush(TEXT("Profiler.Misc.GenericGroup"));
		default:						return nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// StatsNode DataType Helper
////////////////////////////////////////////////////////////////////////////////////////////////////

FText StatsNodeDataTypeHelper::ToText(const EStatsNodeDataType DataType)
{
	static_assert(static_cast<int>(EStatsNodeDataType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (DataType)
	{
	case EStatsNodeDataType::Double:	return LOCTEXT("Stats_Name_Double", "Double");
	case EStatsNodeDataType::Int64:		return LOCTEXT("Stats_Name_Int64", "Int64");
	case EStatsNodeDataType::Undefined:	return LOCTEXT("Stats_Name_Undefined", "Undefined");
	default:							return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText StatsNodeDataTypeHelper::ToDescription(const EStatsNodeDataType DataType)
{
	static_assert(static_cast<int>(EStatsNodeDataType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (DataType)
	{
	case EStatsNodeDataType::Double:	return LOCTEXT("Stats_Desc_Double", "Data type for counter values is Double.");
	case EStatsNodeDataType::Int64:		return LOCTEXT("Stats_Desc_Int64", "Data type for counter values is Int64.");
	case EStatsNodeDataType::Undefined:	return LOCTEXT("Stats_Desc_Undefined", "Data type for counter values is undefined (ex.: a mix of Double and Int64).");
	default:							return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName StatsNodeDataTypeHelper::ToBrushName(const EStatsNodeDataType DataType)
{
	static_assert(static_cast<int>(EStatsNodeDataType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (DataType)
	{
	case EStatsNodeDataType::Double:	return TEXT("Profiler.Type.NumberFloat");
	case EStatsNodeDataType::Int64:		return TEXT("Profiler.Type.NumberInt");
	case EStatsNodeDataType::Undefined:	return TEXT("Profiler.Type.NumberFloat");
	default:							return NAME_None;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* StatsNodeDataTypeHelper::GetIcon(const EStatsNodeDataType DataType)
{
	static_assert(static_cast<int>(EStatsNodeDataType::InvalidOrMax) == 3, "Not all cases are handled in switch below!?");
	switch (DataType)
	{
	case EStatsNodeDataType::Double:	return FEditorStyle::GetBrush(TEXT("Profiler.Type.NumberFloat")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.StatsType.Float"));
	case EStatsNodeDataType::Int64:		return FEditorStyle::GetBrush(TEXT("Profiler.Type.NumberInt")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.StatsType.Int64"));
	case EStatsNodeDataType::Undefined:	return FEditorStyle::GetBrush(TEXT("Profiler.Type.NumberFloat")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.StatsType.Float"));
	default:							return nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// StatsNode Grouping Helper
////////////////////////////////////////////////////////////////////////////////////////////////////

FText StatsNodeGroupingHelper::ToText(const EStatsGroupingMode GroupingMode)
{
	static_assert(static_cast<int>(EStatsGroupingMode::InvalidOrMax) == 6, "Not all cases are handled in switch below!?");
	switch (GroupingMode)
	{
		case EStatsGroupingMode::Flat:				return LOCTEXT("Grouping_Name_Flat",			"Flat");
		case EStatsGroupingMode::ByName:			return LOCTEXT("Grouping_Name_ByName",			"Stats Name");
		case EStatsGroupingMode::ByMetaGroupName:	return LOCTEXT("Grouping_Name_MetaGroupName",	"Meta Group Name");
		case EStatsGroupingMode::ByType:			return LOCTEXT("Grouping_Name_Type",			"Counter Type");
		case EStatsGroupingMode::ByDataType:		return LOCTEXT("Grouping_Name_DataType",		"Data Type");
		case EStatsGroupingMode::ByCount:			return LOCTEXT("Grouping_Name_Count",			"Count");
		default:									return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText StatsNodeGroupingHelper::ToDescription(const EStatsGroupingMode GroupingMode)
{
	static_assert(static_cast<int>(EStatsGroupingMode::InvalidOrMax) == 6, "Not all cases are handled in switch below!?");
	switch (GroupingMode)
	{
		case EStatsGroupingMode::Flat:				return LOCTEXT("Grouping_Desc_Flat",			"Creates a single group. Includes all counters.");
		case EStatsGroupingMode::ByName:			return LOCTEXT("Grouping_Desc_ByName",			"Creates one group for one letter.");
		case EStatsGroupingMode::ByMetaGroupName:	return LOCTEXT("Grouping_Desc_MetaGroupName",	"Creates groups based on metadata group names of counters.");
		case EStatsGroupingMode::ByType:			return LOCTEXT("Grouping_Desc_Type",			"Creates one group for each counter type.");
		case EStatsGroupingMode::ByDataType:		return LOCTEXT("Grouping_Desc_DataType",		"Creates one group for each data type.");
		case EStatsGroupingMode::ByCount:			return LOCTEXT("Grouping_Desc_Count",			"Creates one group for each logarithmic range ie. 0, [1 .. 10), [10 .. 100), [100 .. 1K), etc.");
		default:									return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName StatsNodeGroupingHelper::ToBrushName(const EStatsGroupingMode GroupingMode)
{
	static_assert(static_cast<int>(EStatsGroupingMode::InvalidOrMax) == 6, "Not all cases are handled in switch below!?");
	switch (GroupingMode)
	{
		case EStatsGroupingMode::Flat:				return TEXT("Profiler.FiltersAndPresets.GroupNameIcon"); //TODO: "Icons.Grouping.Flat"
		case EStatsGroupingMode::ByName:			return TEXT("Profiler.FiltersAndPresets.GroupNameIcon"); //TODO: "Icons.Grouping.ByName"
		case EStatsGroupingMode::ByMetaGroupName:	return TEXT("Profiler.FiltersAndPresets.StatNameIcon"); //TODO
		case EStatsGroupingMode::ByType:			return TEXT("Profiler.FiltersAndPresets.StatTypeIcon"); //TODO
		case EStatsGroupingMode::ByDataType:		return TEXT("Profiler.FiltersAndPresets.StatTypeIcon"); //TODO
		case EStatsGroupingMode::ByCount:			return TEXT("Profiler.FiltersAndPresets.StatValueIcon"); //TODO
		default:									return NAME_None;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
