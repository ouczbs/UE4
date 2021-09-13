// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"
#include "Containers/UnrealString.h"
#include "Model/NetProfilerProvider.h"

namespace TraceServices
{

class IAnalysisSession;

class FNetTraceAnalyzer
	: public UE::Trace::IAnalyzer
{
public:
	FNetTraceAnalyzer(IAnalysisSession& Session, FNetProfilerProvider& NetProfilerProvider);
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;
	virtual void OnAnalysisEnd() override;

private:

	enum : uint16
	{
		RouteId_InitEvent,
		RouteId_InstanceDestroyedEvent,
		RouteId_NameEvent,
		RouteId_PacketContentEvent,
		RouteId_PacketEvent,
		RouteId_PacketDroppedEvent,
		RouteId_ConnectionCreatedEvent,
		RouteId_ConnectionClosedEvent,
		RouteId_ObjectCreatedEvent,
		RouteId_ObjectDestroyedEvent,
	};

	// This must be kept in sync with Event types in NetTrace.h
	enum class EContentEventType : uint8
	{
		Object = 0,
		NameId = 1,
		BunchEvent = 2,
		BunchHeaderEvent = 3,
	};

	IAnalysisSession& Session;
	FNetProfilerProvider& NetProfilerProvider;
	uint32 NetTraceVersion;
	uint32 NetTraceReporterVersion;
	uint32 BunchHeaderNameIndex;

	// Shared for trace
	TMap<uint16, uint32> TracedNameIdToNetProfilerNameIdMap;

	TMap<uint32, uint32> TraceEventTypeToNetProfilerEventTypeIndexMap;

	struct FBunchInfo
	{
		FNetProfilerBunchInfo BunchInfo;
		uint32 BunchBits;
		uint32 HeaderBits;
		int32 FirstBunchEventIndex;
		uint32 EventCount;
		uint16 NameIndex;
	};

	struct FNetTraceConnectionState
	{
		// Index into persistent connection array
		uint32 ConnectionIndex;

		// Current packet data
		uint32 CurrentPacketStartIndex[ENetProfilerConnectionMode::Count];
		uint32 CurrentPacketBitOffset[ENetProfilerConnectionMode::Count];

		// Current bunch data
		TArray<FNetProfilerContentEvent> BunchEvents[ENetProfilerConnectionMode::Count];

		TArray<FBunchInfo> BunchInfos[ENetProfilerConnectionMode::Count];
	};

	struct FNetTraceActiveObjectState
	{
		uint32 ObjectIndex;
		uint32 NameIndex;
	};

	// Map from instance id to index in the persistent instance array
	struct FNetTraceGameInstanceState
	{
		TMap<uint32, TSharedRef<FNetTraceConnectionState>> ActiveConnections;
		TMap<uint64, FNetTraceActiveObjectState> ActiveObjects;
		TMap<int32, uint32> ChannelNames;

		uint32 GameInstanceIndex;
	};

private:

	uint32 GetTracedEventTypeIndex(uint16 NameIndex, uint8 Level);

	TSharedRef<FNetTraceGameInstanceState> GetOrCreateActiveGameInstanceState(uint32 GameInstanceId);
	void DestroyActiveGameInstanceState(uint32 GameInstanceId);

	FNetTraceConnectionState* GetActiveConnectionState(uint32 GameInstanceId, uint32 ConnectionId);
	FNetProfilerTimeStamp GetLastTimestamp() const { return LastTimeStamp; }

	void FlushPacketEvents(FNetTraceConnectionState& ConnectionState, FNetProfilerConnectionData& ConnectionData, const ENetProfilerConnectionMode ConnectionMode);

	void HandlePacketEvent(const FOnEventContext& Context, const FEventData& EventData);	
	void HandlePacketContentEvent(const FOnEventContext& Context, const FEventData& EventData);
	void HandlePacketDroppedEvent(const FOnEventContext& Context, const FEventData& EventData);
	void HandleConnectionCretedEvent(const FOnEventContext& Context, const FEventData& EventData);
	void HandleConnectionClosedEvent(const FOnEventContext& Context, const FEventData& EventData);
	void HandleObjectCreatedEvent(const FOnEventContext& Context, const FEventData& EventData);
	void HandleObjectDestroyedEvent(const FOnEventContext& Context, const FEventData& EventData);

	void AddEvent(TPagedArray<FNetProfilerContentEvent>& Events, const FNetProfilerContentEvent& Event, uint32 Offset, uint32 LevelOffset);
	void AddEvent(TPagedArray<FNetProfilerContentEvent>& Events, uint32 StartPos, uint32 EndPos, uint32 Level, uint32 NameIndex, FNetProfilerBunchInfo Info);

private:

	TMap<uint32, TSharedRef<FNetTraceGameInstanceState>> ActiveGameInstances;
	FNetProfilerTimeStamp LastTimeStamp;
};

} // namespace TraceServices
