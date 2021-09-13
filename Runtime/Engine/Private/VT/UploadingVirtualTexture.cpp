// Copyright Epic Games, Inc. All Rights Reserved.

#include "UploadingVirtualTexture.h"
#include "VirtualTextureChunkManager.h"
#include "VirtualTexturing.h"
#include "SceneUtils.h"
#include "FileCache/FileCache.h"
#include "CrunchCompression.h"
#include "VirtualTextureChunkDDCCache.h"
#include "UObject/PackageResourceManager.h"
#include "Misc/PackageSegment.h"

DECLARE_MEMORY_STAT(TEXT("File Cache Size"), STAT_FileCacheSize, STATGROUP_VirtualTextureMemory);
DECLARE_MEMORY_STAT(TEXT("Total Header Size"), STAT_TotalHeaderSize, STATGROUP_VirtualTextureMemory);
DECLARE_MEMORY_STAT(TEXT("Tile Header Size"), STAT_TileHeaderSize, STATGROUP_VirtualTextureMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Disk Size (KB)"), STAT_TotalDiskSize, STATGROUP_VirtualTextureMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Tile Headers"), STAT_NumTileHeaders, STATGROUP_VirtualTextureMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Codecs"), STAT_NumCodecs, STATGROUP_VirtualTextureMemory);

