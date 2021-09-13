// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Commands.cpp: D3D RHI commands implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "StaticBoundShaderState.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "ShaderCompiler.h"
#include "ScreenRendering.h"
#include "ResolveShader.h"
#include "SceneUtils.h"
#include "RenderUtils.h"

int32 AFRSyncTemporalResources = 1;
static FAutoConsoleVariableRef CVarSyncTemporalResources(
	TEXT("D3D12.AFRSyncTemporalResources"),
	AFRSyncTemporalResources,
	TEXT("Synchronize inter-frame dependencies between GPUs"),
	ECVF_RenderThreadSafe
	);

static int32 GD3D12TransientAllocatorFullAliasingBarrier = 0;
static FAutoConsoleVariableRef CVarD3D12TransientAllocatorFullAliasingBarrier(
	TEXT("d3d12.TransientAllocator.FullAliasingBarrier"),
	GD3D12TransientAllocatorFullAliasingBarrier,
	TEXT("Inserts a full aliasing barrier on an transient acquire operation. Useful to debug if an aliasing barrier is missing."),
	ECVF_RenderThreadSafe);

using namespace D3D12RHI;

#define DECLARE_ISBOUNDSHADER(ShaderType) inline void ValidateBoundShader(FD3D12StateCache& InStateCache, FRHI##ShaderType* ShaderType##RHI) \
{ \
	FD3D12##ShaderType* CachedShader; \
	InStateCache.Get##ShaderType(&CachedShader); \
	FD3D12##ShaderType* ShaderType = FD3D12DynamicRHI::ResourceCast(ShaderType##RHI); \
	ensureMsgf(CachedShader == ShaderType, TEXT("Parameters are being set for a %s which is not currently bound"), TEXT( #ShaderType )); \
}

DECLARE_ISBOUNDSHADER(VertexShader)
DECLARE_ISBOUNDSHADER(PixelShader)
DECLARE_ISBOUNDSHADER(GeometryShader)
DECLARE_ISBOUNDSHADER(HullShader)
DECLARE_ISBOUNDSHADER(DomainShader)
DECLARE_ISBOUNDSHADER(ComputeShader)

#define DECLARE_ISVALIDUNIFORMBUFFERHASH(ShaderType) inline void ValidateBoundUniformBuffer(FD3D12UniformBuffer* InUniformBuffer, FRHI##ShaderType* In##RHI, uint32 InBufferIndex) \
{ \
	FD3D12##ShaderType* D3D12Shader = FD3D12DynamicRHI::ResourceCast(In##RHI); \
	if (InBufferIndex < (uint32)D3D12Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num()) \
	{ \
		uint32 UniformBufferHash = InUniformBuffer->GetLayout().GetHash(); \
		uint32 ShaderTableHash = D3D12Shader->ShaderResourceTable.ResourceTableLayoutHashes[InBufferIndex]; \
		ensureMsgf(ShaderTableHash == 0 || UniformBufferHash == ShaderTableHash, TEXT("Invalid uniform buffer %s bound on shader %s at index %d."), *(InUniformBuffer->GetLayout().GetDebugName()), TEXT( #ShaderType ), InBufferIndex); \
	} \
}

DECLARE_ISVALIDUNIFORMBUFFERHASH(VertexShader)
DECLARE_ISVALIDUNIFORMBUFFERHASH(PixelShader)
DECLARE_ISVALIDUNIFORMBUFFERHASH(GeometryShader)
DECLARE_ISVALIDUNIFORMBUFFERHASH(HullShader)
DECLARE_ISVALIDUNIFORMBUFFERHASH(DomainShader)
DECLARE_ISVALIDUNIFORMBUFFERHASH(ComputeShader)

#if EXECUTE_DEBUG_COMMAND_LISTS
bool GIsDoingQuery = false;
#endif

#if DO_CHECK
#define VALIDATE_BOUND_SHADER(s) ValidateBoundShader(StateCache, s)
#define VALIDATE_BOUND_UNIFORMBUFFER_HASH(ub, s, i) ValidateBoundUniformBuffer(ub, s, i)
#else
#define VALIDATE_BOUND_SHADER(s)
#define VALIDATE_BOUND_UNIFORMBUFFER_HASH(ub, s, i)
#endif

#if !defined(D3D12_PLATFORM_SUPPORTS_RESOLVE_SHADERS)
	#define D3D12_PLATFORM_SUPPORTS_RESOLVE_SHADERS 1
#endif

void FD3D12DynamicRHI::SetupRecursiveResources()
{
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	{
		TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);
		PixelShader.GetPixelShader();
	}

	{
		TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);
		PixelShader.GetPixelShader();
	}

	// TODO: Waiting to integrate MSAA fix for ResolveShader.h
	if (!D3D12_PLATFORM_SUPPORTS_RESOLVE_SHADERS)
		return;

	TShaderMapRef<FResolveVS> ResolveVertexShader(ShaderMap);
	if (GMaxRHIShaderPlatform == SP_PCD3D_SM6 || GMaxRHIShaderPlatform == SP_PCD3D_SM5)
	{
		TShaderMapRef<FResolveDepthPS> ResolvePixelShader_Depth(ShaderMap);
		ResolvePixelShader_Depth.GetPixelShader();

		TShaderMapRef<FResolveDepthPS> ResolvePixelShader_SingleSample(ShaderMap);
		ResolvePixelShader_SingleSample.GetPixelShader();
	}
	else
	{
		TShaderMapRef<FResolveDepthNonMSPS> ResolvePixelShader_DepthNonMS(ShaderMap);
		ResolvePixelShader_DepthNonMS.GetPixelShader();
	}
}

// Vertex state.
void FD3D12CommandContext::RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBufferRHI, uint32 Offset)
{
	FD3D12Buffer* VertexBuffer = RetrieveObject<FD3D12Buffer>(VertexBufferRHI);

	StateCache.SetStreamSource(VertexBuffer ? &VertexBuffer->ResourceLocation : nullptr, StreamIndex, Offset);
}

void FD3D12CommandContext::RHISetComputeShader(FRHIComputeShader* ComputeShaderRHI)
{
	// TODO: Eventually the high-level should just use RHISetComputePipelineState() directly similar to how graphics PSOs are handled.
	TRefCountPtr<FRHIComputePipelineState> ComputePipelineStateRHI = RHICreateComputePipelineState(ComputeShaderRHI);
	FD3D12ComputePipelineState* const ComputePipelineState = FD3D12DynamicRHI::ResourceCast(ComputePipelineStateRHI.GetReference());
	RHISetComputePipelineState(ComputePipelineState);
}

void FD3D12CommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	FD3D12ComputeShader* ComputeShader = nullptr;
	StateCache.GetComputeShader(&ComputeShader);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUDispatch(FIntVector(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));	
	}

	if (ComputeShader->ResourceCounts.bGlobalUniformBufferUsed)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);
	StateCache.ApplyState<D3D12PT_Compute>();

	numDispatches++;
	CommandListHandle->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12Buffer* ArgumentBuffer = RetrieveObject<FD3D12Buffer>(ArgumentBufferRHI);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUDispatch(FIntVector(1, 1, 1));	
	}

	FD3D12ComputeShader* ComputeShader = nullptr;
	StateCache.GetComputeShader(&ComputeShader);

	if (ComputeShader->ResourceCounts.bGlobalUniformBufferUsed)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);

	FD3D12ResourceLocation& Location = ArgumentBuffer->ResourceLocation;

	StateCache.ApplyState<D3D12PT_Compute>();

	// Indirect args buffer can be a previously pending UAV, which becomes PS\Non-PS read. ApplyState will flush pending transitions, so enqueue the indirect
	// arg transition and flush here.
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Validate);
	CommandListHandle.FlushResourceBarriers();	// Must flush so the desired state is actually set.

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	ID3D12CommandSignature* CommandSignature = IsAsyncComputeContext() ? Adapter->GetDispatchIndirectComputeCommandSignature() : Adapter->GetDispatchIndirectGraphicsCommandSignature();
	
	numDispatches++;
	CommandListHandle->ExecuteIndirect(
		CommandSignature,
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
		);
	CommandListHandle.UpdateResidency(Location.GetResource());

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

template <typename FunctionType>
void EnumerateSubresources(FD3D12Resource* Resource, const FRHITransitionInfo& Info, FunctionType Function)
{
	uint32 FirstMipSlice = 0;
	uint32 FirstArraySlice = 0;
	uint32 FirstPlaneSlice = 0;

	uint32 MipCount = Resource->GetMipLevels();
	uint32 ArraySize = Resource->GetArraySize();
	uint32 PlaneCount = Resource->GetPlaneCount();

	uint32 IterationMipCount = MipCount;
	uint32 IterationArraySize = ArraySize;
	uint32 IterationPlaneCount = PlaneCount;

	if (!Info.IsAllMips())
	{
		FirstMipSlice = Info.MipIndex;
		IterationMipCount = 1;
	}

	if (!Info.IsAllArraySlices())
	{
		FirstArraySlice = Info.ArraySlice;
		IterationArraySize = 1;
	}

	if (!Info.IsAllPlaneSlices())
	{
		FirstPlaneSlice = Info.PlaneSlice;
		IterationPlaneCount = 1;
	}

	for (uint32 PlaneSlice = FirstPlaneSlice; PlaneSlice < FirstPlaneSlice + IterationPlaneCount; ++PlaneSlice)
	{
		for (uint32 ArraySlice = FirstArraySlice; ArraySlice < FirstArraySlice + IterationArraySize; ++ArraySlice)
		{
			for (uint32 MipSlice = FirstMipSlice; MipSlice < FirstMipSlice + IterationMipCount; ++MipSlice)
			{
				const uint32 Subresource = D3D12CalcSubresource(MipSlice, ArraySlice, PlaneSlice, MipCount, ArraySize);
				Function(Subresource);
			}
		}
	}
}

