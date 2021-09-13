// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshRepresentationCommon.h"
#include "MeshUtilities.h"
#include "MeshUtilitiesPrivate.h"
#include "DerivedMeshDataTaskUtils.h"

void MeshUtilities::GenerateStratifiedUniformHemisphereSamples(int32 NumSamples, FRandomStream& RandomStream, TArray<FVector4>& Samples)
{
	const int32 NumThetaSteps = FMath::TruncToInt(FMath::Sqrt(NumSamples / (2.0f * (float)PI)));
	const int32 NumPhiSteps = FMath::TruncToInt(NumThetaSteps * (float)PI);

	Samples.Empty(NumThetaSteps * NumPhiSteps);
	for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
	{
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			const float U1 = RandomStream.GetFraction();
			const float U2 = RandomStream.GetFraction();

			const float Fraction1 = (ThetaIndex + U1) / (float)NumThetaSteps;
			const float Fraction2 = (PhiIndex + U2) / (float)NumPhiSteps;

			const float R = FMath::Sqrt(1.0f - Fraction1 * Fraction1);

			const float Phi = 2.0f * (float)PI * Fraction2;
			// Convert to Cartesian
			Samples.Add(FVector4(FMath::Cos(Phi) * R, FMath::Sin(Phi) * R, Fraction1));
		}
	}
}

// [Frisvad 2012, "Building an Orthonormal Basis from a 3D Unit Vector Without Normalization"]
FMatrix MeshRepresentation::GetTangentBasisFrisvad(FVector TangentZ)
{
	FVector TangentX;
	FVector TangentY;

	if (TangentZ.Z < -0.9999999f)
	{
		TangentX = FVector(0, -1, 0);
		TangentY = FVector(-1, 0, 0);
	}
	else
	{
		float A = 1.0f / (1.0f + TangentZ.Z);
		float B = -TangentZ.X * TangentZ.Y * A;
		TangentX = FVector(1.0f - TangentZ.X * TangentZ.X * A, B, -TangentZ.X);
		TangentY = FVector(B, 1.0f - TangentZ.Y * TangentZ.Y * A, -TangentZ.Y);
	}

	FMatrix LocalBasis;
	LocalBasis.SetIdentity();
	LocalBasis.SetAxis(0, TangentX);
	LocalBasis.SetAxis(1, TangentY);
	LocalBasis.SetAxis(2, TangentZ);
	return LocalBasis;
}

#if USE_EMBREE
void EmbreeFilterFunc(const struct RTCFilterFunctionNArguments* args)
{
	FEmbreeGeometry* EmbreeGeometry = (FEmbreeGeometry*)args->geometryUserPtr;
	FEmbreeTriangleDesc Desc = EmbreeGeometry->TriangleDescs[RTCHitN_primID(args->hit, 1, 0)];

	FEmbreeIntersectionContext& IntersectionContext = *static_cast<FEmbreeIntersectionContext*>(args->context);
	IntersectionContext.ElementIndex = Desc.ElementIndex;
}

void EmbreeErrorFunc(void* userPtr, RTCError code, const char* str)
{
	FString ErrorString;
	TArray<TCHAR>& ErrorStringArray = ErrorString.GetCharArray();
	ErrorStringArray.Empty();

	int32 StrLen = FCStringAnsi::Strlen(str);
	int32 Length = FUTF8ToTCHAR_Convert::ConvertedLength(str, StrLen);
	ErrorStringArray.AddUninitialized(Length + 1); // +1 for the null terminator
	FUTF8ToTCHAR_Convert::Convert(ErrorStringArray.GetData(), ErrorStringArray.Num(), reinterpret_cast<const ANSICHAR*>(str), StrLen);
	ErrorStringArray[Length] = TEXT('\0');

	UE_LOG(LogMeshUtilities, Error, TEXT("Embree error: %s Code=%u"), *ErrorString, (uint32)code);
}
#endif