static TAutoConsoleVariable<int32> CVarVTCodecAgeThreshold(
	TEXT("r.VT.CodecAgeThreshold"),
	120,
	TEXT("Mininum number of frames VT codec must be unused before possibly being retired"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVTCodecNumThreshold(
	TEXT("r.VT.CodecNumThreshold"),
	100,
	TEXT("Once number of VT codecs exceeds this number, attempt to retire codecs that haven't been recently used"),
	ECVF_RenderThreadSafe);

int32 GVirtualTextureIOPriority_NormalPagePri = (int32)AIOP_Low;
static FAutoConsoleVariableRef CVarVirtualTextureIOPriority_NormalPagePri(
	TEXT("r.VT.IOPriority_NormalPagePri"),
	GVirtualTextureIOPriority_NormalPagePri,
	TEXT("Priority of default priority VT I/O requests"),
	ECVF_Default
);

int32 GVirtualTextureIOPriority_HighPagePri = (int32)AIOP_BelowNormal;
static FAutoConsoleVariableRef CVarVirtualTextureIOPriority_HighPagePri(
	TEXT("r.VT.IOPriority_HighPagePri"),
	GVirtualTextureIOPriority_HighPagePri,
	TEXT("Priority of high priority VT I/O requests"),
	ECVF_Default
);

static EAsyncIOPriorityAndFlags GetAsyncIOPriority(EVTRequestPagePriority Priority)
{
	switch (Priority)
	{
	case EVTRequestPagePriority::High: return (EAsyncIOPriorityAndFlags)GVirtualTextureIOPriority_HighPagePri | EAsyncIOPriorityAndFlags::AIOP_FLAG_DONTCACHE;
	case EVTRequestPagePriority::Normal: return (EAsyncIOPriorityAndFlags)GVirtualTextureIOPriority_NormalPagePri | EAsyncIOPriorityAndFlags::AIOP_FLAG_DONTCACHE;
	default: check(false); return (EAsyncIOPriorityAndFlags)GVirtualTextureIOPriority_NormalPagePri | EAsyncIOPriorityAndFlags::AIOP_FLAG_DONTCACHE;
	}
}

FVirtualTextureCodec* FVirtualTextureCodec::ListHead = nullptr;
FVirtualTextureCodec FVirtualTextureCodec::ListTail;
uint32 FVirtualTextureCodec::NumCodecs = 0u;

FUploadingVirtualTexture::FUploadingVirtualTexture(const FName& InName, FVirtualTextureBuiltData* InData, int32 InFirstMipToUse)
	: Name(InName)
	, Data(InData)
	, FirstMipOffset(InFirstMipToUse)
{
	HandlePerChunk.AddDefaulted(InData->Chunks.Num());
	CodecPerChunk.AddDefaulted(InData->Chunks.Num());
	InvalidChunks.Init(false, InData->Chunks.Num());

	StreamingManager = &IStreamingManager::Get().GetVirtualTextureStreamingManager();

	INC_MEMORY_STAT_BY(STAT_TotalHeaderSize, InData->GetMemoryFootprint());
	INC_MEMORY_STAT_BY(STAT_TileHeaderSize, InData->GetTileMemoryFootprint());
	INC_DWORD_STAT_BY(STAT_TotalDiskSize, InData->GetDiskMemoryFootprint() / 1024);
	INC_DWORD_STAT_BY(STAT_NumTileHeaders, InData->GetNumTileHeaders());
}

FUploadingVirtualTexture::~FUploadingVirtualTexture()
{
	check(Data);
	DEC_MEMORY_STAT_BY(STAT_TotalHeaderSize, Data->GetMemoryFootprint());
	DEC_MEMORY_STAT_BY(STAT_TileHeaderSize, Data->GetTileMemoryFootprint());
	DEC_DWORD_STAT_BY(STAT_TotalDiskSize, Data->GetDiskMemoryFootprint() / 1024);
	DEC_DWORD_STAT_BY(STAT_NumTileHeaders, Data->GetNumTileHeaders());

	// Complete all open transcode requests before deleting IFileCacheHandle objects
	StreamingManager->WaitTasksFinished();

	for (TUniquePtr<FVirtualTextureCodec>& Codec : CodecPerChunk)
	{
		if (Codec)
		{
			Codec->Unlink();
			Codec.Reset();
		}
	}
}

uint32 FUploadingVirtualTexture::GetLocalMipBias(uint8 vLevel, uint32 vAddress) const
{
	const uint32 NumMips = Data->NumMips;
	uint32 NumNonResidentLevels = 0u;
	while (vLevel < NumMips)
	{
		const uint32 TileIndex = Data->GetTileIndex(vLevel, vAddress);
		if (TileIndex == ~0u)
		{
			// vAddress is out-of-bounds for the given producer, this is a 
			NumNonResidentLevels += (NumMips - vLevel);
			break;
		}

		const int32 ChunkIndex = Data->GetChunkIndex(TileIndex);
		if (ChunkIndex >= 0)
		{
			break;
		}

		++NumNonResidentLevels;
		++vLevel;
		vAddress >>= 2;
	}

	return NumNonResidentLevels;
}

FVTRequestPageResult FUploadingVirtualTexture::RequestPageData(const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint64 vAddress, EVTRequestPagePriority Priority)
{
	check(vAddress <= MAX_uint32); // Not supporting 64 bit vAddress here. Only currrently supported for adaptive runtime virtual texture.
	return StreamingManager->RequestTile(this, ProducerHandle, LayerMask, FirstMipOffset + vLevel, (uint32)vAddress, Priority);
}

IVirtualTextureFinalizer* FUploadingVirtualTexture::ProducePageData(FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	EVTProducePageFlags Flags,
	const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint64 vAddress,
	uint64 RequestHandle,
	const FVTProduceTargetLayer* TargetLayers)
{
	INC_DWORD_STAT(STAT_VTP_NumUploads);

	const uint32 SkipBorderSize = (Flags & EVTProducePageFlags::SkipPageBorders) != EVTProducePageFlags::None ? Data->TileBorderSize : 0;
	return StreamingManager->ProduceTile(RHICmdList, SkipBorderSize, Data->GetNumLayers(), LayerMask, RequestHandle, TargetLayers);
}

void FUploadingVirtualTexture::GatherProducePageDataTasks(uint64 RequestHandle, FGraphEventArray& InOutTasks) const
{
	StreamingManager->GatherProducePageDataTasks(RequestHandle, InOutTasks);
}

void FVirtualTextureCodec::RetireOldCodecs()
{
	const uint32 AgeThreshold = CVarVTCodecAgeThreshold.GetValueOnRenderThread();
	const uint32 NumThreshold = CVarVTCodecNumThreshold.GetValueOnRenderThread();
	const uint32 CurrentFrame = GFrameNumberRenderThread;

	FVirtualTextureCodec::TIterator It(ListHead);
	while (It && NumCodecs > NumThreshold)
	{
		FVirtualTextureCodec& Codec = *It;
		It.Next();

		bool bRetiredCodec = false;
		// Can't retire codec if it's not even finished loading yet
		if (Codec.Owner && Codec.IsIdle())
		{
			check(CurrentFrame >= Codec.LastFrameUsed);
			const uint32 Age = CurrentFrame - Codec.LastFrameUsed;
			if (Age > AgeThreshold)
			{
				Codec.Unlink();
				Codec.Owner->CodecPerChunk[Codec.ChunkIndex].Reset();
				bRetiredCodec = true;
			}
		}

		if (!bRetiredCodec)
		{
			// List is kept sorted, so once we find a codec too new to retire, don't need to check any further
			break;
		}
	}
}

void FVirtualTextureCodec::Init(IMemoryReadStreamRef& HeaderData)
{
	const FVirtualTextureBuiltData* VTData = Owner->GetVTData();
	const FVirtualTextureDataChunk& Chunk = VTData->Chunks[ChunkIndex];
	const uint32 NumLayers = VTData->GetNumLayers();

	TArray<uint8, TInlineAllocator<16u * 1024>> TempBuffer;

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const uint32 CodecPayloadOffset = Chunk.CodecPayloadOffset[LayerIndex];
		const uint32 NextOffset = (LayerIndex + 1u) < NumLayers ? Chunk.CodecPayloadOffset[LayerIndex + 1] : Chunk.CodecPayloadSize;
		const uint32 CodecPayloadSize = NextOffset - CodecPayloadOffset;
		
		const uint8* CodecPayload = nullptr;
		if (CodecPayloadSize > 0u)
		{
			int64 PayloadReadSize = 0;
			CodecPayload = (uint8*)HeaderData->Read(PayloadReadSize, CodecPayloadOffset, CodecPayloadSize);
			if (PayloadReadSize < (int64)CodecPayloadSize)
			{
				// Generally this should not be needed, since payload is at start of file, should generally not cross a file cache page boundary
				TempBuffer.SetNumUninitialized(CodecPayloadSize);
				HeaderData->CopyTo(TempBuffer.GetData(), CodecPayloadOffset, CodecPayloadSize);
				CodecPayload = TempBuffer.GetData();
			}
		}

		switch (Chunk.CodecType[LayerIndex])
		{
		case EVirtualTextureCodec::Crunch:
#if WITH_CRUNCH
			Contexts[LayerIndex] = CrunchCompression::InitializeDecoderContext(CodecPayload, CodecPayloadSize);
#endif // WITH_CRUNCH
			check(Contexts[LayerIndex]);
			break;
		default:
			break;
		}
	}
}

void FVirtualTextureCodec::LinkGlobalHead()
{
	if (!ListHead)
	{
		ListTail.LinkHead(ListHead);
	}
	LinkHead(ListHead);
}

void FVirtualTextureCodec::LinkGlobalTail()
{
	if (!ListHead)
	{
		ListTail.LinkHead(ListHead);
	}
	LinkBefore(&ListTail);
}

FVirtualTextureCodec::~FVirtualTextureCodec()
{
	if (Owner)
	{
		checkf(IsCreationComplete(), TEXT("Codec is being released before its construction task hasn't finished."));
		checkf(AllTranscodeTasksComplete(), TEXT("Codec is being released while there are tasks that still reference it."));

		check(!IsLinked());

		const FVirtualTextureBuiltData* VTData = Owner->GetVTData();
		const FVirtualTextureDataChunk& Chunk = VTData->Chunks[ChunkIndex];
		const uint32 NumLayers = VTData->GetNumLayers();
		for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			switch (Chunk.CodecType[LayerIndex])
			{
			case EVirtualTextureCodec::Crunch:
#if WITH_CRUNCH
				check(Contexts[LayerIndex]);
				CrunchCompression::DestroyDecoderContext(Contexts[LayerIndex]);
#endif // WITH_CRUNCH
				break;
			default:
				break;
			}
		}

		check(NumCodecs > 0u);
		--NumCodecs;
		DEC_DWORD_STAT(STAT_NumCodecs);
	}
}