template <typename FunctionType>
void ProcessResource(FD3D12CommandContext& Context, const FRHITransitionInfo& Info, FunctionType Function)
{
	switch (Info.Type)
	{
	case FRHITransitionInfo::EType::UAV:
	{
		FD3D12UnorderedAccessView* UAV = Context.RetrieveObject<FD3D12UnorderedAccessView>(Info.UAV);
		check(UAV);

		FRHITransitionInfo LocalInfo = Info;
		LocalInfo.MipIndex = UAV->GetViewSubresourceSubset().MostDetailedMip();
		Function(LocalInfo, UAV->GetResource());
		break;
	}
	case FRHITransitionInfo::EType::Buffer:
	{
		FD3D12Buffer* Buffer = Context.RetrieveObject<FD3D12Buffer>(Info.Buffer);
		check(Buffer);
		Function(Info, Buffer->GetResource());
		break;
	}
	case FRHITransitionInfo::EType::Texture:
	{
		FD3D12TextureBase* Texture = Context.RetrieveTextureBase(Info.Texture);
		check(Texture);
		Function(Info, Texture->GetResource());
		break;
	}
	default:
		checkNoEntry();
		break;
	}
}

static bool ProcessTransitionDuringBegin(const FD3D12TransitionData* Data)
{
	// Pipe changes which are not ending with graphics or targeting all pipelines are handle during begin
	return ((Data->SrcPipelines != Data->DstPipelines && Data->DstPipelines != ERHIPipeline::Graphics) || EnumHasAllFlags(Data->DstPipelines, ERHIPipeline::All));
}

struct FD3D12DiscardResource
{
	FD3D12DiscardResource(FD3D12Resource* InResource, EResourceTransitionFlags InFlags, uint32 InSubresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		: Resource(InResource)
		, Flags(InFlags)
		, Subresource(InSubresource)
	{}

	FD3D12Resource* Resource;
	EResourceTransitionFlags Flags;
	uint32 Subresource;
};

using FD3D12DiscardResourceArray = TArray<FD3D12DiscardResource>;

static void HandleResourceDiscardTransitions(
	FD3D12CommandContext& Context,
	const FD3D12TransitionData* TransitionData,
	D3D12_RESOURCE_STATES SkipFastClearEliminateState,
	FD3D12DiscardResourceArray& ResourcesToDiscard)
{
	for (const FRHITransitionInfo& Info : TransitionData->TransitionInfos)
	{
		if (Info.AccessBefore != ERHIAccess::Discard || Info.Type != FRHITransitionInfo::EType::Texture || !EnumHasAnyFlags(Info.Flags, EResourceTransitionFlags::Clear | EResourceTransitionFlags::Discard))
		{
			continue;
		}

		ProcessResource(Context, Info, [&](const FRHITransitionInfo& Info, FD3D12Resource* Resource)
		{
			if (!Resource->RequiresResourceStateTracking())
			{
				return;
			}

			// Get the initial state to force a 'nop' transition so the internal command list resource tracking has the correct state
			// already to make sure it's not added to the pending transition list to be kicked before this command list (resource is not valid then yet)
			D3D12_RESOURCE_STATES InitialState = GetInitialResourceState(Resource->GetDesc());
			if (Info.IsWholeResource() || Resource->GetSubresourceCount() == 1)
			{
				FD3D12DynamicRHI::TransitionResource(Context.CommandListHandle, Resource, InitialState, InitialState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Apply);
				ResourcesToDiscard.Emplace(Resource, Info.Flags, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}
			else
			{
				EnumerateSubresources(Resource, Info, [&](uint32 Subresource)
				{
					FD3D12DynamicRHI::TransitionResource(Context.CommandListHandle, Resource, InitialState, InitialState, Subresource, FD3D12DynamicRHI::ETransitionMode::Apply);
					ResourcesToDiscard.Emplace(Resource, Info.Flags, Subresource);
				});
			}
		});
	}
}

static void HandleDiscardResources(
	FD3D12CommandContext& Context,
	D3D12_RESOURCE_STATES SkipFastClearEliminateState,
	TArrayView<const FRHITransition*> Transitions)
{
	FD3D12DiscardResourceArray ResourcesToDiscard;

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		HandleResourceDiscardTransitions(Context, Data, SkipFastClearEliminateState, ResourcesToDiscard);
	}

	Context.CommandListHandle.FlushResourceBarriers();

	for (const FD3D12DiscardResource& DiscardResource : ResourcesToDiscard)
	{
		if (EnumHasAnyFlags(DiscardResource.Flags, EResourceTransitionFlags::Clear))
		{
			// add correct ops for clear
			check(false);
		}
		else if (EnumHasAnyFlags(DiscardResource.Flags, EResourceTransitionFlags::Discard))
		{
			if (DiscardResource.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				Context.CommandListHandle->DiscardResource(DiscardResource.Resource->GetResource(), nullptr);
			}
			else
			{
				D3D12_DISCARD_REGION Region;
				Region.NumRects = 0;
				Region.pRects = nullptr;
				Region.FirstSubresource = DiscardResource.Subresource;
				Region.NumSubresources = 1;

				Context.CommandListHandle->DiscardResource(DiscardResource.Resource->GetResource(), &Region);
			}
		}
	}
}

static void HandleTransientAliasing(FD3D12CommandContext& Context, const FD3D12TransitionData* TransitionData)
{
	for (const FRHITransientAliasingInfo& Info : TransitionData->AliasingInfos)
	{
		FD3D12BaseShaderResource* BaseShaderResource = nullptr;
		switch (Info.Type)
		{
		case FRHITransientAliasingInfo::EType::Buffer:
		{
			FD3D12Buffer* Buffer = Context.RetrieveObject<FD3D12Buffer>(Info.Buffer);
			check(Buffer);
			BaseShaderResource = Buffer;
			break;
		}
		case FRHITransientAliasingInfo::EType::Texture:
		{
			FD3D12TextureBase* Texture = Context.RetrieveTextureBase(Info.Texture);
			check(Texture);
			BaseShaderResource = Texture;
			break;
		}
		default:
			checkNoEntry();
			break;
		}

		FD3D12Resource* Resource = BaseShaderResource->ResourceLocation.GetResource();
		if (Info.Action == FRHITransientAliasingInfo::EAction::Acquire)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::AcquireTransient);

			if (GD3D12TransientAllocatorFullAliasingBarrier != 0)
			{
				Context.CommandListHandle.AddAliasingBarrier(nullptr, Resource);
			}
			else
			{
				for (const FRHITransientAliasingOverlap& Overlap : Info.Overlaps)
				{
					FD3D12Resource* ResourceBefore{};

					switch (Overlap.Type)
					{
					case FRHITransientAliasingOverlap::EType::Texture:
						ResourceBefore = Context.RetrieveTextureBase(Overlap.Texture)->GetResource();
						break;
					case FRHITransientAliasingOverlap::EType::Buffer:
						ResourceBefore = Context.RetrieveObject<FD3D12Buffer>(Overlap.Buffer)->GetResource();
						break;
					}

					check(ResourceBefore);
					Context.CommandListHandle.AddAliasingBarrier(ResourceBefore, Resource);
				}
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::DiscardTransient);

			// Restore the resource back to the initial state when done
			D3D12_RESOURCE_STATES FinalState = GetInitialResourceState(Resource->GetDesc());
			FD3D12DynamicRHI::TransitionResource(Context.CommandListHandle, Resource, D3D12_RESOURCE_STATE_TBD, FinalState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Apply);

			// Remove from caches
			Context.ConditionalClearShaderResource(&BaseShaderResource->ResourceLocation);
		}
	}
}

static void HandleResourceTransitions(FD3D12CommandContext& Context, const FD3D12TransitionData* TransitionData, D3D12_RESOURCE_STATES SkipFastClearEliminateState, bool& bUAVBarrier)
{
	const bool bDstAllPipelines = EnumHasAllFlags(TransitionData->DstPipelines, ERHIPipeline::All);

	for (const FRHITransitionInfo& Info : TransitionData->TransitionInfos)
	{
		// A transition back to discard is a no-op.
		if (Info.AccessAfter == ERHIAccess::Discard)
		{
			continue;
		}

		const bool bUAVAccess = EnumHasAnyFlags(Info.AccessAfter, ERHIAccess::UAVMask);

		// If targeting all pipelines and UAV access then add UAV barrier even with RHI based transitions
		if (bDstAllPipelines)
		{
			bUAVBarrier |= bUAVAccess;
		}

		// Use the engine defined transitions?
		if (GUseInternalTransitions)
		{
			// Only need to check for UAV barriers here, all other transitions are handled inside the RHI
			bUAVBarrier |= bUAVAccess;
		}
		else
		{
			// Sometimes we could still have barriers without resources, invalid but can still happen
			if (!Info.Resource)
			{
				// Add a UAV barrier in case of no resource and after state is UAV - user probably requested a force UAV barrier here				
				bUAVBarrier |= bUAVAccess;
			}
			else
			{
				ProcessResource(Context, Info, [&](const FRHITransitionInfo& Info, FD3D12Resource* Resource)
				{
					if (!Resource->RequiresResourceStateTracking())
					{
						return;
					}

					const bool bIsAsyncCompute = EnumHasAnyFlags(TransitionData->DstPipelines, ERHIPipeline::AsyncCompute);
					const bool bKnownBeforeState = (Info.AccessBefore != ERHIAccess::Unknown && Info.AccessBefore != ERHIAccess::Discard);

					// Derive or get the correct before & after state
					D3D12_RESOURCE_STATES BeforeState = bKnownBeforeState ? GetD3D12ResourceState(Info.AccessBefore, bIsAsyncCompute) : D3D12_RESOURCE_STATE_TBD;
					D3D12_RESOURCE_STATES AfterState = GetD3D12ResourceState(Info.AccessAfter, bIsAsyncCompute);

					// Add the compression flags if needed
					if (EnumHasAnyFlags(Info.Flags, EResourceTransitionFlags::MaintainCompression))
					{
						AfterState |= SkipFastClearEliminateState;
					}

					// enqueue the correct transitions
					if (Info.IsWholeResource() || Resource->GetSubresourceCount() == 1)
					{
						if (FD3D12DynamicRHI::TransitionResource(Context.CommandListHandle, Resource, BeforeState, AfterState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Apply))
						{
							bUAVBarrier = true;
						}
					}
					else
					{
						EnumerateSubresources(Resource, Info, [&](uint32 Subresource)
						{
							if (FD3D12DynamicRHI::TransitionResource(Context.CommandListHandle, Resource, BeforeState, AfterState, Subresource, FD3D12DynamicRHI::ETransitionMode::Apply))
							{
								bUAVBarrier = true;
							}
						});
					}
				});
			}
		}
	}
}

