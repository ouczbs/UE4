// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenDiffuseIndirect.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "LumenSceneUtils.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "SceneTextureParameters.h"
#include "IndirectLightRendering.h"
#include "LumenRadianceCache.h"
#include "GlobalDistanceField.h"

FLumenGatherCvarState GLumenGatherCvars;

FLumenGatherCvarState::FLumenGatherCvarState()
{
	TraceMeshSDFs = 1;
	MeshSDFTraceDistance = 180.0f;
	SurfaceBias = 5.0f;
	VoxelTracingMode = 0;
}

int32 GAllowLumenDiffuseIndirect = 1;
FAutoConsoleVariableRef CVarLumenGlobalIllumination(
	TEXT("r.Lumen.DiffuseIndirect.Allow"),
	GAllowLumenDiffuseIndirect,
	TEXT("Whether to allow Lumen Global Illumination.  Lumen GI is enabled in the project settings, this cvar can only disable it."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

FAutoConsoleVariableRef GVarLumenDiffuseMaxMeshSDFTraceDistance(
	TEXT("r.Lumen.DiffuseIndirect.MaxMeshSDFTraceDistance"),
	GLumenGatherCvars.MeshSDFTraceDistance,
	TEXT("Max trace distance for the diffuse indirect card rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GDiffuseTraceStepFactor = 1;
FAutoConsoleVariableRef CVarDiffuseTraceStepFactor(
	TEXT("r.Lumen.DiffuseIndirect.TraceStepFactor"),
	GDiffuseTraceStepFactor,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseMinSampleRadius = 10;
FAutoConsoleVariableRef CVarLumenDiffuseMinSampleRadius(
	TEXT("r.Lumen.DiffuseIndirect.MinSampleRadius"),
	GLumenDiffuseMinSampleRadius,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseMinTraceDistance = 0;
FAutoConsoleVariableRef CVarLumenDiffuseMinTraceDistance(
	TEXT("r.Lumen.DiffuseIndirect.MinTraceDistance"),
	GLumenDiffuseMinTraceDistance,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

FAutoConsoleVariableRef CVarLumenDiffuseSurfaceBias(
	TEXT("r.Lumen.DiffuseIndirect.SurfaceBias"),
	GLumenGatherCvars.SurfaceBias,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseCardInterpolateInfluenceRadius = 10;
FAutoConsoleVariableRef CVarDiffuseCardInterpolateInfluenceRadius(
	TEXT("r.Lumen.DiffuseIndirect.CardInterpolateInfluenceRadius"),
	GLumenDiffuseCardInterpolateInfluenceRadius,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseVoxelStepFactor = 1.0f;
FAutoConsoleVariableRef CVarLumenDiffuseVoxelStepFactor(
	TEXT("r.Lumen.DiffuseIndirect.VoxelStepFactor"),
	GLumenDiffuseVoxelStepFactor,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GDiffuseCardTraceEndDistanceFromCamera = 4000.0f;
FAutoConsoleVariableRef CVarDiffuseCardTraceEndDistanceFromCamera(
	TEXT("r.Lumen.DiffuseIndirect.CardTraceEndDistanceFromCamera"),
	GDiffuseCardTraceEndDistanceFromCamera,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenMaxTraceDistance = 10000.0f;
FAutoConsoleVariableRef CVarLumenMaxTraceDistance(
	TEXT("r.Lumen.MaxTraceDistance"),
	GLumenMaxTraceDistance,
	TEXT("Max tracing distance for all tracing methods and Lumen features."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// Project setting driven by RendererSettings
int32 GLumenTraceMeshSDFs = 1;
FAutoConsoleVariableRef CVarLumenTraceMeshSDFs(
	TEXT("r.Lumen.TraceMeshSDFs"),
	GLumenTraceMeshSDFs,
	TEXT("Whether Lumen should trace against Mesh Signed Distance fields.  When enabled, Lumen's Software Tracing will be more accurate, but scenes with high instance density (overlapping meshes) will have high tracing costs.  When disabled, lower resolution Global Signed Distance Field will be used instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// Scalability setting driven by scalability ini
int32 GLumenAllowTracingMeshSDFs = 1;
FAutoConsoleVariableRef CVarLumenAllowTraceMeshSDFs(
	TEXT("r.Lumen.TraceMeshSDFs.Allow"),
	GLumenAllowTracingMeshSDFs,
	TEXT("Whether Lumen should trace against Mesh Signed Distance fields.  When enabled, Lumen's Software Tracing will be more accurate, but scenes with high instance density (overlapping meshes) will have high tracing costs.  When disabled, lower resolution Global Signed Distance Field will be used instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GCardFroxelGridPixelSize = 64;
FAutoConsoleVariableRef CVarLumenDiffuseFroxelGridPixelSize(
	TEXT("r.Lumen.DiffuseIndirect.CullGridPixelSize"),
	GCardFroxelGridPixelSize,
	TEXT("Size of a cell in the card grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GCardGridDistributionLogZScale = .01f;
FAutoConsoleVariableRef CCardGridDistributionLogZScale(
	TEXT("r.Lumen.DiffuseIndirect.CullGridDistributionLogZScale"),
	GCardGridDistributionLogZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GCardGridDistributionLogZOffset = 1.0f;
FAutoConsoleVariableRef CCardGridDistributionLogZOffset(
	TEXT("r.Lumen.DiffuseIndirect.CullGridDistributionLogZOffset"),
	GCardGridDistributionLogZOffset,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GCardGridDistributionZScale = 4.0f;
FAutoConsoleVariableRef CVarCardGridDistributionZScale(
	TEXT("r.Lumen.DiffuseIndirect.CullGridDistributionZScale"),
	GCardGridDistributionZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool Lumen::UseMeshSDFTracing()
{
	return GLumenTraceMeshSDFs != 0 && GLumenAllowTracingMeshSDFs != 0;
}

float Lumen::GetMaxTraceDistance()
{
	return FMath::Clamp(GLumenMaxTraceDistance, .01f, (float)HALF_WORLD_MAX);
}

void FHemisphereDirectionSampleGenerator::GenerateSamples(int32 TargetNumSamples, int32 InPowerOfTwoDivisor, int32 InSeed, bool bInFullSphere, bool bInCosineDistribution)
{
	int32 NumThetaSteps = FMath::TruncToInt(FMath::Sqrt(TargetNumSamples / ((float)PI)));
	//int32 NumPhiSteps = FMath::TruncToInt(NumThetaSteps * (float)PI);
	int32 NumPhiSteps = FMath::DivideAndRoundDown(TargetNumSamples, NumThetaSteps);
	NumPhiSteps = FMath::Max(FMath::DivideAndRoundDown(NumPhiSteps, InPowerOfTwoDivisor), 1) * InPowerOfTwoDivisor;

	if (SampleDirections.Num() != NumThetaSteps * NumPhiSteps || PowerOfTwoDivisor != InPowerOfTwoDivisor || Seed != InSeed || bInFullSphere != bFullSphere)
	{
		SampleDirections.Empty(NumThetaSteps * NumPhiSteps);
		FRandomStream RandomStream(InSeed);

		for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
		{
			for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
			{
				const float U1 = RandomStream.GetFraction();
				const float U2 = RandomStream.GetFraction();

				float Fraction1 = (ThetaIndex + U1) / (float)NumThetaSteps;

				if (bInFullSphere)
				{
					Fraction1 = Fraction1 * 2 - 1;
				}

				const float Fraction2 = (PhiIndex + U2) / (float)NumPhiSteps;
				const float Phi = 2.0f * (float)PI * Fraction2;

				if (bInCosineDistribution)
				{
					const float CosTheta = FMath::Sqrt(Fraction1);
					const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
					SampleDirections.Add(FVector4(FMath::Cos(Phi) * SinTheta, FMath::Sin(Phi) * SinTheta, CosTheta));
				}
				else
				{
					const float CosTheta = Fraction1;
					const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
					SampleDirections.Add(FVector4(FMath::Cos(Phi) * SinTheta, FMath::Sin(Phi) * SinTheta, CosTheta));
				}
			}
		}

		ConeHalfAngle = FMath::Acos(1 - 1.0f / (float)SampleDirections.Num());
		Seed = InSeed;
		PowerOfTwoDivisor = InPowerOfTwoDivisor;
		bFullSphere = bInFullSphere;
		bCosineDistribution = bInCosineDistribution;
	}
}

bool ShouldRenderLumenDiffuseGI(const FScene* Scene, const FViewInfo& View, bool bRequireSoftwareTracing) 
{
	return Lumen::IsLumenFeatureAllowedForView(Scene, View, bRequireSoftwareTracing) 
		&& View.FinalPostProcessSettings.DynamicGlobalIlluminationMethod == EDynamicGlobalIlluminationMethod::Lumen
		&& GAllowLumenDiffuseIndirect != 0
		&& View.Family->EngineShowFlags.GlobalIllumination 
		&& View.Family->EngineShowFlags.LumenGlobalIllumination;
}

void SetupLumenDiffuseTracingParameters(FLumenIndirectTracingParameters& OutParameters)
{
	OutParameters.StepFactor = FMath::Clamp(GDiffuseTraceStepFactor, .1f, 10.0f);
	OutParameters.VoxelStepFactor = FMath::Clamp(GLumenDiffuseVoxelStepFactor, .1f, 10.0f);
	OutParameters.CardTraceEndDistanceFromCamera = GDiffuseCardTraceEndDistanceFromCamera;
	OutParameters.MinSampleRadius = FMath::Clamp(GLumenDiffuseMinSampleRadius, .01f, 100.0f);
	OutParameters.MinTraceDistance = FMath::Clamp(GLumenDiffuseMinTraceDistance, .01f, 1000.0f);
	OutParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance();
	OutParameters.MaxMeshSDFTraceDistance = FMath::Clamp(GLumenGatherCvars.MeshSDFTraceDistance, OutParameters.MinTraceDistance, OutParameters.MaxTraceDistance);
	OutParameters.SurfaceBias = FMath::Clamp(GLumenGatherCvars.SurfaceBias, .01f, 100.0f);
	OutParameters.CardInterpolateInfluenceRadius = FMath::Clamp(GLumenDiffuseCardInterpolateInfluenceRadius, .01f, 1000.0f);
	//@todo - remove
	OutParameters.DiffuseConeHalfAngle = 0.1f;
	OutParameters.TanDiffuseConeHalfAngle = FMath::Tan(OutParameters.DiffuseConeHalfAngle);
	OutParameters.SpecularFromDiffuseRoughnessStart = 0.0f;
	OutParameters.SpecularFromDiffuseRoughnessEnd = 0.0f;
}

void SetupLumenDiffuseTracingParametersForProbe(FLumenIndirectTracingParameters& OutParameters, float DiffuseConeHalfAngle)
{
	SetupLumenDiffuseTracingParameters(OutParameters);

	// Probe tracing doesn't have surface bias, but should bias MinTraceDistance due to the mesh SDF world space error
	OutParameters.SurfaceBias = 0.0f;
	OutParameters.MinTraceDistance = FMath::Clamp(FMath::Max(GLumenGatherCvars.SurfaceBias, GLumenDiffuseMinTraceDistance), .01f, 1000.0f);

	if (DiffuseConeHalfAngle >= 0.0f)
	{
		OutParameters.DiffuseConeHalfAngle = DiffuseConeHalfAngle;
		OutParameters.TanDiffuseConeHalfAngle = FMath::Tan(DiffuseConeHalfAngle);
	}
}

void GetCardGridZParams(float NearPlane, float FarPlane, FVector& OutZParams, int32& OutGridSizeZ)
{
	OutGridSizeZ = FMath::TruncToInt(FMath::Log2((FarPlane - NearPlane) * GCardGridDistributionLogZScale) * GCardGridDistributionZScale) + 1;
	OutZParams = FVector(GCardGridDistributionLogZScale, GCardGridDistributionLogZOffset, GCardGridDistributionZScale);
}

void CullForCardTracing(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	FLumenCardTracingInputs TracingInputs,
	const FLumenIndirectTracingParameters& IndirectTracingParameters,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters)
{
	LLM_SCOPE_BYTAG(Lumen);

	FVector ZParams;
	int32 CardGridSizeZ;
	GetCardGridZParams(View.NearClippingDistance, IndirectTracingParameters.CardTraceEndDistanceFromCamera, ZParams, CardGridSizeZ);

	MeshSDFGridParameters.CardGridPixelSizeShift = FMath::FloorLog2(GCardFroxelGridPixelSize);
	MeshSDFGridParameters.CardGridZParams = ZParams;

	const FIntPoint CardGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GCardFroxelGridPixelSize);
	const FIntVector CullGridSize(CardGridSizeXY.X, CardGridSizeXY.Y, CardGridSizeZ);
	MeshSDFGridParameters.CullGridSize = CullGridSize;

	CullMeshSDFObjectsToViewGrid(
		View,
		Scene,
		IndirectTracingParameters.MaxMeshSDFTraceDistance,
		IndirectTracingParameters.CardTraceEndDistanceFromCamera,
		GCardFroxelGridPixelSize,
		CardGridSizeZ,
		ZParams,
		GraphBuilder,
		MeshSDFGridParameters);
}
