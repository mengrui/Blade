// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Blade : ModuleRules
{
	public Blade(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "AIModule", "MotionWarping", "PhysicsControl", "EnhancedInput" });

        if (Target.Type == TargetType.Editor)
        {
            PublicDependencyModuleNames.Add("AnimationBlueprintLibrary");
        }
    }
}