void MeshRepresentation::SetupEmbreeScene(
	FString MeshName,
	const FSourceMeshDataForDerivedDataTask& SourceMeshData,
	const FStaticMeshLODResources& LODModel,
	const TArray<FSignedDistanceFieldBuildMaterialData>& MaterialBlendModes,
	bool bGenerateAsIfTwoSided,
	FEmbreeScene& EmbreeScene)
{
	const uint32 NumIndices = SourceMeshData.IsValid() ? SourceMeshData.GetNumIndices() : LODModel.IndexBuffer.GetNumIndices();
	const int32 NumTriangles = NumIndices / 3;
	const uint32 NumVertices = SourceMeshData.IsValid() ? SourceMeshData.GetNumVertices() : LODModel.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	EmbreeScene.NumIndices = NumTriangles;

	TArray<FkDOPBuildCollisionTriangle<uint32> > BuildTriangles;

#if USE_EMBREE
	EmbreeScene.bUseEmbree = true;

	if (EmbreeScene.bUseEmbree)
	{
		EmbreeScene.EmbreeDevice = rtcNewDevice(nullptr);
		rtcSetDeviceErrorFunction(EmbreeScene.EmbreeDevice, EmbreeErrorFunc, nullptr);

		RTCError ReturnErrorNewDevice = rtcGetDeviceError(EmbreeScene.EmbreeDevice);
		if (ReturnErrorNewDevice != RTC_ERROR_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcNewDevice failed. Code: %d"), *MeshName, (int32)ReturnErrorNewDevice);
			return;
		}

		EmbreeScene.EmbreeScene = rtcNewScene(EmbreeScene.EmbreeDevice);
		rtcSetSceneFlags(EmbreeScene.EmbreeScene, RTC_SCENE_FLAG_NONE);

		RTCError ReturnErrorNewScene = rtcGetDeviceError(EmbreeScene.EmbreeDevice);
		if (ReturnErrorNewScene != RTC_ERROR_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcNewScene failed. Code: %d"), *MeshName, (int32)ReturnErrorNewScene);
			rtcReleaseDevice(EmbreeScene.EmbreeDevice);
			return;
		}
	}
#endif

	TArray<int32> FilteredTriangles;
	FilteredTriangles.Empty(NumTriangles);

	if (SourceMeshData.IsValid())
	{
		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			const uint32 I0 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 0];
			const uint32 I1 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 1];
			const uint32 I2 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 2];

			const FVector V0 = SourceMeshData.VertexPositions[I0];
			const FVector V1 = SourceMeshData.VertexPositions[I1];
			const FVector V2 = SourceMeshData.VertexPositions[I2];

			const FVector TriangleNormal = ((V1 - V2) ^ (V0 - V2));
			const bool bDegenerateTriangle = TriangleNormal.SizeSquared() < SMALL_NUMBER;
			if (!bDegenerateTriangle)
			{
				FilteredTriangles.Add(TriangleIndex);
			}
		}
	}
	else
	{
		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			const FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
			const uint32 I0 = Indices[TriangleIndex * 3 + 0];
			const uint32 I1 = Indices[TriangleIndex * 3 + 1];
			const uint32 I2 = Indices[TriangleIndex * 3 + 2];

			const FVector V0 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I0);
			const FVector V1 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I1);
			const FVector V2 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I2);

			const FVector TriangleNormal = ((V1 - V2) ^ (V0 - V2));
			const bool bDegenerateTriangle = TriangleNormal.SizeSquared() < SMALL_NUMBER;
			if (!bDegenerateTriangle)
			{
				bool bTriangleIsOpaqueOrMasked = false;

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

					if ((uint32)(TriangleIndex * 3) >= Section.FirstIndex && (uint32)(TriangleIndex * 3) < Section.FirstIndex + Section.NumTriangles * 3)
					{
						if (MaterialBlendModes.IsValidIndex(Section.MaterialIndex))
						{
							bTriangleIsOpaqueOrMasked = !IsTranslucentBlendMode(MaterialBlendModes[Section.MaterialIndex].BlendMode);
						}

						break;
					}
				}

				if (bTriangleIsOpaqueOrMasked)
				{
					FilteredTriangles.Add(TriangleIndex);
				}
			}
		}
	}

	EmbreeScene.Geometry.VertexArray.Empty(NumVertices);
	EmbreeScene.Geometry.VertexArray.AddUninitialized(NumVertices);

	const int32 NumFilteredIndices = FilteredTriangles.Num() * 3;

	EmbreeScene.Geometry.IndexArray.Empty(NumFilteredIndices);
	EmbreeScene.Geometry.IndexArray.AddUninitialized(NumFilteredIndices);

	FVector* EmbreeVertices = EmbreeScene.Geometry.VertexArray.GetData();
	uint32* EmbreeIndices = EmbreeScene.Geometry.IndexArray.GetData();
	EmbreeScene.Geometry.TriangleDescs.Empty(FilteredTriangles.Num());

	for (int32 FilteredTriangleIndex = 0; FilteredTriangleIndex < FilteredTriangles.Num(); FilteredTriangleIndex++)
	{
		uint32 I0, I1, I2;
		FVector V0, V1, V2;

		const int32 TriangleIndex = FilteredTriangles[FilteredTriangleIndex];
		if (SourceMeshData.IsValid())
		{
			I0 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 0];
			I1 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 1];
			I2 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 2];

			V0 = SourceMeshData.VertexPositions[I0];
			V1 = SourceMeshData.VertexPositions[I1];
			V2 = SourceMeshData.VertexPositions[I2];
		}
		else
		{
			const FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
			I0 = Indices[TriangleIndex * 3 + 0];
			I1 = Indices[TriangleIndex * 3 + 1];
			I2 = Indices[TriangleIndex * 3 + 2];

			V0 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I0);
			V1 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I1);
			V2 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I2);
		}

		bool bTriangleIsTwoSided = false;

		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

			if ((uint32)(TriangleIndex * 3) >= Section.FirstIndex && (uint32)(TriangleIndex * 3) < Section.FirstIndex + Section.NumTriangles * 3)
			{
				if (MaterialBlendModes.IsValidIndex(Section.MaterialIndex))
				{
					bTriangleIsTwoSided = MaterialBlendModes[Section.MaterialIndex].bTwoSided;
				}

				break;
			}
		}

		if (EmbreeScene.bUseEmbree)
		{
			EmbreeIndices[FilteredTriangleIndex * 3 + 0] = I0;
			EmbreeIndices[FilteredTriangleIndex * 3 + 1] = I1;
			EmbreeIndices[FilteredTriangleIndex * 3 + 2] = I2;

			EmbreeVertices[I0] = V0;
			EmbreeVertices[I1] = V1;
			EmbreeVertices[I2] = V2;

			FEmbreeTriangleDesc Desc;
			// Store bGenerateAsIfTwoSided in material index
			Desc.ElementIndex = bGenerateAsIfTwoSided || bTriangleIsTwoSided ? 1 : 0;
			EmbreeScene.Geometry.TriangleDescs.Add(Desc);
		}
		else
		{
			BuildTriangles.Add(FkDOPBuildCollisionTriangle<uint32>(
				// Store bGenerateAsIfTwoSided in material index
				bGenerateAsIfTwoSided || bTriangleIsTwoSided ? 1 : 0,
				V0,
				V1,
				V2));
		}
	}

