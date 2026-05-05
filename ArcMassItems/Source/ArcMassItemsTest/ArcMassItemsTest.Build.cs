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
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassItemsRuntime")),
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
			"ArcCoreTest",
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"MassSignals",
			"MassActors",
			"GameplayTags",
			"StructUtils",
			"NetCore",
			"IrisCore",
			"DeveloperSettings",
			"ArcMassAbilities"
		});
	}
}
