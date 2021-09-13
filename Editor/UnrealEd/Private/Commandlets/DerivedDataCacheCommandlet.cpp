// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DerivedDataCacheCommandlet.cpp: Commandlet for DDC maintenence
=============================================================================*/
#include "Commandlets/DerivedDataCacheCommandlet.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "GlobalShader.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "ShaderCompiler.h"
#include "DistanceFieldAtlas.h"
#include "MeshCardRepresentation.h"
#include "Misc/RedirectCollector.h"
#include "Engine/Texture.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "Settings/ProjectPackagingSettings.h"
#include "AssetCompilingManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDerivedDataCacheCommandlet, Log, All);

UDerivedDataCacheCommandlet::UDerivedDataCacheCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}

void UDerivedDataCacheCommandlet::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	if (ProcessedPackages.Contains(Package->GetFName()))
	{
		UE_LOG(LogDerivedDataCacheCommandlet, Verbose, TEXT("Marking %s already loaded."), *Package->GetName());
		Package->SetPackageFlags(PKG_ReloadingForCooker);
	}
}

static void WaitForCurrentShaderCompilationToFinish(bool& bInOutHadActivity)
{
	if (GShaderCompilingManager->IsCompiling())
	{
		bInOutHadActivity = true;
		int32 CachedShaderCount = GShaderCompilingManager->GetNumRemainingJobs();
		if (CachedShaderCount > 0)
		{
			UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Waiting for %d shaders to finish."), CachedShaderCount);
		}
		int32 NumCompletedShadersSinceLastLog = 0;
		while (GShaderCompilingManager->IsCompiling())
		{
			const int32 CurrentShaderCount = GShaderCompilingManager->GetNumRemainingJobs();
			NumCompletedShadersSinceLastLog += (CachedShaderCount - CurrentShaderCount);
			CachedShaderCount = CurrentShaderCount;

			if (NumCompletedShadersSinceLastLog >= 1000)
			{
				UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Waiting for %d shaders to finish."), CachedShaderCount);
				NumCompletedShadersSinceLastLog = 0;
			}

			// Process any asynchronous shader compile results that are ready, limit execution time
			GShaderCompilingManager->ProcessAsyncResults(true, false);
			GDistanceFieldAsyncQueue->ProcessAsyncTasks();
			GCardRepresentationAsyncQueue->ProcessAsyncTasks();
		}
		GShaderCompilingManager->FinishAllCompilation(); // Final blocking check as IsCompiling() may be non-deterministic
		UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Done waiting for shaders to finish."));
	}

	// these shouldn't be predicated on whether the shaders were being compiled
	GDistanceFieldAsyncQueue->BlockUntilAllBuildsComplete();
	GCardRepresentationAsyncQueue->BlockUntilAllBuildsComplete();
}

static void WaitForCurrentTextureBuildingToFinish(bool& bInOutHadActivity)
{
	for (TObjectIterator<UTexture> Texture; Texture; ++Texture)
	{
		Texture->FinishCachePlatformData();
	}
};

static void PumpAsync(bool* bInOutHadActivity = nullptr)
{
	bool bHadActivity = false;
	WaitForCurrentShaderCompilationToFinish(bHadActivity);
	WaitForCurrentTextureBuildingToFinish(bHadActivity);
	FAssetCompilingManager::Get().ProcessAsyncTasks(true);
	if (bInOutHadActivity)
	{
		*bInOutHadActivity = *bInOutHadActivity || bHadActivity;
	}
}