void FD3D12CommandContext::RHIBeginTransitionsWithoutFencing(TArrayView<const FRHITransition*> Transitions)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIBeginTransitions, bShowTransitionEvents, TEXT("RHIBeginTransitions"));

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		HandleTransientAliasing(*this, Data);
	}

	HandleDiscardResources(*this, SkipFastClearEliminateState, Transitions);

	bool bUAVBarrier = false;

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		// Handle transition during BeginTransitions?
		if (ProcessTransitionDuringBegin(Data))
		{
			HandleResourceTransitions(*this, Data, SkipFastClearEliminateState, bUAVBarrier);
		}
	}

	if (bUAVBarrier)
	{
		StateCache.FlushComputeShaderCache(true);
	}
}

void FD3D12CommandContext::RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions)
{
	// Can't correctly handle RHITransitions from multiple threads at the samae time right now - only internal transitions work correctly then
	// (will be fixed if unknown before state is not allowed anymore)
	check(bIsDefaultContext || GUseInternalTransitions);

	RHIBeginTransitionsWithoutFencing(Transitions);
	SignalTransitionFences(Transitions);
}

void FD3D12CommandContext::RHIEndTransitions(TArrayView<const FRHITransition*> Transitions)
{
	WaitForTransitionFences(Transitions);

	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIEndTransitions, bShowTransitionEvents, TEXT("RHIEndTransitions"));

	bool bUAVBarrier = false;

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		// Handle transition during EndTransitions?
		if (!ProcessTransitionDuringBegin(Data))
		{
			HandleResourceTransitions(*this, Data, SkipFastClearEliminateState, bUAVBarrier);
		}
	}

	if (bUAVBarrier)
	{
		StateCache.FlushComputeShaderCache(true);
	}
}

void FD3D12CommandContext::RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers)
{
	FMemory::Memzero(GlobalUniformBuffers.GetData(), GlobalUniformBuffers.Num() * sizeof(FRHIUniformBuffer*));

	for (int32 Index = 0; Index < InUniformBuffers.GetUniformBufferCount(); ++Index)
	{
		GlobalUniformBuffers[InUniformBuffers.GetSlot(Index)] = InUniformBuffers.GetUniformBuffer(Index);
	}
}

void FD3D12CommandContext::RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* StagingBufferRHI, uint32 Offset, uint32 NumBytes)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12CopyToStagingBufferTime);

	FD3D12StagingBuffer* StagingBuffer = FD3D12DynamicRHI::ResourceCast(StagingBufferRHI);
	check(StagingBuffer);
	ensureMsgf(!StagingBuffer->bIsLocked, TEXT("Attempting to Copy to a locked staging buffer. This may have undefined behavior"));

	FD3D12Buffer* VertexBuffer = RetrieveObject<FD3D12Buffer>(SourceBufferRHI);
	check(VertexBuffer);

	ensureMsgf((SourceBufferRHI->GetUsage() & BUF_SourceCopy) != 0, TEXT("Buffers used as copy source need to be created with BUF_SourceCopy"));


	// Ensure our shadow buffer is large enough to hold the readback.
	if (!StagingBuffer->ResourceLocation.IsValid() || StagingBuffer->ShadowBufferSize < NumBytes)
	{
		StagingBuffer->SafeRelease();

		// Unknown aligment requirement for sub allocated read back buffer data
		uint32 AllocationAlignment = 16;
		const D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(NumBytes, D3D12_RESOURCE_FLAG_NONE);		
		GetParentDevice()->GetDefaultBufferAllocator().AllocDefaultResource(D3D12_HEAP_TYPE_READBACK, BufferDesc, (EBufferUsageFlags)BUF_None, ED3D12ResourceStateMode::SingleState, D3D12_RESOURCE_STATE_COPY_DEST, StagingBuffer->ResourceLocation, AllocationAlignment, TEXT("StagedRead"));
		check(StagingBuffer->ResourceLocation.GetSize() == NumBytes);
		StagingBuffer->ShadowBufferSize = NumBytes;
	}

	// No need to check the GPU mask as staging buffers are in CPU memory and visible to all GPUs.
	
	{
		FD3D12Resource* pSourceResource = VertexBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();
		uint32 SourceOffset = VertexBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

		FD3D12Resource* pDestResource = StagingBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();
		uint32 DestOffset = StagingBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

		if (pSourceResource->RequiresResourceStateTracking())
		{
			FD3D12DynamicRHI::TransitionResource(CommandListHandle, pSourceResource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, FD3D12DynamicRHI::ETransitionMode::Validate);
			CommandListHandle.FlushResourceBarriers();	// Must flush so the desired state is actually set.
		}

		numCopies++;

		CommandListHandle->CopyBufferRegion(pDestResource->GetResource(), DestOffset, pSourceResource->GetResource(), Offset + SourceOffset, NumBytes);
		CommandListHandle.UpdateResidency(pDestResource);
		CommandListHandle.UpdateResidency(pSourceResource);
	}
}

void FD3D12CommandContext::RHIWriteGPUFence(FRHIGPUFence* FenceRHI)
{
	check(FenceRHI)

	// @todo-mattc we don't want to flush here. That should be the caller's responsibility.
	RHISubmitCommandsHint();
	FD3D12GPUFence* Fence = FD3D12DynamicRHI::ResourceCast(FenceRHI);
	Fence->WriteInternal(ED3D12CommandQueueType::Direct);
}

void FD3D12CommandContext::RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
	// These are the maximum viewport extents for D3D12. Exceeding them leads to badness.
	check(MinX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MinY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);

	D3D12_VIEWPORT Viewport = { MinX, MinY, (MaxX - MinX), (MaxY - MinY), MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		// Setting a viewport will also set the scissor rect appropriately.
		StateCache.SetViewport(Viewport);
		RHISetScissorRect(true, MinX, MinY, MaxX, MaxY);
	}
}

void FD3D12CommandContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	if (bEnable)
	{
		const CD3DX12_RECT ScissorRect(MinX, MinY, MaxX, MaxY);
		StateCache.SetScissorRect(ScissorRect);
	}
	else
	{
		const D3D12_VIEWPORT& Viewport = StateCache.GetViewport();
		const CD3DX12_RECT ScissorRect((LONG) Viewport.TopLeftX, (LONG) Viewport.TopLeftY, (LONG) Viewport.TopLeftX + (LONG) Viewport.Width, (LONG) Viewport.TopLeftY + (LONG) Viewport.Height);
		StateCache.SetScissorRect(ScissorRect);
	}
}

void FD3D12CommandContext::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, bool bApplyAdditionalState)
{
	FD3D12GraphicsPipelineState* GraphicsPipelineState = FD3D12DynamicRHI::ResourceCast(GraphicsState);

	// TODO: [PSO API] Every thing inside this scope is only necessary to keep the PSO shadow in sync while we convert the high level to only use PSOs
	const bool bWasUsingTessellation = bUsingTessellation;
	bUsingTessellation = GraphicsPipelineState->GetHullShader() && GraphicsPipelineState->GetDomainShader();
	// Ensure the command buffers are reset to reduce the amount of data that needs to be versioned.
	VSConstantBuffer.Reset();
	PSConstantBuffer.Reset();
	HSConstantBuffer.Reset();
	DSConstantBuffer.Reset();
	GSConstantBuffer.Reset();
	
	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedGraphicsConstants = true;

	if (!GraphicsPipelineState->PipelineStateInitializer.bDepthBounds)
	{
		StateCache.SetDepthBounds(0.0f, 1.0f);
	}

	if (GRHISupportsVariableRateShading  )
	{
		StateCache.SetShadingRate(GraphicsPipelineState->PipelineStateInitializer.ShadingRate, VRSRB_Passthrough);
	}

	StateCache.SetGraphicsPipelineState(GraphicsPipelineState, bUsingTessellation != bWasUsingTessellation);
	StateCache.SetStencilRef(0);

	if (bApplyAdditionalState)
	{
		ApplyStaticUniformBuffers(GraphicsPipelineState->GetVertexShader());
		ApplyStaticUniformBuffers(GraphicsPipelineState->GetHullShader());
		ApplyStaticUniformBuffers(GraphicsPipelineState->GetDomainShader());
		ApplyStaticUniformBuffers(GraphicsPipelineState->GetGeometryShader());
		ApplyStaticUniformBuffers(GraphicsPipelineState->GetPixelShader());
	}
}

