// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileDecalRendering.cpp: Decals for mobile renderer
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "DecalRenderingShared.h"

extern FRHIRasterizerState* GetDecalRasterizerState(EDecalRasterizerState DecalRasterizerState);
extern void RenderMeshDecalsMobile(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);
void RenderDeferredDecalsMobile(FRHICommandListImmediate& RHICmdList, const FScene& Scene, const FViewInfo& View);

FRHIBlendState* MobileForward_GetDecalBlendState(EDecalBlendMode DecalBlendMode)
{
	switch(DecalBlendMode)
	{
	case DBM_Translucent:
	case DBM_DBuffer_Color:
	case DBM_DBuffer_ColorNormal:
	case DBM_DBuffer_ColorRoughness:
	case DBM_DBuffer_ColorNormalRoughness:
		return TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
	case DBM_Stain:
		// Modulate
		return TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_InverseSourceAlpha>::GetRHI();
	case DBM_Emissive:
	case DBM_DBuffer_Emissive:
		// Additive
		return TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_One>::GetRHI();
	case DBM_AlphaComposite:
	case DBM_DBuffer_AlphaComposite:
	case DBM_DBuffer_EmissiveAlphaComposite:
		// Premultiplied alpha
		return TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
	default:
		check(0);
	};
	return TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
}

FRHIBlendState* MobileDeferred_GetDecalBlendState(EDecalBlendMode DecalBlendMode, bool bHasNormal)
{
	switch (DecalBlendMode)
	{
	case DBM_Translucent:
		if (bHasNormal)
		{
			return TStaticBlendState<
				CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,				// Emissive
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One	// BaseColor
			>::GetRHI();
		}
		else
		{
			return TStaticBlendState<
				CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,				// Emissive
				CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,						
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One	// BaseColor
			>::GetRHI();
		}
	case DBM_Stain:
		if (bHasNormal)
		{
			return TStaticBlendState<
				CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,				// Emissive
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
				CW_RGB, BO_Add, BF_DestColor, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One	// BaseColor
			>::GetRHI();
		}
		else
		{
			return TStaticBlendState<
				CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,				// Emissive
				CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,						
				CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
				CW_RGB, BO_Add, BF_DestColor, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One	// BaseColor
			>::GetRHI();
		}
	case DBM_Emissive:
	case DBM_DBuffer_Emissive:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_SourceAlpha, BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,				
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One
		>::GetRHI();
	
	case DBM_DBuffer_EmissiveAlphaComposite:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_One,	 BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,				
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One
		>::GetRHI();
	
	case DBM_AlphaComposite:
	case DBM_DBuffer_AlphaComposite:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_One,  BF_One, BO_Add, BF_Zero, BF_One,	// Emissive
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,				
			CW_RGB, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
			CW_RGB, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One	// BaseColor
		>::GetRHI();

	case DBM_DBuffer_ColorNormalRoughness:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One  // BaseColor
		>::GetRHI();
	
	case DBM_DBuffer_ColorRoughness:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,						
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One  // BaseColor
		>::GetRHI();

	case DBM_DBuffer_ColorNormal:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,						
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One  // BaseColor
		>::GetRHI();
	
	case DBM_DBuffer_NormalRoughness:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One
		>::GetRHI();

	case DBM_DBuffer_Color:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,						
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One  // BaseColor
		>::GetRHI();

	case DBM_DBuffer_Roughness:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One
		>::GetRHI();
	
	case DBM_Normal:
	case DBM_DBuffer_Normal:
		return TStaticBlendState<
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,	// Normal
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,						
			CW_RGB, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One
		>::GetRHI();

	case DBM_Volumetric_DistanceFunction:
		return TStaticBlendState<>::GetRHI();
	
	case DBM_AmbientOcclusion:
		return TStaticBlendState<CW_RED, BO_Add, BF_DestColor, BF_Zero>::GetRHI();
	
	default:
		check(0);
	};

	return TStaticBlendState<>::GetRHI(); 
}

void FMobileSceneRenderer::RenderDecals(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (!IsMobileHDR() || !ViewFamily.EngineShowFlags.Decals || View.bIsPlanarReflection)
	{
		return;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderDecals);
	SCOPE_CYCLE_COUNTER(STAT_DecalsDrawTime);

	// Deferred decals
	if (Scene->Decals.Num() > 0)
	{
		RenderDeferredDecalsMobile(RHICmdList, *Scene, View);
	}

	// Mesh decals
	if (View.MeshDecalBatches.Num() > 0)
	{
		RenderMeshDecalsMobile(RHICmdList, View);
	}
}

void RenderDeferredDecalsMobile(FRHICommandListImmediate& RHICmdList, const FScene& Scene, const FViewInfo& View)
{
	const uint32 DecalCount = Scene.Decals.Num();
	int32 SortedDecalCount = 0;
	FTransientDecalRenderDataList SortedDecals;

	if (DecalCount > 0)
	{
		// Build a list of decals that need to be rendered for this view
		FDecalRendering::BuildVisibleDecalList(Scene, View, DRS_Mobile, &SortedDecals);
		SortedDecalCount = SortedDecals.Num();
		INC_DWORD_STAT_BY(STAT_Decals, SortedDecalCount);
	}

	if (SortedDecalCount > 0)
	{
		const bool bDeferredShading = IsMobileDeferredShadingEnabled(View.GetShaderPlatform());

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
		RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

		for (int32 DecalIndex = 0; DecalIndex < SortedDecalCount; DecalIndex++)
		{
			const FTransientDecalRenderData& DecalData = SortedDecals[DecalIndex];
			const FDeferredDecalProxy& DecalProxy = *DecalData.DecalProxy;
			const FMatrix ComponentToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();
			const FMatrix FrustumComponentToClip = FDecalRendering::ComputeComponentToClipMatrix(View, ComponentToWorldMatrix);

			const float ConservativeRadius = DecalData.ConservativeRadius;
			const bool bInsideDecal = ((FVector)View.ViewMatrices.GetViewOrigin() - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f);
			bool bReverseHanded = false;
			{
				// Account for the reversal of handedness caused by negative scale on the decal
				const auto& Scale3d = DecalProxy.ComponentTrans.GetScale3D();
				bReverseHanded = Scale3d[0] * Scale3d[1] * Scale3d[2] < 0.f;
			}
			EDecalRasterizerState DecalRasterizerState = FDecalRenderingCommon::ComputeDecalRasterizerState(bInsideDecal, bReverseHanded, View.bReverseCulling);
			GraphicsPSOInit.RasterizerState = GetDecalRasterizerState(DecalRasterizerState);

			if (bInsideDecal)
			{
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
					false, CF_Always,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), 0x00>::GetRHI();
			}
			else
			{
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
					false, CF_DepthNearOrEqual,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), 0x00>::GetRHI();
			}

			if (bDeferredShading)
			{
				GraphicsPSOInit.BlendState = MobileDeferred_GetDecalBlendState(DecalData.FinalDecalBlendMode, DecalData.bHasNormal);
			}
			else
			{
				GraphicsPSOInit.BlendState = MobileForward_GetDecalBlendState(DecalData.FinalDecalBlendMode);
			}

			// Set shader params
			FDecalRendering::SetShader(RHICmdList, GraphicsPSOInit, View, DecalData, DRS_Mobile, FrustumComponentToClip);

			RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), 0, 0, 8, 0, UE_ARRAY_COUNT(GCubeIndices) / 3, 1);
		}
	}
}
