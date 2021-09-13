// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureCompiler.h"

#if WITH_EDITOR

#include "Engine/Texture.h"
#include "ObjectCacheContext.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "TextureDerivedDataTask.h"
#include "Misc/IQueuedWork.h"
#include "Components/PrimitiveComponent.h"
#include "AsyncCompilationHelpers.h"
#include "AssetCompilingManager.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "TextureCompiler"

static AsyncCompilationHelpers::FAsyncCompilationStandardCVars CVarAsyncTextureStandard(
	TEXT("Texture"),
	TEXT("textures"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			FTextureCompilingManager::Get().FinishAllCompilation();
		}
	));

namespace TextureCompilingManagerImpl
{
	static FString GetLODGroupName(UTexture* Texture)
	{
		return StaticEnum<TextureGroup>()->GetMetaData(TEXT("DisplayName"), Texture->LODGroup);
	}

	static EQueuedWorkPriority GetBasePriority(UTexture* InTexture)
	{
		switch (InTexture->LODGroup)
		{
		case TEXTUREGROUP_UI:
			return EQueuedWorkPriority::High;
		case TEXTUREGROUP_Terrain_Heightmap:
			return EQueuedWorkPriority::Normal;
		default:
			return EQueuedWorkPriority::Lowest;
		}
	}

	static EQueuedWorkPriority GetBoostPriority(UTexture* InTexture)
	{
		return (EQueuedWorkPriority)(FMath::Max((uint8)1, (uint8)GetBasePriority(InTexture)) - 1);
	}

	static void EnsureInitializedCVars()
	{
		static bool bIsInitialized = false;

		if (!bIsInitialized)
		{
			bIsInitialized = true;

			AsyncCompilationHelpers::EnsureInitializedCVars(
				TEXT("texture"),
				CVarAsyncTextureStandard.AsyncCompilation,
				CVarAsyncTextureStandard.AsyncCompilationMaxConcurrency,
				GET_MEMBER_NAME_CHECKED(UEditorExperimentalSettings, bEnableAsyncTextureCompilation));
		}
	}
}

FTextureCompilingManager::FTextureCompilingManager()
	: Notification(LOCTEXT("Textures", "Textures"))
{
}

EQueuedWorkPriority FTextureCompilingManager::GetBasePriority(UTexture* InTexture) const
{
	return TextureCompilingManagerImpl::GetBasePriority(InTexture);
}

FQueuedThreadPool* FTextureCompilingManager::GetThreadPool() const
{
	static FQueuedThreadPoolWrapper* GTextureThreadPool = nullptr;
	if (GTextureThreadPool == nullptr)
	{
		TextureCompilingManagerImpl::EnsureInitializedCVars();

		const auto TexturePriorityMapper = [](EQueuedWorkPriority TexturePriority) { return FMath::Max(TexturePriority, EQueuedWorkPriority::Low); };

		// Textures will be scheduled on the asset thread pool, where concurrency limits might by dynamically adjusted depending on memory constraints.
		GTextureThreadPool = new FQueuedThreadPoolWrapper(FAssetCompilingManager::Get().GetThreadPool(), -1, TexturePriorityMapper);

		AsyncCompilationHelpers::BindThreadPoolToCVar(
			GTextureThreadPool,
			CVarAsyncTextureStandard.AsyncCompilation,
			CVarAsyncTextureStandard.AsyncCompilationResume,
			CVarAsyncTextureStandard.AsyncCompilationMaxConcurrency
		);
	}

	return GTextureThreadPool;
}

void FTextureCompilingManager::Shutdown()
{
	bHasShutdown = true;
	if (GetNumRemainingTextures())
	{
		TArray<UTexture*> PendingTextures;
		PendingTextures.Reserve(GetNumRemainingTextures());

		for (TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
		{
			for (TWeakObjectPtr<UTexture>& WeakTexture : Bucket)
			{
				if (WeakTexture.IsValid())
				{
					UTexture* Texture = WeakTexture.Get();
					
					if (!Texture->TryCancelCachePlatformData())
					{
						PendingTextures.Add(Texture);
					}
				}
			}
		}

		// Wait on textures already in progress we couldn't cancel
		FinishCompilation(PendingTextures);
	}
}

bool FTextureCompilingManager::IsAsyncTextureCompilationEnabled() const
{
	if (bHasShutdown)
	{
		return false;
	}

	TextureCompilingManagerImpl::EnsureInitializedCVars();

	return CVarAsyncTextureStandard.AsyncCompilation.GetValueOnAnyThread() != 0;
}

void FTextureCompilingManager::UpdateCompilationNotification()
{
	Notification.Update(GetNumRemainingTextures());
}

void FTextureCompilingManager::PostCompilation(UTexture* Texture)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::PostCompilation);

	UE_LOG(LogTexture, Verbose, TEXT("Refreshing texture %s because it is ready"), *Texture->GetName());

	Texture->FinishCachePlatformData();
	Texture->UpdateResource();

	// Generate an empty property changed event, to force the asset registry tag
	// to be refreshed now that pixel format and alpha channels are available.
	FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Texture, EmptyPropertyChangedEvent);
}