struct FCreateCodecTask
{
	IMemoryReadStreamRef HeaderData;
	FVirtualTextureCodec* Codec;

	FCreateCodecTask(const IMemoryReadStreamRef& InHeader, FVirtualTextureCodec* InCodec)
		: HeaderData(InHeader)
		, Codec(InCodec)
	{}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Codec->Init(HeaderData);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCreateCodecTask, STATGROUP_VTP);
	}
};

FVTCodecAndStatus FUploadingVirtualTexture::GetCodecForChunk(FGraphEventArray& OutCompletionEvents, uint32 ChunkIndex, EVTRequestPagePriority Priority)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUploadingVirtualTexture::GetCodecForChunk);

	const FVirtualTextureDataChunk& Chunk = Data->Chunks[ChunkIndex];
	if (Chunk.CodecPayloadSize == 0u)
	{
		// Chunk has no codec
		return EVTRequestPageStatus::Available;
	}

	TUniquePtr<FVirtualTextureCodec>& Codec = CodecPerChunk[ChunkIndex];
	if (Codec)
	{
		const bool bIsComplete = !Codec->CompletedEvent || Codec->CompletedEvent->IsComplete();
		if (!bIsComplete)
		{
			OutCompletionEvents.Add(Codec->CompletedEvent);
		}
		// Update LastFrameUsed and re-link to the back of the list
		Codec->Unlink();
		Codec->LinkGlobalTail();
		Codec->LastFrameUsed = GFrameNumberRenderThread;
		return FVTCodecAndStatus(bIsComplete ? EVTRequestPageStatus::Available : EVTRequestPageStatus::Pending, Codec.Get());
	}

	FGraphEventArray ReadDataCompletionEvents;
	const FVTDataAndStatus HeaderResult = ReadData(ReadDataCompletionEvents, ChunkIndex, 0, Chunk.CodecPayloadSize, Priority);
	if (!VTRequestPageStatus_HasData(HeaderResult.Status))
	{
		// GetData may fail if file cache is saturated
		return HeaderResult.Status;
	}

	INC_DWORD_STAT(STAT_NumCodecs);
	++FVirtualTextureCodec::NumCodecs;
	Codec.Reset(new FVirtualTextureCodec());
	Codec->LinkGlobalTail();
	Codec->Owner = this;
	Codec->ChunkIndex = ChunkIndex;
	Codec->LastFrameUsed = GFrameNumberRenderThread;
	Codec->CompletedEvent = TGraphTask<FCreateCodecTask>::CreateTask(&ReadDataCompletionEvents).ConstructAndDispatchWhenReady(HeaderResult.Data, Codec.Get());
	OutCompletionEvents.Add(Codec->CompletedEvent);
	return FVTCodecAndStatus(EVTRequestPageStatus::Pending, Codec.Get());
}

