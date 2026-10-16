// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcCuesTest : ModuleRules
{
	public ArcCuesTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(ModuleDirectory))
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcCues",
			"ArcMass",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"MassSignals",
			"GameplayTags",
		});
	}
}