bool FTextureCompilingManager::IsAsyncCompilationAllowed(UTexture* Texture) const
{
	return IsAsyncTextureCompilationEnabled();
}

FTextureCompilingManager& FTextureCompilingManager::Get()
{
	static FTextureCompilingManager Singleton;
	return Singleton;
}

int32 FTextureCompilingManager::GetNumRemainingTextures() const
{
	int32 Num = 0;
	for (const TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
	{
		Num += Bucket.Num();
	}

	return Num;
}

void FTextureCompilingManager::AddTextures(TArrayView<UTexture* const> InTextures)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::AddTextures)
	check(IsInGameThread());

	// Register new textures after ProcessTextures to avoid
	// potential reentrant calls to CreateResource on the
	// textures being added. This would cause multiple
	// TextureResource to be created and assigned to the same Owner
	// which would obviously be bad and causing leaks including
	// in the RHI.
	for (UTexture* Texture : InTextures)
	{
		int32 TexturePriority = 2;
		switch (Texture->LODGroup)
		{
			case TEXTUREGROUP_UI:
				TexturePriority = 0;
			break;
			case TEXTUREGROUP_Terrain_Heightmap:
				TexturePriority = 1;
			break;
		}

		if (RegisteredTextureBuckets.Num() <= TexturePriority)
		{
			RegisteredTextureBuckets.SetNum(TexturePriority + 1);
		}
		RegisteredTextureBuckets[TexturePriority].Emplace(Texture);
	}
}

void FTextureCompilingManager::FinishCompilation(TArrayView<UTexture* const> InTextures)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::FinishCompilation);

	using namespace TextureCompilingManagerImpl;
	check(IsInGameThread());

	TSet<UTexture*> PendingTextures;
	PendingTextures.Reserve(InTextures.Num());

	int32 TextureIndex = 0;
	for (UTexture* Texture : InTextures)
	{
		for (TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
		{
			if (Bucket.Contains(Texture))
			{
				PendingTextures.Add(Texture);
			}
		}
	}

	if (PendingTextures.Num())
	{
		class FCompilableTexture : public AsyncCompilationHelpers::TCompilableAsyncTask<FTextureAsyncCacheDerivedDataTask>
		{
		public:
			FCompilableTexture(UTexture* InTexture)
				: Texture(InTexture)
			{
			}

			FTextureAsyncCacheDerivedDataTask* GetAsyncTask() override
			{
				if (FTexturePlatformData** PlatformDataPtr = Texture->GetRunningPlatformData())
				{
					if (FTexturePlatformData* PlatformData = *PlatformDataPtr)
					{
						return PlatformData->AsyncTask;
					}
				}

				return nullptr;
			}

			TStrongObjectPtr<UTexture> Texture;
			FName GetName() override { return Texture->GetFName(); }
		};

		TArray<UTexture*> UniqueTextures(PendingTextures.Array());
		TArray<FCompilableTexture> CompilableTextures(UniqueTextures);
		using namespace AsyncCompilationHelpers;
		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableTextures](int32 Index)	-> ICompilable& { return CompilableTextures[Index]; },
			CompilableTextures.Num(),
			LOCTEXT("Textures", "Textures"),
			LogTexture,
			[this](ICompilable* Object)
			{
				UTexture* Texture = static_cast<FCompilableTexture*>(Object)->Texture.Get();
				PostCompilation(Texture);

				for (TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
				{
					Bucket.Remove(Texture);
				}
			}
		);

		PostCompilation(UniqueTextures);
	}
}