FVTDataAndStatus FUploadingVirtualTexture::ReadData(FGraphEventArray& OutCompletionEvents, uint32 ChunkIndex, size_t Offset, size_t Size, EVTRequestPagePriority Priority)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUploadingVirtualTexture::ReadData);

	FVirtualTextureDataChunk& Chunk = Data->Chunks[ChunkIndex];
	FByteBulkData& BulkData = Chunk.BulkData;
#if WITH_EDITOR
	// It could be that the bulkdata has no file associated yet (aka lightmaps have been build but not saved to disk yet) but still contains valid data
	// Streaming is done from memory
	if (BulkData.IsBulkDataLoaded() && BulkData.GetBulkDataSize() > 0)
	{
		ensure(Size <= (size_t)BulkData.GetBulkDataSize());
		const uint8 *P = (uint8*)BulkData.LockReadOnly() + Offset;
		IMemoryReadStreamRef Buffer = IMemoryReadStream::CreateFromCopy(P, Size);
		BulkData.Unlock();
		return FVTDataAndStatus(EVTRequestPageStatus::Available, Buffer);
	}
#endif // WITH_EDITOR

	TUniquePtr<IFileCacheHandle>& Handle = HandlePerChunk[ChunkIndex];
	if (!Handle)
	{
		enum class EChunkSource
		{
			PackagePath,
			File,
			BulkData,
			Invalid
		};
		EChunkSource ChunkSource = EChunkSource::Invalid;
		FPackagePath ChunkPackagePath;
		EPackageSegment ChunkPackageSegment = EPackageSegment::Header;
		FString ChunkFileName;
		int64 ChunkOffsetInFile = 0;
#if WITH_EDITOR
		// If the bulkdata has a file associated with it, we stream directly from it.
		// This only happens for lightmaps atm
		if (!BulkData.GetPackagePath().IsEmpty())
		{
			ensure(Size <= (size_t)BulkData.GetBulkDataSize());
			ChunkPackagePath = BulkData.GetPackagePath();
			ChunkPackageSegment = BulkData.GetPackageSegment();
			ChunkSource = EChunkSource::PackagePath;
			ChunkOffsetInFile = BulkData.GetBulkDataOffsetInFile();
		}
		// Else it should be VT data that is injected into the DDC (and stream from VT DDC cache)
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_VTP_MakeChunkAvailable);
			check(Chunk.DerivedDataKey.IsEmpty() == false);
			FString ChunkFileNameDCC;

			// If request is flagged as high priority, we will block here until DCC cache is populated
			// This way we can service these high priority tasks immediately
			// Would be better to have DCC cache return a task event handle, which could be used to chain a subsequent read operation,
			// but that would be more complicated, and this should generally not be a critical runtime path
			const bool bAsyncDCC = (Priority == EVTRequestPagePriority::Normal);
			const bool Available = GetVirtualTextureChunkDDCCache()->MakeChunkAvailable(&Data->Chunks[ChunkIndex], bAsyncDCC, ChunkFileNameDCC, ChunkOffsetInFile);
			if (!Available)
			{
				return EVTRequestPageStatus::Saturated;
			}
			ChunkFileName = ChunkFileNameDCC;
			ChunkSource = EChunkSource::File;
		}
