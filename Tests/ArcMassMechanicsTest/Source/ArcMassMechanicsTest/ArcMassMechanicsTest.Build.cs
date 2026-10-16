// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassMechanicsTest : ModuleRules
{
	public ArcMassMechanicsTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"GameplayTags",
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"MassSignals",
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"ArcMassItemsRuntime",
			"ArcMassMechanics",
			"ArcMassDamageSystem",
			"DataValidation"
		});
	}
}
