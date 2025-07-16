// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcGun : ModuleRules
{
	public ArcGun(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGun"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGun"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGun"))
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreOnline"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "NetCore"
				, "GameplayAbilities"
				, "GameplayTasks"
				, "GameplayTags"
				, "GameplayDebugger"
				, "DeveloperSettings"
				, "GameFeatures"
				, "ModularGameplay"
				, "EnhancedInput"
				, "AIModule"
				, "AudioMixer"
				, "InputCore"
				, "AssetRegistry"
				, "DeveloperSettings"
				, "DataRegistry"
				, "Niagara"
				, "ArcCore"
				, "TargetingSystem"
				, "GameplayCameras"

				// ... add other public dependencies that you statically link with here ...
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
