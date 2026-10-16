// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcAbilityKit : ModuleRules
{
	public ArcAbilityKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory)));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AsyncMessageSystem",
				"GameplayTags",
				"MassEntity",
				"MassCore",
				"MassSpawner",
				"MassActors",
				"MassSignals",
				"StateTreeModule",
				"TargetingSystem",
				"Niagara",
				"Mover",
				"ArcMover",
				"ArcCore",
				"ArcMass",
				"ArcMassAbilities",
				"ArcMassItemsRuntime",
				"ArcTargeting",
				"ArcCues",
			}
		);

	}
}
