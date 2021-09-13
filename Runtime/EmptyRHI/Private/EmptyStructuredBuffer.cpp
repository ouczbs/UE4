// Copyright Epic Games, Inc. All Rights Reserved.


#include "EmptyRHIPrivate.h"


FEmptyStructuredBuffer::FEmptyStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
	: FRHIBuffer(Stride, Size, Usage)
{
	check((Size % Stride) == 0);

	// copy any resources to the CPU address
	if (ResourceArray)
	{
// 		FMemory::Memcpy( , ResourceArray->GetResourceData(), Size);
		ResourceArray->Discard();
	}
}

FEmptyStructuredBuffer::~FEmptyStructuredBuffer()
{

}




FBufferRHIRef FEmptyDynamicRHI::RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return nullptr;//new FEmptyStructuredBuffer(Stride, Size, ResourceArray, InUsage);
}

void* FEmptyDynamicRHI::LockStructuredBuffer_BottomOfPipe(FRHICommandListImmediate& RHICmdList, FRHIBuffer* StructuredBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FEmptyStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	return nullptr;
}

void FEmptyDynamicRHI::UnlockStructuredBuffer_BottomOfPipe(FRHICommandListImmediate& RHICmdList, FRHIBuffer* StructuredBufferRHI)
{

}
