// Copyright Epic Games, Inc. All Rights Reserved.

#include "AllocatedVirtualTexture.h"

#include "VT/VirtualTextureScalability.h"
#include "VT/VirtualTextureSystem.h"
#include "VT/VirtualTextureSpace.h"
#include "VT/VirtualTexturePhysicalSpace.h"
#include "Misc/StringBuilder.h"

FAllocatedVirtualTexture::FAllocatedVirtualTexture(FVirtualTextureSystem* InSystem,
	uint32 InFrame,
	const FAllocatedVTDescription& InDesc,
	FVirtualTextureProducer* const* InProducers,
	uint32 InBlockWidthInTiles,
	uint32 InBlockHeightInTiles,
	uint32 InWidthInBlocks,
	uint32 InHeightInBlocks,
	uint32 InDepthInTiles)
	: IAllocatedVirtualTexture(InDesc, InBlockWidthInTiles, InBlockHeightInTiles, InWidthInBlocks, InHeightInBlocks, InDepthInTiles)
	, RefCount(1)
	, FrameAllocated(InFrame)
	, Space(nullptr)
	, VirtualPageX(~0u)
	, VirtualPageY(~0u)
{
	check(IsInRenderingThread());
	FMemory::Memzero(TextureLayers);

	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumTextureLayers; ++LayerIndex)
	{
		FVirtualTextureProducer const* Producer = InProducers[LayerIndex];
		// Can't have missing entries for null producers if we're not merging duplicate layers
		if (Producer || !Description.bShareDuplicateLayers)
		{
			const uint32 UniqueProducerIndex = AddUniqueProducer(InDesc.ProducerHandle[LayerIndex], Producer);
			const int32 ProducerLayerIndex = InDesc.ProducerLayerIndex[LayerIndex];
			uint32 ProducerPhysicalGroupIndex = 0u;
			FVirtualTexturePhysicalSpace* PhysicalSpace = nullptr;
			if (Producer)
			{
				ProducerPhysicalGroupIndex = Producer->GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex);
				PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
			}
			const uint32 UniquePhysicalSpaceIndex = AddUniquePhysicalSpace(PhysicalSpace, UniqueProducerIndex, ProducerPhysicalGroupIndex);
			UniquePageTableLayers[UniquePhysicalSpaceIndex].ProducerTextureLayerMask |= 1 << ProducerLayerIndex;
			const uint8 PageTableLayerLocalIndex = UniquePageTableLayers[UniquePhysicalSpaceIndex].TextureLayerCount++;
			
			TextureLayers[LayerIndex].UniquePageTableLayerIndex = UniquePhysicalSpaceIndex;
			TextureLayers[LayerIndex].PhysicalTextureIndex = PageTableLayerLocalIndex;
		}
	}

	// Must have at least 1 valid layer/producer
	check(UniqueProducers.Num() > 0u);

	// Max level of overall allocated VT is limited by size in tiles
	// With multiple layers of different sizes, some layers may have mips smaller than a single tile
	// We can either use the Min or Max of Width/Height to determine the number of mips
	// - Using Max will allow more mips for rectangular VTs, which could potentially reduce aliasing in certain situations
	// - Using Min will relax alignment requirements for the page table allocator, which will tend to reduce overall VRAM usage
	MaxLevel = FMath::Min(MaxLevel, FMath::CeilLogTwo(FMath::Min(GetWidthInTiles(), GetHeightInTiles())));

	MaxLevel = FMath::Min(MaxLevel, VIRTUALTEXTURE_LOG2_MAX_PAGETABLE_SIZE - 1u);

	// Lock lowest resolution mip from each producer
	// Depending on the block dimensions of the producers that make up this allocated VT, different allocated VTs may need to lock different low resolution mips from the same producer
	// In the common case where block dimensions match, same mip will be locked by all allocated VTs that make use of the same producer
	for (int32 ProducerIndex = 0u; ProducerIndex < UniqueProducers.Num(); ++ProducerIndex)
	{
		FVirtualTextureProducerHandle ProducerHandle = UniqueProducers[ProducerIndex].Handle;
		FVirtualTextureProducer* Producer = InSystem->FindProducer(ProducerHandle);
		if (Producer && Producer->GetDescription().bPersistentHighestMip)
		{
			const uint32 MipBias = UniqueProducers[ProducerIndex].MipBias;
			check(MipBias <= MaxLevel);
			const uint32 Local_vLevel = MaxLevel - MipBias;
			checkf(Local_vLevel <= Producer->GetMaxLevel(), TEXT("Invalid Local_vLevel %d for VT producer %s, Producer MaxLevel %d, MipBias %d, AllocatedVT MaxLevel %d"),
				Local_vLevel,
				*Producer->GetName().ToString(),
				Producer->GetMaxLevel(),
				MipBias,
				MaxLevel);

			const uint32 MipScaleFactor = (1u << Local_vLevel);
			const uint32 RootWidthInTiles = FMath::DivideAndRoundUp(Producer->GetWidthInTiles(), MipScaleFactor);
			const uint32 RootHeightInTiles = FMath::DivideAndRoundUp(Producer->GetHeightInTiles(), MipScaleFactor);
			for (uint32 TileY = 0u; TileY < RootHeightInTiles; ++TileY)
			{
				for (uint32 TileX = 0u; TileX < RootWidthInTiles; ++TileX)
				{
					const uint32 Local_vAddress = FMath::MortonCode2(TileX) | (FMath::MortonCode2(TileY) << 1);
					const FVirtualTextureLocalTile TileToUnlock(ProducerHandle, Local_vAddress, Local_vLevel);
					InSystem->LockTile(TileToUnlock);
				}
			}
		}
	}

	// Use 16bit page table entries if all physical spaces are small enough
	bool bSupport16BitPageTable = true;
	for (int32 Index = 0; Index < UniquePageTableLayers.Num(); ++Index)
	{
		const FVirtualTexturePhysicalSpace* PhysicalSpace = UniquePageTableLayers[Index].PhysicalSpace;
		if (PhysicalSpace && !PhysicalSpace->DoesSupport16BitPageTable())
		{
			bSupport16BitPageTable = false;
			break;
		}
	}

	FVTSpaceDescription SpaceDesc;
	SpaceDesc.Dimensions = InDesc.Dimensions;
	SpaceDesc.NumPageTableLayers = UniquePageTableLayers.Num();
	SpaceDesc.TileSize = InDesc.TileSize;
	SpaceDesc.TileBorderSize = InDesc.TileBorderSize;
	SpaceDesc.bPrivateSpace = InDesc.bPrivateSpace;
	SpaceDesc.MaxSpaceSize = InDesc.MaxSpaceSize > 0 ? InDesc.MaxSpaceSize : SpaceDesc.MaxSpaceSize;
	SpaceDesc.IndirectionTextureSize = InDesc.IndirectionTextureSize;
	SpaceDesc.PageTableFormat = bSupport16BitPageTable ? EVTPageTableFormat::UInt16 : EVTPageTableFormat::UInt32;

	Space = InSystem->AcquireSpace(SpaceDesc, InDesc.ForceSpaceID, this);
	SpaceID = Space->GetID();
	PageTableFormat = Space->GetPageTableFormat();
}