#else // WITH_EDITOR
		ChunkOffsetInFile = BulkData.GetBulkDataOffsetInFile();
		if (BulkData.GetBulkDataSize() == 0)
		{
			if (!InvalidChunks[ChunkIndex])
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("BulkData for chunk %d in file '%s' is empty."), ChunkIndex, *BulkData.GetPackagePath().GetDebugName());
				InvalidChunks[ChunkIndex] = true;
			}
			return EVTRequestPageStatus::Invalid;
		}
		ChunkSource = EChunkSource::BulkData;
#endif // !WITH_EDITOR
		// If we have a valid file name then we can create a file handle directly to it.
		// If we do not then pass in the BulkData object which will create the IAsyncReadFileHandle for us.
		switch (ChunkSource)
		{
		case EChunkSource::PackagePath:
		{
			IAsyncReadFileHandle* AsyncFileHandle = IPackageResourceManager::Get().OpenAsyncReadPackage(ChunkPackagePath, ChunkPackageSegment);
			Handle.Reset(IFileCacheHandle::CreateFileCacheHandle(AsyncFileHandle, ChunkOffsetInFile));
			break;
		}
		case EChunkSource::File:
			Handle.Reset(IFileCacheHandle::CreateFileCacheHandle(*ChunkFileName, ChunkOffsetInFile));
			break;
		case EChunkSource::BulkData:
			Handle.Reset(IFileCacheHandle::CreateFileCacheHandle(BulkData.OpenAsyncReadHandle(), ChunkOffsetInFile));
			break;
		default:
			check(false);
			return EVTRequestPageStatus::Invalid;
		}
		
		// Don't expect CreateFileCacheHandle() to fail, async files should never fail to open
		if (!ensure(Handle.IsValid()))
		{
			if (!InvalidChunks[ChunkIndex])
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Could not create a file cache for '%s'."), *ChunkFileName);
				InvalidChunks[ChunkIndex] = true;
			}
			return EVTRequestPageStatus::Invalid;
		}
		SET_MEMORY_STAT(STAT_FileCacheSize, IFileCacheHandle::GetFileCacheSize());
	}

	IMemoryReadStreamRef ReadData = Handle->ReadData(OutCompletionEvents, Offset, Size, GetAsyncIOPriority(Priority));
	if (!ReadData)
	{
		return EVTRequestPageStatus::Saturated;
	}
	return FVTDataAndStatus(EVTRequestPageStatus::Pending, ReadData);
}

