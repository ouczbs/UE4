// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsRendering.h"
#include "HairStrandsData.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"

static TRDGUniformBufferRef<FHairStrandsViewUniformParameters> InternalCreateHairStrandsViewUniformBuffer(
	FRDGBuilder& GraphBuilder, 
	FHairStrandsVisibilityData* In)
{
	FHairStrandsViewUniformParameters* Parameters = GraphBuilder.AllocParameters<FHairStrandsViewUniformParameters>();
	Parameters->HairDualScatteringRoughnessOverride = GetHairDualScatteringRoughnessOverride();
	if (In && In->CategorizationTexture)
	{
		Parameters->HairCategorizationTexture = In->CategorizationTexture;
		Parameters->HairOnlyDepthTexture = In->HairOnlyDepthTexture;
		Parameters->HairSampleOffset = In->NodeIndex;
		Parameters->HairSampleData = GraphBuilder.CreateSRV(In->NodeData);
		Parameters->HairSampleCoords = GraphBuilder.CreateSRV(In->NodeCoord, FHairStrandsVisibilityData::NodeCoordFormat);
		Parameters->HairSampleCount = In->NodeCount;
		Parameters->HairSampleViewportResolution = In->SampleLightingViewportResolution;
		if (In->TileData.IsValid())
		{
			Parameters->HairTileData = In->TileData.TileDataSRV;
			Parameters->HairTileCount = GraphBuilder.CreateSRV(In->TileData.TileCountBuffer, PF_R32_UINT);
			Parameters->HairTileCountXY = In->TileData.TileCountXY;
		}
		else
		{
			FRDGBufferRef DummyBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 1), TEXT("Hair.DummyBuffer"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DummyBuffer, PF_R16G16_UINT), 0);
			FRDGBufferSRVRef DummyBufferR32SRV = GraphBuilder.CreateSRV(DummyBuffer, PF_R32_UINT);
			FRDGBufferSRVRef DummyBufferRG16SRV = GraphBuilder.CreateSRV(DummyBuffer, PF_R16G16_UINT);
			Parameters->HairTileData = DummyBufferR32SRV;
			Parameters->HairTileCount = DummyBufferRG16SRV;
			Parameters->HairTileCountXY = FIntPoint(0,0);
		}
	}
	else
	{
		FRDGBufferRef DummyBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4,1), TEXT("Hair.DummyBuffer"));
		FRDGBufferRef DummyNodeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(20, 1), TEXT("Hair.DummyNodeBuffer"));

		FRDGTextureRef BlackTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
		FRDGTextureRef ZeroR32_UINT = GSystemTextures.GetZeroUIntDummy(GraphBuilder);
		FRDGTextureRef ZeroRGBA16_UINT = GSystemTextures.GetZeroUShort4Dummy(GraphBuilder);
		FRDGTextureRef FarDepth = GSystemTextures.GetDepthDummy(GraphBuilder);

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DummyNodeBuffer), 0);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DummyBuffer, PF_R16G16_UINT), 0);

		FRDGBufferSRVRef DummyBufferR32SRV = GraphBuilder.CreateSRV(DummyBuffer, PF_R32_UINT);
		FRDGBufferSRVRef DummyBufferRG16SRV = GraphBuilder.CreateSRV(DummyBuffer, PF_R16G16_UINT);

		Parameters->HairOnlyDepthTexture = FarDepth;
		Parameters->HairCategorizationTexture = ZeroRGBA16_UINT;
		Parameters->HairSampleCount = ZeroR32_UINT;
		Parameters->HairSampleOffset = ZeroR32_UINT;
		Parameters->HairSampleCoords = DummyBufferRG16SRV;
		Parameters->HairSampleData	 = GraphBuilder.CreateSRV(DummyNodeBuffer);
		Parameters->HairSampleViewportResolution = FIntPoint(0, 0);

		Parameters->HairTileData = DummyBufferR32SRV;
		Parameters->HairTileCount = DummyBufferRG16SRV;
		Parameters->HairTileCountXY = FIntPoint(0, 0);
	}

	return GraphBuilder.CreateUniformBuffer(Parameters);
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FHairStrandsViewUniformParameters, "HairStrands");

