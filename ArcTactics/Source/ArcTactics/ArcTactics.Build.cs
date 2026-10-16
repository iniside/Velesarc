// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcTactics : ModuleRules
{
	public ArcTactics(ReadOnlyTargetRules Target) : base(Target)
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
				"MassCommon",
				"MassCore",
				"MassEntity",
				"MassSpawner",
				"MassSignals",
				"MassMovement",
				"MassNavigation",
				"MassNavMeshNavigation",
				"PropertyBindingUtils",
				"TraceLog",
				"ArcMass",
				"ArcAI",
				"ArcTargetQuery",
				"ArcMassAbilities",
				"ArcMassItemsRuntime",
				"ArcAbilityKit"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcKnowledge"
			}
		);
	}
}
