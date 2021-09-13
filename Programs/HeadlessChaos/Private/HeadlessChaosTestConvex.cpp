// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaosTestEPA.h"

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"
#include "Chaos/Core.h"
#include "Chaos/GJK.h"
#include "Chaos/Convex.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/Particles.h"
#include "../Resource/TestGeometry2.h"
#include "Logging/LogScopedVerbosityOverride.h"

namespace ChaosTest
{
	using namespace Chaos;

	// Check that convex creation with face merging is working correctly.
	// The initial creation generates a set of triangles, and the merge step should
	// leave the hull with only one face per normal.
	void TestConvexBuilderConvexBoxFaceMerge(const FVec3* Vertices, const int32 NumVertices)
	{
		TArray<Chaos::FVec3> Particles;
		Particles.SetNum(NumVertices);
		for (int32 ParticleIndex = 0; ParticleIndex < NumVertices; ++ParticleIndex)
		{
			Particles[ParticleIndex] = Vertices[ParticleIndex];
		}

		TArray<TPlaneConcrete<FReal, 3>> Planes;
		TArray<TArray<int32>> FaceVertices;
		TArray<Chaos::FVec3> SurfaceParticles;
		TAABB<FReal, 3> LocalBounds;

		FConvexBuilder::Build(Particles, Planes, FaceVertices, SurfaceParticles, LocalBounds);
		FConvexBuilder::MergeFaces(Planes, FaceVertices, SurfaceParticles, 1.0f);

		// Check that we have the right number of faces and particles
		EXPECT_EQ(SurfaceParticles.Num(), 8);
		EXPECT_EQ(Planes.Num(), 6);
		EXPECT_EQ(FaceVertices.Num(), 6);

		// Make sure the verts are correct and agree on the normal
		for (int32 FaceIndex = 0; FaceIndex < FaceVertices.Num(); ++FaceIndex)
		{
			EXPECT_EQ(FaceVertices[FaceIndex].Num(), 4);
			for (int32 VertexIndex0 = 0; VertexIndex0 < FaceVertices[FaceIndex].Num(); ++VertexIndex0)
			{
				int32 VertexIndex1 = Chaos::Utilities::WrapIndex(VertexIndex0 + 1, 0, FaceVertices[FaceIndex].Num());
				int32 VertexIndex2 = Chaos::Utilities::WrapIndex(VertexIndex0 + 2, 0, FaceVertices[FaceIndex].Num());
				const FVec3 Vertex0 = SurfaceParticles[FaceVertices[FaceIndex][VertexIndex0]];
				const FVec3 Vertex1 = SurfaceParticles[FaceVertices[FaceIndex][VertexIndex1]];
				const FVec3 Vertex2 = SurfaceParticles[FaceVertices[FaceIndex][VertexIndex2]];

				// All vertices should lie in a plane at the same distance
				const FReal Dist0 = FVec3::DotProduct(Vertex0, Planes[FaceIndex].Normal());
				const FReal Dist1 = FVec3::DotProduct(Vertex1, Planes[FaceIndex].Normal());
				const FReal Dist2 = FVec3::DotProduct(Vertex2, Planes[FaceIndex].Normal());
				EXPECT_NEAR(Dist0, 50.0f, 1.e-3f);
				EXPECT_NEAR(Dist1, 50.0f, 1.e-3f);
				EXPECT_NEAR(Dist2, 50.0f, 1.e-3f);

				// All sequential edge pairs should agree on winding
				const FReal Winding = FVec3::DotProduct(FVec3::CrossProduct(Vertex1 - Vertex0, Vertex2 - Vertex1), Planes[FaceIndex].Normal());
				EXPECT_GT(Winding, 0.0f);
			}
		}
	}

	// Check that face merging works for a convex box
	GTEST_TEST(ConvexStructureTests, TestConvexBoxFaceMerge)
	{
		const FVec3 Vertices[] =
		{
			FVec3(-50,		-50,	-50),
			FVec3(-50,		-50,	50),
			FVec3(-50,		50,		-50),
			FVec3(-50,		50,		50),
			FVec3(50,		-50,	-50),
			FVec3(50,		-50,	50),
			FVec3(50,		50,		-50),
			FVec3(50,		50,		50),
		};

		TestConvexBuilderConvexBoxFaceMerge(Vertices, UE_ARRAY_COUNT(Vertices));
	}

