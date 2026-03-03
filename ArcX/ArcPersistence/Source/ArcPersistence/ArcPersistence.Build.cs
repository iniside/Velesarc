// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcPersistence : ModuleRules
{
	public ArcPersistence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcPersistence"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcPersistence"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcPersistence"))
			}
		);
		
		PublicDefinitions.Add("BOOST_ALL_NO_LIB");
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ArcJson",
				"StructUtils",
				//"Boost"
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
				"Json",
				"JsonUtilities",
				"AssetRegistry",
				"DeveloperSettings",
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"GameplayTags",
				
				// ... add other public dependencies that you statically link with here ...
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
