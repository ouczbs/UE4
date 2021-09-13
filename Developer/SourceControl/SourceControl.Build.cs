// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceControl : ModuleRules
{
	public SourceControl(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Developer/SourceControl/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"SlateCore",
				"InputCore",
			}
		);

		PublicDefinitions.Add("SOURCE_CONTROL_WITH_SLATE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"Slate",
				"EditorStyle"
			}
		);

        if (Target.bBuildEditor)
        {
			PrivateDependencyModuleNames.AddRange(
                new string[] {
					"EditorFramework",
					"Engine",
					"UnrealEd",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AssetTools"
				}
			);

			CircularlyReferencedDependentModules.Add("UnrealEd");
        }

		if (Target.bBuildDeveloperTools)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MessageLog",
				}
			);
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
	}
}
