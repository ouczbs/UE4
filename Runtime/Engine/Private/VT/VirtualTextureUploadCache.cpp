// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureUploadCache.h"
#include "VirtualTextureChunkManager.h"
#include "RHI.h"
#include "RenderGraphBuilder.h"

// allow uploading CPU buffer directly to GPU texture
// this is slow under D3D11
// Should be pretty decent on D3D12X...UpdateTexture does make an extra copy of the data, but Lock/Unlock texture also buffers an extra copy of texture on this platform
// Might also be worth enabling this path on PC D3D12, need to measure
#if !defined(ALLOW_UPDATE_TEXTURE)
	#define ALLOW_UPDATE_TEXTURE 0
#endif

DECLARE_MEMORY_STAT_POOL(TEXT("Total GPU Upload Memory"), STAT_TotalGPUUploadSize, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);
DECLARE_MEMORY_STAT(TEXT("Total CPU Upload Memory"), STAT_TotalCPUUploadSize, STATGROUP_VirtualTextureMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Upload Entries"), STAT_NumUploadEntries, STATGROUP_VirtualTextureMemory);

FVirtualTextureUploadCache::FStagingBuffer::FStagingBuffer()
{
}

FVirtualTextureUploadCache::FStagingBuffer::~FStagingBuffer()
{
	if (RHIBuffer)
	{
		// Unmap and release the staging buffer if present
		// In this case 'Memory' was the mapped pointer, so release it
		Memory = nullptr;
		RHIUnlockBuffer(RHIBuffer);
		RHIBuffer.SafeRelease();
	}

	if (Memory)
	{
		// If we still have 'Memory' pointer, it came from heap, release it now
		FMemory::Free(Memory);
		Memory = nullptr;
	}
}

FVirtualTextureUploadCache::FTileEntry::FTileEntry()
{
}

FVirtualTextureUploadCache::FTileEntry::~FTileEntry()
{
}

FVirtualTextureUploadCache::FVirtualTextureUploadCache()
{
	StagingBuffers.AddDefaulted(); // empty buffer
	Tiles.AddDefaulted(LIST_COUNT);
	for (int i = 0; i < LIST_COUNT; ++i)
	{
		FTileEntry& Entry = Tiles[i];
		Entry.NextIndex = Entry.PrevIndex = i;
	}
}

FVirtualTextureUploadCache::~FVirtualTextureUploadCache()
{
}

int32 FVirtualTextureUploadCache::GetOrCreatePoolIndex(EPixelFormat InFormat, uint32 InTileSize)
{
	for (int32 i = 0; i < Pools.Num(); ++i)
	{
		const FPoolEntry& Entry = Pools[i];
		if (Entry.Format == InFormat && Entry.TileSize == InTileSize)
		{
			return i;
		}
	}

	const int32 PoolIndex = Pools.AddDefaulted();
	FPoolEntry& Entry = Pools[PoolIndex];
	Entry.Format = InFormat;
	Entry.TileSize = InTileSize;
	Entry.FreeTileListHead = CreateTileEntry(PoolIndex);
	Entry.SubmitTileListHead = CreateTileEntry(PoolIndex);

	return PoolIndex;
}

void FVirtualTextureUploadCache::Finalize(FRDGBuilder& GraphBuilder)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_FlushUpload)

	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

	// Multi-GPU support : May be ineffecient for AFR.
	SCOPED_GPU_MASK(RHICmdList, FRHIGPUMask::All());

	for (int PoolIndex = 0; PoolIndex < Pools.Num(); ++PoolIndex)
	{
		FPoolEntry& PoolEntry = Pools[PoolIndex];
		const uint32 BatchCount = PoolEntry.BatchCount;
		if (BatchCount == 0u)
		{
			continue;
		}

		const FPixelFormatInfo& FormatInfo = GPixelFormats[PoolEntry.Format];
		const uint32 TileSize = PoolEntry.TileSize;
		const uint32 TileWidthInBlocks = FMath::DivideAndRoundUp(TileSize, (uint32)FormatInfo.BlockSizeX);
		const uint32 TileHeightInBlocks = FMath::DivideAndRoundUp(TileSize, (uint32)FormatInfo.BlockSizeY);

		const uint32 TextureIndex = PoolEntry.BatchTextureIndex;
		PoolEntry.BatchTextureIndex = (PoolEntry.BatchTextureIndex + 1u) % NUM_STAGING_TEXTURES;
		FStagingTexture& StagingTexture = PoolEntry.StagingTexture[TextureIndex];

		// On some platforms the staging texture create/lock behavior will depend on whether we are running with RHI threading
		const bool bIsCpuWritable = !IsRunningRHIInSeparateThread();

		if (BatchCount > StagingTexture.BatchCapacity || bIsCpuWritable != StagingTexture.bIsCPUWritable)
		{
			// Staging texture is vertical stacked in widths of multiples of 4
			// Smaller widths mean smaller stride which is more efficient for copying
			// Round up to 4 to reduce likely wasted memory from width not aligning to whatever GPU prefers
			const uint32 MaxTextureDimension = GetMax2DTextureDimension();
			const uint32 MaxSizeInTiles = FMath::DivideAndRoundDown(MaxTextureDimension, TileSize);
			const uint32 MaxCapacity = MaxSizeInTiles * MaxSizeInTiles;
			check(BatchCount <= MaxCapacity);
			const uint32 WidthInTiles = FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(BatchCount, MaxSizeInTiles), 4u) * 4;
			check(WidthInTiles > 0u);
			const uint32 HeightInTiles = FMath::DivideAndRoundUp(BatchCount, WidthInTiles);
			check(HeightInTiles > 0u);

			if (StagingTexture.RHITexture)
			{
				DEC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, CalcTextureSize(StagingTexture.RHITexture->GetSizeX(), StagingTexture.RHITexture->GetSizeY(), PoolEntry.Format, 1u));
			}

			FRHIResourceCreateInfo CreateInfo(TEXT("FVirtualTextureUploadCache_StagingTexture"));
			StagingTexture.RHITexture = RHICreateTexture2D(TileSize * WidthInTiles, TileSize * HeightInTiles, PoolEntry.Format, 1, 1, bIsCpuWritable ? TexCreate_CPUWritable : TexCreate_None, CreateInfo);
			StagingTexture.WidthInTiles = WidthInTiles;
			StagingTexture.BatchCapacity = WidthInTiles * HeightInTiles;
			StagingTexture.bIsCPUWritable = bIsCpuWritable;
			INC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, CalcTextureSize(TileSize * WidthInTiles, TileSize * HeightInTiles, PoolEntry.Format, 1u));
		}

		uint32 BatchStride = 0u;
		void* BatchMemory = RHICmdList.LockTexture2D(StagingTexture.RHITexture, 0, RLM_WriteOnly, BatchStride, false, false);

		// copy all tiles to the staging texture
		const int32 SubmitListHead = PoolEntry.SubmitTileListHead;
		int32 Index = Tiles[SubmitListHead].NextIndex;
		while (Index != SubmitListHead)
		{
			const FTileEntry& Entry = Tiles[Index];
			const FStagingBuffer& StagingBuffer = StagingBuffers[Entry.BufferIndex];
			const int32 NextIndex = Entry.NextIndex;
			const uint32_t SrcTileX = Entry.SubmitBatchIndex % StagingTexture.WidthInTiles;
			const uint32_t SrcTileY = Entry.SubmitBatchIndex / StagingTexture.WidthInTiles;

			uint8* BatchDst = (uint8*)BatchMemory + TileHeightInBlocks * SrcTileY * BatchStride + TileWidthInBlocks * SrcTileX * FormatInfo.BlockBytes;
			for (uint32 y = 0u; y < TileHeightInBlocks; ++y)
			{
				FMemory::Memcpy(
					BatchDst + y * BatchStride,
					(uint8*)StagingBuffer.Memory + Entry.BufferOffset + y * Entry.Stride,
					TileWidthInBlocks * FormatInfo.BlockBytes);
			}

			Index = NextIndex;
		}

		RHICmdList.UnlockTexture2D(StagingTexture.RHITexture, 0u, false, false);
		RHICmdList.Transition(FRHITransitionInfo(StagingTexture.RHITexture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));

		// upload each tile from staging texture to physical texture
		Index = Tiles[SubmitListHead].NextIndex;
		while (Index != SubmitListHead)
		{
			FTileEntry& Entry = Tiles[Index];
			const int32 NextIndex = Entry.NextIndex;
			const uint32_t SrcTileX = Entry.SubmitBatchIndex % StagingTexture.WidthInTiles;
			const uint32_t SrcTileY = Entry.SubmitBatchIndex / StagingTexture.WidthInTiles;

			const uint32 SkipBorderSize = Entry.SubmitSkipBorderSize;
			const uint32 SubmitTileSize = TileSize - SkipBorderSize * 2;
			const FIntVector SourceBoxStart(SrcTileX * TileSize + SkipBorderSize, SrcTileY * TileSize + SkipBorderSize, 0);
			const FIntVector DestinationBoxStart(Entry.SubmitDestX * SubmitTileSize, Entry.SubmitDestY * SubmitTileSize, 0);

			if (!UpdatedTextures.Contains(Entry.RHISubmitTexture))
			{
				RHICmdList.Transition(FRHITransitionInfo(Entry.RHISubmitTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
				UpdatedTextures.Add(Entry.RHISubmitTexture);
			}

			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size = FIntVector(SubmitTileSize, SubmitTileSize, 1);
			CopyInfo.SourcePosition = SourceBoxStart;
			CopyInfo.DestPosition = DestinationBoxStart;
			RHICmdList.CopyTexture(StagingTexture.RHITexture, Entry.RHISubmitTexture, CopyInfo);

			Entry.RHISubmitTexture = nullptr;
			Entry.SubmitBatchIndex = 0u;
			Entry.SubmitDestX = 0;
			Entry.SubmitDestY = 0;
			Entry.SubmitSkipBorderSize = 0;

			RemoveFromList(Index);
			AddToList(PoolEntry.FreeTileListHead, Index);
			Index = NextIndex;
		}

		RHICmdList.Transition(FRHITransitionInfo(StagingTexture.RHITexture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

		PoolEntry.BatchCount = 0u;
	}

	// Transition all updates textures back to SRV
	FMemMark Mark(FMemStack::Get());
	TArray<FRHITransitionInfo, TMemStackAllocator<>> SRVTransitions;
	SRVTransitions.Reserve(UpdatedTextures.Num());
	for (int32 Index = 0; Index < UpdatedTextures.Num(); ++Index)
	{
		SRVTransitions.Add(FRHITransitionInfo(UpdatedTextures[Index], ERHIAccess::CopyDest, ERHIAccess::SRVMask));
	}
	RHICmdList.Transition(SRVTransitions);
	UpdatedTextures.Reset();
}

void FVirtualTextureUploadCache::ReleaseRHI()
{
	Pools.Empty();
	Tiles.Empty();
}

FVTUploadTileHandle FVirtualTextureUploadCache::PrepareTileForUpload(FVTUploadTileBuffer& OutBuffer, EPixelFormat InFormat, uint32 InTileSize)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_StageTile)

	checkSlow(IsInRenderingThread());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList(); // only valid on the render thread

	const int32 PoolIndex = GetOrCreatePoolIndex(InFormat, InTileSize);
	const FPoolEntry& PoolEntry = Pools[PoolIndex];

	int32 Index = Tiles[PoolEntry.FreeTileListHead].NextIndex;
	if (Index == PoolEntry.FreeTileListHead)
	{
		const FPixelFormatInfo& FormatInfo = GPixelFormats[InFormat];
		const uint32 TileWidthInBlocks = FMath::DivideAndRoundUp(InTileSize, (uint32)FormatInfo.BlockSizeX);
		const uint32 TileHeightInBlocks = FMath::DivideAndRoundUp(InTileSize, (uint32)FormatInfo.BlockSizeY);
		const uint32 Stride = TileWidthInBlocks * FormatInfo.BlockBytes;
		const uint32 MemorySize = Stride * TileHeightInBlocks;

		int32 StagingBufferIndex = StagingBuffers.Num() - 1;
		uint32 StagingBufferOffset = Align(StagingBuffers[StagingBufferIndex].CurrentOffset, 128u);
		if (StagingBufferOffset + MemorySize > StagingBuffers[StagingBufferIndex].Size)
		{
			StagingBufferIndex = StagingBuffers.AddDefaulted();
			StagingBufferOffset = 0u;
			FStagingBuffer& StagingBuffer = StagingBuffers[StagingBufferIndex];
			StagingBuffer.Size = MemorySize * 1024u;

			// We support several different methods for staging tile data to GPU textures
			// On some platforms, CPU can write linear texture data to persist mapped buffer, then this can be uploaded directly to GPU...this is fastest method
			// Otherwise, CPU writes texture data to temp buffer, then this is copied to GPU via a batched staging texture...this involves more copying, but is best method under default D3D11
			// Can potentially write each tile to a separate staging texture, but this has too much lock/unlock overhead
			
			
			// Stage to persist mapped GPU buffer then GPU copy into texture
			// this is fast where supported
			if (GRHISupportsDirectGPUMemoryLock)
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("StagingBuffer"));
				StagingBuffer.RHIBuffer = RHICreateStructuredBuffer(FormatInfo.BlockBytes, StagingBuffer.Size, BUF_ShaderResource | BUF_Static | BUF_KeepCPUAccessible, CreateInfo);

				// Here we bypass 'normal' RHI operations in order to get a persistent pointer to GPU memory, on supported platforms
				// This should be encapsulated into a proper RHI method at some point
				StagingBuffer.Memory = RHICmdList.LockBuffer(StagingBuffer.RHIBuffer, 0u, StagingBuffer.Size, RLM_WriteOnly_NoOverwrite);

				INC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, StagingBuffer.Size);
			}
			else
			{
				StagingBuffer.Memory = FMemory::Malloc(StagingBuffer.Size);
				INC_MEMORY_STAT_BY(STAT_TotalCPUUploadSize, StagingBuffer.Size);
			}
		}

		StagingBuffers[StagingBufferIndex].CurrentOffset = StagingBufferOffset + MemorySize;

		Index = CreateTileEntry(PoolIndex);
		FTileEntry& NewEntry = Tiles[Index];
		NewEntry.BufferIndex = StagingBufferIndex;
		NewEntry.BufferOffset = StagingBufferOffset;
		NewEntry.Stride = Stride;
		NewEntry.MemorySize = MemorySize;
		INC_DWORD_STAT(STAT_NumUploadEntries);
	}
	else
	{
		RemoveFromList(Index);
	}

	FTileEntry& Entry = Tiles[Index];
	++NumPendingTiles;
	
	OutBuffer.Memory = (uint8*)StagingBuffers[Entry.BufferIndex].Memory + Entry.BufferOffset;
	OutBuffer.MemorySize = Entry.MemorySize;
	OutBuffer.Stride = Entry.Stride;
	return FVTUploadTileHandle(Index);
}