void AddServiceLocalQueuePass(FRDGBuilder& GraphBuilder);

void RenderHairPrePass(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	TArray<FViewInfo>& Views,
	FInstanceCullingManager& InstanceCullingManager)
{
	// #hair_todo: Add multi-view
	for (FViewInfo& View : Views)
	{
		const bool bIsViewCompatible = IsHairStrandsEnabled(EHairStrandsShaderType::Strands, View.GetShaderPlatform());
		if (!View.Family || !bIsViewCompatible)
			continue;

		const ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();

		//SCOPED_GPU_STAT(RHICmdList, HairRendering);
		CreateHairStrandsMacroGroups(GraphBuilder, Scene, View);
		AddServiceLocalQueuePass(GraphBuilder);

		// Voxelization and Deep Opacity Maps
		VoxelizeHairStrands(GraphBuilder, Scene, View, InstanceCullingManager);
		RenderHairStrandsDeepShadows(GraphBuilder, Scene, View, InstanceCullingManager);

		AddServiceLocalQueuePass(GraphBuilder);
	}
}

void RenderHairBasePass(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	const FSceneTextures& SceneTextures,
	TArray<FViewInfo>& Views,
	FInstanceCullingManager& InstanceCullingManager)
{
	for (FViewInfo& View : Views)
	{
		const bool bIsViewCompatible = IsHairStrandsEnabled(EHairStrandsShaderType::Strands, View.GetShaderPlatform());
		if (View.Family && bIsViewCompatible && View.HairStrandsViewData.MacroGroupDatas.Num() > 0)
		{
			RenderHairStrandsVisibilityBuffer(
				GraphBuilder, 
				Scene, 
				View, 
				SceneTextures.GBufferA,
				SceneTextures.GBufferB,
				SceneTextures.GBufferC,
				SceneTextures.GBufferD,
				SceneTextures.GBufferE,
				SceneTextures.Color.Resolve,
				SceneTextures.Depth.Resolve,
				SceneTextures.Velocity,
				InstanceCullingManager);
			
		}
		
		if (View.HairStrandsViewData.VisibilityData.CategorizationTexture)
		{			
			View.HairStrandsViewData.UniformBuffer = InternalCreateHairStrandsViewUniformBuffer(GraphBuilder, &View.HairStrandsViewData.VisibilityData);
			View.HairStrandsViewData.bIsValid = true;
		}
		else
		{
			View.HairStrandsViewData.UniformBuffer = InternalCreateHairStrandsViewUniformBuffer(GraphBuilder, nullptr);
			View.HairStrandsViewData.bIsValid = false;
		}
	}
}

namespace HairStrands
{

TRDGUniformBufferRef<FHairStrandsViewUniformParameters> CreateDefaultHairStrandsViewUniformBuffer(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	return InternalCreateHairStrandsViewUniformBuffer(GraphBuilder, nullptr);
}

TRDGUniformBufferRef<FHairStrandsViewUniformParameters> BindHairStrandsViewUniformParameters(const FViewInfo& View)
{
	return View.HairStrandsViewData.UniformBuffer;
}

TRDGUniformBufferRef<FVirtualVoxelParameters> BindHairStrandsVoxelUniformParameters(const FViewInfo& View)
{
	// Voxel uniform buffer exist only if the view has hair strands data
	check(View.HairStrandsViewData.bIsValid && View.HairStrandsViewData.VirtualVoxelResources.IsValid());
	return View.HairStrandsViewData.VirtualVoxelResources.UniformBuffer;
}

bool HasViewHairStrandsData(const FViewInfo& View)
{
	return View.HairStrandsViewData.bIsValid;
}

bool HasViewHairStrandsVoxelData(const FViewInfo& View)
{
	return View.HairStrandsViewData.bIsValid && View.HairStrandsViewData.VirtualVoxelResources.IsValid();
}

bool HasViewHairStrandsData(const TArrayView<FViewInfo>& Views)
{
	for (const FViewInfo& View : Views)
	{
		if (View.HairStrandsViewData.bIsValid)
		{
			return true;
		}
	}
	return false;
}

}