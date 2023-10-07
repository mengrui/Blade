// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Blade : ModuleRules
{
	public Blade(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", "CoreUObject", "Engine", "PhysicsCore", "Chaos", "InputCore",   "EnhancedInput",
            "HeadMountedDisplay", "AIModule", "AnimGraphRuntime", "AnimationLocomotionLibraryRuntime", 
            "MotionWarping", "PhysicsControl", "Niagara" });

        if (Target.Type == TargetType.Editor)
        {
            PublicDependencyModuleNames.Add("AnimationBlueprintLibrary");
        }
    }
}
