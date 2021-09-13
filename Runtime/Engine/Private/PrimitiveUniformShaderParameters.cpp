// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrimitiveUniformShaderParameters.h"
#include "InstanceUniformShaderParameters.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveSceneInfo.h"
#include "ProfilingDebugging/LoadTimeTracker.h"

void FSinglePrimitiveStructured::InitRHI() 
{
	SCOPED_LOADTIMER(FSinglePrimitiveStructuredBuffer_InitRHI);

	if (RHISupportsComputeShaders(GMaxRHIShaderPlatform))
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("PrimitiveSceneDataBuffer"));

		{	
			PrimitiveSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4), FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s * sizeof(FVector4), BUF_Static | BUF_ShaderResource, CreateInfo);
			PrimitiveSceneDataBufferSRV = RHICreateShaderResourceView(PrimitiveSceneDataBufferRHI);
		}

		{
			CreateInfo.DebugName = TEXT("PrimitiveSceneDataTexture");
			PrimitiveSceneDataTextureRHI = RHICreateTexture2D(FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s, 1, PF_A32B32G32R32F, 1, 1, TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
			PrimitiveSceneDataTextureSRV = RHICreateShaderResourceView(PrimitiveSceneDataTextureRHI, 0);
		}

		CreateInfo.DebugName = TEXT("LightmapSceneDataBuffer");
		LightmapSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4), FLightmapSceneShaderData::LightmapDataStrideInFloat4s * sizeof(FVector4), BUF_Static | BUF_ShaderResource, CreateInfo);
		LightmapSceneDataBufferSRV = RHICreateShaderResourceView(LightmapSceneDataBufferRHI);

		CreateInfo.DebugName = TEXT("InstanceSceneDataBuffer");
		InstanceSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4), FInstanceSceneShaderData::InstanceDataStrideInFloat4s * sizeof(FVector4), BUF_Static | BUF_ShaderResource, CreateInfo);
		InstanceSceneDataBufferSRV = RHICreateShaderResourceView(InstanceSceneDataBufferRHI);

		CreateInfo.DebugName = TEXT("SkyIrradianceEnvironmentMap");
		SkyIrradianceEnvironmentMapRHI = RHICreateStructuredBuffer(sizeof(FVector4), sizeof(FVector4) * 8, BUF_Static | BUF_ShaderResource, CreateInfo);
		SkyIrradianceEnvironmentMapSRV = RHICreateShaderResourceView(SkyIrradianceEnvironmentMapRHI);
	}

	UploadToGPU();
}

void FSinglePrimitiveStructured::UploadToGPU()
{
	if (RHISupportsComputeShaders(GMaxRHIShaderPlatform))
	{
		void* LockedData = nullptr;

		LockedData = RHILockBuffer(PrimitiveSceneDataBufferRHI, 0, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s * sizeof(FVector4), RLM_WriteOnly);
		FPlatformMemory::Memcpy(LockedData, PrimitiveSceneData.Data, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s * sizeof(FVector4));
		RHIUnlockBuffer(PrimitiveSceneDataBufferRHI);

		LockedData = RHILockBuffer(LightmapSceneDataBufferRHI, 0, FLightmapSceneShaderData::LightmapDataStrideInFloat4s * sizeof(FVector4), RLM_WriteOnly);
		FPlatformMemory::Memcpy(LockedData, LightmapSceneData.Data, FLightmapSceneShaderData::LightmapDataStrideInFloat4s * sizeof(FVector4));
		RHIUnlockBuffer(LightmapSceneDataBufferRHI);

		LockedData = RHILockBuffer(InstanceSceneDataBufferRHI, 0, FInstanceSceneShaderData::InstanceDataStrideInFloat4s * sizeof(FVector4), RLM_WriteOnly);
		FPlatformMemory::Memcpy(LockedData, InstanceSceneData.Data, FInstanceSceneShaderData::InstanceDataStrideInFloat4s * sizeof(FVector4));
		RHIUnlockBuffer(InstanceSceneDataBufferRHI);
	}

//#if WITH_EDITOR
	if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5))
	{
		// Create level instance SRV
		FRHIResourceCreateInfo LevelInstanceBufferCreateInfo(TEXT("EditorVisualizeLevelInstanceDataBuffer"));
		EditorVisualizeLevelInstanceDataBufferRHI = RHICreateVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, LevelInstanceBufferCreateInfo);

		void* LockedData = RHILockBuffer(EditorVisualizeLevelInstanceDataBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);

		*reinterpret_cast<uint32*>(LockedData) = 0;

		RHIUnlockBuffer(EditorVisualizeLevelInstanceDataBufferRHI);

		EditorVisualizeLevelInstanceDataBufferSRV = RHICreateShaderResourceView(EditorVisualizeLevelInstanceDataBufferRHI, sizeof(uint32), PF_R32_UINT);

		// Create selection outline SRV
		FRHIResourceCreateInfo SelectionBufferCreateInfo(TEXT("EditorSelectedDataBuffer"));
		EditorSelectedDataBufferRHI = RHICreateVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, SelectionBufferCreateInfo);

		LockedData = RHILockBuffer(EditorSelectedDataBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);

		*reinterpret_cast<uint32*>(LockedData) = 0;

		RHIUnlockBuffer(EditorSelectedDataBufferRHI);

		EditorSelectedDataBufferSRV = RHICreateShaderResourceView(EditorSelectedDataBufferRHI, sizeof(uint32), PF_R32_UINT);
	}