void FVirtualTextureUploadCache::SubmitTile(FRHICommandListImmediate& RHICmdList, const FVTUploadTileHandle& InHandle, FRHITexture2D* InDestTexture, int InDestX, int InDestY, int InSkipBorderSize)
{
	checkSlow(IsInRenderingThread());
	check(NumPendingTiles > 0u);
	--NumPendingTiles;

	const int32 Index = InHandle.Index;
	FTileEntry& Entry = Tiles[Index];
	Entry.FrameSubmitted = GFrameNumberRenderThread;

	FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];
	const uint32 TileSize = PoolEntry.TileSize - InSkipBorderSize * 2;

	const FStagingBuffer& StagingBuffer = StagingBuffers[Entry.BufferIndex];
	if (StagingBuffer.RHIBuffer)
	{
		if (!UpdatedTextures.Contains(InDestTexture))
		{
			RHICmdList.Transition(FRHITransitionInfo(InDestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			UpdatedTextures.Add(InDestTexture);
		}

		check(GRHISupportsUpdateFromBufferTexture);
		const FUpdateTextureRegion2D UpdateRegion(InDestX * TileSize, InDestY * TileSize, InSkipBorderSize, InSkipBorderSize, TileSize, TileSize);
		RHICmdList.UpdateFromBufferTexture2D(InDestTexture, 0u, UpdateRegion, Entry.Stride, StagingBuffer.RHIBuffer, Entry.BufferOffset);

		// move to pending list, so we won't re-use this buffer until the GPU has finished the copy
		// (we're using persist mapped buffer here, so this is the only synchronization method in place...without this delay we'd get corrupt textures)
		AddToList(LIST_SUBMITTED, Index);
	}
	else if(ALLOW_UPDATE_TEXTURE)
	{
		if (!UpdatedTextures.Contains(InDestTexture))
		{
			RHICmdList.Transition(FRHITransitionInfo(InDestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			UpdatedTextures.Add(InDestTexture);
		}

		const FUpdateTextureRegion2D UpdateRegion(InDestX * TileSize, InDestY * TileSize, InSkipBorderSize, InSkipBorderSize, TileSize, TileSize);
		RHICmdList.UpdateTexture2D(InDestTexture, 0u, UpdateRegion, Entry.Stride, (uint8*)StagingBuffer.Memory + Entry.BufferOffset);

		// UpdateTexture2D makes internal copy of data, no need to wait before re-using tile
		AddToList(PoolEntry.FreeTileListHead, Index);
	}
	else
	{
		Entry.RHISubmitTexture = InDestTexture;
		Entry.SubmitDestX = InDestX;
		Entry.SubmitDestY = InDestY;
		Entry.SubmitSkipBorderSize = InSkipBorderSize;
		Entry.SubmitBatchIndex = PoolEntry.BatchCount++;

		// move to list of batched updates for the current pool
		AddToList(PoolEntry.SubmitTileListHead, Index);
	}
}

void FVirtualTextureUploadCache::CancelTile(const FVTUploadTileHandle& InHandle)
{
	checkSlow(IsInRenderingThread());
	check(NumPendingTiles > 0u);
	--NumPendingTiles;

	const int32 Index = InHandle.Index;
	FTileEntry& Entry = Tiles[Index];
	FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];

	AddToList(PoolEntry.FreeTileListHead, Index);
}

void FVirtualTextureUploadCache::UpdateFreeList()
{
	check(IsInRenderingThread());
	const uint32 CurrentFrame = GFrameNumberRenderThread;

	int32 Index = Tiles[LIST_SUBMITTED].NextIndex;
	while (Index != LIST_SUBMITTED)
	{
		FTileEntry& Entry = Tiles[Index];
		const int32 NextIndex = Entry.NextIndex;

		check(CurrentFrame >= Entry.FrameSubmitted);
		const uint32 FramesSinceSubmitted = CurrentFrame - Entry.FrameSubmitted;
		if (FramesSinceSubmitted < 2u)
		{
			break;
		}

		const FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];
		RemoveFromList(Index);
		AddToList(PoolEntry.FreeTileListHead, Index);

		Index = NextIndex;
	}
}
