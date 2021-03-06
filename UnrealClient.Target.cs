// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealClientTarget : TargetRules
{
    public UnrealClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		BuildEnvironment = TargetBuildEnvironment.Shared;
		ExtraModuleNames.Add("UnrealGame");
	}
}