void FD3D12CommandContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputeState)
{
#if D3D12_RHI_RAYTRACING
	StateCache.TransitionComputeState(D3D12PT_Compute);
#endif

	FD3D12ComputePipelineState* ComputePipelineState = FD3D12DynamicRHI::ResourceCast(ComputeState);

	CSConstantBuffer.Reset();
	bDiscardSharedComputeConstants = true;

	// TODO: [PSO API] Every thing inside this scope is only necessary to keep the PSO shadow in sync while we convert the high level to only use PSOs
	{
		StateCache.SetComputeShader(ComputePipelineState->ComputeShader);
	}

	StateCache.SetComputePipelineState(ComputePipelineState);

	ApplyStaticUniformBuffers(ComputePipelineState->ComputeShader.GetReference());
}

void FD3D12CommandContext::RHISetShaderTexture(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
	{
		FRHIVertexShader* VertexShaderRHI = static_cast<FRHIVertexShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(VertexShaderRHI);
		StateCache.SetShaderResourceView<SF_Vertex>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
	}
	break;
	case SF_Hull:
	{
		FRHIHullShader* HullShaderRHI = static_cast<FRHIHullShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(HullShaderRHI);
		StateCache.SetShaderResourceView<SF_Hull>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
	}
	break;
	case SF_Domain:
	{
		FRHIDomainShader* DomainShaderRHI = static_cast<FRHIDomainShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(DomainShaderRHI);
		StateCache.SetShaderResourceView<SF_Domain>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
	}
	break;
	case SF_Geometry:
	{
		FRHIGeometryShader* GeometryShaderRHI = static_cast<FRHIGeometryShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(GeometryShaderRHI);
		StateCache.SetShaderResourceView<SF_Geometry>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
	}
	break;
	case SF_Pixel:
	{
		FRHIPixelShader* PixelShaderRHI = static_cast<FRHIPixelShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(PixelShaderRHI);
		StateCache.SetShaderResourceView<SF_Pixel>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
	}
	break;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
	}
}

void FD3D12CommandContext::RHISetShaderTexture(FRHIComputeShader* ComputeShaderRHI, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Compute>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetUAVParameter(FRHIPixelShader* PixelShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
	FD3D12UnorderedAccessView* UAV = RetrieveObject<FD3D12UnorderedAccessView>(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	uint32 InitialCount = -1;

	// Actually set the UAV
	StateCache.SetUAVs<SF_Pixel>(UAVIndex, 1, &UAV, &InitialCount);
}


void FD3D12CommandContext::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12UnorderedAccessView* UAV = RetrieveObject<FD3D12UnorderedAccessView>(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	uint32 InitialCount = -1;

	// Actually set the UAV
	StateCache.SetUAVs<SF_Compute>(UAVIndex, 1, &UAV, &InitialCount);
}

void FD3D12CommandContext::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI, uint32 InitialCount)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12UnorderedAccessView* UAV = RetrieveObject<FD3D12UnorderedAccessView>(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	StateCache.SetUAVs<SF_Compute>(UAVIndex, 1, &UAV, &InitialCount);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
	{
		FRHIVertexShader* VertexShaderRHI = static_cast<FRHIVertexShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(VertexShaderRHI);
		StateCache.SetShaderResourceView<SF_Vertex>(SRV, TextureIndex);
	}
	break;
	case SF_Hull:
	{
		FRHIHullShader* HullShaderRHI = static_cast<FRHIHullShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(HullShaderRHI);
		StateCache.SetShaderResourceView<SF_Hull>(SRV, TextureIndex);
	}
	break;
	case SF_Domain:
	{
		FRHIDomainShader* DomainShaderRHI = static_cast<FRHIDomainShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(DomainShaderRHI);
		StateCache.SetShaderResourceView<SF_Domain>(SRV, TextureIndex);
	}
	break;
	case SF_Geometry:
	{
		FRHIGeometryShader* GeometryShaderRHI = static_cast<FRHIGeometryShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(GeometryShaderRHI);
		StateCache.SetShaderResourceView<SF_Geometry>(SRV, TextureIndex);
	}
	break;
	case SF_Pixel:
	{
		FRHIPixelShader* PixelShaderRHI = static_cast<FRHIPixelShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(PixelShaderRHI);
		StateCache.SetShaderResourceView<SF_Pixel>(SRV, TextureIndex);
	}
	break;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
	}

}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FRHIComputeShader* ComputeShaderRHI, uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Compute>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FRHIGraphicsShader* ShaderRHI, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
	{
		FRHIVertexShader* VertexShaderRHI = static_cast<FRHIVertexShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(VertexShaderRHI);
		StateCache.SetSamplerState<SF_Vertex>(NewState, SamplerIndex);
	}
	break;
	case SF_Hull:
	{
		FRHIHullShader* HullShaderRHI = static_cast<FRHIHullShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(HullShaderRHI);
		StateCache.SetSamplerState<SF_Hull>(NewState, SamplerIndex);
	}
	break;
	case SF_Domain:
	{
		FRHIDomainShader* DomainShaderRHI = static_cast<FRHIDomainShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(DomainShaderRHI);
		StateCache.SetSamplerState<SF_Domain>(NewState, SamplerIndex);
	}
	break;
	case SF_Geometry:
	{
		FRHIGeometryShader* GeometryShaderRHI = static_cast<FRHIGeometryShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(GeometryShaderRHI);
		StateCache.SetSamplerState<SF_Geometry>(NewState, SamplerIndex);
	}
	break;
	case SF_Pixel:
	{
		FRHIPixelShader* PixelShaderRHI = static_cast<FRHIPixelShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(PixelShaderRHI);
		StateCache.SetSamplerState<SF_Pixel>(NewState, SamplerIndex);
	}
	break;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
	}
}

void FD3D12CommandContext::RHISetShaderSampler(FRHIComputeShader* ComputeShaderRHI, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Compute>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FRHIGraphicsShader* ShaderRHI, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);
	EShaderFrequency Stage = SF_NumFrequencies;
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
	{
		FRHIVertexShader* VertexShaderRHI = static_cast<FRHIVertexShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(VertexShaderRHI);
		VALIDATE_BOUND_UNIFORMBUFFER_HASH(Buffer, VertexShaderRHI, BufferIndex);
		StateCache.SetConstantsFromUniformBuffer<SF_Vertex>(BufferIndex, Buffer);
		Stage = SF_Vertex;
	}
	break;
	case SF_Hull:
	{
		FRHIHullShader* HullShaderRHI = static_cast<FRHIHullShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(HullShaderRHI);
		VALIDATE_BOUND_UNIFORMBUFFER_HASH(Buffer, HullShaderRHI, BufferIndex);
		StateCache.SetConstantsFromUniformBuffer<SF_Hull>(BufferIndex, Buffer);
		Stage = SF_Hull;
	}
	break;
	case SF_Domain:
	{
		FRHIDomainShader* DomainShaderRHI = static_cast<FRHIDomainShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(DomainShaderRHI);
		VALIDATE_BOUND_UNIFORMBUFFER_HASH(Buffer, DomainShaderRHI, BufferIndex);
		StateCache.SetConstantsFromUniformBuffer<SF_Domain>(BufferIndex, Buffer);
		Stage = SF_Domain;
	}
	break;
	case SF_Geometry:
	{
		FRHIGeometryShader* GeometryShaderRHI = static_cast<FRHIGeometryShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(GeometryShaderRHI);
		VALIDATE_BOUND_UNIFORMBUFFER_HASH(Buffer, GeometryShaderRHI, BufferIndex);
		StateCache.SetConstantsFromUniformBuffer<SF_Geometry>(BufferIndex, Buffer);
		Stage = SF_Geometry;
	}
	break;
	case SF_Pixel:
	{
		FRHIPixelShader* PixelShaderRHI = static_cast<FRHIPixelShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(PixelShaderRHI);
		VALIDATE_BOUND_UNIFORMBUFFER_HASH(Buffer, PixelShaderRHI, BufferIndex);
		StateCache.SetConstantsFromUniformBuffer<SF_Pixel>(BufferIndex, Buffer);
		Stage = SF_Pixel;
	}
	break;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
		return;
	}

	if (!GRHINeedsExtraDeletionLatency)
	{
		BoundUniformBufferRefs[Stage][BufferIndex] = BufferRHI;
	}

	BoundUniformBuffers[Stage][BufferIndex] = Buffer;
	DirtyUniformBuffers[Stage] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FRHIComputeShader* ComputeShader, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	//VALIDATE_BOUND_SHADER(ComputeShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);

	StateCache.SetConstantsFromUniformBuffer<SF_Compute>(BufferIndex, Buffer);

	if (!GRHINeedsExtraDeletionLatency)
	{
		BoundUniformBufferRefs[SF_Compute][BufferIndex] = BufferRHI;
	}

	BoundUniformBuffers[SF_Compute][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Compute] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderParameter(FRHIGraphicsShader* ShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	checkSlow(BufferIndex == 0);

	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
	{
		FRHIVertexShader* VertexShaderRHI = static_cast<FRHIVertexShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(VertexShaderRHI);
		VSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
	}
	break;
	case SF_Hull:
	{
		FRHIHullShader* HullShaderRHI = static_cast<FRHIHullShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(HullShaderRHI);
		HSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
	}
	break;
	case SF_Domain:
	{
		FRHIDomainShader* DomainShaderRHI = static_cast<FRHIDomainShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(DomainShaderRHI);
		DSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
	}
	break;
	case SF_Geometry:
	{
		FRHIGeometryShader* GeometryShaderRHI = static_cast<FRHIGeometryShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(GeometryShaderRHI);
		GSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
	}
	break;
	case SF_Pixel:
	{
		FRHIPixelShader* PixelShaderRHI = static_cast<FRHIPixelShader*>(ShaderRHI);
		VALIDATE_BOUND_SHADER(PixelShaderRHI);
		PSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
	}
	break;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
	}
}

