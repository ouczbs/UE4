// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenReflectionTracing.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "LumenReflections.h"

int32 GLumenReflectionScreenTraces = 1;
FAutoConsoleVariableRef CVarLumenReflectionScreenTraces(
	TEXT("r.Lumen.Reflections.ScreenTraces"),
	GLumenReflectionScreenTraces,
	TEXT("Whether to trace against the screen for reflections before falling back to other methods."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReflectionHierarchicalScreenTracesMaxIterations = 50;
FAutoConsoleVariableRef CVarLumenReflectionHierarchicalScreenTracesMaxIterations(
	TEXT("r.Lumen.Reflections.HierarchicalScreenTraces.MaxIterations"),
	GLumenReflectionHierarchicalScreenTracesMaxIterations,
	TEXT("Max iterations for HZB tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReflectionHierarchicalScreenTraceRelativeDepthThreshold = .01f;
FAutoConsoleVariableRef GVarLumenReflectionHierarchicalScreenTraceRelativeDepthThreshold(
	TEXT("r.Lumen.Reflections.HierarchicalScreenTraces.UncertainTraceRelativeDepthThreshold"),
	GLumenReflectionHierarchicalScreenTraceRelativeDepthThreshold,
	TEXT("Determines depth thickness of objects hit by HZB tracing, as a relative depth threshold."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

class FReflectionClearTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionClearTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionClearTracesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionClearTracesCS, "/Engine/Private/Lumen/LumenReflectionTracing.usf", "ReflectionClearTracesCS", SF_Compute);

class FReflectionTraceScreenTexturesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionTraceScreenTexturesCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionTraceScreenTexturesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ClosestHZBTexture)
		SHADER_PARAMETER(FVector4, HZBUvFactorAndInvFactor)
		SHADER_PARAMETER(FVector4, PrevScreenPositionScaleBias)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER(FVector2D, HZBBaseTexelSize)
		SHADER_PARAMETER(FVector4, HZBUVToScreenUVScaleBias)
		SHADER_PARAMETER(float, MaxHierarchicalScreenTraceIterations)
		SHADER_PARAMETER(float, UncertainTraceRelativeDepthThreshold)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionTraceScreenTexturesCS, "/Engine/Private/Lumen/LumenReflectionTracing.usf", "ReflectionTraceScreenTexturesCS", SF_Compute);

class FReflectionCompactTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionCompactTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionCompactTracesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
		SHADER_PARAMETER(float, CompactionTracingEndDistanceFromCamera)
		SHADER_PARAMETER(float, CompactionMaxTraceDistance)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedTraceTexelAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedTraceTexelData)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DIFFUSE_TRACE_CARDS"), 1);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionCompactTracesCS, "/Engine/Private/Lumen/LumenReflectionTracing.usf", "ReflectionCompactTracesCS", SF_Compute);


class FSetupReflectionCompactedTracesIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetupReflectionCompactedTracesIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FSetupReflectionCompactedTracesIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionCompactTracingIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWReflectionCompactRayTraceDispatchIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CompactedTraceTexelAllocator)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSetupReflectionCompactedTracesIndirectArgsCS, "/Engine/Private/Lumen/LumenReflectionTracing.usf", "SetupCompactedTracesIndirectArgsCS", SF_Compute);

class FReflectionTraceMeshSDFsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionTraceMeshSDFsCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionTraceMeshSDFsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenMeshSDFGridParameters, MeshSDFGridParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedReflectionTraceParameters, CompactedTraceParameters)
	END_SHADER_PARAMETER_STRUCT()
		
	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionTraceMeshSDFsCS, "/Engine/Private/Lumen/LumenReflectionTracing.usf", "ReflectionTraceMeshSDFsCS", SF_Compute);


class FReflectionTraceVoxelsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReflectionTraceVoxelsCS)
	SHADER_USE_PARAMETER_STRUCT(FReflectionTraceVoxelsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedReflectionTraceParameters, CompactedTraceParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FDynamicSkyLight>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FReflectionTraceVoxelsCS, "/Engine/Private/Lumen/LumenReflectionTracing.usf", "ReflectionTraceVoxelsCS", SF_Compute);