	// Check that the convex structure data is consistent (works for TBox and TConvex)
	template<typename T_GEOM> void TestConvexStructureDataImpl(const T_GEOM& Convex)
	{
		// Note: This tolerance matches the one passed to FConvexBuilder::MergeFaces in the FConvex constructor, but it should be dependent on size
		//const FReal Tolerance = 1.e-4f * Convex.BoundingBox().OriginRadius();
		const FReal Tolerance = 1.0f;

		// Check all per-plane data
		for (int32 PlaneIndex = 0; PlaneIndex < Convex.NumPlanes(); ++PlaneIndex)
		{
			// All vertices should be on the plane
			for (int32 PlaneVertexIndex = 0; PlaneVertexIndex < Convex.NumPlaneVertices(PlaneIndex); ++PlaneVertexIndex)
			{
				const TPlaneConcrete<FReal, 3> Plane = Convex.GetPlane(PlaneIndex);
				const int32 VertexIndex = Convex.GetPlaneVertex(PlaneIndex, PlaneVertexIndex);
				const FVec3 Vertex = Convex.GetVertex(VertexIndex);
				const FReal VertexDistance = FVec3::DotProduct(Plane.Normal(), Vertex - Plane.X());
				EXPECT_NEAR(VertexDistance, 0.0f, Tolerance);
			}
		}

		// Check all per-vertex data
		for (int32 VertexIndex = 0; VertexIndex < Convex.NumVertices(); ++VertexIndex)
		{
			// All planes should pass through the vertex
			for (int32 VertexPlaneIndex = 0; VertexPlaneIndex < Convex.NumVertexPlanes(VertexIndex); ++VertexPlaneIndex)
			{
				const int32 PlaneIndex = Convex.GetVertexPlane(VertexIndex, VertexPlaneIndex);
				const TPlaneConcrete<FReal, 3> Plane = Convex.GetPlane(PlaneIndex);
				const FVec3 Vertex = Convex.GetVertex(VertexIndex);
				const FReal VertexDistance = FVec3::DotProduct(Plane.Normal(), Vertex - Plane.X());
				EXPECT_NEAR(VertexDistance, 0.0f, Tolerance);
			}
		}
	}

	// Check that the convex structure data is consistent
	void TestConvexStructureData(const FVec3* Vertices, const int32 NumVertices)
	{
		TArray<Chaos::FVec3> Particles;
		Particles.SetNum(NumVertices);
		for (int32 ParticleIndex = 0; ParticleIndex < NumVertices; ++ParticleIndex)
		{
			Particles[ParticleIndex] = Vertices[ParticleIndex];
		}

		FConvex Convex(Particles, 0.0f);

		TestConvexStructureDataImpl(Convex);
	}

	// Check that the convex structure data is consistent for a simple convex box
	GTEST_TEST(ConvexStructureTests, TestConvexStructureData)
	{
		const FVec3 Vertices[] =
		{
			FVec3(-50,		-50,	-50),
			FVec3(-50,		-50,	50),
			FVec3(-50,		50,		-50),
			FVec3(-50,		50,		50),
			FVec3(50,		-50,	-50),
			FVec3(50,		-50,	50),
			FVec3(50,		50,		-50),
			FVec3(50,		50,		50),
		};

		TestConvexStructureData(Vertices, UE_ARRAY_COUNT(Vertices));
	}

	// Check that the convex structure data is consistent for a complex convex shape
	GTEST_TEST(ConvexStructureTests, TestConvexStructureData2)
	{
		const FVec3 Vertices[] =
		{
			FVec3(0, 0, 12.0f),
			FVec3(-0.707f, -0.707f, 10.0f),
			FVec3(0, -1, 10.0f),
			FVec3(0.707f, -0.707f, 10.0f),
			FVec3(1, 0, 10.0f),
			FVec3(0.707f, 0.707f, 10.0f),
			FVec3(0.0f, 1.0f, 10.0f),
			FVec3(-0.707f, 0.707f, 10.0f),
			FVec3(-1.0f, 0.0f, 10.0f),
			FVec3(-0.707f, -0.707f, 0.0f),
			FVec3(0, -1, 0.0f),
			FVec3(0.707f, -0.707f, 0.0f),
			FVec3(1, 0, 0.0f),
			FVec3(0.707f, 0.707f, 0.0f),
			FVec3(0.0f, 1.0f, 0.0f),
			FVec3(-0.707f, 0.707f, 0.0f),
			FVec3(-1.0f, 0.0f, 0.0f),
			FVec3(0, 0, -2.0f),
		};

		TestConvexStructureData(Vertices, UE_ARRAY_COUNT(Vertices));
	}

