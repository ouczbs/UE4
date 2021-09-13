// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TargetPlatform : ModuleRules
{
	public TargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("SlateCore");
		PrivateDependencyModuleNames.Add("Slate");
		PrivateDependencyModuleNames.Add("EditorStyle");
		PrivateDependencyModuleNames.Add("DeveloperToolSettings");
		PrivateDependencyModuleNames.Add("Projects");
		PublicDependencyModuleNames.Add("DeveloperSettings");
		PublicDependencyModuleNames.Add("AudioPlatformConfiguration");
		PublicDependencyModuleNames.Add("DesktopPlatform");
		PublicDependencyModuleNames.Add("LauncherPlatform");

		PrivateIncludePathModuleNames.Add("Engine");
		PrivateIncludePathModuleNames.Add("PhysicsCore");

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		DynamicallyLoadedModuleNames.Add("TurnkeySupport");
		PrivateIncludePathModuleNames.Add("TurnkeySupport");

		// no need for all these modules if the program doesn't want developer tools at all (like UnrealFileServer)
		if (!Target.bBuildRequiresCookedData && Target.bBuildDeveloperTools)
		{
			// these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
			DynamicallyLoadedModuleNames.Add("ShaderPreprocessor");
			DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
            DynamicallyLoadedModuleNames.Add("ShaderFormatVectorVM");
            DynamicallyLoadedModuleNames.Add("ImageWrapper");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				DynamicallyLoadedModuleNames.Add("TextureFormatIntelISPCTexComp");

				// these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
				DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
				DynamicallyLoadedModuleNames.Add("MetalShaderFormat");

				DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
				DynamicallyLoadedModuleNames.Add("TextureFormatASTC");

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM"); // For IOS cooking
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacTargetPlatform");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
				DynamicallyLoadedModuleNames.Add("TextureFormatASTC");

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
				}
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
				DynamicallyLoadedModuleNames.Add("TextureFormatASTC");

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
				}
			}
		}

		if (Target.bBuildDeveloperTools == true && Target.bBuildRequiresCookedData && Target.bCompileAgainstEngine && Target.bCompilePhysX)
		{
			DynamicallyLoadedModuleNames.Add("PhysXCooking");
		}
	}
}