FAllocatedVirtualTexture::~FAllocatedVirtualTexture()
{
}

void FAllocatedVirtualTexture::AssignVirtualAddress(uint32 vAddress)
{
	checkf(VirtualAddress == ~0u, TEXT("Trying to assign vAddress to AllocatedVT, already assigned"));
	check(vAddress != ~0u);
	VirtualAddress = vAddress;
	VirtualPageX = FMath::ReverseMortonCode2(vAddress);
	VirtualPageY = FMath::ReverseMortonCode2(vAddress >> 1);
}

void FAllocatedVirtualTexture::Destroy(FVirtualTextureSystem* System)
{
	const int32 NewRefCount = RefCount.Decrement();
	check(NewRefCount >= 0);
	if (NewRefCount == 0)
	{
		System->ReleaseVirtualTexture(this);
	}
}

void FAllocatedVirtualTexture::Release(FVirtualTextureSystem* System)
{
	check(IsInRenderingThread());
	check(RefCount.GetValue() == 0);

	// Unlock any locked tiles
	for (int32 ProducerIndex = 0u; ProducerIndex < UniqueProducers.Num(); ++ProducerIndex)
	{
		const FVirtualTextureProducerHandle ProducerHandle = UniqueProducers[ProducerIndex].Handle;
		FVirtualTextureProducer* Producer = System->FindProducer(ProducerHandle);
		if (Producer && Producer->GetDescription().bPersistentHighestMip)
		{
			const uint32 MipBias = UniqueProducers[ProducerIndex].MipBias;
			check(MipBias <= MaxLevel);
			const uint32 Local_vLevel = MaxLevel - MipBias;
			check(Local_vLevel <= Producer->GetMaxLevel());

			const uint32 MipScaleFactor = (1u << Local_vLevel);
			const uint32 RootWidthInTiles = FMath::DivideAndRoundUp(Producer->GetWidthInTiles(), MipScaleFactor);
			const uint32 RootHeightInTiles = FMath::DivideAndRoundUp(Producer->GetHeightInTiles(), MipScaleFactor);
			for (uint32 TileY = 0u; TileY < RootHeightInTiles; ++TileY)
			{
				for (uint32 TileX = 0u; TileX < RootWidthInTiles; ++TileX)
				{
					const uint32 Local_vAddress = FMath::MortonCode2(TileX) | (FMath::MortonCode2(TileY) << 1);
					const FVirtualTextureLocalTile TileToUnlock(ProducerHandle, Local_vAddress, Local_vLevel);
					System->UnlockTile(TileToUnlock, Producer);
				}
			}
		}
	}

	// Physical pool needs to evict all pages that belong to this VT
	{
		const uint32 WidthInTiles = GetWidthInTiles();
		const uint32 HeightInTiles = GetHeightInTiles();

		TArray<FVirtualTexturePhysicalSpace*> UniquePhysicalSpaces;
		for (int32 PageTableIndex = 0u; PageTableIndex < UniquePageTableLayers.Num(); ++PageTableIndex)
		{
			if (UniquePageTableLayers[PageTableIndex].PhysicalSpace)
			{
				UniquePhysicalSpaces.Add(UniquePageTableLayers[PageTableIndex].PhysicalSpace);
			}
		}

		for (FVirtualTexturePhysicalSpace* PhysicalSpace : UniquePhysicalSpaces)
		{
			PhysicalSpace->GetPagePool().UnmapAllPagesForSpace(System, Space->GetID(), VirtualAddress, WidthInTiles, HeightInTiles, MaxLevel);
		}

		for (int32 PageTableIndex = 0u; PageTableIndex < UniquePageTableLayers.Num(); ++PageTableIndex)
		{
			UniquePageTableLayers[PageTableIndex].PhysicalSpace.SafeRelease();
		}

#if DO_CHECK
		for (uint32 LayerIndex = 0; LayerIndex < Space->GetNumPageTableLayers(); ++LayerIndex)
		{
			const FTexturePageMap& PageMap = Space->GetPageMapForPageTableLayer(LayerIndex);

			TArray<FMappedTexturePage> MappedPages;
			PageMap.GetMappedPagesInRange(VirtualAddress, WidthInTiles, HeightInTiles, MappedPages);
			if (MappedPages.Num() > 0)
			{
				TStringBuilder<2048> Message;
				Message.Appendf(TEXT("Mapped pages remain after releasing AllocatedVT - vAddress: %d, Size: %d x %d, PhysicalSpaces: ["), VirtualAddress, WidthInTiles, HeightInTiles);
				for (FVirtualTexturePhysicalSpace* PhysicalSpace : UniquePhysicalSpaces)
				{
					Message.Appendf(TEXT("%d "), PhysicalSpace->GetID());
				}
				Message.Appendf(TEXT("], MappedPages: ["));

				for (const FMappedTexturePage& MappedPage : MappedPages)
				{
					Message.Appendf(TEXT("(vAddress: %d, PhysicalSpace: %d) "), MappedPage.Page.vAddress, MappedPage.PhysicalSpaceID);
				}
				Message.Appendf(TEXT("]"));
				UE_LOG(LogVirtualTexturing, Warning, TEXT("%s"), Message.ToString());
			}
		}
#endif // DO_CHECK
	}

	Space->FreeVirtualTexture(this);
	System->RemoveAllocatedVT(this);
	System->ReleaseSpace(Space);

	delete this;
}

