// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ELightmapUVVersion : int32
{
	BitByBit = 0,
	Segments = 1,
	SmallChartPacking = 2,
	ScaleChartsOrderingFix = 3,
	ChartJoiningLFix = 4,
	Allocator2DFlipFix = 5,
	ConsiderLightmapPadding = 6,
	ForceLightmapPadding = 7,
	Segments2D = 8,
	OptimalSurfaceArea = 9,
	Latest = OptimalSurfaceArea
};

/** Helper struct for building acceleration structures. */
struct FIndexAndZ
{
	float Z;
	int32 Index;

	/** Default constructor. */
	FIndexAndZ() {}

	/** Initialization constructor. */
	FIndexAndZ(int32 InIndex, FVector V)
	{
		Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
		Index = InIndex;
	}
};

/** Sorting function for vertex Z/index pairs. */
struct FCompareIndexAndZ
{
	FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
};

/**
* Returns true if the specified points are about equal
*/
inline bool PointsEqual(const FVector& V1, const FVector& V2, float ComparisonThreshold)
{
	if (FMath::Abs(V1.X - V2.X) > ComparisonThreshold
		|| FMath::Abs(V1.Y - V2.Y) > ComparisonThreshold
		|| FMath::Abs(V1.Z - V2.Z) > ComparisonThreshold)
	{
		return false;
	}
	return true;
}

namespace TriangleUtilities
{
	/*
	 * This function compute the area of a triangle, it will return zero if the triangle is degenerated
	 */
	static float ComputeTriangleArea(const FVector& PointA, const FVector& PointB, const FVector& PointC)
	{
		return FVector::CrossProduct((PointB - PointA), (PointC - PointA)).Size() / 2.0f;
	}

	/*
	 * This function compute the angle of a triangle corner, it will return zero if the triangle is degenerated
	 */
	static float ComputeTriangleCornerAngle(const FVector& PointA, const FVector& PointB, const FVector& PointC)
	{
		FVector E1 = (PointB - PointA);
		FVector E2 = (PointC - PointA);
		//Normalize both edges (unit vector) of the triangle so we get a dotProduct result that will be a valid acos input [-1, 1]
		if (!E1.Normalize() || !E2.Normalize())
		{
			//Return a null ratio if the polygon is degenerate
			return 0.0f;
		}
		float DotProduct = FVector::DotProduct(E1, E2);
		return FMath::Acos(DotProduct);
	}
}
