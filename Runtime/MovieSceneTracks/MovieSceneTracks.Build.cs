// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneTracks : ModuleRules
{
	public MovieSceneTracks(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[]
        { 
            "Runtime/MovieSceneTracks/Private",
			"Runtime/MovieSceneTracks/Private/Sections",
			"Runtime/MovieSceneTracks/Private/Tracks",
        });

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"TimeManagement",
                "AnimationCore",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
                "AnimGraphRuntime"
			}
		);

		if (Target.bBuildWithEditorOnlyData && Target.bBuildEditor)
		{
            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "BlueprintGraph"
                }
            );
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"EditorFramework",
                    "UnrealEd"
                }
            );
        }
	}
}