uint32 FAllocatedVirtualTexture::AddUniqueProducer(FVirtualTextureProducerHandle const& InHandle, const FVirtualTextureProducer* InProducer)
{
	for (int32 Index = 0u; Index < UniqueProducers.Num(); ++Index)
	{
		if (UniqueProducers[Index].Handle == InHandle)
		{
			return Index;
		}
	}
	const uint32 Index = UniqueProducers.AddDefaulted();
	check(Index < VIRTUALTEXTURE_SPACE_MAXLAYERS);
	
	uint32 MipBias = 0u;
	if (InProducer)
	{
		const FVTProducerDescription& ProducerDesc = InProducer->GetDescription();
		// maybe these values should just be set by producers, rather than also set on AllocatedVT desc
		check(ProducerDesc.Dimensions == Description.Dimensions);
		check(ProducerDesc.TileSize == Description.TileSize);
		check(ProducerDesc.TileBorderSize == Description.TileBorderSize);

		const uint32 MipBiasX = FMath::CeilLogTwo(BlockWidthInTiles / ProducerDesc.BlockWidthInTiles);
		const uint32 MipBiasY = FMath::CeilLogTwo(BlockHeightInTiles / ProducerDesc.BlockHeightInTiles);
		check(ProducerDesc.BlockWidthInTiles << MipBiasX == BlockWidthInTiles);
		check(ProducerDesc.BlockHeightInTiles << MipBiasY == BlockHeightInTiles);

		// If the producer aspect ratio doesn't match the aspect ratio for the AllocatedVT, there's no way to choose a 100% mip bias
		// By chossing the minimum of X/Y bias, we'll effectively crop this producer to match the aspect ratio of the AllocatedVT
		// This case can happen as base materials will choose to group VTs together into a stack as long as all the textures assigned in the base material share the same aspect ratio
		// But it's possible for a MI to overide some of thse textures such that the aspect ratios no longer match
		// This will be fine for some cases, especially if the common case where the mismatched texture is a small dummy texture with a constant color
		MipBias = FMath::Min(MipBiasX, MipBiasY);

		MaxLevel = FMath::Max<uint32>(MaxLevel, ProducerDesc.MaxLevel + MipBias);
	}

	UniqueProducers[Index].Handle = InHandle;
	UniqueProducers[Index].MipBias = MipBias;
	
	return Index;
}

