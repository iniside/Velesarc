// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcCraftTest : ModuleRules
{
	public ArcCraftTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "ArcCore"
				, "ArcCraft"
				, "GameplayTags"
				, "GameplayAbilities"
				, "GameplayTasks"
				, "DeveloperSettings"
				, "CQTest"
				, "StructUtils"
				, "ModularGameplay"
				, "GameFeatures"
			}
			);
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"EngineSettings",
					"LevelEditor",
					"UnrealEd"
				});
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