#if USE_EMBREE
	if (EmbreeScene.bUseEmbree)
	{
		RTCGeometry Geometry = rtcNewGeometry(EmbreeScene.EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
		EmbreeScene.Geometry.InternalGeometry = Geometry;

		rtcSetSharedGeometryBuffer(Geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, EmbreeVertices, 0, sizeof(FVector), NumVertices); 
		rtcSetSharedGeometryBuffer(Geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, EmbreeIndices, 0, sizeof(uint32) * 3, FilteredTriangles.Num());

		rtcSetGeometryUserData(Geometry, &EmbreeScene.Geometry);
		rtcSetGeometryIntersectFilterFunction(Geometry, EmbreeFilterFunc);

		rtcCommitGeometry(Geometry);
		rtcAttachGeometry(EmbreeScene.EmbreeScene, Geometry);
		rtcReleaseGeometry(Geometry);

		rtcCommitScene(EmbreeScene.EmbreeScene);

		RTCError ReturnError = rtcGetDeviceError(EmbreeScene.EmbreeDevice);
		if (ReturnError != RTC_ERROR_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcCommitScene failed. Code: %d"), *MeshName, (int32)ReturnError);
			return;
		}
	}
	else
#endif
	{
		EmbreeScene.kDopTree.Build(BuildTriangles);
	}
}

void MeshRepresentation::DeleteEmbreeScene(FEmbreeScene& EmbreeScene)
{
#if USE_EMBREE
	if (EmbreeScene.bUseEmbree)
	{
		rtcReleaseScene(EmbreeScene.EmbreeScene);
		rtcReleaseDevice(EmbreeScene.EmbreeDevice);
	}
#endif
}