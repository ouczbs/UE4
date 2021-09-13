// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TiledDeferredLightRendering.cpp: Implementation of tiled deferred shading
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "ShaderPrintParameters.h"
#include "ShaderPrint.h"

#include "SceneFilterRendering.h"
#include "PostProcessing.h"

// TODO: This looks like a strange and hacky way to get hold of this resource, but it seems to work.
ENGINE_API IPooledRenderTarget* GetSubsufaceProfileTexture_RT(FRHICommandListImmediate& RHICmdList);

// This is used to switch on and off the clustered deferred shading implementation, that uses the light grid to perform shading.
int32 GUseClusteredDeferredShading = 0;
static FAutoConsoleVariableRef CVarUseClusteredDeferredShading(
	TEXT("r.UseClusteredDeferredShading"),
	GUseClusteredDeferredShading,
	TEXT("Toggle use of clustered deferred shading for lights that support it. 0 is off (default), 1 is on (also required is SM5 to actually turn on)."),
	ECVF_RenderThreadSafe
);

DECLARE_GPU_STAT_NAMED(ClusteredShading, TEXT("Clustered Shading"));



bool FDeferredShadingSceneRenderer::ShouldUseClusteredDeferredShading() const
{
	// The feature level is the same as in the shader compile conditions below, maybe we don't need SM5?
	// NOTE: should also take into account the conditions for building the light grid, since these 
	//       shaders might have another feature level.
	return GUseClusteredDeferredShading != 0 && Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5;
}

bool FDeferredShadingSceneRenderer::AreLightsInLightGrid() const
{
	return bAreLightsInLightGrid;
}


/**
 * Clustered deferred shading shader. Use a custom vertex shader for hair strands lighting, to covered all sample in sample space
 */
class FClusteredShadingVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClusteredShadingVS);
	SHADER_USE_PARAMETER_STRUCT(FClusteredShadingVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// OLATODO: what level do we actually need for this?
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClusteredShadingVS, "/Engine/Private/ClusteredDeferredShadingVertexShader.usf", "ClusteredShadingVertexShader", SF_Vertex);

/**
 * Clustered deferred shading shader, used in a full-screen pass to apply all lights in the light grid.
 */
class FClusteredShadingPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClusteredShadingPS);
	SHADER_USE_PARAMETER_STRUCT(FClusteredShadingPS, FGlobalShader)

	class FVisualizeLightCullingDim : SHADER_PERMUTATION_BOOL("VISUALIZE_LIGHT_CULLING");
	class FHairStrandsLighting : SHADER_PERMUTATION_BOOL("USE_HAIR_LIGHTING");
	using FPermutationDomain = TShaderPermutationDomain<FVisualizeLightCullingDim, FHairStrandsLighting>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FForwardLightData, Forward)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderDrawDebug::FShaderDrawDebugParameters, ShaderDrawParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ShadowMaskBits)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, HairTransmittanceBuffer)

		SHADER_PARAMETER_TEXTURE(Texture2D, LTCMatTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCMatSampler)

		SHADER_PARAMETER_TEXTURE(Texture2D, LTCAmpTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LTCAmpSampler)

		SHADER_PARAMETER_TEXTURE(Texture2D, SSProfilesTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, TransmissionProfilesLinearSampler)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// OLATODO: what level do we actually need for this?
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClusteredShadingPS, "/Engine/Private/ClusteredDeferredShadingPixelShader.usf", "ClusteredShadingPixelShader", SF_Pixel);

enum class EClusterPassInputType : uint8
{
	GBuffer,
	HairStrands
};