void FTextureCompilingManager::PostCompilation(TArrayView<UTexture* const> InCompiledTextures)
{
	using namespace TextureCompilingManagerImpl;
	if (InCompiledTextures.Num())
	{
		FObjectCacheContextScope ObjectCacheScope;
		TRACE_CPUPROFILER_EVENT_SCOPE(PostTextureCompilation);
		{
			TSet<UMaterialInterface*> AffectedMaterials;
			for (UTexture* Texture : InCompiledTextures)
			{
				for (UMaterialInterface* Material : ObjectCacheScope.GetContext().GetMaterialsAffectedByTexture(Texture))
				{
					AffectedMaterials.Add(Material);
				}
			}

			if (AffectedMaterials.Num())
			{
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UpdateMaterials);

					for (UMaterialInterface* MaterialToUpdate : AffectedMaterials)
					{
						FMaterialRenderProxy* RenderProxy = MaterialToUpdate->GetRenderProxy();
						if (RenderProxy)
						{
							ENQUEUE_RENDER_COMMAND(TextureCompiler_RecacheUniformExpressions)(
								[RenderProxy](FRHICommandListImmediate& RHICmdList)
								{
									RenderProxy->CacheUniformExpressions(false);
								});
						}
					}
				}

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UpdatePrimitives);

					TSet<UPrimitiveComponent*> AffectedPrimitives;
					for (UMaterialInterface* MaterialInterface : AffectedMaterials)
					{
						for (UPrimitiveComponent* Component : ObjectCacheScope.GetContext().GetPrimitivesAffectedByMaterial(MaterialInterface))
						{
							AffectedPrimitives.Add(Component);
						}
					}

					for (UPrimitiveComponent* AffectedPrimitive : AffectedPrimitives)
					{
						AffectedPrimitive->MarkRenderStateDirty();
					}
				}
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(OnAssetPostCompileEvent);

			TArray<FAssetCompileData> AssetsData;
			AssetsData.Reserve(InCompiledTextures.Num());

			for (UTexture* Texture : InCompiledTextures)
			{
				AssetsData.Emplace(Texture);
			}

			FAssetCompilingManager::Get().OnAssetPostCompileEvent().Broadcast(AssetsData);
		}
	}
}

void FTextureCompilingManager::FinishAllCompilation()
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::FinishAllCompilation)

	if (GetNumRemainingTextures())
	{
		TArray<UTexture*> PendingTextures;
		PendingTextures.Reserve(GetNumRemainingTextures());

		for (TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
		{
			for (TWeakObjectPtr<UTexture>& Texture : Bucket)
			{
				if (Texture.IsValid())
				{
					PendingTextures.Add(Texture.Get());
				}
			}
		}

		FinishCompilation(PendingTextures);
	}
}

bool FTextureCompilingManager::RequestPriorityChange(UTexture* InTexture, EQueuedWorkPriority InPriority)
{
	using namespace TextureCompilingManagerImpl;
	if (InTexture)
	{
		FTexturePlatformData** Data = InTexture->GetRunningPlatformData();
		if (Data && *Data)
		{
			FTextureAsyncCacheDerivedDataTask* AsyncTask = (*Data)->AsyncTask;
			if (AsyncTask)
			{
				EQueuedWorkPriority OldPriority = AsyncTask->GetPriority();
				if (OldPriority != InPriority)
				{
					if (AsyncTask->Reschedule(GetThreadPool(), InPriority))
					{
						UE_LOG(
							LogTexture,
							Verbose,
							TEXT("Changing priority of %s (%s) from %s to %s"),
							*InTexture->GetName(),
							*GetLODGroupName(InTexture),
							LexToString(OldPriority),
							LexToString(InPriority)
						);

						return true;
					}
				}
			}
		}
	}

	return false;
}

