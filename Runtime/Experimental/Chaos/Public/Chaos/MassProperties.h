// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Defines.h"
#include "Chaos/Matrix.h"
#include "Chaos/Rotation.h"
#include "Chaos/Vector.h"
#include "Containers/ArrayView.h"

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
namespace Chaos
{
	template<class T>
	class TTriangleMesh;

	template<class T, int d>
	class TParticles;

	template<class T, int d>
	struct TMassProperties
	{
		TMassProperties()
		    : Mass(0)
			, Volume(0)
		    , CenterOfMass(0)
		    , RotationOfMass(TRotation<T, d>::FromElements(TVector<T, d>(0), 1))
		    , InertiaTensor(0)
		{}
		T Mass;
		T Volume;
		TVector<T, d> CenterOfMass;
		TRotation<T, d> RotationOfMass;
		PMatrix<T, d, d> InertiaTensor;
	};

	template<class T, int d>
	TRotation<T, d> TransformToLocalSpace(PMatrix<T, d, d>& Inertia);

	template<typename T, int d, typename TSurfaces>
	void CalculateVolumeAndCenterOfMass(const TParticles<T, d>& Vertices, const TSurfaces& Surfaces, T& OutVolume, TVector<T, d>& OutCenterOfMass);

	template<class T, int d, typename TSurfaces>
	TMassProperties<T, d> CalculateMassProperties(
	    const TParticles<T, d>& Vertices,
		const TSurfaces& Surfaces,
	    const T Mass);

	template<typename T, int d, typename TSurfaces>
	void CalculateInertiaAndRotationOfMass(const TParticles<T, d>& Vertices, const TSurfaces& Surfaces, const T Density, const TVector<T, d>& CenterOfMass,
	    PMatrix<T, d, d>& OutInertiaTensor, TRotation<T, d>& OutRotationOfMass);


	// Combine a list of transformed inertia tensors into a single inertia. Also diagonalize the inertia
	// and set the rotation of mass accordingly.
	template<class T, int d>
	TMassProperties<T, d> Combine(const TArray<TMassProperties<T, d>>& MPArray);

	// Combine a list of transformed inertia tensors into a single inertia. 
	// NOTE: If there is more than one item in the list, the output may be non-diagonaly and will have a zero rotation.
	// If there is only 1 item in the list it will return it directly, so the rotation of mass may be non-zero.
	template<class T, int d>
	TMassProperties<T, d> CombineWorldSpace(const TArray<TMassProperties<T, d>>& MPArray);

}
#endif