static void InternalAddClusteredDeferredShadingPass(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	const FMinimalSceneTextures& SceneTextures,
	const FSortedLightSetSceneInfo &SortedLightsSet,
	EClusterPassInputType InputType,
	FRDGTextureRef ShadowMaskBits,
	FRDGBufferSRVRef HairTransmittanceBuffer)
{
	check(SortedLightsSet.ClusteredSupportedEnd > 0);
	const FIntPoint SceneTextureExtent = SceneTextures.Config.Extent;
	const bool bHairStrands = InputType == EClusterPassInputType::HairStrands;
	
	FClusteredShadingPS::FParameters *PassParameters = GraphBuilder.AllocParameters<FClusteredShadingPS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
	PassParameters->Forward = View.ForwardLightingResources->ForwardLightDataUniformBuffer;
	PassParameters->SceneTextures = SceneTextures.UniformBuffer;
	PassParameters->ShadowMaskBits = ShadowMaskBits;
	PassParameters->HairTransmittanceBuffer = HairTransmittanceBuffer;

	PassParameters->LTCMatTexture = GSystemTextures.LTCMat->GetShaderResourceRHI();
	PassParameters->LTCMatSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->LTCAmpTexture = GSystemTextures.LTCAmp->GetShaderResourceRHI();
	PassParameters->LTCAmpSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->SSProfilesTexture = GetSubsufaceProfileTexture_RT(GraphBuilder.RHICmdList)->GetShaderResourceRHI();
	PassParameters->TransmissionProfilesLinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	if (ShaderDrawDebug::IsShaderDrawDebugEnabled(View))
	{
		ShaderDrawDebug::SetParameters(GraphBuilder, View.ShaderDrawData, PassParameters->ShaderDrawParameters);
	}

	if (ShaderPrint::IsSupported(View) && ShaderPrint::IsEnabled())
	{
		ShaderPrint::SetParameters(GraphBuilder, View, PassParameters->ShaderPrintUniformBuffer);
	}

	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
	if (bHairStrands)
	{
		PassParameters->RenderTargets[0] = FRenderTargetBinding(View.HairStrandsViewData.VisibilityData.SampleLightingBuffer, ERenderTargetLoadAction::ELoad);
	}

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ClusteredDeferredShading(%s), #Lights: %d", bHairStrands ? TEXT("HairStrands") : TEXT("GBuffer"), SortedLightsSet.ClusteredSupportedEnd),
		PassParameters,
		ERDGPassFlags::Raster,
		[PassParameters, &View, SceneTextureExtent, bHairStrands](FRHICommandListImmediate& InRHICmdList)
	{
		TShaderMapRef<FClusteredShadingVS> HairVertexShader(View.ShaderMap);
		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

		FClusteredShadingPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FClusteredShadingPS::FVisualizeLightCullingDim>(View.Family->EngineShowFlags.VisualizeLightCulling);
		PermutationVector.Set<FClusteredShadingPS::FHairStrandsLighting>(bHairStrands);
		TShaderMapRef<FClusteredShadingPS> PixelShader(View.ShaderMap, PermutationVector);
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			// Additive blend to accumulate lighting contributions.
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = bHairStrands ? HairVertexShader.GetVertexShader() : VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit);
		}

		SetShaderParameters(InRHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

		if (bHairStrands)
		{
			InRHICmdList.SetViewport(0, 0, 0.0f, View.HairStrandsViewData.VisibilityData.SampleLightingViewportResolution.X, View.HairStrandsViewData.VisibilityData.SampleLightingViewportResolution.Y, 1.0f);

			FClusteredShadingVS::FParameters VertexParameters;
			VertexParameters.View = PassParameters->View;
			VertexParameters.HairStrands = PassParameters->HairStrands;
			VertexParameters.SceneTextures = PassParameters->SceneTextures;
			SetShaderParameters(InRHICmdList, HairVertexShader, HairVertexShader.GetVertexShader(), VertexParameters);
			InRHICmdList.SetStreamSource(0, nullptr, 0);
			InRHICmdList.DrawPrimitive(0, 1, 1);
		}
		else
		{
			InRHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			DrawRectangle(InRHICmdList, 0, 0, View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()), SceneTextureExtent, VertexShader);
		}
	});
}


void FDeferredShadingSceneRenderer::AddClusteredDeferredShadingPass(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FSortedLightSetSceneInfo &SortedLightsSet,
	FRDGTextureRef ShadowMaskBits,
	FRDGTextureRef HairStrandsShadowMaskBits)
{
	check(GUseClusteredDeferredShading);

	const int32 NumLightsToRender = SortedLightsSet.ClusteredSupportedEnd;

	if (NumLightsToRender > 0)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, ClusteredShading);
		RDG_EVENT_SCOPE(GraphBuilder, "ClusteredShading");

		for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			InternalAddClusteredDeferredShadingPass(
				GraphBuilder,
				View,
				SceneTextures,
				SortedLightsSet,
				EClusterPassInputType::GBuffer,
				ShadowMaskBits,
				nullptr);

			if (HairStrands::HasViewHairStrandsData(View))
			{
				FHairStrandsTransmittanceMaskData TransmittanceMask = RenderHairStrandsOnePassTransmittanceMask(GraphBuilder, View, HairStrandsShadowMaskBits);
				InternalAddClusteredDeferredShadingPass(
					GraphBuilder,
					View,
					SceneTextures,
					SortedLightsSet,
					EClusterPassInputType::HairStrands,
					HairStrandsShadowMaskBits,
					GraphBuilder.CreateSRV(TransmittanceMask.TransmittanceMask, FHairStrandsTransmittanceMaskData::Format));
			}
		}
	}
}