void FD3D12CommandContext::RHISetShaderParameter(FRHIComputeShader* ComputeShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	checkSlow(BufferIndex == 0);
	CSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil RequestedAccess) const
{
	const bool bSrcDepthWrite = RequestedAccess.IsDepthWrite();
	const bool bSrcStencilWrite = RequestedAccess.IsStencilWrite();

	if (bSrcDepthWrite || bSrcStencilWrite)
	{
		// New Rule: You have to call SetRenderTarget[s]() before
		ensure(CurrentDepthTexture);

		const bool bDstDepthWrite = CurrentDSVAccessType.IsDepthWrite();
		const bool bDstStencilWrite = CurrentDSVAccessType.IsStencilWrite();

		// requested access is not possible, fix SetRenderTarget EExclusiveDepthStencil or request a different one
		check(!bSrcDepthWrite || bDstDepthWrite);
		check(!bSrcStencilWrite || bDstStencilWrite);
	}
}

void FD3D12CommandContext::RHISetStencilRef(uint32 StencilRef)
{
	StateCache.SetStencilRef(StencilRef);
}

void FD3D12CommandContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	StateCache.SetBlendFactor((const float*)&BlendFactor);
}

void FD3D12CommandContext::CommitRenderTargetsAndUAVs()
{
	StateCache.SetRenderTargets(NumSimultaneousRenderTargets, CurrentRenderTargets, CurrentDepthStencilTarget);
	StateCache.ClearUAVs<SF_Pixel>();
}

struct FRTVDesc
{
	uint32 Width;
	uint32 Height;
	DXGI_SAMPLE_DESC SampleDesc;
};

// Return an FRTVDesc structure whose
// Width and height dimensions are adjusted for the RTV's miplevel.
FRTVDesc GetRenderTargetViewDesc(FD3D12RenderTargetView* RenderTargetView)
{
	const D3D12_RENDER_TARGET_VIEW_DESC &TargetDesc = RenderTargetView->GetDesc();

	FD3D12Resource* BaseResource = RenderTargetView->GetResource();
	uint32 MipIndex = 0;
	FRTVDesc ret;
	memset(&ret, 0, sizeof(ret));

	switch (TargetDesc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE2D:
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc = Desc.SampleDesc;
		if (TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D || TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
		{
			// All the non-multisampled texture types have their mip-slice in the same position.
			MipIndex = TargetDesc.Texture2D.MipSlice;
		}
		break;
	}
	case D3D12_RTV_DIMENSION_TEXTURE3D:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc.Count = 1;
		ret.SampleDesc.Quality = 0;
		MipIndex = TargetDesc.Texture3D.MipSlice;
		break;
	}
	default:
	{
		// not expecting 1D targets.
		checkNoEntry();
	}
	}
	ret.Width >>= MipIndex;
	ret.Height >>= MipIndex;
	return ret;
}

void FD3D12CommandContext::SetRenderTargets(
	uint32 NewNumSimultaneousRenderTargets,
	const FRHIRenderTargetView* NewRenderTargetsRHI,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI
	)
{
	FD3D12TextureBase* NewDepthStencilTarget = NewDepthStencilTargetRHI ? RetrieveTextureBase(NewDepthStencilTargetRHI->Texture) : nullptr;

	check(NewNumSimultaneousRenderTargets <= MaxSimultaneousRenderTargets);

	bool bTargetChanged = false;

	// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
	FD3D12DepthStencilView* DepthStencilView = NULL;
	if (NewDepthStencilTarget)
	{
		check(NewDepthStencilTargetRHI);	// Calm down static analysis
		CurrentDSVAccessType = NewDepthStencilTargetRHI->GetDepthStencilAccess();
		DepthStencilView = NewDepthStencilTarget->GetDepthStencilView(CurrentDSVAccessType);

		// Unbind any shader views of the depth stencil target that are bound.
		ConditionalClearShaderResource(&NewDepthStencilTarget->ResourceLocation);
	}

	// Check if the depth stencil target is different from the old state.
	if (CurrentDepthStencilTarget != DepthStencilView)
	{
		CurrentDepthTexture = NewDepthStencilTarget;
		CurrentDepthStencilTarget = DepthStencilView;
		bTargetChanged = true;
	}

	// Gather the render target views for the new render targets.
	FD3D12RenderTargetView* NewRenderTargetViews[MaxSimultaneousRenderTargets];
	for (uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets;++RenderTargetIndex)
	{
		FD3D12RenderTargetView* RenderTargetView = NULL;
		if (RenderTargetIndex < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[RenderTargetIndex].Texture != nullptr)
		{
			int32 RTMipIndex = NewRenderTargetsRHI[RenderTargetIndex].MipIndex;
			int32 RTSliceIndex = NewRenderTargetsRHI[RenderTargetIndex].ArraySliceIndex;
			FD3D12TextureBase* NewRenderTarget = RetrieveTextureBase(NewRenderTargetsRHI[RenderTargetIndex].Texture);
			RenderTargetView = NewRenderTarget->GetRenderTargetView(RTMipIndex, RTSliceIndex);

			ensureMsgf(RenderTargetView, TEXT("Texture being set as render target has no RTV"));

			// Unbind any shader views of the render target that are bound.
			ConditionalClearShaderResource(&NewRenderTarget->ResourceLocation);
		}

		NewRenderTargetViews[RenderTargetIndex] = RenderTargetView;

		// Check if the render target is different from the old state.
		if (CurrentRenderTargets[RenderTargetIndex] != RenderTargetView)
		{
			CurrentRenderTargets[RenderTargetIndex] = RenderTargetView;
			bTargetChanged = true;
		}
	}
	if (NumSimultaneousRenderTargets != NewNumSimultaneousRenderTargets)
	{
		NumSimultaneousRenderTargets = NewNumSimultaneousRenderTargets;
		bTargetChanged = true;
	}

	// Only make the D3D call to change render targets if something actually changed.
	if (bTargetChanged)
	{
		CommitRenderTargetsAndUAVs();
	}

	// Set the viewport to the full size of render target 0.
	if (NewRenderTargetViews[0])
	{
		// check target 0 is valid
		check(0 < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[0].Texture != nullptr);
		FRTVDesc RTTDesc = GetRenderTargetViewDesc(NewRenderTargetViews[0]);
		RHISetViewport(0.0f, 0.0f, 0.0f, (float)RTTDesc.Width, (float)RTTDesc.Height, 1.0f);
	}
	else if (DepthStencilView)
	{
		FD3D12Resource* DepthTargetTexture = DepthStencilView->GetResource();
		D3D12_RESOURCE_DESC const& DTTDesc = DepthTargetTexture->GetDesc();
		RHISetViewport(0.0f, 0.0f, 0.0f, (float)DTTDesc.Width, (float)DTTDesc.Height, 1.0f);
	}
}

void FD3D12CommandContext::SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FRHIUnorderedAccessView* UAVs[MaxSimultaneousUAVs] = {};

	this->SetRenderTargets(RenderTargetsInfo.NumColorRenderTargets,
		RenderTargetsInfo.ColorRenderTarget,
		&RenderTargetsInfo.DepthStencilRenderTarget);

	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	FD3D12DepthStencilView* DSView = nullptr;
	uint32 NumSimultaneousRTs = 0;
	StateCache.GetRenderTargets(RenderTargetViews, &NumSimultaneousRTs, &DSView);
	FD3D12BoundRenderTargets BoundRenderTargets(RenderTargetViews, NumSimultaneousRTs, DSView);
	FD3D12DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	// perform the correct barrier transitions to RTV
	{
		for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
		{
			FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);
			if (RTView != nullptr)
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, RTView, D3D12_RESOURCE_STATE_RENDER_TARGET, FD3D12DynamicRHI::ETransitionMode::Validate);
			}
		}
	}

	// perform the optimal barrier transition for depth stencil as well
	if (DepthStencilView)
	{
		// Find out the desired state for both the depth & the stencil
		FExclusiveDepthStencil DepthStencilAccess = RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess();
		D3D12_RESOURCE_STATES DepthState = DepthStencilAccess.IsDepthWrite() ? D3D12_RESOURCE_STATE_DEPTH_WRITE : (DepthStencilAccess.IsDepthRead() ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);
		D3D12_RESOURCE_STATES StencilState = DepthStencilAccess.IsStencilWrite() ? D3D12_RESOURCE_STATE_DEPTH_WRITE : (DepthStencilAccess.IsStencilRead() ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

		// Same target state for both depth & stencil, then it can be done with single barrier
		if (DepthState == StencilState)
		{
			if (DepthState != D3D12_RESOURCE_STATE_COMMON)
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView, DepthState, FD3D12DynamicRHI::ETransitionMode::Validate);
			}
		}
		else
		{
			if (DepthState != D3D12_RESOURCE_STATE_COMMON)
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView->GetResource(), D3D12_RESOURCE_STATE_TBD, DepthState, DepthStencilView->GetDepthOnlyViewSubresourceSubset(), FD3D12DynamicRHI::ETransitionMode::Validate);
			}
			if (StencilState != D3D12_RESOURCE_STATE_COMMON)
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView->GetResource(), D3D12_RESOURCE_STATE_TBD, StencilState, DepthStencilView->GetStencilOnlyViewSubresourceSubset(), FD3D12DynamicRHI::ETransitionMode::Validate);
			}
		}
	}

	if (RenderTargetsInfo.bClearColor || RenderTargetsInfo.bClearStencil || RenderTargetsInfo.bClearDepth)
	{
		FLinearColor ClearColors[MaxSimultaneousRenderTargets];
		float DepthClear = 0.0;
		uint32 StencilClear = 0;

		if (RenderTargetsInfo.bClearColor)
		{
			for (int32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; ++i)
			{
				if (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr)
				{
					const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[i].Texture->GetClearBinding();
					checkf(ClearValue.ColorBinding == EClearBinding::EColorBound, TEXT("Texture: %s does not have a color bound for fast clears"), *RenderTargetsInfo.ColorRenderTarget[i].Texture->GetName().GetPlainNameString());
					ClearColors[i] = ClearValue.GetClearColor();
				}
				else
				{
					ClearColors[i] = FLinearColor(ForceInitToZero);
				}
			}
		}
		if (RenderTargetsInfo.bClearDepth || RenderTargetsInfo.bClearStencil)
		{
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			checkf(ClearValue.ColorBinding == EClearBinding::EDepthStencilBound, TEXT("Texture: %s does not have a DS value bound for fast clears"), *RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetName().GetPlainNameString());
			ClearValue.GetDepthStencil(DepthClear, StencilClear);
		}

		this->RHIClearMRTImpl(RenderTargetsInfo.bClearColor, RenderTargetsInfo.NumColorRenderTargets, ClearColors, RenderTargetsInfo.bClearDepth, DepthClear, RenderTargetsInfo.bClearStencil, StencilClear);
	}
}

