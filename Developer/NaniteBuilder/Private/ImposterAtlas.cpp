// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImposterAtlas.h"
#include "Rasterizer.h"
#include "Cluster.h"

inline FVector OctahedronToUnitVector( const FVector2D& Oct )
{
	FVector N( Oct.X, Oct.Y, 1.0f - FMath::Abs( Oct.X ) - FMath::Abs( Oct.Y ) );
	float t = FMath::Max( -N.Z, 0.0f );
	N.X += N.X >= 0.0f ? -t : t;
	N.Y += N.Y >= 0.0f ? -t : t;
	return N.GetUnsafeNormal();
}

namespace Nanite
{

FImposterAtlas::FImposterAtlas( TArray< uint16 >& InPixels, const FBounds& Bounds )
	: Pixels( InPixels )
{
	BoundsCenter = 0.5f * ( Bounds.Max + Bounds.Min );
	BoundsExtent = 0.5f * ( Bounds.Max - Bounds.Min );

	Pixels.AddZeroed( FMath::Square( AtlasSize * TileSize ) );
}

FMatrix FImposterAtlas::GetLocalToImposter( const FIntPoint& TilePos ) const
{
	FVector2D Oct = ( FVector2D( TilePos ) + 0.5f ) / AtlasSize * 2.0f - 1.0f;

	FVector ImposterZ = OctahedronToUnitVector( Oct );

	// [Frisvad 2012, "Building an Orthonormal Basis from a 3D Unit Vector Without Normalization"]
	// Invalid for ImposterZ.z == -1
	float A = 1.0f / ( 1.0f + ImposterZ.Z );
	float B = -ImposterZ.X * ImposterZ.Y * A;
	FVector ImposterX( 1.0f - FMath::Square( ImposterZ.X ) * A, B, -ImposterZ.X );
	FVector ImposterY( B, 1.0f - FMath::Square( ImposterZ.Y ) * A, -ImposterZ.Y );

	FVector ImposterExtent(
		BoundsExtent | ImposterX.GetAbs(),
		BoundsExtent | ImposterY.GetAbs(),
		BoundsExtent | ImposterZ.GetAbs() );
	
	FMatrix LocalToImposter = FMatrix(
		ImposterX,
		ImposterY,
		ImposterZ,
		FVector::ZeroVector ).GetTransposed();

	return FTranslationMatrix( -BoundsCenter ) * LocalToImposter * FScaleMatrix( FVector::OneVector / ImposterExtent );
}

void FImposterAtlas::Rasterize( const FIntPoint& TilePos, const FCluster& Cluster, uint32 ClusterIndex )
{
	constexpr uint32 ViewSize = TileSize;// * SuperSample;

	FIntRect Scissor( 0, 0, ViewSize, ViewSize );

	FMatrix LocalToImposter = GetLocalToImposter( TilePos );

	TArray< FVector, TInlineAllocator<128> > Positions;
	Positions.SetNum( Cluster.NumVerts, false );

	for( uint32 VertIndex = 0; VertIndex < Cluster.NumVerts; VertIndex++ )
	{
		FVector Position = Cluster.GetPosition( VertIndex );
		//checkSlow( FBox( BoundsCenter - BoundsExtent, BoundsCenter + BoundsExtent ).ExpandBy( KINDA_SMALL_NUMBER ).IsInside( Position ) );

		Position = LocalToImposter.TransformPosition( Position );
		//checkSlow( Position.GetAbsMax() < 1.0001f );

		// TODO Bake into matrix
		Positions[ VertIndex ].X = ( Position.X * 0.5f + 0.5f ) * ViewSize;
		Positions[ VertIndex ].Y = ( Position.Y * 0.5f + 0.5f ) * ViewSize;
		Positions[ VertIndex ].Z = ( Position.Z * 0.5f + 0.5f ) * 254.0f + 1.0f;	// zero is reserved as masked
	}

	for( uint32 TriIndex = 0; TriIndex < Cluster.NumTris; TriIndex++ )
	{
		FVector Verts[3];
		Verts[0] = Positions[ Cluster.Indexes[ TriIndex * 3 + 0 ] ];
		Verts[1] = Positions[ Cluster.Indexes[ TriIndex * 3 + 1 ] ];
		Verts[2] = Positions[ Cluster.Indexes[ TriIndex * 3 + 2 ] ];

		RasterizeTri( Verts, Scissor, 0,
			[&]( int32 x, int32 y, float z )
			{
				uint32 Depth = FMath::RoundToInt( FMath::Clamp( z, 1.0f, 255.0f ) );
				uint16 PixelValue = ( Depth << 8 ) | ( ClusterIndex << 7 ) | TriIndex;
				//uint32 PixelIndex = x + y * ViewSize;
				uint32 PixelIndex = x + ( y + ( TilePos.X + TilePos.Y * AtlasSize ) * TileSize ) * TileSize;
				Pixels[ PixelIndex ] = FMath::Max( Pixels[ PixelIndex ], PixelValue );
			} );
	}
}

#if 0
void FImposterAtlas::DownSample( const FIntPoint& TilePos, TArray< uint16 >& Atlas ) const
{
	constexpr uint32 ViewSize = TileSize * SuperSample;

	for( int32 y = 0; y < TileSize; y++ )
	{
		for( int32 x = 0; x < TileSize; x++ )
		{
			TArray< uint8, TFixedAllocator< SuperSample * SuperSample > > UniqueTris;
			uint8 Counts[ SuperSample * SuperSample ] = {};

			uint32 SumDepth = 0;
			uint32 SumCount = 0;
			for( int32 sy = 0; sy < SuperSample; sy++ )
			{
				for( int32 sx = 0; sx < SuperSample; sx++ )
				{
					uint32 PixelIndex = ( sx + x * SuperSample ) + ( sy + y * SuperSample ) * ViewSize;
					uint32 PixelValue = Pixels[ PixelIndex ];
					uint32 TriIndex = PixelValue & 0xff;
					uint32 Depth = PixelValue >> 8;
					
					if( Depth )
					{
						Counts[ UniqueTris.AddUnique( TriIndex ) ]++;
						SumDepth += Depth;
						SumCount++;
					}
				}
			}

			uint16 PixelValue = 0;
			if( SumCount >= SuperSample * SuperSample / 2 )
			{
				uint32 Depth = FMath::Clamp( ( SumDepth + (SumCount >> 1) ) / SumCount, 1u, 255u );
				uint32 TriIndex = 0;
				uint32 MaxCount = 0;
				
				for( int i = 0; i < UniqueTris.Num(); i++ )
				{
					if( Counts[i] > MaxCount )
						TriIndex = UniqueTris[i];
				}

				PixelValue = ( Depth << 8 ) | TriIndex;
			}

			uint32 AtlasIndex = x + ( y + ( TilePos.X + TilePos.Y * AtlasSize ) * TileSize ) * TileSize;
			Atlas[ AtlasIndex ] = PixelValue;
		}
	}
}
#endif

} // namespace Nanite
