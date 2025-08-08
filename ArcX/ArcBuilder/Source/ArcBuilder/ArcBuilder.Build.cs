// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcBuilder : ModuleRules
{
	public ArcBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcBuilder"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcBuilder"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcBuilder"))
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"GameplayTags",
				"TargetingSystem",
				"MassActors",
				"MassSpawner",
				"MassRepresentation",
				"ArcCore",
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
