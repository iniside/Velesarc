// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcEnviroEffectsTest : ModuleRules
{
	public ArcEnviroEffectsTest(ReadOnlyTargetRules Target) : base(Target)
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
			"MassSpawner",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"ArcWeather",
			"ArcEnviroEffects",
			"ArcMassMechanics"
		});
	}
}