// Occlusion/Timer queries.
void FD3D12CommandContext::RHIBeginRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FD3D12RenderQuery* Query = RetrieveObject<FD3D12RenderQuery>(QueryRHI);
	check(IsDefaultContext());
	check(Query->Type == RQT_Occlusion);

	GetParentDevice()->GetOcclusionQueryHeap(CommandQueueType)->BeginQuery(*this, Query);

#if EXECUTE_DEBUG_COMMAND_LISTS
	GIsDoingQuery = true;
#endif
}

void FD3D12CommandContext::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FD3D12RenderQuery* Query = RetrieveObject<FD3D12RenderQuery>(QueryRHI);
	check(IsDefaultContext());

	FD3D12QueryHeap* QueryHeap = nullptr;

	switch (Query->Type)
	{
	case RQT_Occlusion:
		QueryHeap = GetParentDevice()->GetOcclusionQueryHeap(CommandQueueType);
		break;

	case RQT_AbsoluteTime:
		QueryHeap = GetParentDevice()->GetTimestampQueryHeap(CommandQueueType);
		break;

	default:
		ensure(false);
	}

	QueryHeap->EndQuery(*this, Query);
	// Multi-GPU support : by setting a timestamp, we can filter only the relevant GPUs when getting the query results.
	Query->Timestamp = GFrameNumberRenderThread;

	// Query data isn't ready until it has been resolved.
	ensure(Query->bResultIsCached == false && Query->bResolved == false);

#if EXECUTE_DEBUG_COMMAND_LISTS
	GIsDoingQuery = false;
#endif
}

void FD3D12CommandContext::RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery)
{
	FGPUTimingCalibrationTimestamp Timestamp = GetParentDevice()->GetCommandListManager().GetCalibrationTimestamp();
	CalibrationQuery->CPUMicroseconds = Timestamp.CPUMicroseconds;
	CalibrationQuery->GPUMicroseconds = Timestamp.GPUMicroseconds;
}

// Primitive drawing.

void FD3D12CommandContext::CommitNonComputeShaderConstants()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitGraphicsConstants);

	const FD3D12GraphicsPipelineState* const RESTRICT GraphicPSO = StateCache.GetGraphicsPipelineState();

	check(GraphicPSO);

	// Only set the constant buffer if this shader needs the global constant buffer bound
	// Otherwise we will overwrite a different constant buffer
	if (GraphicPSO->bShaderNeedsGlobalConstantBuffer[SF_Vertex])
	{
		StateCache.SetConstantBuffer<SF_Vertex>(VSConstantBuffer, bDiscardSharedGraphicsConstants);
	}

	// Skip HS/DS CB updates in cases where tessellation isn't being used
	// Note that this is *potentially* unsafe because bDiscardSharedGraphicsConstants is cleared at the
	// end of the function, however we're OK for now because bDiscardSharedGraphicsConstants
	// is always reset whenever bUsingTessellation changes in SetBoundShaderState()
	if (bUsingTessellation)
	{
		if (GraphicPSO->bShaderNeedsGlobalConstantBuffer[SF_Hull])
		{
			StateCache.SetConstantBuffer<SF_Hull>(HSConstantBuffer, bDiscardSharedGraphicsConstants);
		}

		if (GraphicPSO->bShaderNeedsGlobalConstantBuffer[SF_Domain])
		{
			StateCache.SetConstantBuffer<SF_Domain>(DSConstantBuffer, bDiscardSharedGraphicsConstants);
		}
	}

	if (GraphicPSO->bShaderNeedsGlobalConstantBuffer[SF_Geometry])
	{
		StateCache.SetConstantBuffer<SF_Geometry>(GSConstantBuffer, bDiscardSharedGraphicsConstants);
	}

	if (GraphicPSO->bShaderNeedsGlobalConstantBuffer[SF_Pixel])
	{
		StateCache.SetConstantBuffer<SF_Pixel>(PSConstantBuffer, bDiscardSharedGraphicsConstants);
	}

	bDiscardSharedGraphicsConstants = false;
}

void FD3D12CommandContext::CommitComputeShaderConstants()
{
	StateCache.SetConstantBuffer<SF_Compute>(CSConstantBuffer, bDiscardSharedComputeConstants);

	bDiscardSharedComputeConstants = false;
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12ShaderResourceView* RESTRICT SRV)
{
	// We set the resource through the RHI to track state for the purposes of unbinding SRVs when a UAV or RTV is bound.
	CmdContext.StateCache.SetShaderResourceView<Frequency>(SRV, BindIndex);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12SamplerState* RESTRICT SamplerState)
{
	CmdContext.StateCache.SetSamplerState<Frequency>(SamplerState, BindIndex);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12UnorderedAccessView* UAV)
{
	uint32 InitialCount = -1;

	// Actually set the UAV
	CmdContext.StateCache.SetUAVs<SF_Pixel>(BindIndex, 1, &UAV, &InitialCount);
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Surface(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
#if ENABLE_RHI_VALIDATION
	constexpr ERHIAccess SRVAccess = (ShaderFrequency == SF_Compute) ? ERHIAccess::SRVCompute : ERHIAccess::SRVGraphics;
#endif

	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	const float CurrentTime = FApp::GetCurrentTime();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FRHITexture* TextureRHI = (FRHITexture*)Resources[ResourceIndex].GetReference();
			TextureRHI->SetLastRenderTime(CurrentTime);

			FD3D12TextureBase* TextureD3D12 = CmdContext.RetrieveTextureBase(TextureRHI);
			FD3D12ShaderResourceView* D3D12Resource = TextureD3D12->GetShaderResourceView();
			if (D3D12Resource == nullptr)
			{
				D3D12Resource = CmdContext.RetrieveTextureBase(GWhiteTexture->TextureRHI)->GetShaderResourceView();
			}

#if ENABLE_RHI_VALIDATION
			if (CmdContext.Tracker)
			{
				CmdContext.Tracker->Assert(TextureRHI->GetWholeResourceIdentitySRV(), SRVAccess);
			}
#endif

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_SRV(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
#if ENABLE_RHI_VALIDATION
	constexpr ERHIAccess SRVAccess = (ShaderFrequency == SF_Compute) ? ERHIAccess::SRVCompute : ERHIAccess::SRVGraphics;
#endif

	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D12ShaderResourceView* D3D12Resource = CmdContext.RetrieveObject<FD3D12ShaderResourceView>((FRHIShaderResourceView*)(Resources[ResourceIndex].GetReference()));

#if ENABLE_RHI_VALIDATION
			if (CmdContext.Tracker)
			{
				CmdContext.Tracker->Assert(D3D12Resource->ViewIdentity, SRVAccess);
			}
#endif

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Sampler(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			// todo: could coalesce adjacent bound resources.
			FD3D12SamplerState* D3D12Resource = CmdContext.RetrieveObject<FD3D12SamplerState>((FRHISamplerState*)(Resources[ResourceIndex].GetReference()));

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}

template <class ShaderType>
void FD3D12CommandContext::SetResourcesFromTables(const ShaderType* RESTRICT Shader)
{
	checkSlow(Shader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers[ShaderType::StaticFrequency];
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FD3D12UniformBuffer* Buffer = BoundUniformBuffers[ShaderType::StaticFrequency][BufferIndex];
		check(Buffer);
		check(BufferIndex < Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// todo: could make this two pass: gather then set
		SetShaderResourcesFromBuffer_Surface<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer_SRV<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer_Sampler<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}

	DirtyUniformBuffers[ShaderType::StaticFrequency] = 0;
}


template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_UAVPS(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
#if ENABLE_RHI_VALIDATION
	constexpr ERHIAccess UAVAccess = (ShaderFrequency == SF_Compute) ? ERHIAccess::UAVCompute : ERHIAccess::UAVGraphics;
#endif

	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FRHIUnorderedAccessView* RHIUAV = (FRHIUnorderedAccessView*)(Resources[ResourceIndex].GetReference());

			FD3D12UnorderedAccessView* D3D12Resource = CmdContext.RetrieveObject<FD3D12UnorderedAccessView>(RHIUAV);

#if ENABLE_RHI_VALIDATION
			if (CmdContext.Tracker)
			{
				CmdContext.Tracker->Assert(D3D12Resource->ViewIdentity, UAVAccess);
			}
#endif

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);

			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}


template <class ShaderType>
uint32 FD3D12CommandContext::SetUAVPSResourcesFromTables(const ShaderType* RESTRICT Shader)
{
	checkSlow(Shader);

	int32 NumChanged = 0;
	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers[ShaderType::StaticFrequency];
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FD3D12UniformBuffer* Buffer = BoundUniformBuffers[ShaderType::StaticFrequency][BufferIndex];
		check(Buffer);
		check(BufferIndex < Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		if ((EShaderFrequency)ShaderType::StaticFrequency == SF_Pixel)
		{
			NumChanged += SetShaderResourcesFromBuffer_UAVPS<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.UnorderedAccessViewMap.GetData(), BufferIndex);
		}
	}
	return NumChanged;

}

void FD3D12CommandContext::CommitGraphicsResourceTables()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitResourceTables);

	const FD3D12GraphicsPipelineState* const RESTRICT GraphicPSO = StateCache.GetGraphicsPipelineState();
	check(GraphicPSO);

	auto* PixelShader = GraphicPSO->GetPixelShader();
	if (PixelShader)
	{
		SetUAVPSResourcesFromTables(PixelShader);
	}
	if (auto* Shader = GraphicPSO->GetVertexShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (PixelShader)
	{
		SetResourcesFromTables(PixelShader);
	}
	if (auto* Shader = GraphicPSO->GetHullShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = GraphicPSO->GetDomainShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = GraphicPSO->GetGeometryShader())
	{
		SetResourcesFromTables(Shader);
	}
}

void FD3D12CommandContext::CommitComputeResourceTables(FD3D12ComputeShader* InComputeShader)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitResourceTables);

	FD3D12ComputeShader* RESTRICT ComputeShader = InComputeShader;
	check(ComputeShader);
	SetResourcesFromTables(ComputeShader);
}

