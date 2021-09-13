// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyIndexBuffer.cpp: Empty Index buffer RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


/** Constructor */
FEmptyIndexBuffer::FEmptyIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIBuffer(InStride, InSize, InUsage)
{
}

void* FEmptyIndexBuffer::Lock(EResourceLockMode LockMode, uint32 Size)
{
	return NULL;
}

void FEmptyIndexBuffer::Unlock()
{

}

FBufferRHIRef FEmptyDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.bCreateRHIObjectOnly)
	{
		return new FEmptyIndexBuffer();
	}

	// make the RHI object, which will allocate memory
	FEmptyIndexBuffer* IndexBuffer = new FEmptyIndexBuffer(Stride, Size, InUsage);
	
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockBuffer(IndexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return IndexBuffer;
}

void* FEmptyDynamicRHI::LockIndexBuffer_BottomOfPipe(FRHICommandListImmediate& RHICmdList, FRHIBuffer* IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FEmptyIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	return (uint8*)IndexBuffer->Lock(LockMode, Size) + Offset;
}

void FEmptyDynamicRHI::UnlockIndexBuffer_BottomOfPipe(FRHICommandListImmediate& RHICmdList, FRHIBuffer* IndexBufferRHI)
{
	FEmptyIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	IndexBuffer->Unlock();
}

void FEmptyDynamicRHI::RHITransferIndexBufferUnderlyingResource(FRHIBuffer* DestIndexBuffer, FRHIBuffer* SrcIndexBuffer)
{

}