int32 UDerivedDataCacheCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	bool bFillCache = Switches.Contains("FILL");   // do the equivalent of a "loadpackage -all" to fill the DDC
	bool bStartupOnly = Switches.Contains("STARTUPONLY");   // regardless of any other flags, do not iterate packages

	// Subsets for parallel processing
	uint32 SubsetMod = 0;
	uint32 SubsetTarget = MAX_uint32;
	FParse::Value(*Params, TEXT("SubsetMod="), SubsetMod);
	FParse::Value(*Params, TEXT("SubsetTarget="), SubsetTarget);
	bool bDoSubset = SubsetMod > 0 && SubsetTarget < SubsetMod;

	// Timing variables
	double FindProcessedPackagesTime = 0.0;
	double GCTime = 0.0;
	double FinishCacheTime = 0.;
	double BeginCacheTime = 0.;

	if (!bStartupOnly && bFillCache)
	{
		FCoreUObjectDelegates::PackageCreatedForLoad.AddUObject(this, &UDerivedDataCacheCommandlet::MaybeMarkPackageAsAlreadyLoaded);

		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());

		FString MapList;
		if(FParse::Value(*Params, TEXT("Map="), MapList))
		{
			for(int StartIdx = 0; StartIdx < MapList.Len();)
			{
				int EndIdx = StartIdx;
				while(EndIdx < MapList.Len() && MapList[EndIdx] != '+')
				{
					EndIdx++;
				}
				Tokens.Add(MapList.Mid(StartIdx, EndIdx - StartIdx) + FPackageName::GetMapPackageExtension());
				StartIdx = EndIdx + 1;
			}
		}
		else
		{
			Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());
		}

		uint8 PackageFilter = NORMALIZE_DefaultFlags;
		if ( Switches.Contains(TEXT("MAPSONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if ( Switches.Contains(TEXT("PROJECTONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeEnginePackages;
		}

		if ( !Switches.Contains(TEXT("DEV")) )
		{
			PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
		}

		if ( !Switches.Contains(TEXT("NOREDIST")) )
		{
			PackageFilter |= NORMALIZE_ExcludeNoRedistPackages;
		}

		// assume the first token is the map wildcard/pathname
		TArray<FString> FilesInPath;
		TArray<FString> Unused;
		TArray<FString> TokenFiles;
		for ( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			TokenFiles.Reset();
			if ( !NormalizePackageNames( Unused, TokenFiles, Tokens[TokenIndex], PackageFilter) )
			{
				UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
				continue;
			}

			FilesInPath += TokenFiles;
		}

		TArray<TPair<FString, FName>> PackagePaths;
		PackagePaths.Reserve(FilesInPath.Num());
		for (FString& Filename : FilesInPath)
		{
			FString PackageName;
			FString FailureReason;
			if (!FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName, &FailureReason))
			{
				UE_LOG(LogDerivedDataCacheCommandlet, Warning, TEXT("Unable to resolve filename %s to package name because: %s"), *Filename, *FailureReason);
				continue;
			}
			PackagePaths.Emplace(MoveTemp(Filename), FName(*PackageName));
		}

		// Respect settings that instruct us not to enumerate some paths
		TArray<FString> LocalDirsToNotSearch;
		const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
		for (const FDirectoryPath& DirToNotSearch : PackagingSettings->TestDirectoriesToNotSearch)
		{
			FString LocalPath;
			if (FPackageName::TryConvertGameRelativePackagePathToLocalPath(DirToNotSearch.Path, LocalPath))
			{
				LocalDirsToNotSearch.Add(LocalPath);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("'ProjectSettings -> Project -> Packaging -> Test directories to not search' has invalid element '%s'"), *DirToNotSearch.Path);
			}
		}

		TArray<FString> LocalFilenamesToSkip;
		if (FPackageName::FindPackagesInDirectories(LocalFilenamesToSkip, LocalDirsToNotSearch))
		{
			TSet<FName> PackageNamesToSkip;
			Algo::Transform(LocalFilenamesToSkip, PackageNamesToSkip, [](const FString& Filename)
				{
					FString PackageName;
					if (FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName))
					{
						return FName(*PackageName);
					}
					return FName(NAME_None);
				});

			int32 NewNum = Algo::StableRemoveIf(PackagePaths, [&PackageNamesToSkip](const TPair<FString,FName>& PackagePath) { return PackageNamesToSkip.Contains(PackagePath.Get<1>()); });
			PackagePaths.SetNum(NewNum);
		}

		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			TArray<FName> DesiredShaderFormats;
			Platforms[Index]->GetAllTargetedShaderFormats(DesiredShaderFormats);

			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
				// Kick off global shader compiles for each target platform. Note that shader platform alone is not sufficient to distinguish between WindowsEditor and WindowsClient, which after UE 4.25 have different DDC
				CompileGlobalShaderMap(ShaderPlatform, Platforms[Index], false);
			}
		}

		const int32 GCInterval = 100;
		int32 NumProcessedSinceLastGC = 0;
		bool bLastPackageWasMap = false;

		if (PackagePaths.Num() == 0)
		{
			UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("No packages found to load."));
		}
		else
		{
			UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("%d packages to load..."), PackagePaths.Num());
		}

		TArray<UObject*> CachingObjects;
		TArray<UObject*> ObjectBuffer;
		TArray<UPackage*> NewPackages;
		for (int32 PackageIndex = PackagePaths.Num() - 1; PackageIndex >= 0; PackageIndex-- )
		{
			{
				TPair<FString, FName>& PackagePath = PackagePaths[PackageIndex];
				const FString& Filename = PackagePath.Get<0>();
				FName PackageFName = PackagePath.Get<1>();
				if (ProcessedPackages.Contains(PackageFName))
				{
					continue;
				}
				if (bDoSubset)
				{
					FString PackageName = PackageFName.ToString();
					if (FCrc::StrCrc_DEPRECATED(*PackageName.ToUpper()) % SubsetMod != SubsetTarget)
					{
						continue;
					}
				}

				UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("Loading (%d) %s"), FilesInPath.Num() - PackageIndex, *Filename);

				UPackage* Package = LoadPackage(NULL, *Filename, LOAD_None);
				if (Package == NULL)
				{
					UE_LOG(LogDerivedDataCacheCommandlet, Error, TEXT("Error loading %s!"), *Filename);
				}
				else
				{
					bLastPackageWasMap = Package->ContainsMap();
					NumProcessedSinceLastGC++;
				}
			}

			// even if the load failed this could be the first time through the loop so it might have all the startup packages to resolve
			GRedirectCollector.ResolveAllSoftObjectPaths();

			// Find any new packages and cache all the objects in each package
			{
				const double BeginCacheTimeStart = FPlatformTime::Seconds();
				CachingObjects.Reset();
				NewPackages.Reset();
				for (TObjectIterator<UPackage> PackageIter; PackageIter; ++PackageIter)
				{
					UPackage* ExistingPackage = *PackageIter;
					if ((PackageFilter & NORMALIZE_ExcludeEnginePackages) == 0 || !ExistingPackage->GetName().StartsWith(TEXT("/Engine")))
					{
						FName ExistingPackageName = ExistingPackage->GetFName();
						if (!ProcessedPackages.Contains(ExistingPackageName))
						{
							ProcessedPackages.Add(ExistingPackageName);
							NewPackages.Add(ExistingPackage);
							check((ExistingPackage->GetPackageFlags() & PKG_ReloadingForCooker) == 0);

							ObjectBuffer.Reset();
							GetObjectsWithOuter(ExistingPackage, ObjectBuffer, true /* bIncludeNestedObjects */, RF_ClassDefaultObject /* ExclusionFlags */);
							for (UObject* Object : ObjectBuffer)
							{
								for (auto Platform : Platforms)
								{
									Object->BeginCacheForCookedPlatformData(Platform);
								}
								CachingObjects.Add(Object);
							}
						}
					}
				}
				PumpAsync();
				BeginCacheTime += FPlatformTime::Seconds() - BeginCacheTimeStart;
			}

			{
				const double FinishCacheTimeStart = FPlatformTime::Seconds();
				ObjectBuffer.Reset();
				ObjectBuffer.Append(CachingObjects);

				double LastActivityTime = FinishCacheTimeStart;
				const double MaxSecondsWithNoActivity = 120.0;
				const double WaitingForCacheSleepTime = 0.050;
				while (ObjectBuffer.Num() > 0)
				{
					bool bHadActivity = false;
					for (int32 ObjectIndex = 0; ObjectIndex < ObjectBuffer.Num();)
					{
						UObject* Object = ObjectBuffer[ObjectIndex];
						bool bIsFinished = true;
						for (auto Platform : Platforms)
						{
							bIsFinished = Object->IsCachedCookedPlatformDataLoaded(Platform) && bIsFinished;
						}
						if (bIsFinished)
						{
							ObjectBuffer.RemoveAtSwap(ObjectIndex);
							bHadActivity = true;
						}
						else
						{
							++ObjectIndex;
						}
					}

					double CurrentTime = FPlatformTime::Seconds();
					if (!bHadActivity)
					{
						PumpAsync(&bHadActivity);
					}
					if (!bHadActivity)
					{
						if (CurrentTime - LastActivityTime >= MaxSecondsWithNoActivity)
						{
							UE_LOG(LogDerivedDataCacheCommandlet, Error, TEXT("Timed out for %.2lfs waiting for %d objects to finish caching. First object: %s."),
								MaxSecondsWithNoActivity, ObjectBuffer.Num(), *ObjectBuffer[0]->GetFullName());
							ObjectBuffer.Reset();
						}
						else
						{
							FPlatformProcess::Sleep(WaitingForCacheSleepTime);
						}
					}
					else
					{
						LastActivityTime = CurrentTime;
					}
				}

				// Tear down all of the Cached data; we do this only after all objects have finished because we need to not
				// tear down any object until all objects in its package have finished
				for (UObject* Object : CachingObjects)
				{
					Object->WillNeverCacheCookedPlatformDataAgain();
					Object->ClearAllCachedCookedPlatformData();
				}
				// Mark the packages as processed
				for (UPackage* NewPackage : NewPackages)
				{
					NewPackage->SetPackageFlags(PKG_ReloadingForCooker);
				}

				PumpAsync();
				FinishCacheTime += FPlatformTime::Seconds() - FinishCacheTimeStart;
			}

			if (NumProcessedSinceLastGC >= GCInterval || PackageIndex < 0 || bLastPackageWasMap)
			{
				const double StartGCTime = FPlatformTime::Seconds();
				if (NumProcessedSinceLastGC >= GCInterval || PackageIndex < 0)
				{
					UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("GC (Full)..."));
					CollectGarbage(RF_NoFlags);
					NumProcessedSinceLastGC = 0;
				}
				else
				{
					UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("GC..."));
					CollectGarbage(RF_Standalone);
				}
				GCTime += FPlatformTime::Seconds() - StartGCTime;

				bLastPackageWasMap = false;
			}
		}
	}

	GetDerivedDataCacheRef().WaitForQuiescence(true);

	UE_LOG(LogDerivedDataCacheCommandlet, Display, TEXT("BeginCacheTime=%.2lfs, FinishCacheTime=%.2lfs, GCTime=%.2lfs."), BeginCacheTime, FinishCacheTime, GCTime);

	return 0;
}
