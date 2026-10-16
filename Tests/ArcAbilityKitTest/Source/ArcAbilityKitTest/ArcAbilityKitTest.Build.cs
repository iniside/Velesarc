// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcAbilityKitTest : ModuleRules
{
	public ArcAbilityKitTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"GameplayTags",
			"MassEntity",
			"MassCore",
			"MassSignals",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"ArcMassItemsRuntime",
			"StateTreeModule"
		});
	}
}
