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
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassItemsRuntime"))
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
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"MassSignals",
			"GameplayTags",
			"StructUtils",
			"NetCore",
			"IrisCore"
		});
	}
}
