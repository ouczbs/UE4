// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalysisSession.h"
#include "CoreTypes.h"
#include "Templates/UniquePtr.h"

#include <new>

namespace TraceServices
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class IAllocationsProvider : public IProvider
{
public:
	struct TRACESERVICES_API FEditScopeLock
	{
		FEditScopeLock(const IAllocationsProvider& InAllocationsProvider)
			: AllocationsProvider(InAllocationsProvider)
		{
			AllocationsProvider.BeginEdit();
		}

		~FEditScopeLock()
		{
			AllocationsProvider.EndEdit();
		}

		const IAllocationsProvider& AllocationsProvider;
	};

	struct TRACESERVICES_API FReadScopeLock
	{
		FReadScopeLock(const IAllocationsProvider& InAllocationsProvider)
			: AllocationsProvider(InAllocationsProvider)
		{
			AllocationsProvider.BeginRead();
		}

		~FReadScopeLock()
		{
			AllocationsProvider.EndRead();
		}

		const IAllocationsProvider& AllocationsProvider;
	};

	// Allocation query rules.
	// The enum uses the following naming convention:
	//     A, B, C, D = time markers
	//     a = time when "alloc" event occurs
	//     f = time when "free" event occurs (can be infinite)
	// Ex.: "AaBf" means "all memory allocations allocated between time A and time B and freed after time B".
	enum class EQueryRule
	{
		aAf,     // active allocs at A
		afA,     // before
		Aaf,     // after
		aAfB,    // decline
		AaBf,    // growth
		AafB,    // short living allocs
		aABf,    // long living allocs
		AaBCf,   // memory leaks
		AaBfC,   // limited lifetime
		aABfC,   // decline of long living allocs
		AaBCfD,  // specific lifetime
		//A_vs_B,  // compare A vs. B; {aAf} vs. {aBf}
		//A_or_B,  // live at A or at B; {aAf} U {aBf}
		//A_xor_B, // live either at A or at B; ({aAf} U {aBf}) \ {aABf}
	};

	struct TRACESERVICES_API FQueryParams
	{
		EQueryRule Rule;
		double TimeA;
		double TimeB;
		double TimeC;
		double TimeD;
	};

	struct TRACESERVICES_API FAllocation
	{
		double GetStartTime() const;
		double GetEndTime() const;
		uint64 GetAddress() const;
		uint64 GetSize() const;
		uint32 GetAlignment() const;
		const struct FCallstack* GetCallstack() const;
		uint32 GetTag() const;
	};

	class TRACESERVICES_API FAllocations
	{
	public:
		void operator delete (void* Address);
		uint32 Num() const;
		const FAllocation* Get(uint32 Index) const;
	};

	typedef TUniquePtr<const FAllocations> FQueryResult;

	enum class EQueryStatus
	{
		Unknown,
		Done,
		Working,
		Available,
	};

	struct TRACESERVICES_API FQueryStatus
	{
		FQueryResult NextResult() const;

		EQueryStatus Status;
		mutable UPTRINT Handle;
	};

	typedef UPTRINT FQueryHandle;

public:
	virtual void BeginEdit() const = 0;
	virtual void EndEdit() const = 0;
	virtual void BeginRead() const = 0;
	virtual void EndRead() const = 0;

	virtual bool IsInitialized() const = 0;

	// Returns the number of points in each timeline (Min/Max Total Allocated Memory, Min/Max Live Allocations, Total Alloc Events, Total Free Events).
	virtual uint32 GetTimelineNumPoints() const = 0;

	// Returns the inclusive index range [StartIndex, EndIndex] for a time range [StartTime, EndTime].
	// Index values are in range { -1, 0, .. , N-1, N }, where N = GetTimelineNumPoints().
	virtual void GetTimelineIndexRange(double StartTime, double EndTime, int32& StartIndex, int32& EndIndex) const = 0;

	// Enumerates the Min Total Allocated Memory timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMinTotalAllocatedMemoryTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint64 Value)> Callback) const = 0;

	// Enumerates the Max Total Allocated Memory timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMaxTotalAllocatedMemoryTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint64 Value)> Callback) const = 0;

	// Enumerates the Min Live Allocations timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMinLiveAllocationsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	// Enumerates the Max Live Allocations timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateMaxLiveAllocationsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	// Enumerates the Alloc Events timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateAllocEventsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	// Enumerates the Free Events timeline points in the inclusive index interval [StartIndex, EndIndex].
	virtual void EnumerateFreeEventsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const = 0;

	virtual FQueryHandle StartQuery(const FQueryParams& Params) const = 0;
	virtual void CancelQuery(FQueryHandle Query) const = 0;
	virtual const FQueryStatus PollQuery(FQueryHandle Query) const = 0;

	// Returns the display name of the specified LLM tag.
	// Lifetime of returned string matches the session lifetime.
	virtual const TCHAR* GetTagName(int32 Tag) const = 0;
};

TRACESERVICES_API const IAllocationsProvider* ReadAllocationsProvider(const IAnalysisSession& Session);

} // namespace TraceServices
