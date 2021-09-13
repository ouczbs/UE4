// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/WorldPartitionBuilderCommandlet.h"
#include "WorldPartition/WorldPartitionBuilder.h"

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFileManager.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Trace/Trace.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionBuilderCommandlet, All, All);

UWorldPartitionBuilderCommandlet::UWorldPartitionBuilderCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UWorldPartitionBuilder* UWorldPartitionBuilderCommandlet::CreateBuilder(const FString& WorldConfigFilename)
{
	// Parse builder class name
	FString BuilderClassName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Builder="), BuilderClassName, false))
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Invalid builder name."));
		return nullptr;
	}

	UClass* BuilderClass = FindObject<UClass>(ANY_PACKAGE, *BuilderClassName);

	if (!BuilderClass)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Unknown builder %s."), *BuilderClassName);
		return nullptr;
	}

	// Create builder instance
	UWorldPartitionBuilder* Builder = NewObject<UWorldPartitionBuilder>(this, BuilderClass);
	check(Builder);

	Builder->AddToRoot();

	// Validate builder settings
	if (Builder->RequiresCommandletRendering() && !IsAllowCommandletRendering())
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("The option \"-AllowCommandletRendering\" must be provided for the %s process to work"), *BuilderClassName);
		return nullptr;
	}

	// Load configuration file
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename))
	{
		LoadConfig(GetClass(), *WorldConfigFilename);
	}

	// Load builder configuration
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename))
	{
		Builder->LoadConfig(BuilderClass, *WorldConfigFilename);
	}

	return Builder;
}

int32 UWorldPartitionBuilderCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionBuilderCommandlet::Main);

	UE_SCOPED_TIMER(TEXT("Execution"), LogWorldPartitionBuilderCommandlet);

	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Tokens.Num() != 1)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Missing world name"));
		return 1;
	}

	if (Switches.Contains(TEXT("Verbose")))
	{
		LogWorldPartitionBuilderCommandlet.SetVerbosity(ELogVerbosity::Verbose);
	}

	// This will convert incomplete package name to a fully qualified path
	FString WorldFilename;
	if (!FPackageName::SearchForPackageOnDisk(Tokens[0], &Tokens[0], &WorldFilename))
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Unknown world '%s'"), *Tokens[0]);
		return 1;
	}

	FString WorldConfigFilename = FPaths::ChangeExtension(WorldFilename, TEXT("ini"));

	// Create builder instance
	UWorldPartitionBuilder* Builder = CreateBuilder(WorldConfigFilename);
	if (!Builder)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Failed to create builder, exiting."));
		return 1;
	}

	// Perform builder pre world initialisation
	Builder->PreWorldInitialization(*this);

	// Load the map package
	UPackage* MapPackage = LoadPackage(NULL, *Tokens[0], LOAD_None);
	if (!MapPackage)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Couldn't load package %s."), *Tokens[0]);
		return 1;
	}

	// Find the world in the given package
	UWorld* World = UWorld::FindWorldInPackage(MapPackage);
	if (!World)
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("No world in specified package %s."));
		return 1;
	}

	// Setup the world.
	World->WorldType = EWorldType::Editor;
	World->AddToRoot();
	if (!World->bIsWorldInitialized)
	{
		UWorld::InitializationValues IVS;
		IVS.RequiresHitProxies(false);
		IVS.ShouldSimulatePhysics(false);
		IVS.EnableTraceCollision(false);
		IVS.CreateNavigation(false);
		IVS.CreateAISystem(false);
		IVS.AllowAudioPlayback(false);
		IVS.CreatePhysicsScene(true);

		World->InitWorld(UWorld::InitializationValues(IVS));
		World->PersistentLevel->UpdateModelComponents();
		World->UpdateWorldComponents(true /*bRerunConstructionScripts*/, false /*bCurrentLevelOnly*/);
	}

	// Make sure the world is partitioned.
	if (!World->HasSubsystem<UWorldPartitionSubsystem>())
	{
		UE_LOG(LogWorldPartitionBuilderCommandlet, Error, TEXT("Commandlet only works on partitioned maps."));
		return 1;
	}

	// Commandlets aren't loading level instances by default, change that behavior
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>())
	{
		LevelInstanceSubsystem->SetLoadInstancesOnRegistration(true);
	}

	// Retrieve the world partition.
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	check(WorldPartition);

	FWorldContext& WorldContext = GEditor->GetEditorWorldContext(true /*bEnsureIsGWorld*/);
	WorldContext.SetCurrentWorld(World);
	GWorld = World;

	if (Builder->RequiresEntireWorldLoading())
	{
		const FBox LoadBox(FVector(-WORLD_MAX, -WORLD_MAX, -WORLD_MAX), FVector(WORLD_MAX, WORLD_MAX, WORLD_MAX));
		WorldPartition->LoadEditorCells(LoadBox);
	}

	// Run builder
	if (!Builder->Run(World, *this))
	{
		return 1;
	}

	// Save default configuration
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*WorldConfigFilename) ||
		!FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*WorldConfigFilename))
	{
		SaveConfig(CPF_Config, *WorldConfigFilename);

		Builder->SaveConfig(CPF_Config, *WorldConfigFilename);
	}

	// Cleanup
	World->RemoveFromRoot();
	WorldContext.SetCurrentWorld(nullptr);
	GWorld = nullptr;

	return 0;
}
