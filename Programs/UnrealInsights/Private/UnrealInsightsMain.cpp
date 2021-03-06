// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealInsightsMain.h"

#include "CoreGlobals.h"
#include "RequiredProgramMainCPPInclude.h"
#include "UserInterfaceCommand.h"


IMPLEMENT_APPLICATION(UnrealInsights, "UnrealInsights");


/**
 * Platform agnostic implementation of the main entry point.
 */
int32 UnrealInsightsMain(const TCHAR* CommandLine)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	//im: ???
	FCommandLine::Set(CommandLine);

	// Initialize core.
	GEngineLoop.PreInit(CommandLine);

	// Make sure all UObject classes are registered and default properties have been initialized.
	//im: ??? ProcessNewlyLoadedUObjects();

	// Tell the module manager it may now process newly-loaded UObjects when new C++ modules are loaded.
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	FUserInterfaceCommand::Run();

	// Shut down.
	//im: ??? FCoreDelegates::OnExit.Broadcast();
	FEngineLoop::AppPreExit(); //im: ???
	FModuleManager::Get().UnloadModulesAtShutdown();

#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown(); //im: ???

	return 0;
}