	// Check that the convex structure data is consistent for a standard box
	GTEST_TEST(ConvexStructureTests, TestBoxStructureData)
	{
		FImplicitBox3 Box(FVec3(-50, -50, -50), FVec3(50, 50, 50), 0.0f);

		TestConvexStructureDataImpl(Box);

		// Make sure all planes are at the correct distance
		for (int32 PlaneIndex = 0; PlaneIndex < Box.NumPlanes(); ++PlaneIndex)
		{
			// All vertices should be on the plane
			const TPlaneConcrete<FReal, 3> Plane = Box.GetPlane(PlaneIndex);
			EXPECT_NEAR(FVec3::DotProduct(Plane.X(), Plane.Normal()), 50.0f, KINDA_SMALL_NUMBER);
		}
	}

	// Check the reverse mapping planes->vertices->planes is intact
	template<typename T_STRUCTUREDATA>
	void TestConvexStructureDataMapping(const T_STRUCTUREDATA& StructureData)
	{
		// For each plane, get the list of vertices that make its edges.
		// Then check that we list of planes used by that vertex contains the original plane
		for (int32 PlaneIndex = 0; PlaneIndex < StructureData.NumPlanes(); ++PlaneIndex)
		{
			for (int32 PlaneVertexIndex = 0; PlaneVertexIndex < StructureData.NumPlaneVertices(PlaneIndex); ++PlaneVertexIndex)
			{
				const int32 VertexIndex = StructureData.GetPlaneVertex(PlaneIndex, PlaneVertexIndex);

				// Check that the plane's vertex has the plane in its list
				bool bFoundPlane = false;
				for (int32 VertexPlaneIndex = 0; VertexPlaneIndex < StructureData.NumVertexPlanes(VertexIndex); ++VertexPlaneIndex)
				{
					const int32 SearchPlaneIndex = StructureData.GetVertexPlane(VertexIndex, VertexPlaneIndex);
					if (PlaneIndex == SearchPlaneIndex)
					{
						bFoundPlane = true;
						break;
					}
				}
				EXPECT_TRUE(bFoundPlane);
			}
		}
	}

	// Check that the structure data is good for convex shapes that have faces merged during construction
	// This test uses the small index size in StructureData.
	GTEST_TEST(ConvexStructureTests, TestSmallIndexStructureData)
	{
		FMath::RandInit(53799058);
		const FReal Radius = 1000.0f;

		const int32 NumVertices = TestGeometry2::RawVertexArray.Num() / 3;
		TArray<FVec3> Particles;
		Particles.SetNum(NumVertices);
		for (int32 ParticleIndex = 0; ParticleIndex < NumVertices; ++ParticleIndex)
		{
			const FVec3 VertexPos = FVec3(
				TestGeometry2::RawVertexArray[3 * ParticleIndex + 0],
				TestGeometry2::RawVertexArray[3 * ParticleIndex + 1],
				TestGeometry2::RawVertexArray[3 * ParticleIndex + 2]
			);
			Particles[ParticleIndex] = VertexPos;
		}

		FConvex Convex(Particles, 0.0f);

		const FConvexStructureDataU8& StructureData = Convex.GetStructureData().Data8();
		TestConvexStructureDataMapping(StructureData);
		TestConvexStructureDataImpl(Convex);
	}

