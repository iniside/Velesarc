// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassAbilitiesTest : ModuleRules
{
	public ArcMassAbilitiesTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"ArcMassItemsRuntime",
			"ArcSharedTestHarness",
			"MassEntity",
			"MassCore",
			"GameplayTags",
			"MassSignals",
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils"
		});
	}
}