FCompactedReflectionTraceParameters CompactTraces(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	float CompactionTracingEndDistanceFromCamera,
	float CompactionMaxTraceDistance)
{
	FCompactedReflectionTraceParameters CompactedTraceParameters;
	FRDGBufferRef CompactedTraceTexelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.Reflections.CompactedTraceTexelAllocator"));
	const int32 NumCompactedTraceTexelDataElements = ReflectionTracingParameters.ReflectionTracingBufferSize.X * ReflectionTracingParameters.ReflectionTracingBufferSize.Y;
	FRDGBufferRef CompactedTraceTexelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, NumCompactedTraceTexelDataElements), TEXT("Lumen.Reflections.CompactedTraceTexelData"));

	CompactedTraceParameters.IndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflections.CompactTracingIndirectArgs"));
	CompactedTraceParameters.RayTraceDispatchIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflections.CompactRayTraceDispatchIndirectArgs"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT), 0);

	{
		FReflectionCompactTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionCompactTracesCS::FParameters>();
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->RWCompactedTraceTexelAllocator = GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT);
		PassParameters->RWCompactedTraceTexelData = GraphBuilder.CreateUAV(CompactedTraceTexelData, PF_R32G32_UINT);
		PassParameters->CompactionTracingEndDistanceFromCamera = CompactionTracingEndDistanceFromCamera;
		PassParameters->CompactionMaxTraceDistance = CompactionMaxTraceDistance;

		auto ComputeShader = View.ShaderMap->GetShader<FReflectionCompactTracesCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactTraces"),
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.TracingIndirectArgs,
			0);
	}

	{
		FSetupReflectionCompactedTracesIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetupReflectionCompactedTracesIndirectArgsCS::FParameters>();
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->RWReflectionCompactTracingIndirectArgs = GraphBuilder.CreateUAV(CompactedTraceParameters.IndirectArgs, PF_R32_UINT);
		PassParameters->RWReflectionCompactRayTraceDispatchIndirectArgs = GraphBuilder.CreateUAV(CompactedTraceParameters.RayTraceDispatchIndirectArgs, PF_R32_UINT);
		PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CompactedTraceTexelAllocator, PF_R32_UINT));

		auto ComputeShader = View.ShaderMap->GetShader<FSetupReflectionCompactedTracesIndirectArgsCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SetupCompactedTracesIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	CompactedTraceParameters.CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CompactedTraceTexelAllocator, PF_R32_UINT));
	CompactedTraceParameters.CompactedTraceTexelData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CompactedTraceTexelData, PF_R32G32_UINT));

	return CompactedTraceParameters;
}

void SetupIndirectTracingParametersForReflections(FLumenIndirectTracingParameters& OutParameters)
{
	//@todo - cleanup
	OutParameters.StepFactor = 1.0f;
	OutParameters.VoxelStepFactor = 1.0f;
	extern float GDiffuseCardTraceEndDistanceFromCamera;
	OutParameters.CardTraceEndDistanceFromCamera = GDiffuseCardTraceEndDistanceFromCamera;
	OutParameters.MinSampleRadius = 0.0f;
	OutParameters.MinTraceDistance = 0.0f;
	OutParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance();
	extern FLumenGatherCvarState GLumenGatherCvars;
	OutParameters.MaxMeshSDFTraceDistance = FMath::Clamp(GLumenGatherCvars.MeshSDFTraceDistance, OutParameters.MinTraceDistance, OutParameters.MaxTraceDistance);
	OutParameters.SurfaceBias = FMath::Clamp(GLumenGatherCvars.SurfaceBias, .01f, 100.0f);
	OutParameters.CardInterpolateInfluenceRadius = 10.0f;
	OutParameters.DiffuseConeHalfAngle = 0.0f;
	OutParameters.TanDiffuseConeHalfAngle = 0.0f;
	OutParameters.SpecularFromDiffuseRoughnessStart = 0.0f;
	OutParameters.SpecularFromDiffuseRoughnessEnd = 0.0f;
}

