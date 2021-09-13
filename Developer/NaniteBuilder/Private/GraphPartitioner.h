// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Bounds.h"
#include "metis.h"
#include "Async/ParallelFor.h"
#include "DisjointSet.h"

class FGraphPartitioner
{
public:
	struct FGraphData
	{
		int32	Offset;
		int32	Num;

		TArray< idx_t >	Adjacency;
		TArray< idx_t >	AdjacencyCost;
		TArray< idx_t >	AdjacencyOffset;
	};

	// Inclusive
	struct FRange
	{
		uint32	Begin;
		uint32	End;

		bool operator<( const FRange& Other) const { return Begin < Other.Begin; }
	};
	TArray< FRange >	Ranges;
	TArray< uint32 >	Indexes;

public:
				FGraphPartitioner( uint32 InNumElements );

	FGraphData*	NewGraph( uint32 NumAdjacency ) const;

	void		AddAdjacency( FGraphData* Graph, uint32 AdjIndex, idx_t Cost );
	void		AddLocalityLinks( FGraphData* Graph, uint32 Index, idx_t Cost );

	template< typename FGetCenter >
	void		BuildLocalityLinks( FDisjointSet& DisjointSet, const FBounds& Bounds, FGetCenter& GetCenter );

	void		Partition( FGraphData* Graph, int32 InMinPartitionSize, int32 InMaxPartitionSize );
	void		PartitionStrict( FGraphData* Graph, int32 InMinPartitionSize, int32 InMaxPartitionSize, bool bThreaded );

private:
	void		BisectGraph( FGraphData* Graph, FGraphData* ChildGraphs[2] );
	void		RecursiveBisectGraph( FGraphData* Graph );

	uint32		NumElements;
	int32		MinPartitionSize = 0;
	int32		MaxPartitionSize = 0;

	TAtomic< uint32 >	NumPartitions;

	TArray< idx_t >		PartitionIDs;
	TArray< int32 >		SwappedWith;
	TArray< uint32 >	SortedTo;

	TMultiMap< uint32, uint32 >	LocalityLinks;
};

FORCEINLINE void FGraphPartitioner::AddAdjacency( FGraphData* Graph, uint32 AdjIndex, idx_t Cost )
{
	Graph->Adjacency.Add( SortedTo[ AdjIndex ] );
	Graph->AdjacencyCost.Add( Cost );
}

FORCEINLINE void FGraphPartitioner::AddLocalityLinks( FGraphData* Graph, uint32 Index, idx_t Cost )
{
	for( auto Iter = LocalityLinks.CreateKeyIterator( Index ); Iter; ++Iter )
	{
		uint32 AdjIndex = Iter.Value();
		Graph->Adjacency.Add( SortedTo[ AdjIndex ] );
		Graph->AdjacencyCost.Add( Cost );
	}
}

template< typename FGetCenter >
void FGraphPartitioner::BuildLocalityLinks( FDisjointSet& DisjointSet, const FBounds& Bounds, FGetCenter& GetCenter )
{
	TArray< uint32 > SortKeys;
	SortKeys.AddUninitialized( NumElements );
	SortedTo.AddUninitialized( NumElements );

	const bool bSingleThreaded = NumElements < 5000;
	const EParallelForFlags ThreadingFlags = bSingleThreaded ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;
	const EParallelForFlags PriorityFlags = IsInGameThread() ? EParallelForFlags::None : EParallelForFlags::BackgroundPriority;
	ParallelFor( NumElements,
		[&]( uint32 Index )
		{
			FVector Center = GetCenter( Index );
			FVector CenterLocal = ( Center - Bounds.Min ) / ( Bounds.Max - Bounds.Min );

			uint32 Morton;
			Morton  = FMath::MortonCode3( CenterLocal.X * 1023 );
			Morton |= FMath::MortonCode3( CenterLocal.Y * 1023 ) << 1;
			Morton |= FMath::MortonCode3( CenterLocal.Z * 1023 ) << 2;
			SortKeys[ Index ] = Morton;
		}, ThreadingFlags | PriorityFlags);

	RadixSort32( SortedTo.GetData(), Indexes.GetData(), NumElements,
		[&]( uint32 Index )
		{
			return SortKeys[ Index ];
		} );
	
	SortKeys.Empty();

	Swap( Indexes, SortedTo );
	for( uint32 i = 0; i < NumElements; i++ )
	{
		SortedTo[ Indexes[i] ] = i;
	}

	TArray< FRange > IslandRuns;
	IslandRuns.AddUninitialized( NumElements );
	
	// Run length acceleration
	// Range of identical IslandID denoting that elements are connected.
	// Used for jumping past connected elements to the next nearby disjoint element.
	{
		uint32 RunIslandID = 0;
		uint32 RunFirstElement = 0;

		for( uint32 i = 0; i < NumElements; i++ )
		{
			uint32 IslandID = DisjointSet.Find( Indexes[i] );

			if( RunIslandID != IslandID )
			{
				// We found the end so rewind to the beginning of the run and fill.
				for( uint32 j = RunFirstElement; j < i; j++ )
				{
					IslandRuns[j].End = i - 1;
				}
			
				// Start the next run
				RunIslandID = IslandID;
				RunFirstElement = i;
			}

			IslandRuns[i].Begin = RunFirstElement;
		}
		// Finish the last run
		for( uint32 j = RunFirstElement; j < NumElements; j++ )
		{
			IslandRuns[j].End = NumElements - 1;
		}
	}

	for( uint32 i = 0; i < NumElements; i++ )
	{
		uint32 Index = Indexes[i];

		uint32 RunLength = IslandRuns[i].End - IslandRuns[i].Begin + 1;
		if( RunLength < 128 )
		{
			uint32 IslandID = DisjointSet[ Index ];

			FVector Center = GetCenter( Index );

			uint32 ClosestIndex[3] = { ~0u, ~0u, ~0u };
			float  ClosestDist2[3] = { MAX_flt, MAX_flt, MAX_flt };

			for( int Direction = 0; Direction < 2; Direction++ )
			{
				uint32 Limit = Direction ? NumElements - 1 : 0;
				uint32 Step  = Direction ? 1 : -1;

				uint32 Adj = i;
				for( int32 Iterations = 0; Iterations < 16; Iterations++ )
				{
					if( Adj == Limit )
						break;
					Adj += Step;

					uint32 AdjIndex = Indexes[ Adj ];
					uint32 AdjIslandID = DisjointSet[ AdjIndex ];
					if( IslandID == AdjIslandID )
					{
						// Skip past this run
						if( Direction )
							Adj = IslandRuns[ Adj ].End;
						else
							Adj = IslandRuns[ Adj ].Begin;
					}
					else
					{
						// Add to sorted list
						float AdjDist2 = ( Center - GetCenter( AdjIndex ) ).SizeSquared();
						for( int k = 0; k < 3; k++ )
						{
							if( AdjDist2 < ClosestDist2[k] )
							{
								Swap( AdjIndex, ClosestIndex[k] );
								Swap( AdjDist2, ClosestDist2[k] );
							}
						}
					}
				}
			}

			for( int k = 0; k < 3; k++ )
			{
				if( ClosestIndex[k] != ~0u )
				{
					// Add both directions
					LocalityLinks.AddUnique( Index, ClosestIndex[k] );
					LocalityLinks.AddUnique( ClosestIndex[k], Index );
				}
			}
		}
	}
}