uint32 FAllocatedVirtualTexture::AddUniquePhysicalSpace(FVirtualTexturePhysicalSpace* InPhysicalSpace, uint32 InUniqueProducerIndex, uint32 InProducerPhysicalSpaceIndex)
{
	if (Description.bShareDuplicateLayers)
	{
		for (int32 Index = 0u; Index < UniquePageTableLayers.Num(); ++Index)
		{
			if (UniquePageTableLayers[Index].PhysicalSpace == InPhysicalSpace &&
				UniquePageTableLayers[Index].UniqueProducerIndex == InUniqueProducerIndex &&
				UniquePageTableLayers[Index].ProducerPhysicalGroupIndex == InProducerPhysicalSpaceIndex)
			{
				return Index;
			}
		}
	}
	const uint32 Index = UniquePageTableLayers.AddDefaulted();
	check(Index < VIRTUALTEXTURE_SPACE_MAXLAYERS);

	UniquePageTableLayers[Index].PhysicalSpace = InPhysicalSpace;
	UniquePageTableLayers[Index].UniqueProducerIndex = InUniqueProducerIndex;
	UniquePageTableLayers[Index].ProducerPhysicalGroupIndex = InProducerPhysicalSpaceIndex;
	UniquePageTableLayers[Index].ProducerTextureLayerMask = 0;
	UniquePageTableLayers[Index].TextureLayerCount = 0;

	return Index;
}

uint32 FAllocatedVirtualTexture::GetNumPageTableTextures() const
{
	return Space->GetNumPageTableTextures();
}

FRHITexture* FAllocatedVirtualTexture::GetPageTableTexture(uint32 InPageTableIndex) const
{
	return Space->GetPageTableTexture(InPageTableIndex);
}

FRHITexture* FAllocatedVirtualTexture::GetPageTableIndirectionTexture() const
{
	return Space->GetPageTableIndirectionTexture();
}

uint32 FAllocatedVirtualTexture::GetPhysicalTextureSize(uint32 InLayerIndex) const
{
	if (InLayerIndex < Description.NumTextureLayers)
	{
		const FVirtualTexturePhysicalSpace* PhysicalSpace = UniquePageTableLayers[TextureLayers[InLayerIndex].UniquePageTableLayerIndex].PhysicalSpace;
		return PhysicalSpace ? PhysicalSpace->GetTextureSize() : 0u;
	}
	return 0u;
}

FRHITexture* FAllocatedVirtualTexture::GetPhysicalTexture(uint32 InLayerIndex) const
{
	if (InLayerIndex < Description.NumTextureLayers)
	{
		const FVirtualTexturePhysicalSpace* PhysicalSpace = UniquePageTableLayers[TextureLayers[InLayerIndex].UniquePageTableLayerIndex].PhysicalSpace;
		return PhysicalSpace ? PhysicalSpace->GetPhysicalTexture(TextureLayers[InLayerIndex].PhysicalTextureIndex) : nullptr;
	}
	return nullptr;
}

