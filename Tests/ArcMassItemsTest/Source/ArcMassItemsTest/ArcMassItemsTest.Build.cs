// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassItemsTest : ModuleRules
{
	public ArcMassItemsTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		SetupIrisSupport(Target);

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "../ArcCoreTest/Source/ArcCoreTest"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(GetModuleDirectory("IrisCore"), "Private"),
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcMassItemsRuntime",
			"ArcMassReplicationRuntime",
			"ArcCore",
			"ArcPersistence",
			"ArcCoreTest",
			"ArcSharedTestHarness",
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"MassSignals",
			"MassActors",
			"GameplayTags",
			"NetCore",
			"IrisCore",
			"DeveloperSettings",
			"ArcMassAbilities",
			"ArcAbilityKit",
			"ArcMassGameplayFramework",
			"WorldConditions"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"LevelEditor",
			});
		}
	}
}