void FD3D12CommandContext::RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(StateCache.GetGraphicsPipelinePrimitiveType(), FMath::Max(NumInstances, 1U) * NumPrimitives);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	uint32 VertexCount = StateCache.GetVertexCountAndIncrementStat(NumPrimitives);
	NumInstances = FMath::Max<uint32>(1, NumInstances);
	numDraws++;
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUWork(NumPrimitives * NumInstances, VertexCount * NumInstances);
	}

	StateCache.ApplyState<D3D12PT_Graphics>();
	CommandListHandle->DrawInstanced(VertexCount, NumInstances, BaseVertexIndex, 0);

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12Buffer* ArgumentBuffer = RetrieveObject<FD3D12Buffer>(ArgumentBufferRHI);

	numDraws++;
	RHI_DRAW_CALL_INC();
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	FD3D12ResourceLocation& Location = ArgumentBuffer->ResourceLocation;

	StateCache.ApplyState<D3D12PT_Graphics>();

	// Indirect args buffer can be a previously pending UAV, which becomes PS\Non-PS read. ApplyState will flush pending transitions, so enqueue the indirect
	// arg transition and flush here.
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Validate);
	CommandListHandle.FlushResourceBarriers();	// Must flush so the desired state is actually set.

	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(Location.GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	const uint32 IndexBufferStride = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI)->GetStride();
	const uint32 ArgumentsBufferStride = FD3D12DynamicRHI::ResourceCast(ArgumentsBufferRHI)->GetStride();

	FD3D12Buffer* IndexBuffer = RetrieveObject<FD3D12Buffer>(IndexBufferRHI);
	FD3D12Buffer* ArgumentsBuffer = RetrieveObject<FD3D12Buffer>(ArgumentsBufferRHI);

	numDraws++;
	RHI_DRAW_CALL_INC();
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUWork(1);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	const DXGI_FORMAT Format = (IndexBufferStride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation, Format, 0);

	FD3D12ResourceLocation& Location = ArgumentsBuffer->ResourceLocation;

	StateCache.ApplyState<D3D12PT_Graphics>();

	// Indirect args buffer can be a previously pending UAV, which becomes PS\Non-PS read. ApplyState will flush pending transitions, so enqueue the indirect
	// arg transition and flush here.
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Validate);
	CommandListHandle.FlushResourceBarriers();	// Must flush so the desired state is actually set.

	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndexedIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + DrawArgumentsIndex * ArgumentsBufferStride,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(Location.GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	// called should make sure the input is valid, this avoid hidden bugs
	ensure(NumPrimitives > 0);
	RHI_DRAW_CALL_STATS(StateCache.GetGraphicsPipelinePrimitiveType(), FMath::Max(NumInstances, 1U) * NumPrimitives);

	NumInstances = FMath::Max<uint32>(1, NumInstances);
	numDraws++;
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	uint32 IndexCount = StateCache.GetVertexCountAndIncrementStat(NumPrimitives);

	FD3D12Buffer* IndexBuffer = RetrieveObject<FD3D12Buffer>(IndexBufferRHI);

	// Verify that we are not trying to read outside the index buffer range
	// test is an optimized version of: StartIndex + IndexCount <= IndexBuffer->GetSize() / IndexBuffer->GetStride() 
	checkf((StartIndex + IndexCount) * IndexBuffer->GetStride() <= IndexBuffer->GetSize(),
		TEXT("Start %u, Count %u, Type %u, Buffer Size %u, Buffer stride %u"), StartIndex, IndexCount, StateCache.GetGraphicsPipelinePrimitiveType(), IndexBuffer->GetSize(), IndexBuffer->GetStride());

	// determine 16bit vs 32bit indices
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
	StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation, Format, 0);
	StateCache.ApplyState<D3D12PT_Graphics>();

	CommandListHandle->DrawIndexedInstanced(IndexCount, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	const uint32 IndexBufferStride = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI)->GetStride();
	FD3D12Buffer* IndexBuffer = RetrieveObject<FD3D12Buffer>(IndexBufferRHI);
	FD3D12Buffer* ArgumentBuffer = RetrieveObject<FD3D12Buffer>(ArgumentBufferRHI);

	numDraws++;
	RHI_DRAW_CALL_INC();
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// Set the index buffer.
	const DXGI_FORMAT Format = (IndexBufferStride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
	StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation, Format, 0);

	FD3D12ResourceLocation& Location = ArgumentBuffer->ResourceLocation;

	StateCache.ApplyState<D3D12PT_Graphics>();

	// Indirect args buffer can be a previously pending UAV, which becomes PS\Non-PS read. ApplyState will flush pending transitions, so enqueue the indirect
	// arg transition and flush here.
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Validate);
	CommandListHandle.FlushResourceBarriers();	// Must flush so the desired state is actually set.

	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndexedIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(Location.GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

// Raster operations.
void FD3D12CommandContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	RHIClearMRTImpl(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FD3D12CommandContext::RHIClearMRTImpl(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ClearMRT);

	const D3D12_VIEWPORT& Viewport = StateCache.GetViewport();
	const D3D12_RECT& ScissorRect = StateCache.GetScissorRect();

	if (ScissorRect.left >= ScissorRect.right || ScissorRect.top >= ScissorRect.bottom)
	{
		return;
	}

	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	FD3D12DepthStencilView* DSView = nullptr;
	uint32 NumSimultaneousRTs = 0;
	StateCache.GetRenderTargets(RenderTargetViews, &NumSimultaneousRTs, &DSView);
	FD3D12BoundRenderTargets BoundRenderTargets(RenderTargetViews, NumSimultaneousRTs, DSView);
	FD3D12DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	// Use rounding for when the number can't be perfectly represented by a float
	const LONG Width = static_cast<LONG>(FMath::RoundToInt(Viewport.Width));
	const LONG Height = static_cast<LONG>(FMath::RoundToInt(Viewport.Height));

	// When clearing we must pay attention to the currently set scissor rect
	bool bClearCoversEntireSurface = false;
	if (ScissorRect.left <= 0 && ScissorRect.top <= 0 &&
		ScissorRect.right >= Width && ScissorRect.bottom >= Height)
	{
		bClearCoversEntireSurface = true;
	}

	// Must specify enough clear colors for all active RTs
	check(!bClearColor || NumClearColors >= BoundRenderTargets.GetNumActiveTargets());

	const bool bSupportsFastClear = true;
	uint32 ClearRectCount = 0;
	D3D12_RECT* pClearRects = nullptr;
	D3D12_RECT ClearRects[4];

	// Only pass a rect down to the driver if we specifically want to clear a sub-rect
	if (!bSupportsFastClear || !bClearCoversEntireSurface)
	{
		{
			ClearRects[ClearRectCount] = ScissorRect;
			ClearRectCount++;
		}

		pClearRects = ClearRects;

		static const bool bSpewPerfWarnings = false;

		if (bSpewPerfWarnings)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Using non-fast clear path! This has performance implications"));
			UE_LOG(LogD3D12RHI, Warning, TEXT("       Viewport: Width %d, Height: %d"), static_cast<LONG>(FMath::RoundToInt(Viewport.Width)), static_cast<LONG>(FMath::RoundToInt(Viewport.Height)));
			UE_LOG(LogD3D12RHI, Warning, TEXT("   Scissor Rect: Width %d, Height: %d"), ScissorRect.right, ScissorRect.bottom);
		}
	}

	const bool ClearRTV = bClearColor && BoundRenderTargets.GetNumActiveTargets() > 0;
	const bool ClearDSV = (bClearDepth || bClearStencil) && DepthStencilView;

	uint32 ClearFlags = 0;
	if (ClearDSV)
	{
		if (bClearDepth && DepthStencilView->HasDepth())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
		}
		else if (bClearDepth)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store depth."));
		}

		if (bClearStencil && DepthStencilView->HasStencil())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_STENCIL;
		}
		else if (bClearStencil)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store stencil."));
		}
	}

	if (ClearRTV || ClearDSV)
	{
		CommandListHandle.FlushResourceBarriers();

		if (ClearRTV)
		{
			for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
			{
				FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);

				if (RTView != nullptr)
				{
					numClears++;
					CommandListHandle->ClearRenderTargetView(RTView->GetView(), (float*)&ClearColorArray[TargetIndex], ClearRectCount, pClearRects);
					CommandListHandle.UpdateResidency(RTView->GetResource());
				}
			}
		}

		if (ClearDSV)
		{
			numClears++;
			CommandListHandle->ClearDepthStencilView(DepthStencilView->GetView(), (D3D12_CLEAR_FLAGS)ClearFlags, Depth, Stencil, ClearRectCount, pClearRects);
			CommandListHandle.UpdateResidency(DepthStencilView->GetResource());
		}
	}

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D12DynamicRHI::RHIBlockUntilGPUIdle()
{
	const int32 NumAdapters = ChosenAdapters.Num();
	for (int32 Index = 0; Index < NumAdapters; ++Index)
	{
		GetAdapter(Index).BlockUntilIdle();
	}
}

