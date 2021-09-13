// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Core : ModuleRules
{
	public Core(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivatePCHHeaderFile = "Private/CorePrivatePCH.h";

		SharedPCHHeaderFile = "Public/CoreSharedPCH.h";

		PrivateDependencyModuleNames.Add("BuildSettings");
		if (Target.Platform == UnrealTargetPlatform.Android && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.Add("HWCPipe");
		}

		PrivateDependencyModuleNames.Add("BLAKE3");

		PublicDependencyModuleNames.Add("TraceLog");
		PublicIncludePaths.Add("Runtime/TraceLog/Public");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DerivedDataCache/Public",
				"Runtime/SynthBenchmark/Public",
				"Runtime/Core/Private",
				"Runtime/Core/Private/Misc",
				"Runtime/Core/Private/Internationalization",
				"Runtime/Core/Private/Internationalization/Cultures",
				"Runtime/Engine/Public",
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"DerivedDataCache",
				"InputDevice",
				"Analytics",
				"RHI"
			}
			);

		if (Target.bBuildEditor == true)
		{
			DynamicallyLoadedModuleNames.Add("SourceCodeAccess");

			PrivateIncludePathModuleNames.Add("DirectoryWatcher");
			DynamicallyLoadedModuleNames.Add("DirectoryWatcher");
		}

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib");

			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"IntelTBB",
				"IntelVTune"
				);

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"mimalloc");

			if (Target.WindowsPlatform.bUseBundledDbgHelp)
			{
				PublicDelayLoadDLLs.Add("DBGHELP.DLL");
				PrivateDefinitions.Add("USE_BUNDLED_DBGHELP=1");
				RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/DbgHelp/dbghelp.dll");
			}
			else
			{
				PrivateDefinitions.Add("USE_BUNDLED_DBGHELP=0");
			}
			PrivateDefinitions.Add("YIELD_BETWEEN_TASKS=1");

			if (Target.WindowsPlatform.bPixProfilingEnabled && Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrivateDependencyModuleNames.Add("WinPixEventRuntime");
			}
		}
		else if ((Target.Platform == UnrealTargetPlatform.HoloLens))
		{
			PublicIncludePaths.Add("Runtime/Core/Public/HoloLens");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib");

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"XInput"
				);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"zlib",
				"PLCrashReporter",
				"rd_route"
				);
			PublicFrameworks.AddRange(new string[] { "Cocoa", "Carbon", "IOKit", "Security" });
			
			if (Target.bBuildEditor == true)
			{
				string SDKROOT = Utils.RunLocalProcessAndReturnStdOut("/usr/bin/xcrun", "--sdk macosx --show-sdk-path");
				PublicAdditionalLibraries.Add(SDKROOT + "/System/Library/PrivateFrameworks/MultitouchSupport.framework/Versions/Current/MultitouchSupport.tbd");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib"
				);
			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation", "AudioToolbox", "AVFoundation", "GameKit", "StoreKit", "CoreVideo", "CoreMedia", "CoreGraphics", "GameController", "SystemConfiguration", "DeviceCheck", "UserNotifications" });
			if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PublicFrameworks.AddRange(new string[] { "CoreMotion", "AdSupport", "WebKit" });
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"PLCrashReporter"
					);
			}

			PrivateIncludePathModuleNames.Add("ApplicationCore");

			bool bSupportAdvertising = Target.Platform == UnrealTargetPlatform.IOS;
			if (bSupportAdvertising)
			{
				PublicFrameworks.AddRange(new string[] { "iAD" });
			}

			// export Core symbols for embedded Dlls
			ModuleSymbolVisibility = ModuleRules.SymbolVisibility.VisibileForDll;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"cxademangle",
				"zlib",
				"libunwind"
				);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib",
				"jemalloc"
				);

			// Core uses dlopen()
			PublicSystemLibraries.Add("dl");
		}

		if (Target.bCompileICU == true)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "ICU");
		}
		PublicDefinitions.Add("UE_ENABLE_ICU=" + (Target.bCompileICU ? "1" : "0")); // Enable/disable (=1/=0) ICU usage in the codebase. NOTE: This flag is for use while integrating ICU and will be removed afterward.

		// If we're compiling with the engine, then add Core's engine dependencies
		if (Target.bCompileAgainstEngine == true)
		{
			if (!Target.bBuildRequiresCookedData)
			{
				DynamicallyLoadedModuleNames.AddRange(new string[] { "DerivedDataCache" });
			}
		}

		// On Windows platform, VSPerfExternalProfiler.cpp needs access to "VSPerf.h".  This header is included with Visual Studio, but it's not in a standard include path.
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			bool VSPerfDefined = false;
			string VisualStudioInstallation = Target.WindowsPlatform.IDEDir;
			if (VisualStudioInstallation != null && VisualStudioInstallation != string.Empty && Directory.Exists(VisualStudioInstallation))
			{
				string SubFolderName = "x64/PerfSDK/";
				string PerfIncludeDirectory = Path.Combine(VisualStudioInstallation, String.Format("Team Tools/Performance Tools/{0}", SubFolderName));

				if (File.Exists(Path.Combine(PerfIncludeDirectory, "VSPerf.h"))
					&& Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PrivateIncludePaths.Add(PerfIncludeDirectory);
					PublicDefinitions.Add("WITH_VS_PERF_PROFILER=1");
					VSPerfDefined = true;
				}
			}

			if (!VSPerfDefined)
			{
				PublicDefinitions.Add("WITH_VS_PERF_PROFILER=0");
			}
		}

		if (Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			PublicDefinitions.Add("WITH_VS_PERF_PROFILER=0");
		}

		// Superluminal instrumentation support, if one has it installed
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string SuperluminalInstallDir = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Superluminal\Performance", "InstallDir", null) as string;
			if (String.IsNullOrEmpty(SuperluminalInstallDir))
			{
				SuperluminalInstallDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Superluminal/Performance");
			}

			string SuperluminalApiDir = Path.Combine(SuperluminalInstallDir, "API/");
			string SubFolderName = "lib/x64/";
			string SuperluminalLibDir = Path.Combine(SuperluminalApiDir, SubFolderName);

			if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
				File.Exists(Path.Combine(SuperluminalApiDir, "include/Superluminal/PerformanceAPI_capi.h")))
			{
				PublicSystemIncludePaths.Add(Path.Combine(SuperluminalApiDir, "include/"));
				PublicAdditionalLibraries.Add(Path.Combine(SuperluminalLibDir, "PerformanceAPI_MD.lib"));
				PublicDefinitions.Add("WITH_SUPERLUMINAL_PROFILER=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_SUPERLUMINAL_PROFILER=0");
			}
		}

		if (Target.bWithDirectXMath && Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("WITH_DIRECTXMATH=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_DIRECTXMATH=0");
		}

		if ((Target.Platform == UnrealTargetPlatform.Mac) || (Target.Platform == UnrealTargetPlatform.IOS) || (Target.Platform == UnrealTargetPlatform.TVOS) 
			|| (Target.Platform == UnrealTargetPlatform.HoloLens) || (Target.Platform == UnrealTargetPlatform.Android))
		{
			PublicDefinitions.Add("IS_RUNNING_GAMETHREAD_ON_EXTERNAL_THREAD=1");
		}

		// Set a macro to allow FApp::GetBuildTargetType() to detect client targts
		if (Target.Type == TargetRules.TargetType.Client)
		{
			PrivateDefinitions.Add("IS_CLIENT_TARGET=1");
		}
		else
		{
			PrivateDefinitions.Add("IS_CLIENT_TARGET=0");
		}

		// Decide if validating memory allocator (aka MallocStomp) can be used on the current platform.
		// Run-time validation must be enabled through '-stompmalloc' command line argument.

		bool bWithMallocStomp = false;
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			if (Target.Platform == UnrealTargetPlatform.Mac
				|| Target.Platform == UnrealTargetPlatform.Linux
				|| Target.Platform == UnrealTargetPlatform.LinuxAArch64
				|| Target.Platform == UnrealTargetPlatform.Win64
				|| Target.Platform.IsInGroup(UnrealPlatformGroup.XboxCommon)	// Base Xbox will run out of virtual memory very quickly but it can be utilized on some hardware configs
				)
			{
				bWithMallocStomp = true;
			}
		}

		// temporary thing.
		PrivateDefinitions.Add("PLATFORM_SUPPORTS_BINARYCONFIG=" + (SupportsBinaryConfig(Target) ? "1" : "0"));

		// temporary thing to enable backing out in case of disaster, remove after initial testing period.
		PublicDefinitions.Add("GPUCULL_TODO=1");

		PublicDefinitions.Add("WITH_MALLOC_STOMP=" + (bWithMallocStomp ? "1" : "0"));

		PrivateDefinitions.Add("PLATFORM_COMPILER_OPTIMIZATION_LTCG=" + (Target.bAllowLTCG ? "1" : "0"));
		PrivateDefinitions.Add("PLATFORM_COMPILER_OPTIMIZATION_PG=" + (Target.bPGOOptimize ? "1" : "0"));
		PrivateDefinitions.Add("PLATFORM_COMPILER_OPTIMIZATION_PG_PROFILING=" + (Target.bPGOProfile ? "1" : "0"));

		UnsafeTypeCastWarningLevel = WarningLevel.Warning;
	}

	protected virtual bool SupportsBinaryConfig(ReadOnlyTargetRules Target)
	{
		return Target.Platform != UnrealTargetPlatform.Android;
	}
}
