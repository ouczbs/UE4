// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyCommands.cpp: Empty RHI commands implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"

void FEmptyDynamicRHI::RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBufferRHI, uint32 Offset)
{
	FEmptyVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

}

void FEmptyDynamicRHI::RHISetRasterizerState(FRHIRasterizerState* NewStateRHI)
{
	FEmptyRasterizerState* NewState = ResourceCast(NewStateRHI);

}

void FEmptyDynamicRHI::RHISetComputeShader(FRHIComputeShader* ComputeShaderRHI)
{
	FEmptyComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

}

void FEmptyDynamicRHI::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) 
{ 

}

void FEmptyDynamicRHI::RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{ 
	FEmptyVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

}

void FEmptyDynamicRHI::RHISetViewport(float MinX, float MinY,float MinZ, float MaxX, float MaxY,float MaxZ)
{

}

void FEmptyDynamicRHI::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) 
{ 

}

void FEmptyDynamicRHI::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{

}

void FEmptyDynamicRHI::RHISetBoundShaderState(FRHIBoundShaderState* BoundShaderStateRHI)
{
	FEmptyBoundShaderState* BoundShaderState = ResourceCast(BoundShaderStateRHI);

}

void FEmptyDynamicRHI::RHISetUAVParameter(FRHIPixelShader* PixelShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
	FEmptyUnorderedAccessView* UAV = ResourceCast(UAVRHI);

}

void FEmptyDynamicRHI::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
	FEmptyUnorderedAccessView* UAV = ResourceCast(UAVRHI);

}

void FEmptyDynamicRHI::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI,uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI, uint32 InitialCount)
{
	FEmptyUnorderedAccessView* UAV = ResourceCast(UAVRHI);

}


void FEmptyDynamicRHI::RHISetShaderTexture(FRHIVertexShader* VertexShaderRHI, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FRHIHullShader* HullShader, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FRHIDomainShader* DomainShader, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FRHIGeometryShader* GeometryShader, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FRHIPixelShader* PixelShader, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FRHIComputeShader* ComputeShader, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{

}


void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FRHIComputeShader* ComputeShaderRHI,uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{

}


void FEmptyDynamicRHI::RHISetShaderSampler(FRHIGraphicsShader* ShaderRHI, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
	FEmptySamplerState* NewState = ResourceCast(NewStateRHI);
}

void FEmptyDynamicRHI::RHISetShaderSampler(FRHIComputeShader* ComputeShader, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
	FEmptySamplerState* NewState = ResourceCast(NewStateRHI);

}


void FEmptyDynamicRHI::RHISetShaderParameter(FRHIVertexShader* VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FRHIHullShader* HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FRHIPixelShader* PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FRHIDomainShader* DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FRHIGeometryShader* GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FRHIComputeShader* ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FRHIVertexShader* VertexShaderRHI, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FEmptyUniformBuffer* Buffer = ResourceCast(BufferRHI);
	FEmptyVertexShader* VertexShader = ResourceCast(VertexShaderRHI);

}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FRHIHullShader* HullShader, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FEmptyUniformBuffer* Buffer = ResourceCast(BufferRHI);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FRHIDomainShader* DomainShader, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FEmptyUniformBuffer* Buffer = ResourceCast(BufferRHI);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FRHIGeometryShader* GeometryShader, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FEmptyUniformBuffer* Buffer = ResourceCast(BufferRHI);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FRHIPixelShader* PixelShader, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FEmptyUniformBuffer* Buffer = ResourceCast(BufferRHI);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FRHIComputeShader* ComputeShader, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FEmptyUniformBuffer* Buffer = ResourceCast(BufferRHI);
}


void FEmptyDynamicRHI::RHISetDepthStencilState(FRHIDepthStencilState* NewStateRHI, uint32 StencilRef)
{
	FEmptyDepthStencilState* NewState = ResourceCast(NewStateRHI);

}

void FEmptyDynamicRHI::RHISetBlendState(FRHIBlendState* NewStateRHI, const FLinearColor& BlendFactor)
{
	FEmptyBlendState* NewState = ResourceCast(NewStateRHI);

}


// Occlusion/Timer queries.
void FEmptyDynamicRHI::RHIBeginRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FEmptyRenderQuery* Query = ResourceCast(QueryRHI);

	Query->Begin();
}

void FEmptyDynamicRHI::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FEmptyRenderQuery* Query = ResourceCast(QueryRHI);

	Query->End();
}


void FEmptyDynamicRHI::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
}

void FEmptyDynamicRHI::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FEmptyBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

}


void FEmptyDynamicRHI::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	FEmptyBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

}

void FEmptyDynamicRHI::RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, uint32 PrimitiveType, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	FEmptyBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FEmptyBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);

}

void FEmptyDynamicRHI::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI,uint32 ArgumentOffset)
{
	FEmptyBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FEmptyBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

}


void FEmptyDynamicRHI::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil)
{

}

void FEmptyDynamicRHI::RHIBlockUntilGPUIdle()
{

}

uint32 FEmptyDynamicRHI::RHIGetGPUFrameCycles(uint32 GPUIndex)
{
	return GGPUFrameTime;
}

void* FEmptyDynamicRHI::RHIGetNativeDevice()
{
	return nullptr;
}

void* FEmptyDynamicRHI::RHIGetNativeInstance()
{
	return nullptr;
}


void FEmptyDynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
}

void FEmptyDynamicRHI::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
}
