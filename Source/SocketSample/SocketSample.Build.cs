// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SocketSample : ModuleRules
{
	public SocketSample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "Networking", "Sockets" });
	}
}