void FD3D12DynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	FD3D12Adapter& Adapter = GetAdapter();
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		Adapter.GetDevice(GPUIndex)->GetDefaultCommandContext().RHISubmitCommandsHint();
	}
}

/*
* Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
*/
uint32 FD3D12DynamicRHI::RHIGetGPUFrameCycles(uint32 GPUIndex)
{
	return GGPUFrameTime;
}

void FD3D12DynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	check(0); // this path has gone stale and needs updated methods, starting at ERCT_SetScissorRect
}


void FD3D12CommandContext::RHISetDepthBounds(float MinDepth, float MaxDepth)
{
	StateCache.SetDepthBounds(MinDepth, MaxDepth);
}

void FD3D12CommandContext::SetDepthBounds(float MinDepth, float MaxDepth)
{
#if PLATFORM_WINDOWS
	if (GSupportsDepthBoundsTest && CommandListHandle.GraphicsCommandList1())
	{
		// This should only be called if Depth Bounds Test is supported.
		CommandListHandle.GraphicsCommandList1()->OMSetDepthBounds(MinDepth, MaxDepth);
	}
#endif
}


#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
static D3D12_SHADING_RATE_COMBINER ConvertShadingRateCombiner(EVRSRateCombiner InCombiner)
{
	switch (InCombiner)
	{
	case VRSRB_Override:
		return D3D12_SHADING_RATE_COMBINER_OVERRIDE;
	case VRSRB_Min:
		return D3D12_SHADING_RATE_COMBINER_MIN;
	case VRSRB_Max:
		return D3D12_SHADING_RATE_COMBINER_MAX;
	case VRSRB_Sum:
		return D3D12_SHADING_RATE_COMBINER_SUM;
	case VRSRB_Passthrough:
	default:
		return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
	}
	return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
}
#endif

void FD3D12CommandContext::RHISetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner)
{
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
	StateCache.SetShadingRate(ShadingRate, Combiner);
#endif
}

 void FD3D12CommandContext::SetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner)
 {
 #if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
 	if (GRHISupportsVariableRateShading && CommandListHandle.GraphicsCommandList5())
 	{
 		VRSCombiners[0] = ConvertShadingRateCombiner(Combiner);	// Combiner 0 is used to mix per draw and per VS/GS rates
 		VRSShadingRate = static_cast<D3D12_SHADING_RATE>(ShadingRate);
 		CommandListHandle.GraphicsCommandList5()->RSSetShadingRate(VRSShadingRate, VRSCombiners);
 	}
 #endif
}

void FD3D12CommandContext::RHISetShadingRateImage(FRHITexture* RateImageTexture, EVRSRateCombiner Combiner)
{
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
	if (GRHISupportsVariableRateShading && CommandListHandle.GraphicsCommandList5())
	{
		VRSCombiners[1] = ConvertShadingRateCombiner(Combiner); // Combiner 1 is used to mix rates from a texture and the previous combiner
		if (RateImageTexture)
		{
			FD3D12Resource* Resource = RetrieveTextureBase(RateImageTexture)->GetResource();
			CommandListHandle.GraphicsCommandList5()->RSSetShadingRateImage(Resource->GetResource());
		}
		else
		{
			CommandListHandle.GraphicsCommandList5()->RSSetShadingRateImage(nullptr);
		}
		CommandListHandle.GraphicsCommandList5()->RSSetShadingRate(VRSShadingRate, VRSCombiners);
	}
#endif
}

void FD3D12CommandContext::RHISubmitCommandsHint()
{
	// Resolve any timestamp queries so far (if any).
	GetParentDevice()->GetTimestampQueryHeap(CommandQueueType)->EndQueryBatchAndResolveQueryData(*this);

	// Submit the work we have so far, and start a new command list.
	FlushCommands();
}

#define USE_COPY_QUEUE_FOR_RESOURCE_SYNC 1
/*
* When using AFR certain inter-frame dependecies need to be synchronized across all GPUs.
* For example a rendering technique that relies on results from the previous frame (which occured on the other GPU).
*/

void FD3D12CommandContext::RHIWaitForTemporalEffect(const FName& InEffectName)
{
#if WITH_MGPU
	check(IsDefaultContext());

	if (GNumAlternateFrameRenderingGroups == 1 || !AFRSyncTemporalResources)
	{
		return;
	}

	FD3D12Adapter* Adapter = GetParentAdapter();
	FD3D12TemporalEffect* Effect = Adapter->GetTemporalEffect(InEffectName);

	const uint32 GPUIndex = GetGPUIndex();
	if (Effect->ShouldWaitForPrevious(GPUIndex))
	{
		// Execute the current command list so we can have a point to insert a wait
		FlushCommands();

		Effect->WaitForPrevious(GPUIndex, CommandQueueType);
	}
#endif // WITH_MGPU
}

void FD3D12CommandContext::RHIBroadcastTemporalEffect(const FName& InEffectName, const TArrayView<FRHITexture*> InTextures)
{
#if WITH_MGPU
	check(IsDefaultContext());

	if (GNumAlternateFrameRenderingGroups == 1 || !AFRSyncTemporalResources)
	{
		return;
	}

	const uint32 GPUIndex = GetGPUIndex();
	TArray<FD3D12TextureBase*, TInlineAllocator<MAX_NUM_GPUS>> SrcTextures, DstTextures;
	const int32 NumTextures = InTextures.Num();
	for (int32 i = 0; i < NumTextures; i++)
	{
		SrcTextures.Emplace(RetrieveTextureBase(InTextures[i]));
		const uint32 NextSiblingGPUIndex = AFRUtils::GetNextSiblingGPUIndex(GPUIndex);
		DstTextures.Emplace(RetrieveTextureBase(InTextures[i], NextSiblingGPUIndex));
	}

	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	FD3D12TemporalEffect* Effect = Adapter->GetTemporalEffect(InEffectName);

#if USE_COPY_QUEUE_FOR_RESOURCE_SYNC

	for (int32 i = 0; i < NumTextures; i++)
	{
		// Resources must be in the COMMON state before using on the copy queue.
		FD3D12DynamicRHI::TransitionResource(CommandListHandle, SrcTextures[i]->GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Apply);
		FD3D12DynamicRHI::TransitionResource(CommandListHandle, DstTextures[i]->GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, FD3D12DynamicRHI::ETransitionMode::Apply);
	}
	CommandListHandle.FlushResourceBarriers();

	// Finish rendering on the current queue.
	FlushCommands();

	// Tell the copy queue to wait for the current queue to finish rendering before starting the copy.
	Effect->SignalSyncComplete(GPUIndex, CommandQueueType);
	Effect->WaitForPrevious(GPUIndex, ED3D12CommandQueueType::Copy);

	FD3D12CommandAllocatorManager& CopyCommandAllocatorManager = Device->GetTextureStreamingCommandAllocatorManager();
	FD3D12CommandAllocator* CopyCommandAllocator = CopyCommandAllocatorManager.ObtainCommandAllocator();
	FD3D12CommandListManager& CopyCommandListManager = Device->GetCopyCommandListManager();
	FD3D12CommandListHandle hCopyCommandList = CopyCommandListManager.ObtainCommandList(*CopyCommandAllocator);
	hCopyCommandList.SetCurrentOwningContext(this);

	for (int32 i = 0; i < NumTextures; i++)
	{
		// NB: We do not increment numCopies here because the main context isn't doing any work.
		hCopyCommandList->CopyResource(DstTextures[i]->GetResource()->GetResource(), SrcTextures[i]->GetResource()->GetResource());
	}
	hCopyCommandList.Close();

	CopyCommandListManager.ExecuteCommandList(hCopyCommandList);
	CopyCommandAllocatorManager.ReleaseCommandAllocator(CopyCommandAllocator);

	// Signal again once the copy queue copy is complete.
	Effect->SignalSyncComplete(GPUIndex, ED3D12CommandQueueType::Copy);

#else

	for (int32 i = 0; i < NumTextures; i++)
	{
		FD3D12DynamicRHI::TransitionResource(CommandListHandle, SrcTextures[i]->GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		FD3D12DynamicRHI::TransitionResource(CommandListHandle, DstTextures[i]->GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	}
	CommandListHandle.FlushResourceBarriers();

	for (int32 i = 0; i < NumTextures; i++)
	{
		numCopies++;
		CommandListHandle->CopyResource(DstTextures[i]->GetResource()->GetResource(), SrcTextures[i]->GetResource()->GetResource());
	}

	FlushCommands();

	Effect->SignalSyncComplete(GPUIndex, CommandQueueType);

#endif // USE_COPY_QUEUE_FOR_RESOURCE_SYNC
#endif // WITH_MGPU
}