//#endif
}

TGlobalResource<FSinglePrimitiveStructured> GIdentityPrimitiveBuffer;
TGlobalResource<FSinglePrimitiveStructured> GTilePrimitiveBuffer;

FPrimitiveSceneShaderData::FPrimitiveSceneShaderData(const FPrimitiveSceneProxy* RESTRICT Proxy)
{
	bool bHasPrecomputedVolumetricLightmap;
	FMatrix PreviousLocalToWorld;
	int32 SingleCaptureIndex;
	bool bOutputVelocity;

	Proxy->GetScene().GetPrimitiveUniformShaderParameters_RenderThread(Proxy->GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

	FBoxSphereBounds PreSkinnedLocalBounds;
	Proxy->GetPreSkinnedLocalBounds(PreSkinnedLocalBounds);

	FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy->GetPrimitiveSceneInfo();

	Setup(GetPrimitiveUniformShaderParameters(
		Proxy->GetLocalToWorld(),
		PreviousLocalToWorld,
		Proxy->GetActorPosition(), 
		Proxy->GetBounds(), 
		Proxy->GetLocalBounds(),
		PreSkinnedLocalBounds,
		Proxy->ReceivesDecals(), 
		Proxy->HasDistanceFieldRepresentation(), 
		Proxy->HasDynamicIndirectShadowCasterRepresentation(), 
		Proxy->UseSingleSampleShadowFromStationaryLights(),
		bHasPrecomputedVolumetricLightmap,
		Proxy->DrawsVelocity(), 
		Proxy->GetLightingChannelMask(),
		Proxy->GetPrimitiveSceneInfo()->GetLightmapDataOffset(),
		Proxy->GetLightMapCoordinateIndex(),
		SingleCaptureIndex,
		bOutputVelocity,
		Proxy->GetCustomPrimitiveData(),
		Proxy->CastsContactShadow(),
		Proxy->GetPrimitiveSceneInfo()->GetInstanceDataOffset(),
		Proxy->GetPrimitiveSceneInfo()->GetNumInstanceDataEntries(),
		Proxy->CastsDynamicShadow()
		));
}

void FPrimitiveSceneShaderData::Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
{
	static_assert(sizeof(FPrimitiveUniformShaderParameters) == sizeof(FPrimitiveSceneShaderData), "The FPrimitiveSceneShaderData manual layout below and in usf must match FPrimitiveUniformShaderParameters.  Update this assert when adding a new member.");
	
	// Note: layout must match GetPrimitiveData in usf
	Data[0] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[0][0];
	Data[1] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[1][0];
	Data[2] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[2][0];
	Data[3] = *(const FVector4*)&PrimitiveUniformShaderParameters.LocalToWorld.M[3][0];

	Data[4] = PrimitiveUniformShaderParameters.InvNonUniformScaleAndDeterminantSign;
	Data[5] = PrimitiveUniformShaderParameters.ObjectWorldPositionAndRadius;

	Data[6] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[0][0];
	Data[7] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[1][0];
	Data[8] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[2][0];
	Data[9] = *(const FVector4*)&PrimitiveUniformShaderParameters.WorldToLocal.M[3][0];
	Data[10] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[0][0];
	Data[11] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[1][0];
	Data[12] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[2][0];
	Data[13] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousLocalToWorld.M[3][0];
	Data[14] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[0][0];
	Data[15] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[1][0];
	Data[16] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[2][0];
	Data[17] = *(const FVector4*)&PrimitiveUniformShaderParameters.PreviousWorldToLocal.M[3][0];

	Data[18] = FVector4(PrimitiveUniformShaderParameters.ActorWorldPosition, PrimitiveUniformShaderParameters.UseSingleSampleShadowFromStationaryLights);
	Data[19] = FVector4(PrimitiveUniformShaderParameters.ObjectBounds, 0.0f);

	Data[20] = FVector4(
		PrimitiveUniformShaderParameters.DecalReceiverMask, 
		PrimitiveUniformShaderParameters.PerObjectGBufferData, 
		PrimitiveUniformShaderParameters.UseVolumetricLightmapShadowFromStationaryLights, 
		PrimitiveUniformShaderParameters.DrawsVelocity);
	Data[21] = PrimitiveUniformShaderParameters.ObjectOrientation;
	Data[22] = PrimitiveUniformShaderParameters.NonUniformScale;

	// Set W directly in order to bypass NaN check, when passing int through FVector to shader.
	Data[23] = FVector4(PrimitiveUniformShaderParameters.LocalObjectBoundsMin, 0.0f);
	Data[23].W = *(const float*)&PrimitiveUniformShaderParameters.LightingChannelMask;

	Data[24] = FVector4(PrimitiveUniformShaderParameters.LocalObjectBoundsMax, 0.0f);
	Data[24].W = *(const float*)&PrimitiveUniformShaderParameters.LightmapDataIndex;

	Data[25] = FVector4(PrimitiveUniformShaderParameters.PreSkinnedLocalBoundsMin, 0.0f);
	Data[25].W = *(const float*)&PrimitiveUniformShaderParameters.SingleCaptureIndex;

	Data[26] = FVector4(PrimitiveUniformShaderParameters.PreSkinnedLocalBoundsMax, 0.0f);
	Data[26].W = *(const float*)&PrimitiveUniformShaderParameters.OutputVelocity;

	Data[27].X = *(const float*)&PrimitiveUniformShaderParameters.LightmapUVIndex;
	Data[27].Y = *reinterpret_cast<const float*>(&PrimitiveUniformShaderParameters.InstanceDataOffset);
	Data[27].Z = *reinterpret_cast<const float*>(&PrimitiveUniformShaderParameters.NumInstanceDataEntries);
	Data[27].W = *(const float*)&PrimitiveUniformShaderParameters.Flags; // CastShadow=1

	// Set all the custom primitive data float4. This matches the loop in SceneData.ush
	const int32 CustomPrimitiveDataStartIndex = 28;
	for (int i = 0; i < FCustomPrimitiveData::NumCustomPrimitiveDataFloat4s; i++)
	{
		Data[CustomPrimitiveDataStartIndex + i] = PrimitiveUniformShaderParameters.CustomPrimitiveData[i];
	}
}

uint16 FPrimitiveSceneShaderData::GetPrimitivesPerTextureLine()
{
	// @todo texture size limit over 65536, revisit this in the future :). Currently you can have(with primitiveData = 35 floats4) a max of 122,683,392 primitives
	uint16 PrimitivesPerTextureLine = FMath::Min((int32)MAX_uint16, (int32)GMaxTextureDimensions) / (FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s);
	return PrimitivesPerTextureLine;
}
