// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassGameFrameworkTest : ModuleRules
{
	public ArcMassGameFrameworkTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassGameFrameworkTest"))
			}
		);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"ArcCore",
			"ArcMass",
			"ArcMassAbilities",
			"ArcMassGameplayFramework",
			"ArcMassItemsRuntime",
			"ArcAI",
			"ArcSharedTestHarness",
			"CQTest",
			"MassEntity",
			"MassSpawner",
			"MassActors",
			"MassCommon",
			"MassCore",
			"EnhancedInput",
			"FunctionalTesting",
			"GameplayTags",
			"ModularGameplay",
			"ModularGameplayActors",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"DeveloperSettings"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
