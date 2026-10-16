// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassMechanics : ModuleRules
{
	public ArcMassMechanics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"MassEntity",
				"MassCore",
				"MassSpawner",
				"StateTreeModule",
				"ArcCore",
				"ArcMass",
				"ArcMassAbilities",
				"ArcMassDamageSystem"
			}
		);
	}
}