	// Check that the structure data is good for convex shapes that have faces merged during construction
	// This test uses the large index size in StructureData.
	GTEST_TEST(ConvexStructureTests, TestLargeIndexStructureData2)
	{
		FMath::RandInit(53799058);
		const FReal Radius = 1000.0f;
		const int32 NumVertices = 1000;

		// Make a convex with points on a sphere.
		TArray<FVec3> Particles;
		Particles.SetNum(NumVertices);
		for (int32 ParticleIndex = 0; ParticleIndex < NumVertices; ++ParticleIndex)
		{
			const FReal Theta = FMath::RandRange(-PI, PI);
			const FReal Phi = FMath::RandRange(-0.5f * PI, 0.5f * PI);
			const FVec3 VertexPos = Radius * FVec3(FMath::Cos(Theta), FMath::Sin(Theta), FMath::Sin(Phi));
			Particles[ParticleIndex] = VertexPos;
		}
		FConvex Convex(Particles, 0.0f);

		EXPECT_GT(Convex.NumVertices(), 800);
		EXPECT_GT(Convex.NumPlanes(), 500);

		const FConvexStructureDataS32& StructureData = Convex.GetStructureData().Data32();
		TestConvexStructureDataMapping(StructureData);
		TestConvexStructureDataImpl(Convex);
	}

	// Check that extremely small generated triangle don't trigger the normal check
	GTEST_TEST(ConvexStructureTests, TestConvexFaceNormalCheck)
	{
		// Create a long mesh with a extremely small end (YZ plane) 
		// so that it generate extremely sized triangle that will produce extremely small (unormalized) normals
		const float SmallNumber = 0.001f;
		const FVec3 Range{ 100.0f, SmallNumber, SmallNumber };

		const FVec3 Vertices[] =
		{
			FVec3(0, 0, 0),
			FVec3(Range.X, 0, 0),
			FVec3(Range.X, Range.Y, 0),
			FVec3(Range.X, Range.Y, Range.Z),
			FVec3(Range.X+ SmallNumber, Range.Y*0.5f, Range.Z * 0.5f),
		};

		TestConvexStructureData(Vertices, UE_ARRAY_COUNT(Vertices));
	}

	GTEST_TEST(ConvexStructureTests, TestConvexFailsSafelyOnPlanarObject)
	{
		using namespace Chaos;

		// This list of vertices is a plane with many duplicated vertices and previously was causing
		// a check to fire inside the convex builder as we classified the object incorrectly and didn't
		// safely handle a failure due to a planar object. This test verifies that the builder can
		// safely fail to build a convex from a plane.
		const TArray<FVec3> Vertices =
		{
			{-15.1425571, 16.9698563, 0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{-15.1425571, 16.9698563, 0.502334476},
			{16.9772491, 15.1373663, 0.398189038},
			{16.9772491, 15.1373663, 0.398189038},
			{16.9772491, 15.1373663, 0.398189038},
			{-15.1425571, 16.9698563, 0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{-16.9772491, -15.1373663, -0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{-16.9772491, -15.1373663, -0.398189038},
			{16.9772491, 15.1373663, 0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{16.9772491, 15.1373663, 0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{15.1425571, -16.9698563, -0.502334476},
			{16.9772491, 15.1373663, 0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{-15.1425571, 16.9698563, 0.502334476},
			{16.9772491, 15.1373663, 0.398189038},
			{16.9772491, 15.1373663, 0.398189038},
			{16.9772491, 15.1373663, 0.398189038},
			{-15.1425571, 16.9698563, 0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{-16.9772491, -15.1373663, -0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{-16.9772491, -15.1373663, -0.398189038},
			{16.9772491, 15.1373663, 0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-15.1425571, 16.9698563, 0.502334476},
			{16.9772491, 15.1373663, 0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{15.1425571, -16.9698563, -0.502334476},
			{16.9772491, 15.1373663, 0.398189038},
			{15.1425571, -16.9698563, -0.502334476},
			{-16.9772491, -15.1373663, -0.398189038},
			{15.1425571, -16.9698563, -0.502334476}
		};

		TArray <TPlaneConcrete<FReal, 3>> Planes;
		TArray<TArray<int32>> FaceIndices;
		TArray<FVec3> FinalVertices;
		TAABB<FReal, 3> LocalBounds;

		{
			// Temporarily set LogChaos to error, we're expecting this to fire warnings and don't want that to fail a CIS run.
			LOG_SCOPE_VERBOSITY_OVERRIDE(LogChaos, ELogVerbosity::Error);
			FConvexBuilder::Build(Vertices, Planes, FaceIndices, FinalVertices, LocalBounds);
		}


		// Check that we've failed to build a 3D convex hull and safely returned
		EXPECT_EQ(Planes.Num(), 0);
	}
}
