// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealProject : ModuleRules
{
    public UnrealProject(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "PlayFabGSDK", "MediaAssets" });

        PrivateDependencyModuleNames.AddRange(new string[] { "HTTP", "Json", "JsonUtilities", "MoviePlayer" });
    }
}