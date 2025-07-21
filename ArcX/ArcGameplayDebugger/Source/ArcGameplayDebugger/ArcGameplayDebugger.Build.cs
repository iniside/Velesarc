// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArcGameplayDebugger : ModuleRules
{
	public ArcGameplayDebugger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		SetupIrisSupport(Target);
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
				"SlateIM",
				"ArcCore",
				"ArcGun",
				"GameplayTags",
				"GameplayAbilities",
				"ImGui",
				"InputCore",
				"EnhancedInput"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"InputCore",
				"Iris",
				//"IrisCore",
				"Engine",
				"Slate",
				"SlateCore",
				"SlateIM",
				"ArcCore",
				"ArcGun"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelEditor",
					"UnrealEd"
					// ... add other public dependencies that you statically link with here ...
				}
			);
			
		
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelEditor",
					"UnrealEd"
					// ... add private dependencies that you statically link with here ...	
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
