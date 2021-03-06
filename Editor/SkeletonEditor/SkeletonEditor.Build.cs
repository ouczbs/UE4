// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletonEditor : ModuleRules
{
	public SkeletonEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"ApplicationCore",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"EditorFramework",
                "UnrealEd",
                "Persona",
                "AnimGraph",
                "AnimGraphRuntime",
                "ContentBrowser",
                "AssetRegistry",
                "BlueprintGraph",
                "Kismet",
                "PinnedCommandList",
				"ToolMenus"
            }
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "Persona",
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
			}
		);
	}
}
