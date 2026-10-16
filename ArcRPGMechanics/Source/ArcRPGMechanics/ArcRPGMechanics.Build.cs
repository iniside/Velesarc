// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcRPGMechanics : ModuleRules
{
	public ArcRPGMechanics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(ModuleDirectory));

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
				"ArcMassMechanics",
				"ArcMassAbilities",
				"ArcMassDamageSystem",
				"ArcAbilityKit",
				"ArcMassItemsRuntime",
				"ArcNeeds",
				"ArcCore",
				"DaySequence"
			}
		);
	}
}