void TraceReflections(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	bool bTraceMeshSDFs,
	const FSceneTextures& SceneTextures,
	const FLumenCardTracingInputs& TracingInputs,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	const FLumenMeshSDFGridParameters& InMeshSDFGridParameters)
{
	{
		FReflectionClearTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionClearTracesCS::FParameters>();
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FReflectionClearTracesCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearTraces"),
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.TracingIndirectArgs,
			0);
	}

	FLumenIndirectTracingParameters IndirectTracingParameters;
	SetupIndirectTracingParametersForReflections(IndirectTracingParameters);

	const FSceneTextureParameters& SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures);

	const bool bScreenTraces = GLumenReflectionScreenTraces != 0;

	if (bScreenTraces)
	{
		FReflectionTraceScreenTexturesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionTraceScreenTexturesCS::FParameters>();

		FRDGTextureRef CurrentSceneColor = SceneTextures.Color.Resolve;
		FRDGTextureRef InputColor = CurrentSceneColor;

		if (View.PrevViewInfo.TemporalAAHistory.IsValid())
		{
			InputColor = GraphBuilder.RegisterExternalTexture(View.PrevViewInfo.TemporalAAHistory.RT[0]);
		}

		{
			const FVector2D HZBUvFactor(
				float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
				float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y));
			PassParameters->HZBUvFactorAndInvFactor = FVector4(
				HZBUvFactor.X,
				HZBUvFactor.Y,
				1.0f / HZBUvFactor.X,
				1.0f / HZBUvFactor.Y);

			const FVector4 ScreenPositionScaleBias = View.GetScreenPositionScaleBias(SceneTextures.Config.Extent, View.ViewRect);
			const FVector2D HZBUVToScreenUVScale = FVector2D(1.0f / HZBUvFactor.X, 1.0f / HZBUvFactor.Y) * FVector2D(2.0f, -2.0f) * FVector2D(ScreenPositionScaleBias.X, ScreenPositionScaleBias.Y);
			const FVector2D HZBUVToScreenUVBias = FVector2D(-1.0f, 1.0f) * FVector2D(ScreenPositionScaleBias.X, ScreenPositionScaleBias.Y) + FVector2D(ScreenPositionScaleBias.W, ScreenPositionScaleBias.Z);
			PassParameters->HZBUVToScreenUVScaleBias = FVector4(HZBUVToScreenUVScale, HZBUVToScreenUVBias);
		}

		{
			FIntPoint ViewportOffset = View.ViewRect.Min;
			FIntPoint ViewportExtent = View.ViewRect.Size();
			FIntPoint BufferSize = SceneTextures.Config.Extent;

			if (View.PrevViewInfo.TemporalAAHistory.IsValid())
			{
				ViewportOffset = View.PrevViewInfo.TemporalAAHistory.ViewportRect.Min;
				ViewportExtent = View.PrevViewInfo.TemporalAAHistory.ViewportRect.Size();
				BufferSize = View.PrevViewInfo.TemporalAAHistory.ReferenceBufferSize;
			}

			FVector2D InvBufferSize(1.0f / float(BufferSize.X), 1.0f / float(BufferSize.Y));

			PassParameters->PrevScreenPositionScaleBias = FVector4(
				ViewportExtent.X * 0.5f * InvBufferSize.X,
				-ViewportExtent.Y * 0.5f * InvBufferSize.Y,
				(ViewportExtent.X * 0.5f + ViewportOffset.X) * InvBufferSize.X,
				(ViewportExtent.Y * 0.5f + ViewportOffset.Y) * InvBufferSize.Y);
		}

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->PrevSceneColorPreExposureCorrection = InputColor != CurrentSceneColor ? View.PreExposure / View.PrevViewInfo.SceneColorPreExposure : 1.0f;
		PassParameters->SceneTextures = SceneTextureParameters;
		PassParameters->ColorTexture = InputColor;

		if (InputColor == CurrentSceneColor || !PassParameters->SceneTextures.GBufferVelocityTexture)
		{
			PassParameters->SceneTextures.GBufferVelocityTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
		}

		checkf(View.ClosestHZB, TEXT("Lumen screen tracing: ClosestHZB was not setup, should have been setup by FDeferredShadingSceneRenderer::RenderHzb"));
		PassParameters->ClosestHZBTexture = View.ClosestHZB;
		PassParameters->HZBBaseTexelSize = FVector2D(1.0f / View.ClosestHZB->Desc.Extent.X, 1.0f / View.ClosestHZB->Desc.Extent.Y);
		PassParameters->MaxHierarchicalScreenTraceIterations = GLumenReflectionHierarchicalScreenTracesMaxIterations;
		PassParameters->UncertainTraceRelativeDepthThreshold = GLumenReflectionHierarchicalScreenTraceRelativeDepthThreshold;

		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->ReflectionTileParameters = ReflectionTileParameters;
		PassParameters->IndirectTracingParameters = IndirectTracingParameters;

		FReflectionTraceScreenTexturesCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionTraceScreenTexturesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TraceScreen"),
			ComputeShader,
			PassParameters,
			ReflectionTileParameters.TracingIndirectArgs,
			0);
	}
	
	if (Lumen::UseHardwareRayTracedReflections())
	{
		FCompactedReflectionTraceParameters CompactedTraceParameters = CompactTraces(
			GraphBuilder,
			View,
			ReflectionTracingParameters,
			ReflectionTileParameters,
			WORLD_MAX,
			IndirectTracingParameters.MaxTraceDistance);

		RenderLumenHardwareRayTracingReflections(
			GraphBuilder,
			SceneTextureParameters,
			View,
			ReflectionTracingParameters,
			ReflectionTileParameters,
			TracingInputs,
			CompactedTraceParameters,
			IndirectTracingParameters.MaxTraceDistance);
	}
	else if (bTraceMeshSDFs)
	{
		FLumenMeshSDFGridParameters MeshSDFGridParameters = InMeshSDFGridParameters;
		if (!MeshSDFGridParameters.NumGridCulledMeshSDFObjects)
		{
			CullForCardTracing(
				GraphBuilder,
				Scene, View,
				TracingInputs,
				IndirectTracingParameters,
				/* out */ MeshSDFGridParameters);
		}

		if (MeshSDFGridParameters.TracingParameters.DistanceFieldObjectBuffers.NumSceneObjects > 0)
		{
			FCompactedReflectionTraceParameters CompactedTraceParameters = CompactTraces(
				GraphBuilder,
				View,
				ReflectionTracingParameters,
				ReflectionTileParameters,
				IndirectTracingParameters.CardTraceEndDistanceFromCamera,
				IndirectTracingParameters.MaxMeshSDFTraceDistance);

			{
				FReflectionTraceMeshSDFsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionTraceMeshSDFsCS::FParameters>();
				GetLumenCardTracingParameters(View, TracingInputs, PassParameters->TracingParameters);
				PassParameters->MeshSDFGridParameters = MeshSDFGridParameters;
				PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
				PassParameters->IndirectTracingParameters = IndirectTracingParameters;
				PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
				PassParameters->CompactedTraceParameters = CompactedTraceParameters;

				FReflectionTraceMeshSDFsCS::FPermutationDomain PermutationVector;
				auto ComputeShader = View.ShaderMap->GetShader<FReflectionTraceMeshSDFsCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TraceMeshSDFs"),
					ComputeShader,
					PassParameters,
					CompactedTraceParameters.IndirectArgs,
					0);
			}
		}
	}

	FCompactedReflectionTraceParameters CompactedTraceParameters = CompactTraces(
		GraphBuilder,
		View,
		ReflectionTracingParameters,
		ReflectionTileParameters,
		WORLD_MAX,
		// Make sure the shader runs on all misses to apply radiance cache + skylight
		IndirectTracingParameters.MaxTraceDistance + 1);

	{
		FReflectionTraceVoxelsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReflectionTraceVoxelsCS::FParameters>();
		GetLumenCardTracingParameters(View, TracingInputs, PassParameters->TracingParameters);
		PassParameters->ReflectionTracingParameters = ReflectionTracingParameters;
		PassParameters->IndirectTracingParameters = IndirectTracingParameters;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;

		FReflectionTraceVoxelsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FReflectionTraceVoxelsCS::FDynamicSkyLight >(Lumen::ShouldHandleSkyLight(Scene, *View.Family));
		auto ComputeShader = View.ShaderMap->GetShader<FReflectionTraceVoxelsCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TraceVoxels"),
			ComputeShader,
			PassParameters,
			CompactedTraceParameters.IndirectArgs,
			0);
	}
}
