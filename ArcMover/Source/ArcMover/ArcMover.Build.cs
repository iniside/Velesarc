// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMover : ModuleRules
{
	public ArcMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMover")));
		PublicSystemIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMover")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMover")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"NetCore",
				"ChaosCore",
				"Chaos",
				"ChaosSolverEngine",
				"PhysicsCore",
				"Mover",
				"ChaosMover"
			}
		);
	}
}