void FUploadingVirtualTexture::DumpToConsole(bool verbose)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Uploading virtual texture"));
	UE_LOG(LogConsoleResponse, Display, TEXT("FirstMipOffset: %i"), FirstMipOffset);
	UE_LOG(LogConsoleResponse, Display, TEXT("Current Size: %i x %i"), Data->Width >> FirstMipOffset, Data->Height >> FirstMipOffset);
	UE_LOG(LogConsoleResponse, Display, TEXT("Cooked Size: %i x %i"), Data->Width, Data->Height);
	UE_LOG(LogConsoleResponse, Display, TEXT("Cooked Tiles: %i x %i"), Data->GetWidthInTiles(), Data->GetHeightInTiles());
	UE_LOG(LogConsoleResponse, Display, TEXT("Tile Size: %i"), Data->TileSize);
	UE_LOG(LogConsoleResponse, Display, TEXT("Tile Border: %i"), Data->TileBorderSize);
	UE_LOG(LogConsoleResponse, Display, TEXT("Chunks: %i"), Data->Chunks.Num());
	UE_LOG(LogConsoleResponse, Display, TEXT("Layers: %i"), Data->GetNumLayers());

	TSet<FString> BulkDataFiles;

	for (const auto& Chunk : Data->Chunks)
	{
#if WITH_EDITORONLY_DATA
		if (Chunk.DerivedDataKey.IsEmpty() == false)
		{
			BulkDataFiles.Add(Chunk.DerivedDataKey);
		}
		else
#endif
			BulkDataFiles.Add(Chunk.BulkData.GetPackagePath().GetLocalFullPath());
	}

	for (const auto& FileName : BulkDataFiles)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Bulk data file / DDC entry: %s"), *FileName);
	}

#if 0
	if (verbose)
	{
		for (int32 L = 0; L < Data->Tiles.Num(); L++)
		{
			for (int32 T = 0; T < Data->Tiles[L].Num(); T++)
			{
				// TODO - missing VirtualAddress, GetPhysicalAddress

				FVirtualTextureTileInfo &Tile = Data->Tiles[L][T];
				uint32 lX = FMath::ReverseMortonCode2(T);
				uint32 lY = FMath::ReverseMortonCode2(T >> 1);

				uint32 SpaceAddress = VirtualAddress + T;
				uint32 vX = FMath::ReverseMortonCode2(SpaceAddress);
				uint32 vY = FMath::ReverseMortonCode2(SpaceAddress >> 1);


				// Check if the tile is resident if so print physical info as well
				uint32 pAddr = Space->GetPhysicalAddress(L, SpaceAddress);
				if (pAddr != ~0u)
				{
					uint32 pX = FMath::ReverseMortonCode2(pAddr);
					uint32 pY = FMath::ReverseMortonCode2(pAddr >> 1);
					UE_LOG(LogConsoleResponse, Display, TEXT(
						"Tile: Level %i, lAddr %i (%i,%i), vAddr %i (%i,%i), pAddr %i (%i,%i), Chunk %i, Offset %i, Size %i %i %i %i"),
						L, T, lX, lY,
						SpaceAddress, vX, vY,
						pAddr, pX, pY,
						Tile.Chunk, Tile.Offset, Tile.Size[0], (NumLayers > 1) ? Tile.Size[1] : 0, (NumLayers > 2) ? Tile.Size[2] : 0, (NumLayers > 3) ? Tile.Size[3] : 0);
				}
				else
				{
					if (Tile.Chunk < 0 || Tile.Size.Num() == 0)
					{
						UE_LOG(LogConsoleResponse, Display, TEXT("Tile: Level %i, lAddr %i (%i,%i), vAddr %i (%i,%i) - No Data Associated"),
							L, T, lX, lY,
							SpaceAddress, vX, vY);
					}
					else
					{
						UE_LOG(LogConsoleResponse, Display, TEXT("Tile: Level %i, lAddr %i (%i,%i), vAddr %i (%i,%i), Chunk %i, Offset %i, Size %i %i %i %i"),
							L, T, lX, lY,
							SpaceAddress, vX, vY,
							Tile.Chunk, Tile.Offset, Tile.Size[0], (NumLayers > 1) ? Tile.Size[1] : 0, (NumLayers > 2) ? Tile.Size[2] : 0, (NumLayers > 3) ? Tile.Size[3] : 0);
					}
				}
			}
		}
	}
#endif
}