void FTextureCompilingManager::ProcessTextures(bool bLimitExecutionTime, int32 MaximumPriority)
{
	using namespace TextureCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::ProcessTextures);
	const double MaxSecondsPerFrame = 0.016;

	if (GetNumRemainingTextures())
	{
		FObjectCacheContextScope ObjectCacheScope;
		TArray<UTexture*> ProcessedTextures;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedTextures);

			double TickStartTime = FPlatformTime::Seconds();

			if (MaximumPriority == -1 || MaximumPriority > RegisteredTextureBuckets.Num())
			{
				MaximumPriority = RegisteredTextureBuckets.Num();
			}
			
			for (int32 PriorityIndex = 0; PriorityIndex < MaximumPriority; ++PriorityIndex)
			{
				TSet<TWeakObjectPtr<UTexture>>& TexturesToProcess = RegisteredTextureBuckets[PriorityIndex];
				if (TexturesToProcess.Num())
				{
					const bool bIsHighestPrio = PriorityIndex == 0;
			
					TSet<TWeakObjectPtr<UTexture>> TexturesToPostpone;
					for (TWeakObjectPtr<UTexture>& Texture : TexturesToProcess)
					{
						if (Texture.IsValid())
						{
							const bool bHasTimeLeft = bLimitExecutionTime ? ((FPlatformTime::Seconds() - TickStartTime) < MaxSecondsPerFrame) : true;
							if ((bIsHighestPrio || bHasTimeLeft) && Texture->IsAsyncCacheComplete())
							{
								PostCompilation(Texture.Get());
								ProcessedTextures.Add(Texture.Get());
							}
							else
							{
								TexturesToPostpone.Emplace(MoveTemp(Texture));
							}
						}
					}

					RegisteredTextureBuckets[PriorityIndex] = MoveTemp(TexturesToPostpone);
				}
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::Reschedule);

			TSet<UTexture*> ReferencedTextures;
			if (GEngine)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(GatherSeenPrimitiveMaterials);

				TSet<UMaterialInterface*> RenderedMaterials;
				for (UPrimitiveComponent* Component : ObjectCacheScope.GetContext().GetPrimitiveComponents())
				{
					if (Component->IsRegistered() && Component->IsRenderStateCreated() && Component->GetLastRenderTimeOnScreen() > 0.0f)
					{
						for (UMaterialInterface* MaterialInterface : ObjectCacheScope.GetContext().GetUsedMaterials(Component))
						{
							if (MaterialInterface)
							{
								RenderedMaterials.Add(MaterialInterface);
							}
						}
					}
				}

				for (UMaterialInterface* MaterialInstance : RenderedMaterials)
				{
					for (UTexture* Texture : ObjectCacheScope.GetContext().GetUsedTextures(MaterialInstance))
					{
						ReferencedTextures.Add(Texture);
					}
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ApplyPriorityChanges);
				// Reschedule higher priority if they have been rendered
				for (TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
				{
					for (TWeakObjectPtr<UTexture>& WeakPtr : Bucket)
					{
						if (UTexture* Texture = WeakPtr.Get())
						{
							// Reschedule any texture that have been rendered with slightly higher priority 
							// to improve the editor experience for low-core count.
							//
							// Keep in mind that some textures are only accessed once during the construction
							// of a virtual texture, so we can't count on the LastRenderTime to be updated
							// continuously for those even if they're in view.
							if (ReferencedTextures.Contains(Texture) ||
								(Texture->Resource && Texture->Resource->LastRenderTime > 0.0f) ||
								Texture->TextureReference.GetLastRenderTime() > 0.0f)
							{
								RequestPriorityChange(Texture, GetBoostPriority(Texture));
							}
						}
					}
				}
			}
		}

		PostCompilation(ProcessedTextures);
	}
}

void FTextureCompilingManager::FinishCompilationsForGame()
{
	if (GetNumRemainingTextures())
	{
		// Supports both Game and PIE mode
		const bool bIsPlaying =
			(GWorld && !GWorld->IsEditorWorld()) ||
			(GEditor && GEditor->PlayWorld && !GEditor->IsSimulateInEditorInProgress());

		if (bIsPlaying)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FTextureCompilingManager::FinishCompilationsForGame);

			TSet<UTexture*> TexturesRequiredForGame;
			for (TSet<TWeakObjectPtr<UTexture>>& Bucket : RegisteredTextureBuckets)
			{
				for (TWeakObjectPtr<UTexture>& WeakTexture : Bucket)
				{
					if (UTexture* Texture = WeakTexture.Get())
					{
						switch (Texture->LODGroup)
						{
						case TEXTUREGROUP_Terrain_Heightmap:
						case TEXTUREGROUP_Terrain_Weightmap:
							TexturesRequiredForGame.Add(Texture);
							break;
						default:
							break;
						}
					}
				}
			}

			if (TexturesRequiredForGame.Num())
			{
				FinishCompilation(TexturesRequiredForGame.Array());
			}
		}
	}
}

void FTextureCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;
	FinishCompilationsForGame();

	ProcessTextures(bLimitExecutionTime);

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // #if WITH_EDITOR
