// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcAI : ModuleRules
{
	public ArcAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcAI"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcAI"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcAI"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "MassCommon"
				, "MassEntity"
				, "MassSpawner"
				, "MassActors"
				, "StateTreeModule"
				, "PropertyPath"
				, "GameplayAbilities"
				, "PropertyBindingUtils"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "InputCore"
				, "NavigationSystem"
				, "SmartObjectsModule"
				, "AIModule"
				, "GameplayTasks"
				, "GameplayTags"
				, "MassCommon"
				, "MassEntity"
				, "MassSpawner"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		SetupGameplayDebuggerSupport(Target);
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
