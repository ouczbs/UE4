// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    // NaniteBuilder module is an editor module
    public class NaniteBuilder : ModuleRules
	{
		public NaniteBuilder(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"MeshDescription",
					"MeshDescriptionOperations",
					"GraphColor",
					"QuadricMeshReduction",
				}
			);

			PrivateIncludePathModuleNames.Add("MeshUtilitiesCommon");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "metis");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");
		}
	}
}
