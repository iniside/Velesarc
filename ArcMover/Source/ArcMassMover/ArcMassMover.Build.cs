// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassMover : ModuleRules
{
	public ArcMassMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassMover")));
		PublicSystemIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassMover")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassMover")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ModularGameplay",
				"GameplayTags",
				"AIModule",
				"EnhancedInput",
				"MassCore",
				"MassEntity",
				"MassSpawner",
				"MassActors",
				"MassCommon",
				"MassNavigation",
				"MassMovement",
				"Mover",
				"MoverMassIntegration",
				"ArcCore",
				"ArcMass",
				"ArcMassGameplayFramework",
				"ArcMover"
			}
		);
	}
}
