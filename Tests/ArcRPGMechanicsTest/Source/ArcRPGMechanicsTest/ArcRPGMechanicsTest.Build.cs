// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcRPGMechanicsTest : ModuleRules
{
	public ArcRPGMechanicsTest(ReadOnlyTargetRules Target) : base(Target)
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
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"ArcMassAbilities",
			"ArcMassDamageSystem",
			"ArcMassItemsRuntime",
			"ArcAbilityKit",
			"ArcMassMechanics",
			"ArcRPGMechanics",
			"ArcMassMechanicsTest",
			"DataValidation"
		});
	}
}
