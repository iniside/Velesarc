// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassItemsRuntime : ModuleRules
{
	public ArcMassItemsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		SetupIrisSupport(Target);

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassItemsRuntime"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"StructUtils",
				"MassCore",
				"MassEntity",
				"MassSignals",
				"MassSpawner",
				"ArcCore",
				"ArcMassReplicationRuntime"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NetCore",
				"IrisCore"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(GetModuleDirectory("IrisCore"), "Private"),
			}
		);
	}
}
