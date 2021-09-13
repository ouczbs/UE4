// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaosTestGJK.h"

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"
#include "Chaos/GJK.h"
#include "Chaos/Capsule.h"
#include "Chaos/Convex.h"
#include "Chaos/GJK.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Triangle.h"

namespace ChaosTest
{
	using namespace Chaos;

	//for each simplex test:
	//- points get removed
	// - points off simplex return false
	//- points in simplex return true
	//- degenerate simplex

	void SimplexLine()
	{
		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1}, {-1,-1,1} };
			int32 Idxs[] = { 0,1 };
			int32 NumVerts = 2;
			const FVec3 ClosestPoint = LineSimplexFindOrigin(Simplex, Idxs, NumVerts, Barycentric);
			EXPECT_EQ(NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1}, {1,1,1} };
			int32 Idxs[] = { 0,1 };
			int32 NumVerts = 2;
			const FVec3 ClosestPoint = LineSimplexFindOrigin(Simplex, Idxs, NumVerts, Barycentric);
			EXPECT_EQ(NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {1,1,1}, {1,2,3} };
			int32 Idxs[] = { 0,1 };
			int32 NumVerts = 2;
			const FVec3 ClosestPoint = LineSimplexFindOrigin(Simplex, Idxs, NumVerts, Barycentric);
			EXPECT_EQ(NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 1);
			EXPECT_EQ(Idxs[0], 0);
		}

		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {10,11,12}, {1,2,3} };
			int32 Idxs[] = { 0,1 };
			int32 NumVerts = 2;
			const FVec3 ClosestPoint = LineSimplexFindOrigin(Simplex, Idxs, NumVerts, Barycentric);
			EXPECT_EQ(NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 2);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 3);
			EXPECT_FLOAT_EQ(Barycentric[1], 1);
			EXPECT_EQ(Idxs[0], 1);
		}

		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {1,1,1}, {1,1,1} };
			int32 Idxs[] = { 0,1 };
			int32 NumVerts = 2;
			const FVec3 ClosestPoint = LineSimplexFindOrigin(Simplex, Idxs, NumVerts, Barycentric);
			EXPECT_EQ(NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 1);
			EXPECT_EQ(Idxs[0], 0);
		}

		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {1,-1e-16,1}, {1,1e-16,1} };
			int32 Idxs[] = { 0,1 };
			int32 NumVerts = 2;
			const FVec3 ClosestPoint = LineSimplexFindOrigin(Simplex, Idxs, NumVerts, Barycentric);
			EXPECT_EQ(NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
		}
	}

	void SimplexTriangle()
	{
		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1}, {-1,1,-1}, {-2,1,-1} };
			FSimplex Idxs = { 0,1, 2 };
			
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -1);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1},{-2,1,-1}, {-1,1,-1} };
			FSimplex Idxs = { 0,1, 2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -1);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 2);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[2], 0.5);
		}

		{
			//corner
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {1,1,1},{2,1,1}, {2,2,1} };
			FSimplex Idxs = { 1,0, 2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 1);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_FLOAT_EQ(Barycentric[0], 1);
		}

		{
			//corner equal
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {0,0,0},{2,1,1}, {2,2,1} };
			FSimplex Idxs = { 0,1, 2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_FLOAT_EQ(Barycentric[0], 1);
		}

		{
			//edge equal
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,0,0},{1,0,0}, {0,2,0} };
			FSimplex Idxs = { 2,0, 1 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			//triangle equal
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,0,-1},{1,0,-1}, {0,0,1} };
			FSimplex Idxs = { 0,1, 2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 2);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[2], 0.5);
		}

		{
			//co-linear
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1},{-1,1,-1}, {-1,1.2,-1} };
			FSimplex Idxs = { 0,1, 2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -1);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);	//degenerate triangle throws out newest point
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			//single point
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1},{-1,-1,-1}, {-1,-1,-1} };
			FSimplex Idxs = { 0,2, 1 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -1);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_FLOAT_EQ(Barycentric[0], 1);
		}

		{
			//corner perfect split
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,0},{1,-1,0}, {0,-0.5,0} };
			FSimplex Idxs = { 0,2, 1 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], -0.5);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 2);
			EXPECT_FLOAT_EQ(Barycentric[2], 1);
		}

		{
			//triangle face correct distance
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1},{1,-1,-1}, {0,1,-1} };
			FSimplex Idxs = { 0,1,2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -1);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 2);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[2], 0.5);
		}

		{
			//tiny triangle middle point
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1e-9,-1e-9,-1e-9},{-1e-9,1e-9,-1e-9}, {-1e-9,0,1e-9} };
			FSimplex Idxs = { 0,1,2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_FLOAT_EQ(ClosestPoint[0], -1e-9);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 2);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[2], 0.5);
		}

		{
			//non cartesian triangle plane
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {2, 0, -1}, {0, 2, -1}, {1, 1, 1} };
			FSimplex Idxs = { 0,1,2 };
			const FVec3 ClosestPoint = TriangleSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 2);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.25);
			EXPECT_FLOAT_EQ(Barycentric[2], 0.5);
		}
	}

	void SimplexTetrahedron()
	{
		{
			//top corner
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1}, {1,-1,-1}, {0,1,-1}, {0,0,-0.5} };
			FSimplex Idxs = { 0,1,2,3 };
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 1);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -0.5);
			EXPECT_EQ(Idxs[0], 3);
			EXPECT_FLOAT_EQ(Barycentric[3], 1);
		}

		{
			//inside
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,-1}, {1,-1,-1}, {0,1,-1}, {0,0,0.5} };
			FSimplex Idxs = { 0,1,2,3 };
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 4);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 2);
			EXPECT_EQ(Idxs[3], 3);
			EXPECT_FLOAT_EQ(Barycentric[0] + Barycentric[1] + Barycentric[2] + Barycentric[3], 1);
		}

		{
			//face
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {0,0,-1.5}, {-1,-1,-1}, {1,-1,-1}, {0,1,-1} };
			FSimplex Idxs = { 0,1,2,3 }; 
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[2], -1);
			EXPECT_EQ(Idxs[0], 1);
			EXPECT_EQ(Idxs[1], 2);
			EXPECT_EQ(Idxs[2], 3);
			EXPECT_FLOAT_EQ(Barycentric[1] + Barycentric[2] + Barycentric[3], 1);
		}

		{
			//edge
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,0}, {1,-1,0}, {0,-1,-1}, {0, -2, -1} };
			FSimplex Idxs = { 0,1,2,3 };
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			//degenerate
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-1,-1,0}, {1,-1,0}, {0,-1,-1}, {0, -1, -0.5} };
			FSimplex Idxs = { 0,1,2,3 };
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 2);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_FLOAT_EQ(Barycentric[0], 0.5);
			EXPECT_FLOAT_EQ(Barycentric[1], 0.5);
		}

		{
			//wide angle, bad implementation would return edge but it's really a face
			FReal Barycentric[4];
			const FVec3 Simplex[] = { {-10000,-1,10000}, {1,-1,10000}, {4,-3,10000}, {1, -1, -10000} };
			FSimplex Idxs = { 0,1,2,3 };
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_FLOAT_EQ(ClosestPoint[0], 0);
			EXPECT_FLOAT_EQ(ClosestPoint[1], -1);
			EXPECT_FLOAT_EQ(ClosestPoint[2], 0);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 3);
			EXPECT_FLOAT_EQ(Barycentric[0] + Barycentric[1] + Barycentric[3], 1);
		}

		{
			// Previous failing case observed with Voronoi region implementation - Not quite degenerate (totally degenerate cases work)
			FReal Barycentric[4];
			FVec3 Simplex[] = { { -15.9112930, -15.2787428, 1.33070087 },
								{ 1.90487099, 2.25161266, 0.439208984 },
								{ -15.8914719, -15.2915068, 1.34186459 },
								{ 1.90874290, 2.24025059, 0.444719315 } };

			FSimplex Idxs = { 0,1,2,3 };
			const FVec3 ClosestPoint = TetrahedronSimplexFindOrigin(Simplex, Idxs, Barycentric);
			EXPECT_EQ(Idxs.NumVerts, 3);
			EXPECT_EQ(Idxs[0], 0);
			EXPECT_EQ(Idxs[1], 1);
			EXPECT_EQ(Idxs[2], 2);
		}
	}

	//For each gjk test we should test:
	// - thickness
	// - transformed geometry
	// - rotated geometry
	// - degenerate cases
	// - near miss, near hit
	// - multiple initial dir

	void GJKSphereSphereTest()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		TSphere<FReal, 3> B(FVec3(4, 0, 0), 2);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		for (const FVec3& InitialDir : InitialDirs)
		{
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3::Identity, 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(-1.1, 0, 0), FRotation3::Identity), 0, InitialDir));

			//hit from thickness
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(-1.1, 0, 0), FRotation3::Identity), 0.105, InitialDir));

			//miss with thickness
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(-1.1, 0, 0), FRotation3::Identity), 0.095, InitialDir));

			//hit with rotation
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(6.5, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 1, InitialDir));

			//miss with rotation
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(6.5, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 0.01, InitialDir));

			//hit tiny
			TSphere<FReal, 3> Tiny(FVec3(0), 1e-2);
			EXPECT_TRUE(GJKIntersection<FReal>(A, Tiny, FRigidTransform3(FVec3(15, 0, 0), FRotation3::Identity), 0, InitialDir));

			//miss tiny
			EXPECT_FALSE(GJKIntersection<FReal>(A, Tiny, FRigidTransform3(FVec3(15 + 1e-1, 0, 0), FRotation3::Identity), 0, InitialDir));
		}
	}


	void GJKSphereBoxTest()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		FAABB3 B(FVec3(-4, -2, -4), FVec3(4,2,4));

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		for (const FVec3& InitialDir : InitialDirs)
		{
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(0.9, 0, 0), FRotation3::Identity), 0, InitialDir));

			//rotate and hit
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(3.1, 0, 0), FRotation3::FromVector(FVec3(0,0,PI*0.5))), 0, InitialDir));

			//rotate and miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(2.9, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0, InitialDir));

			//rotate and hit from thickness
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(2.9, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0.1, InitialDir));

			//hit thin
			FAABB3 Thin(FVec3(4, -2, -4), FVec3(4, 2, 4));
			EXPECT_TRUE(GJKIntersection<FReal>(A, Thin, FRigidTransform3(FVec3(1+1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, Thin, FRigidTransform3(FVec3(1 - 1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));

			//hit line
			FAABB3 Line(FVec3(4, -2, 0), FVec3(4, 2, 0));
			EXPECT_TRUE(GJKIntersection<FReal>(A, Line, FRigidTransform3(FVec3(1 + 1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, Line, FRigidTransform3(FVec3(1 - 1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));
		}
	}


	void GJKSphereCapsuleTest()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		TCapsule<FReal> B(FVec3(0, 0, -3), FVec3(0, 0, 3), 3);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		for (const FVec3& InitialDir : InitialDirs)
		{
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(2, 0, 0), FRotation3::Identity), 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(2-1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));

			//thickness
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), 1.01, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), 0.99, InitialDir));

			//rotation hit
			EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(-1+1e-2, 0, 0), FRotation3::FromVector(FVec3(0,PI*0.5,0))), 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(-1-1e-2, 0, 0), FRotation3::FromVector(FVec3(0, PI*0.5, 0))), 0, InitialDir));

			//degenerate
			TCapsule<FReal> Line(FVec3(0, 0, -3), FVec3(0, 0, 3), 0);
			EXPECT_TRUE(GJKIntersection<FReal>(A, Line, FRigidTransform3(FVec3(5+1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, Line, FRigidTransform3(FVec3(5 - 1e-2, 0, 0), FRotation3::Identity), 0, InitialDir));
		}
	}


	void GJKSphereConvexTest()
	{
		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);

		{
			//Tetrahedron
			TArray<Chaos::FVec3> HullParticles;
			HullParticles.SetNum(4);
			HullParticles[0] = { -1,-1,-1 };
			HullParticles[1] = { 1,-1,-1 };
			HullParticles[2] = { 0,1,-1 };
			HullParticles[3] = { 0,0,1 };
			FConvex B(HullParticles, 0.0f);

			for (const FVec3& InitialDir : InitialDirs)
			{
				//hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(5, 0, 0), FRotation3::Identity), 0, InitialDir));

				//near hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 + 1e-4, 1, 1), FRotation3::Identity), 0, InitialDir));

				//near miss
				EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 - 1e-2, 1, 1), FRotation3::Identity), 0, InitialDir));

				//rotated hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 + 1e-4, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0, InitialDir));

				//rotated miss
				EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 - 1e-2, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0, InitialDir));

				//rotated and inflated hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(3.5, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0.5 + 1e-4, InitialDir));

				//rotated and inflated miss
				EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(3.5, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0.5 - 1e-2, InitialDir));
			}
		}

		{
			//Triangle
			TArray<Chaos::FVec3> TriangleParticles;
			TriangleParticles.SetNum(3);
			TriangleParticles[0] = { -1,-1,-1 };
			TriangleParticles[1] = { 1,-1,-1 };
			TriangleParticles[2] = { 0,1,-1 };
			FConvex B(TriangleParticles, 0.0f);

			//triangle
			for (const FVec3& InitialDir : InitialDirs)
			{
				//hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(5, 0, 0), FRotation3::Identity), 0, InitialDir));

				//near hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 + 1e-2, 1, 1), FRotation3::Identity), 0, InitialDir));

				//near miss
				EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 - 1e-2, 1, 1), FRotation3::Identity), 0, InitialDir));

				//rotated hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 + 1e-2, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0, InitialDir));

				//rotated miss
				EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(4 - 1e-2, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0, InitialDir));

				//rotated and inflated hit
				EXPECT_TRUE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(3.5, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0.5 + 1e-2, InitialDir));

				//rotated and inflated miss
				EXPECT_FALSE(GJKIntersection<FReal>(A, B, FRigidTransform3(FVec3(3.5, 0, 1), FRotation3::FromVector(FVec3(0, 0, PI*0.5))), 0.5 - 1e-2, InitialDir));
			}
		}
	}


	void GJKSphereScaledSphereTest()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		TUniquePtr<TSphere<FReal, 3>> Sphere = MakeUnique<TSphere<FReal, 3>>(FVec3(4, 0, 0), 2);
		TImplicitObjectScaled<TSphere<FReal, 3>> Unscaled(MakeSerializable(Sphere), FVec3(1));
		TImplicitObjectScaled<TSphere<FReal, 3>> UniformScaled(MakeSerializable(Sphere), FVec3(2));
		TImplicitObjectScaled<TSphere<FReal, 3>> NonUniformScaled(MakeSerializable(Sphere), FVec3(2,1,1));

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		for (const FVec3& InitialDir : InitialDirs)
		{
			EXPECT_TRUE(GJKIntersection<FReal>(A, Unscaled, FRigidTransform3::Identity, 0, InitialDir));
			EXPECT_TRUE(GJKIntersection<FReal>(A, UniformScaled, FRigidTransform3::Identity, 0, InitialDir));
			//EXPECT_TRUE(GJKIntersection<FReal>(A, NonUniformScaled, FRigidTransform3::Identity, 0, InitialDir));

			//miss
			EXPECT_FALSE(GJKIntersection<FReal>(A, Unscaled, FRigidTransform3(FVec3(-1.1, 0, 0), FRotation3::Identity), 0, InitialDir));
			EXPECT_FALSE(GJKIntersection<FReal>(A, UniformScaled, FRigidTransform3(FVec3(-7.1, 0, 0), FRotation3::Identity), 0, InitialDir));
			//EXPECT_FALSE(GJKIntersection<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(-7.1, 0, 0), FRotation3::Identity), 0, InitialDir));

			//hit from thickness
			EXPECT_TRUE(GJKIntersection<FReal>(A, Unscaled, FRigidTransform3(FVec3(-1.1, 0, 0), FRotation3::Identity), 0.105, InitialDir));
			EXPECT_TRUE(GJKIntersection<FReal>(A, UniformScaled, FRigidTransform3(FVec3(-7.1, 0, 0), FRotation3::Identity), 0.105, InitialDir));
			//EXPECT_TRUE(GJKIntersection<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(-7.1, 0, 0), FRotation3::Identity), 0.105, InitialDir));

			//miss with thickness
			EXPECT_FALSE(GJKIntersection<FReal>(A, Unscaled, FRigidTransform3(FVec3(-1.1, 0, 0), FRotation3::Identity), 0.095, InitialDir));
			EXPECT_FALSE(GJKIntersection<FReal>(A, UniformScaled, FRigidTransform3(FVec3(-7.1, 0, 0), FRotation3::Identity), 0.095, InitialDir));
			//EXPECT_FALSE(GJKIntersection<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(-7.1, 0, 0), FRotation3::Identity), 0.095, InitialDir));

			//hit with rotation
			EXPECT_TRUE(GJKIntersection<FReal>(A, Unscaled, FRigidTransform3(FVec3(6.5, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 1, InitialDir));
			EXPECT_TRUE(GJKIntersection<FReal>(A, UniformScaled, FRigidTransform3(FVec3(8.1, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 1, InitialDir));
			//EXPECT_TRUE(GJKIntersection<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(8.1, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 1, InitialDir));

			//miss with rotation
			EXPECT_FALSE(GJKIntersection<FReal>(A, Unscaled, FRigidTransform3(FVec3(6.5, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 0.01, InitialDir));
			EXPECT_FALSE(GJKIntersection<FReal>(A, UniformScaled, FRigidTransform3(FVec3(8.1, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 0.01, InitialDir));
			//EXPECT_FALSE(GJKIntersection<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(8.1, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))), 0.01, InitialDir));
		}
	}

	//For each gjkraycast test we should test:
	// - thickness
	// - initial overlap
	// - transformed geometry
	// - rotated geometry
	// - offset transform
	// - degenerate cases
	// - near miss, near hit
	// - multiple initial dir

	void GJKSphereSphereSweep()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		TSphere<FReal, 3> B(FVec3(1, 0, 0), 2);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(1,0,0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//initial overlap
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(7, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, false, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//MTD
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(7, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -5);
			EXPECT_VECTOR_NEAR(Position, FVec3(5,0,0), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			
			//EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(9, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -7);	//perfect overlap, will default to 0,0,1 normal
			EXPECT_VECTOR_NEAR(Position, FVec3(10,0,5), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 0, 1), Eps);

			//miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit with thickness
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//hit rotated
			const FRotation3 RotatedDown(FRotation3::FromVector(FVec3(0, PI * 0.5, 0)));
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//miss rotated
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 8.1), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit rotated with inflation
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//near hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//degenerate
			TSphere<FReal, 3> Tiny(FVec3(1, 0, 0), 1e-8);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Tiny, FRigidTransform3::Identity, FVec3(1, 0, 0), 8, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 4, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//right at end
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 2, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);

			// not far enough
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 2 - 1e-2, Time, Position, Normal, 0, InitialDir));
		}
	}

	void GJKSphereBoxSweep()
	{
		FAABB3 A(FVec3(3, -1, 0), FVec3(4, 1, 4));
		TSphere<FReal, 3> B(FVec3(0, 0, 0), 1);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 0), Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(1.5, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 0.5, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 0), Eps);

			//initial overlap
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(4, 0, 4), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, false, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//MTD without EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(4.25, 0, 2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -0.75);
			EXPECT_VECTOR_NEAR(Position, FVec3(4, 0, 2), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(1, 0, 0), Eps);

			//MTD with EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(4, 0, 2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -1);
			EXPECT_VECTOR_NEAR(Position, FVec3(4, 0, 2), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(1, 0, 0), Eps);

			//MTD with EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(3.25, 0, 2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -1.25);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 2), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);

			//MTD with EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(3.4, 0, 3.75), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -1.25);
			EXPECT_VECTOR_NEAR(Position, FVec3(3.4, 0, 4), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 0, 1), Eps);

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(1, 0, 6), FRotation3::Identity), FVec3(1, 0, -1).GetUnsafeNormal(), 4, Time, Position, Normal, 0, InitialDir));
			const FReal ExpectedTime = ((FVec3(3, 0, 4) - FVec3(1, 0, 6)).Size() - 1);
			EXPECT_NEAR(Time, ExpectedTime, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-sqrt(2) / 2, 0, sqrt(2) / 2), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 4), Eps);

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 5+1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));

			//near hit with inflation
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 5 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 2e-2, InitialDir));
			const FReal DistanceFromCorner = (Position - FVec3(3, 0, 4)).Size();
			EXPECT_LT(DistanceFromCorner, 1e-1);

			//rotated box
			const FRotation3 Rotated(FRotation3::FromVector(FVec3(0, 0, PI * 0.5)));
			EXPECT_TRUE(GJKRaycast<FReal>(B, A, FRigidTransform3(FVec3(0), Rotated), FVec3(0, -1, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 1, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(0, 1, 0), Eps);

			//degenerate box
			FAABB3 Needle(FVec3(3, 0, 0), FVec3(4, 0, 0));
			EXPECT_TRUE(GJKRaycast<FReal>(B, Needle, FRigidTransform3(FVec3(0), Rotated), FVec3(0, -1, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 1, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(0, 1, 0), Eps);
		}
	}


	void GJKSphereCapsuleSweep()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		TCapsule<FReal> B(FVec3(1, 0, 0), FVec3(-3, 0, 0), 2);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);
			
			//initial overlap
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(7, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, false, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//MTD
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(7, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -5);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);

			//miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit with thickness
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//hit rotated
			const FRotation3 RotatedDown(FRotation3::FromVector(FVec3(0, PI * 0.5, 0)));
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//miss rotated
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 8.1), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit rotated with inflation
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//near hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//degenerate
			TSphere<FReal, 3> Tiny(FVec3(1, 0, 0), 1e-8);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Tiny, FRigidTransform3::Identity, FVec3(1, 0, 0), 8, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 4, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//right at end
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 2, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);

			// not far enough
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 2 - 1e-2, Time, Position, Normal, 0, InitialDir));
		}
	}


	void GJKSphereConvexSweep()
	{
		//Tetrahedron
		TArray<Chaos::FVec3> HullParticles;
		HullParticles.SetNum(4);
		HullParticles[0] = { 3,0,4 };
		HullParticles[1] = { 3,1,0 };
		HullParticles[2] = { 3,-1,0 };
		HullParticles[3] = { 4,0,2 };
		FConvex A(HullParticles, 0.0f);
		TSphere<FReal, 3> B(FVec3(0, 0, 0), 1);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 0), Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(1.5, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 0.5, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 0), Eps);

			//initial overlap
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(4, 0, 4), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, false, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//MTD
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(2.5, 0, 2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -0.5);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0).GetUnsafeNormal(), Eps);

			//MTD
			FReal Penetration;
			FVec3 ClosestA, ClosestB;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			EXPECT_TRUE((GJKPenetration<false, FReal>(A, B, FRigidTransform3(FVec3(2.5, 0, 2), FRotation3::Identity), Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitialDir)));
			EXPECT_FLOAT_EQ(Penetration, 0.5);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0).GetUnsafeNormal(), Eps);
			EXPECT_NEAR(ClosestA[0], 3, Eps);	//could be any point on face, but should have x == 3
			EXPECT_VECTOR_NEAR(ClosestB, FVec3(3.5, 0, 2), Eps);

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(1, 0, 6), FRotation3::Identity), FVec3(1, 0, -1).GetUnsafeNormal(), 4, Time, Position, Normal, 0, InitialDir));
			const FReal ExpectedTime = ((FVec3(3, 0, 4) - FVec3(1, 0, 6)).Size() - 1);
			EXPECT_NEAR(Time, ExpectedTime, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-sqrt(2) / 2, 0, sqrt(2) / 2), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(3, 0, 4), Eps);

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 5 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));

			//near hit with inflation
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 5 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 2e-2, InitialDir));
			const FReal DistanceFromCorner = (Position - FVec3(3, 0, 4)).Size();
			EXPECT_LT(DistanceFromCorner, 1e-1);

			//rotated box
			const FRotation3 Rotated(FRotation3::FromVector(FVec3(0, 0, PI * 0.5)));
			EXPECT_TRUE(GJKRaycast<FReal>(B, A, FRigidTransform3(FVec3(0), Rotated), FVec3(0, -1, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_NEAR(Normal.X, 0, Eps);
			EXPECT_NEAR(Normal.Y, 1, Eps);
			//EXPECT_NEAR(Normal.Z, 0, Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(0, 1, 0), Eps);

			//degenerate box
			FAABB3 Needle(FVec3(3, 0, 0), FVec3(4, 0, 0));
			EXPECT_TRUE(GJKRaycast<FReal>(B, Needle, FRigidTransform3(FVec3(0), Rotated), FVec3(0, -1, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 1, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(0, 1, 0), Eps);
		}
	}

	void GJKSphereScaledSphereSweep()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);
		TUniquePtr<TSphere<FReal, 3>> Sphere = MakeUnique<TSphere<FReal, 3>>(FVec3(0, 0, 0), 2);
		TImplicitObjectScaled<TSphere<FReal, 3>> Unscaled(MakeSerializable(Sphere), FVec3(1));
		TImplicitObjectScaled<TSphere<FReal, 3>> UniformScaled(MakeSerializable(Sphere), FVec3(2));
		TImplicitObjectScaled<TSphere<FReal, 3>> NonUniformScaled(MakeSerializable(Sphere), FVec3(2, 1, 1));

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 3, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			EXPECT_TRUE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3::Identity, FVec3(1, 0, 0), 6, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			EXPECT_TRUE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			EXPECT_TRUE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 0, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			EXPECT_TRUE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 0, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//initial overlap
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(8, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);
			EXPECT_TRUE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3(FVec3(6, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);
			EXPECT_TRUE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(6, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3(FVec3(0, 0, 9.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit with thickness
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3(FVec3(0, 0, 9.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//hit rotated
			const FRotation3 RotatedInPlace(FRotation3::FromVector(FVec3(0, PI * 0.5, 0)));
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(0, 0, 0), RotatedInPlace), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3(FVec3(0, 0, 0), RotatedInPlace), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(0, 0, 0), RotatedInPlace), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//miss rotated
			EXPECT_FALSE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(0, 0, 7.1), RotatedInPlace), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, UniformScaled, FRigidTransform3(FVec3(0, 0, 9.1), RotatedInPlace), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, NonUniformScaled, FRigidTransform3(FVec3(0, 0, 9.1), RotatedInPlace), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//near hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3(FVec3(0, 0, 7 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//degenerate
			TSphere<FReal, 3> Tiny(FVec3(1, 0, 0), 1e-8);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Tiny, FRigidTransform3::Identity, FVec3(1, 0, 0), 8, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 4, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//right at end
			EXPECT_TRUE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3::Identity, FVec3(1, 0, 0), 3, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 3, Eps);

			// not far enough
			EXPECT_FALSE(GJKRaycast<FReal>(A, Unscaled, FRigidTransform3::Identity, FVec3(1, 0, 0), 3 - 1e-2, Time, Position, Normal, 0, InitialDir));
		}
	}


	void GJKSphereTransformedSphereSweep()
	{
		TSphere<FReal, 3> A(FVec3(10, 0, 0), 5);

		TSphere<FReal, 3> Sphere(FVec3(0), 2);
		TSphere<FReal, 3> Translated(Sphere.GetCenter() + FVec3(1, 0, 0), Sphere.GetRadius());
		TSphere<FReal,3> Transformed(FRigidTransform3(FVec3(1, 0, 0), FRotation3::FromVector(FVec3(0, 0, PI))).TransformPosition(Sphere.GetCenter()), Sphere.GetRadius());

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3::Identity, FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(1, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Position, FVec3(5, 0, 0), Eps);

			//initial overlap
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(7, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(7, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit with thickness
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 7.1), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//hit rotated
			const FRotation3 RotatedDown(FRotation3::FromVector(FVec3(0, PI * 0.5, 0)));
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//miss rotated
			EXPECT_FALSE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 8.1), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 8.1), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//hit rotated with inflation
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 7.9), RotatedDown), FVec3(1, 0, 0), 20, Time, Position, Normal, 0.2, InitialDir));

			//near hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, Translated, FRigidTransform3(FVec3(0, 0, 7 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3(FVec3(0, 0, 7 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 20, Time, Position, Normal, 0, InitialDir));

			//right at end
			EXPECT_TRUE(GJKRaycast<FReal>(A, Translated, FRigidTransform3::Identity, FVec3(1, 0, 0), 2, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3::Identity, FVec3(1, 0, 0), 2, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 2, Eps);

			// not far enough
			EXPECT_FALSE(GJKRaycast<FReal>(A, Translated, FRigidTransform3::Identity, FVec3(1, 0, 0), 2 - 1e-2, Time, Position, Normal, 0, InitialDir));
			EXPECT_FALSE(GJKRaycast<FReal>(A, Transformed, FRigidTransform3::Identity, FVec3(1, 0, 0), 2 - 1e-2, Time, Position, Normal, 0, InitialDir));
		}
	}


	void GJKBoxCapsuleSweep()
	{
		FAABB3 A(FVec3(3, -1, 0), FVec3(4, 1, 4));
		TCapsule<FReal> B(FVec3(0, 0, -1), FVec3(0, 0, 1), 2);

		FVec3 InitialDirs[] = { FVec3(1,0,0), FVec3(-1,0,0), FVec3(0,1,0), FVec3(0,-1,0), FVec3(0,0,1), FVec3(0,0,-1) };

		constexpr FReal Eps = 1e-1;

		for (const FVec3& InitialDir : InitialDirs)
		{
			FReal Time;
			FVec3 Position;
			FVec3 Normal;

			//hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3::Identity, FVec3(1, 0, 0), 2, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 1, Eps);
			EXPECT_NEAR(Normal.X, -1, Eps);
			EXPECT_NEAR(Normal.Y, 0, Eps);
			EXPECT_NEAR(Normal.Z, 0, Eps);
			EXPECT_NEAR(Position.X, 3, Eps);
			//EXPECT_NEAR(Position.Y, 0, Eps);	//todo: look into inaccuracy here (0.015) instead of <1e-2
			EXPECT_LE(Position.Z, 1 + Eps);
			EXPECT_GE(Position.Z, -1 - Eps);

			//hit offset
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0.5, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 0.5, Eps);
			EXPECT_NEAR(Normal.X, -1, Eps);
			EXPECT_NEAR(Normal.Y, 0, Eps);
			EXPECT_NEAR(Normal.Z, 0, Eps);
			EXPECT_NEAR(Position.X, 3, Eps);
			//EXPECT_NEAR(Position.Y, 0, Eps);	//todo: look into inaccuracy here (0.015) instead of <1e-2
			EXPECT_LE(Position.Z, 1 + Eps);
			EXPECT_GE(Position.Z, -1 - Eps);

			//initial overlap
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(3, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 2, Time, Position, Normal, 0, false, InitialDir));
			EXPECT_FLOAT_EQ(Time, 0);

			//MTD
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(2.5, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 2, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -1.5);
			EXPECT_NEAR(Position[0], 3, Eps);	//many possible, but x must be on 3
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);

			//MTD
			FReal Penetration;
			FVec3 ClosestA, ClosestB;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			EXPECT_TRUE((GJKPenetration<false, FReal>(A, B, FRigidTransform3(FVec3(2.5, 0, 0), FRotation3::Identity), Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitialDir)));
			EXPECT_FLOAT_EQ(Penetration, 1.5);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_NEAR(ClosestA[0], 3, Eps);	//could be any point on face, but should have x == 3
			EXPECT_NEAR(ClosestB[0], 4.5, Eps);
			EXPECT_NEAR(ClosestB[1], 0, Eps);

			//EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(3, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 2, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -2);
			EXPECT_NEAR(Position[0], 3, Eps);	//many possible, but x must be on 3
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);

			//EPA
			EXPECT_TRUE((GJKPenetration<false, FReal>(A, B, FRigidTransform3(FVec3(3, 0, 0), FRotation3::Identity), Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitialDir)));
			EXPECT_NEAR(Penetration, 2, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_NEAR(ClosestA[0], 3, Eps);	//could be any point on face, but should have x == 3
			EXPECT_NEAR(ClosestB[0], 5, Eps);
			EXPECT_NEAR(ClosestB[1], 0, Eps);

			//EPA
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(3.25, 0, 0), FRotation3::Identity), FVec3(1, 0, 0), 2, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -2.25);
			EXPECT_NEAR(Position[0], 3, Eps);	//many possible, but x must be on 3
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);

			//EPA
			EXPECT_TRUE((GJKPenetration<false, FReal>(A, B, FRigidTransform3(FVec3(3.25, 0, 0), FRotation3::Identity), Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitialDir)));
			EXPECT_NEAR(Penetration, 2.25, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(-1, 0, 0), Eps);
			EXPECT_NEAR(ClosestA[0], 3, Eps);	//could be any point on face, but should have x == 3
			EXPECT_NEAR(ClosestB[0], 5.25, Eps);
			EXPECT_NEAR(ClosestB[1], 0, Eps);

			//MTD
			EXPECT_TRUE(GJKRaycast2<FReal>(A, B, FRigidTransform3(FVec3(3.25, 0, -2.875), FRotation3::Identity), FVec3(1, 0, 0), 2, Time, Position, Normal, 0, true, InitialDir));
			EXPECT_FLOAT_EQ(Time, -0.125);
			EXPECT_VECTOR_NEAR(Position, FVec3(3.25, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 0, -1), Eps);

			//MTD
			EXPECT_TRUE((GJKPenetration<false, FReal>(A, B, FRigidTransform3(FVec3(3.25, 0, -2.875), FRotation3::Identity), Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitialDir)));
			EXPECT_NEAR(Penetration, 0.125, Eps);
			EXPECT_VECTOR_NEAR(Normal, FVec3(0, 0, -1), Eps);
			EXPECT_VECTOR_NEAR(ClosestA, FVec3(3.25, 0, 0), Eps);
			EXPECT_VECTOR_NEAR(ClosestB, FVec3(3.25, 0, 0.125), Eps);

			//near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 + 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));

			//near hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Position.X, 3, Eps);
			EXPECT_NEAR(Position.Z, 4, 10 * Eps);

			//near hit inflation
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 7 - 1e-2), FRotation3::Identity), FVec3(1, 0, 0), 4, Time, Position, Normal, 2e-2, InitialDir));
			EXPECT_NEAR(Position.X, 3, Eps);
			EXPECT_NEAR(Position.Z, 4, 10 * Eps);

			//rotation hit
			FRotation3 Rotated(FRotation3::FromVector(FVec3(0, -PI * 0.5, 0)));
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(-0.5, 0, 0), Rotated), FVec3(1, 0, 0), 1, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 0.5, Eps);
			EXPECT_NEAR(Position.X, 3, Eps);
			EXPECT_NEAR(Normal.X, -1, Eps);
			EXPECT_NEAR(Normal.Y, 0, Eps);
			EXPECT_NEAR(Normal.Z, 0, Eps);

			//rotation near hit
			EXPECT_TRUE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 6 - 1e-2), Rotated), FVec3(1, 0, 0), 10, Time, Position, Normal, 0, InitialDir));

			//rotation near miss
			EXPECT_FALSE(GJKRaycast<FReal>(A, B, FRigidTransform3(FVec3(0, 0, 6 + 1e-2), Rotated), FVec3(1, 0, 0), 10, Time, Position, Normal, 0, InitialDir));

			//degenerate capsule
			TCapsule<FReal> Needle(FVec3(0, 0, -1), FVec3(0, 0, 1), 1e-8);
			EXPECT_TRUE(GJKRaycast<FReal>(A, Needle, FRigidTransform3::Identity, FVec3(1, 0, 0), 6, Time, Position, Normal, 0, InitialDir));
			EXPECT_NEAR(Time, 3, Eps);
			EXPECT_NEAR(Normal.X, -1, Eps);
			EXPECT_NEAR(Normal.Y, 0, Eps);
			EXPECT_NEAR(Normal.Z, 0, Eps);
			EXPECT_NEAR(Position.X, 3, Eps);
			//EXPECT_NEAR(Position.Y, 0, Eps);	//todo: look into inaccuracy here (0.015) instead of <1e-2
			EXPECT_LE(Position.Z, 1 + Eps);
			EXPECT_GE(Position.Z, -1 - Eps);
		}
	}

	void GJKBoxBoxSweep()
	{
		{
			//based on real sweep from game
			const FAABB3 A(FVec3(-2560.00000, -268.000031, -768.000122), FVec3(0.000000000, 3.99996948, 0.000000000));
			const FAABB3 B(FVec3(-248.000000, -248.000000, -9.99999975e-05), FVec3(248.000000, 248.000000, 9.99999975e-05));
			const FRigidTransform3 BToATM(FVec3(-2559.99780, -511.729492, -8.98901367), FRotation3::FromElements(1.51728955e-06, 1.51728318e-06, 0.707108259, 0.707105279));
			const FVec3 LocalDir(-4.29153351e-06, 0.000000000, -1.00000000);
			const FReal Length = 393.000000;
			const FVec3 SearchDir(511.718750, -2560.00000, 9.00000000);

			FReal Time;
			FVec3 Pos, Normal;
			GJKRaycast2<FReal>(A, B, BToATM, LocalDir, Length, Time, Pos, Normal, 0, true, SearchDir, 0);
		}

		{
			//based on real sweep from game
			TArray<Chaos::FVec3> ConvexParticles;
			ConvexParticles.SetNum(10);

			ConvexParticles[0] = { 51870.2305, 54369.6719, 19200.0000 };
			ConvexParticles[1] = { -91008.5625, -59964.0000, -19199.9629 };
			ConvexParticles[2] = { 51870.2305, 54369.6758, -19199.9668 };
			ConvexParticles[3] = { 22164.4883, 124647.500, -19199.9961 };
			ConvexParticles[4] = { 34478.5000, 123975.492, -19199.9961 };
			ConvexParticles[5] = { -91008.5000, -59963.9375, 19200.0000 };
			ConvexParticles[6] = { -91008.5000, 33715.5625, 19200.0000 };
			ConvexParticles[7] = { 34478.4961, 123975.500, 19200.0000 };
			ConvexParticles[8] = { 22164.4922, 124647.500, 19200.0000 };
			ConvexParticles[9] = { -91008.5000, 33715.5625, -19199.9961 };


			const FConvex A(ConvexParticles, 0.0f);
			const FAABB3 B(FVec3{ -6.00000000, -248.000000, -9.99999975e-05 }, FVec3{ 6.00000000, 248.000000, 9.99999975e-05 });
			const FRigidTransform3 BToATM(FVec3{33470.5000, 41570.5000, -1161.00000}, FRotation3::FromIdentity());
			const FVec3 LocalDir(0, 0, -1);
			const FReal Length = 393.000000;
			const FVec3 SearchDir{ -33470.5000, -41570.5000, 1161.00000 };

			FReal Time;
			FVec3 Pos, Normal;
			GJKRaycast2<FReal>(A, B, BToATM, LocalDir, Length, Time, Pos, Normal, 0, true, SearchDir, 0);
		}
	}

	// When we have a capsule and box that are reported as initially-overlapping because they are within
	// the GJK epsilon opf each other (but actually positively separated), verify that we get a zero time of impact.
	// Previously the slihjtly-positive separation would result in a negative penetration and a positiove TOI.
	// Bug fix: CL 10942094.
	// NOTE: this issue no longer manifests with this example because GJK no longer reports this case as
	// overlapping> The GJK epsilon no longer takes part in the distance calculation when the near point
	// is on the face of the convex.
	GTEST_TEST(GJKTests, DISABLED_TestGJKCapsuleConvexInitialOverlapSweep_Fixed)
	{
		{
			TArray<Chaos::FVec3> ConvexParticles;
			ConvexParticles.SetNum(8);

			ConvexParticles[0] ={-256.000031,12.0000601,384.000061};
			ConvexParticles[1] ={256.000031,12.0000601,384.000061};
			ConvexParticles[2] ={256.000031,12.0000601,6.10351563e-05};
			ConvexParticles[3] ={-256.000031,-11.9999399,6.10351563e-05};
			ConvexParticles[4] ={-256.000031,12.0000601,6.10351563e-05};
			ConvexParticles[5] ={-256.000031,-11.9999399,384.000061};
			ConvexParticles[6] ={256.000031,-11.9999399,6.10351563e-05};
			ConvexParticles[7] ={256.000031,-11.9999399,384.000061};

			TUniquePtr<FConvex> UniqueConvex = MakeUnique<FConvex>(ConvexParticles, 0.0f);
			TSerializablePtr<FConvex> AConv(UniqueConvex);
			const TImplicitObjectScaled<FConvex> A(AConv,FVec3(1.0,1.0,1.0));

			const FVec3 Pt0(0.0,0.0,-33.0);
			FVec3 Pt1 = Pt0;
			Pt1 += (FVec3(0.0,0.0,1.0) * 66.0);

			const TCapsule<FReal> B(Pt0,Pt1,42.0);

			const FRigidTransform3 BToATM(FVec3(157.314758,-54.0000839,76.1436157), FRotation3::FromElements(0.0,0.0,0.704960823,0.709246278));
			const FVec3 LocalDir(-0.00641351938,-0.999979556,0.0);
			const FReal Length = 0.0886496082;
			const FVec3 SearchDir(-3.06152344,166.296631,-76.1436157);

			FReal Time;
			FVec3 Position,Normal;
			EXPECT_TRUE(GJKRaycast2<FReal>(A,B,BToATM,LocalDir,Length,Time,Position,Normal,0,true,SearchDir,0));
			EXPECT_FLOAT_EQ(Time,0.0);
		}
	}

	void GJKCapsuleConvexInitialOverlapSweep()
	{
		{
			TArray<FVec3> ConvexParticles;
			ConvexParticles.SetNum(16);

			ConvexParticles[0] ={-127.216454,203.240234,124.726524};
			ConvexParticles[1] ={125.708847,203.240295,124.726524};
			ConvexParticles[2] ={-120.419685,207.124924,-0.386817127};
			ConvexParticles[3] ={-32.9052734,91.5147095,199.922119};
			ConvexParticles[4] ={118.912071,91.3693237,155.363205};
			ConvexParticles[5] ={31.3977623,91.5147705,199.922150};
			ConvexParticles[6] ={115.392204,91.6678925,162.647476};
			ConvexParticles[7] ={-120.419701,91.1026840,-0.386809498};
			ConvexParticles[8] ={118.912086,207.124985,-0.386806667};
			ConvexParticles[9] ={118.912086,91.1027603,-0.386806667};
			ConvexParticles[10] ={-120.419685,91.3692703,155.363174};
			ConvexParticles[11] ={-110.103012,199.020554,160.910324};
			ConvexParticles[12] ={-116.899742,91.6678467,162.647491};
			ConvexParticles[13] ={31.3977337,194.240265,194.534988};
			ConvexParticles[14] ={-32.9052925,194.240204,194.534958};
			ConvexParticles[15] ={108.595482,199.020599,160.910309};

			auto Convex = MakeShared<FConvex, ESPMode::ThreadSafe>(ConvexParticles, 0.0f);
			const auto& A = *Convex;
			//const TImplicitObjectInstanced<FConvex> A(Convex);

			const FVec3 Pt0(0.0,0.0,-45);
			FVec3 Pt1 = Pt0;
			Pt1 += (FVec3(0.0,0.0,1.0) * 90);

			const TCapsule<FReal> B(Pt0,Pt1,33.8499985);

			const FRigidTransform3 ATM(FVec3(2624.00024, -383.998962, 4.00000000), FRotation3::FromElements(-5.07916162e-08, -3.39378659e-08, 0.555569768, 0.831469893));
			const FRigidTransform3 BTM(FVec3(2461.92749, -205.484283, 106.071632), FRotation3::FromElements(0,0,0,1));
			const FRigidTransform3 BToATM(FVec3(102.903252, 218.050415, 102.071655), FRotation3::FromElements(5.07916162e-08, 3.39378659e-08, -0.555569768, 0.831469893));

			FReal Penetration;
			FVec3 ClosestA,ClosestB,Normal;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			const FVec3 Offset ={162.072754,-178.514679,-102.071632};
			EXPECT_TRUE((GJKPenetration<false, FReal>(A,B,BToATM,Penetration,ClosestA,ClosestB,Normal,ClosestVertexIndexA,ClosestVertexIndexB,0,0,Offset)));

			const FRigidTransform3 NewAToBTM (BToATM.GetTranslation() + (0.01 + Penetration) * Normal,BToATM.GetRotation());;

			EXPECT_FALSE((GJKPenetration<false, FReal>(A,B,NewAToBTM,Penetration,ClosestA,ClosestB,Normal,ClosestVertexIndexA,ClosestVertexIndexB,0,0,Offset)));

		}

		{
			//capsule perfectly aligned with another capsule but a bit off on the z
			const FVec3 Pt0(0.0,0.0,-45.0);
			FVec3 Pt1 = Pt0;
			Pt1 += (FVec3(0.0,0.0,1.0) * 90.0);

			const TCapsule<FReal> A(Pt0,Pt1,34.f);
			const TCapsule<FReal> B(Pt0,Pt1,33.8499985f);

			const FRigidTransform3 BToATM(FVec3(0.0f,0.0f,-23.4092140f), FRotation3::FromElements(0.0,0.0,0.0,1.0));

			EXPECT_TRUE(GJKIntersection<FReal>(A,B,BToATM,0.0,FVec3(0,0,23.4092140)));

			FReal Penetration;
			FVec3 ClosestA,ClosestB,Normal;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			EXPECT_TRUE((GJKPenetration<false, FReal>(A,B,BToATM, Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0.0, 0.0, FVec3(0,0,23.4092140))));
			EXPECT_FLOAT_EQ(Normal.Z,0);
			EXPECT_FLOAT_EQ(Penetration,A.GetRadius() + B.GetRadius());
		}

		{
			//capsule vs triangle as we make the sweep longer the world space point of impact should stay the same
			TArray<FVec3> ConvexParticles;
			ConvexParticles.SetNum(3);

			ConvexParticles[0] ={7400.00000, 12600.0000, 206.248123};
			ConvexParticles[1] ={7500.00000, 12600.0000, 199.994904};
			ConvexParticles[2] ={7500.00000, 12700.0000, 189.837433};
			
			TUniquePtr<FConvex> UniqueConvex = MakeUnique<FConvex>(ConvexParticles, 0.0f);
			TSerializablePtr<FConvex> AConv(UniqueConvex);
			const TImplicitObjectScaled<FConvex> AConvScaled(AConv,FVec3(1.0,1.0,1.0));

			TTriangle<FReal> A(ConvexParticles[0],ConvexParticles[1],ConvexParticles[2]);

			const FVec3 Pt0(0.0,0.0,-29.6999969);
			FVec3 Pt1 = Pt0;
			Pt1 += (FVec3(0.0,0.0,1.0) * 59.3999939);

			const TCapsule<FReal> B(Pt0,Pt1,42.0);

			const FRigidTransform3 BToATM(FVec3(7475.74512, 12603.9082, 277.767120), FRotation3::FromElements(0,0,0,1));
			const FVec3 LocalDir(0,0,-0.999999940);
			const FReal Length = 49.9061584;
			const FVec3 SearchDir(1,0,0);

			FReal Time;
			FVec3 Position,Normal;
			EXPECT_TRUE(GJKRaycast2<FReal>(AConvScaled,B,BToATM,LocalDir,Length,Time,Position,Normal,0,true,SearchDir,0));

			const FRigidTransform3 BToATM2(FVec3(7475.74512, 12603.9082, 277.767120+100), FRotation3::FromElements(0,0,0,1));

			FReal Time2;
			FVec3 Position2,Normal2;
			EXPECT_TRUE(GJKRaycast2<FReal>(AConvScaled,B,BToATM2,LocalDir,Length+100,Time2,Position2,Normal2,0,true,SearchDir,0));
			EXPECT_TRUE(GJKRaycast2<FReal>(A,B,BToATM2,LocalDir,Length+100,Time2,Position2,Normal2,0,true,SearchDir,0));

			EXPECT_NEAR(Time+100,Time2, 1.0f); // TODO: Investigate: This used to be 0
			EXPECT_VECTOR_NEAR(Normal,Normal2,1e-3); // TODO: Investigate: This used to be 1e-4
			EXPECT_VECTOR_NEAR(Position,Position2,1e-1); // TODO: Investigate: This used to be 1e-3
		}

		
		{
			// For this test we are clearly not penetrating
			// but we had an actual bug (edge condition) that showed we are

			const FVec3 Pt0(0.0, 0.0, 0.0);
			FVec3 Pt1(100.0,0,0);
			FVec3 Pt2(0, 1000000.0, 0);
			

			const TCapsule<FReal> A(Pt1, Pt2, 1.0);
			const TSphere<FReal, 3> B(Pt0, 1.0);

			const FRigidTransform3 BToATM(FVec3(0, 0, 0), FRotation3::FromElements(0.0, 0.0, 0, 1)); // Unit transform
			const FVec3 InitDir(0.1, 0.0, 0.0);

			FReal Penetration;
			FVec3 ClosestA, ClosestB, Normal;			
			int32 ClosestVertexIndexA, ClosestVertexIndexB;

			// First demonstrate the distance between the shapes are more than 90cm.
			bool IsValid = GJKPenetration<true, FReal>(A, B, BToATM, Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitDir);
			EXPECT_TRUE(IsValid);
			EXPECT_TRUE(Penetration < -90.0f);

			// Since there is no penetration (by more than 90cm) this function should return false when negative penetration is not supported
			bool IsPenetrating = GJKPenetration<false, FReal>(A, B, BToATM, Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, 0, 0, InitDir);
			EXPECT_FALSE(IsPenetrating);
			
		}
		
	}

	// Check that GJKPenetrationCore returns the correct result when two objects are within various distances
	// of each other. When distance is less that GJKEpsilon, GJK will abort and call into EPA.
	void GJKBoxBoxZeroMarginSeparationTest(FReal GJKEpsilon, FReal SeparationSize, int32 SeparationAxis)
	{
		FVec3 SeparationDir = FVec3(0);
		SeparationDir[SeparationAxis] = 1.0f;
		FVec3 Separation = SeparationSize * SeparationDir;

		// Extents covering both boxes - we will split this in the middle using the separation axis
		FVec3 MinExtent = FVec3(-100, -100, -100);
		FVec3 MaxExtent = FVec3(100, 100, 100);

		// A is most positive along separation axis and shifted by SeperationSize (e.g., the top is axis is Z)
		FVec3 MinA = MinExtent;
		FVec3 MaxA = MaxExtent;
		MinA[SeparationAxis] = SeparationSize;
		MaxA[SeparationAxis] = 100.0f + SeparationSize;

		// B is most negative along separation axis and shifted by SeperationSize (e.g., the bottom if axis is Z)
		FVec3 MinB = MinExtent;
		FVec3 MaxB = MaxExtent;
		MaxB[SeparationAxis] = 0.0f;
		
		// Create the shapes
		float MarginA = 0.0f;
		float MarginB = 0.0f;
		FImplicitBox3 ShapeA(MinA, MaxA, MarginA);
		FImplicitBox3 ShapeB(MinB, MaxB, MarginB);
		const FRigidTransform3 TransformA = FRigidTransform3::Identity;
		const FRigidTransform3 TransformB = FRigidTransform3::Identity;
		const FRigidTransform3 TransformBtoA = FRigidTransform3::Identity;
		const FReal ThicknessA = 0.0f;
		const FReal ThicknessB = 0.0f;

		// Run GJK/EPA
		FReal Penetration;
		FVec3 ClosestA, ClosestBInA, Normal;
		int32 ClosestVertexIndexA, ClosestVertexIndexB;
		bool bSuccess = GJKPenetration<true>(ShapeA, ShapeB, TransformBtoA, Penetration, ClosestA, ClosestBInA, Normal, ClosestVertexIndexA, ClosestVertexIndexB, ThicknessA, ThicknessB, FVec3(1, 0, 0), GJKEpsilon);
		EXPECT_TRUE(bSuccess);

		if (bSuccess)
		{
			// Convert the contact data to world-space (not really necessary here)
			const FVec3 ResultLocation = TransformA.TransformPosition(ClosestA + ThicknessA * Normal);
			const FVec3 ResultNormal = -TransformA.TransformVectorNoScale(Normal);
			const FReal ResultPhi = -Penetration;

			const FReal ExpectedLocationI = SeparationSize;
			const FReal ExpectedNormalI = 1.0f;
			const FReal ExpectedPhi = SeparationSize;

			EXPECT_NEAR(ResultLocation[SeparationAxis], ExpectedLocationI, 1.e-3f) << "Separation " << SeparationSize << " Axis " << SeparationAxis;
			EXPECT_NEAR(ResultNormal[SeparationAxis], ExpectedNormalI, 1.e-4f) << "Separation " << SeparationSize << " Axis " << SeparationAxis;
			EXPECT_NEAR(ResultPhi, ExpectedPhi, 1.e-3f) << "Separation " << SeparationSize << " Axis " << SeparationAxis;
		}
	}

	const FReal BoxBoxGJKDistances[] =
	{
		1.0f,
		1.0f / 2.0f,
		1.0f / 4.0f,
		1.0f / 8.0f,
		1.0f / 16.0f,
		1.0f / 32.0f,
		1.0f / 64.0f,
		1.0f / 128.0f,
		1.0f / 256.0f,
		1.0f / 512.0f,
		1.0f / 1024.0f,
		1.0f / 2048.0f,
		1.0f / 4096.0f,
		1.0f / 8192.0f,
		1.0f / 16384.0f,
		1.0f / 32768.0f,
		1.e-4f,
		1.e-5f,
		1.e-6f,
		1.e-7f,
		1.e-8f,
		0.0f,
	};
	const int32 NumBoxBoxGJKDistances = UE_ARRAY_COUNT(BoxBoxGJKDistances);

	// These tests fails in EPA - we need to cover these cases with SAT
	GTEST_TEST(GJKTests, DISABLED_TestGJKBoxBoxTestFails)
	{
		const FReal Epsilon = 1.e-3f;

		// These are the cases that case EPA to fail out with a degenerate simplex
		GJKBoxBoxZeroMarginSeparationTest(Epsilon, -0.125f, 0);
		GJKBoxBoxZeroMarginSeparationTest(Epsilon, -0.03125, 0);
		GJKBoxBoxZeroMarginSeparationTest(Epsilon, -0.015625, 0);
		GJKBoxBoxZeroMarginSeparationTest(Epsilon, -0.0078125, 0);
		GJKBoxBoxZeroMarginSeparationTest(Epsilon, -0.00390625, 0);
		GJKBoxBoxZeroMarginSeparationTest(Epsilon, -0.001953125, 0);
	}

	// Disabled until we have SAT fallback (see DISABLED_TestGJKBoxBoxTestFails)
	GTEST_TEST(GJKTests, DISABLED_TestGJKBoxBoxNegativeSeparation)
	{
		const FReal Epsilon = 1.e-3f;

		for (int32 DistanceIndex = 0; DistanceIndex < NumBoxBoxGJKDistances; ++DistanceIndex)
		{
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				GJKBoxBoxZeroMarginSeparationTest(Epsilon, -BoxBoxGJKDistances[DistanceIndex], AxisIndex);
			}
		}
	}

	GTEST_TEST(GJKTests, TestGJKBoxBoxPositiveSeparation)
	{
		const FReal Epsilon = 1.e-3f;

		for (int32 DistanceIndex = 0; DistanceIndex < NumBoxBoxGJKDistances; ++DistanceIndex)
		{
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				GJKBoxBoxZeroMarginSeparationTest(Epsilon, BoxBoxGJKDistances[DistanceIndex], AxisIndex);
			}
		}
	}

	// Two convex shapes, Shape A on top of Shape B and almost touching. ShapeA is rotated 90 degrees about Z.
	// Check that the contact point lies between Shape A and Shape B with a near zero Phi.
	// This reproduces a bug where GJKPenetrationCore returns points on top of A and at the bottom
	// of B, with a Phi equal to the separation of those points. Resolving this contact
	// would result in Shape B popping to the top of Shape A.
	//
	// The problem was in EPA where the possible set of simplex faces are added to the queue. Here it checks
	// to see if the origin projects to within the face, since if it does not, it cannot the face that is nearest
	// to the origin. However, without a tolerance, this could reject valid faces.
	void GJKConvexConvexEPABoundaryCondition()
	{
		// These verts are those from a rectangular box with bevelled edges
		TArray<FVec3> CoreShapeVerts = 
		{
			{3.54999995f, -1.04999995f, 0.750000000f},
			{3.75000000f, 1.04999995f, 0.549999952f},
			{3.54999995f, 1.04999995f, 0.750000000f},
			{-3.54999995f, 1.04999995f, 0.750000000f},
			{-3.54999995f, 1.25000000f, 0.549999952f},
			{-3.54999995f, 1.25000000f, -0.550000012f},
			{-3.75000000f, 1.04999995f, 0.549999952f},
			{3.54999995f, 1.25000000f, 0.549999952f},
			{3.54999995f, 1.04999995f, -0.750000000f},
			{3.54999995f, 1.25000000f, -0.550000012f},
			{-3.54999995f, 1.04999995f, -0.750000000f},
			{-3.54999995f, -1.04999995f, -0.750000000f},
			{-3.75000000f, 1.04999995f, -0.550000012f},
			{3.54999995f, -1.25000000f, -0.550000012f},
			{3.54999995f, -1.04999995f, -0.750000000f},
			{-3.54999995f, -1.25000000f, 0.549999952f},
			{-3.54999995f, -1.25000000f, -0.550000012f},
			{-3.75000000f, -1.04999995f, -0.550000012f},
			{3.54999995f, -1.25000000f, 0.549999952f},
			{-3.54999995f, -1.04999995f, 0.750000000f},
			{-3.75000000f, -1.04999995f, 0.549999952f},
			{3.75000000f, -1.04999995f, 0.549999952f},
			{3.75000000f, -1.04999995f, -0.550000012f},
			{3.75000000f, 1.04999995f, -0.550000012f},
		};
		const FVec3 Scale = FVec3(50.0f);
		const FReal Margin = 0.75f;

		TUniquePtr<FImplicitConvex3> CoreConvexShapePtr = MakeUnique<FImplicitConvex3>(CoreShapeVerts, 0.0f);
		const TImplicitObjectScaled<FImplicitConvex3> ShapeA(MakeSerializable(CoreConvexShapePtr), Scale, Margin);
		const TImplicitObjectScaled<FImplicitConvex3> ShapeB(MakeSerializable(CoreConvexShapePtr), Scale, Margin);
		const FRigidTransform3 TransformA(FVec3(0.000000000f, 0.000000000f, 182.378937f), FRotation3::FromElements(0.000000000f, 0.000000000f, 0.707106650f, 0.707106888f));	// Top
		const FRigidTransform3 TransformB(FVec3(0.000000000f, 0.000000000f, 107.378944f), FRotation3::FromElements(0.000000000f, 0.000000000f, 0.000000000f, 1.00000000f));		// Bottom

		// Shape Z extents = [50x-0.75, 50x0.75] = [-37.5, 37.5]
		// Shape Z separation = 182.378937 - 107.378944 = 74.999993
		// i.e., the shapes are touching to near float accuracy
		// The top shape is rotated by 90degrees

		const FRigidTransform3 TransformBtoA = TransformB.GetRelativeTransform(TransformA);

		FReal Penetration;
		FVec3 ClosestA, ClosestBInA, Normal;
		int32 ClosestVertexIndexA, ClosestVertexIndexB;
		const FReal Epsilon = 3.e-3f;

		const FReal ThicknessA = 0.0f;
		const FReal ThicknessB = 0.0f;

		const bool bSuccess = GJKPenetration<true>(ShapeA, ShapeB, TransformBtoA, Penetration, ClosestA, ClosestBInA, Normal, ClosestVertexIndexA, ClosestVertexIndexB, ThicknessA, ThicknessB, FVec3(1,0,0), Epsilon);
		EXPECT_TRUE(bSuccess);

		if (bSuccess)
		{
			const FVec3 ContactLocation = TransformA.TransformPosition(ClosestA + ThicknessA * Normal);
			const FVec3 ContactNormal = -TransformA.TransformVectorNoScale(Normal);
			const FReal ContactPhi = -Penetration;

			// Contact should be on bottom of A
			// Normal should point upwards (from B to A)
			// const FReal PreviousIncorrectLocationZ = TransformA.GetTranslation().Z + ShapeA.BoundingBox().Max().Z;
			// const FReal PreviousIncorrectNormalZ = -1.0f;
			const FReal ExpectedContactLocationZ = TransformA.GetTranslation().Z + ShapeA.BoundingBox().Min().Z;
			const FReal ExpectedContactNormalZ = 1.0f;
			const FReal ExpectedContactPhi = (TransformA.GetTranslation().Z + ShapeA.BoundingBox().Min().Z) - (TransformB.GetTranslation().Z + ShapeB.BoundingBox().Max().Z);

			EXPECT_NEAR(ContactLocation.Z, ExpectedContactLocationZ, KINDA_SMALL_NUMBER);
			EXPECT_NEAR(ContactNormal.Z, ExpectedContactNormalZ, KINDA_SMALL_NUMBER);
			EXPECT_NEAR(ContactPhi, ExpectedContactPhi, KINDA_SMALL_NUMBER);
		}
	}

	GTEST_TEST(GJKTests, TestGJKConvexConvexEPABoundaryCondition)
	{
		GJKConvexConvexEPABoundaryCondition();
	}

}