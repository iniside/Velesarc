// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcBuilderTest : ModuleRules
{
	public ArcBuilderTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcBuilder",
			"ArcCore",
			"ArcMassItemsRuntime",
			"ArcSharedTestHarness",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"ArcMass",
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"MassActors",
			"GameplayTags",
			"StructUtils",
			"DeveloperSettings",
		});
	}
}