FRHIShaderResourceView* FAllocatedVirtualTexture::GetPhysicalTextureSRV(uint32 InLayerIndex, bool bSRGB) const
{
	if (InLayerIndex < Description.NumTextureLayers)
	{
		const FVirtualTexturePhysicalSpace* PhysicalSpace = UniquePageTableLayers[TextureLayers[InLayerIndex].UniquePageTableLayerIndex].PhysicalSpace;
		return PhysicalSpace ? PhysicalSpace->GetPhysicalTextureSRV(TextureLayers[InLayerIndex].PhysicalTextureIndex, bSRGB) : nullptr;
	}
	return nullptr;
}

static inline uint32 BitcastFloatToUInt32(float v)
{
	const union
	{
		float FloatValue;
		uint32 UIntValue;
	} u = { v };
	return u.UIntValue;
}

void FAllocatedVirtualTexture::GetPackedPageTableUniform(FUintVector4* OutUniform) const
{
	const uint32 vPageX = FMath::ReverseMortonCode2(VirtualAddress);
	const uint32 vPageY = FMath::ReverseMortonCode2(VirtualAddress >> 1);
	const uint32 vPageSize = GetVirtualTileSize();
	const uint32 PageBorderSize = GetTileBorderSize();
	const uint32 WidthInPages = GetWidthInTiles();
	const uint32 HeightInPages = GetHeightInTiles();
	const uint32 vPageTableMipBias = FMath::FloorLog2(vPageSize);

	const uint32 MaxAnisotropy = FMath::Min<int32>(VirtualTextureScalability::GetMaxAnisotropy(), PageBorderSize);
	const uint32 MaxAnisotropyLog2 = (MaxAnisotropy > 0u) ? FMath::FloorLog2(MaxAnisotropy) : 0u;

	const uint32 AdaptiveLevelBias = 0; // todo[vt]: Required for handling SampleLevel correctly on adaptive page tables.

	// make sure everything fits in the allocated number of bits
	checkSlow(vPageX < 4096u);
	checkSlow(vPageY < 4096u);
	checkSlow(vPageTableMipBias < 16u);
	checkSlow(MaxLevel < 16u);
	checkSlow(AdaptiveLevelBias < 16u);
	checkSlow(SpaceID < 16u);

	OutUniform[0].X = BitcastFloatToUInt32(1.0f / (float)WidthInBlocks);
	OutUniform[0].Y = BitcastFloatToUInt32(1.0f / (float)HeightInBlocks);

	OutUniform[0].Z = BitcastFloatToUInt32((float)WidthInPages);
	OutUniform[0].W = BitcastFloatToUInt32((float)HeightInPages);

	OutUniform[1].X = BitcastFloatToUInt32((float)MaxAnisotropyLog2);
	OutUniform[1].Y = vPageX | (vPageY << 12) | (vPageTableMipBias << 24);
	OutUniform[1].Z = MaxLevel | (AdaptiveLevelBias << 4);
	OutUniform[1].W = (SpaceID << 28);
}

void FAllocatedVirtualTexture::GetPackedUniform(FUintVector4* OutUniform, uint32 LayerIndex) const
{
	const uint32 PhysicalTextureSize = GetPhysicalTextureSize(LayerIndex);
	if (PhysicalTextureSize > 0u)
	{
		const uint32 vPageSize = GetVirtualTileSize();
		const uint32 PageBorderSize = GetTileBorderSize();
		const float RcpPhysicalTextureSize = 1.0f / float(PhysicalTextureSize);
		const uint32 pPageSize = vPageSize + PageBorderSize * 2u;

		OutUniform->X = GetPageTableFormat() == EVTPageTableFormat::UInt16 ? 1 : 0;
		OutUniform->Y = BitcastFloatToUInt32((float)vPageSize * RcpPhysicalTextureSize);
		OutUniform->Z = BitcastFloatToUInt32((float)PageBorderSize * RcpPhysicalTextureSize);
		OutUniform->W = BitcastFloatToUInt32((float)pPageSize * RcpPhysicalTextureSize);
	}
	else
	{
		OutUniform->X = 0u;
		OutUniform->Y = 0u;
		OutUniform->Z = 0u;
		OutUniform->W = 0u;
	}
}
