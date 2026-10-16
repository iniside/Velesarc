// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassAbilities : ModuleRules
{
	public ArcMassAbilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassAbilities"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassAbilities"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"MassCore",
				"MassEntity",
				"MassSignals",
				"MassAIBehavior",
				"StateTreeModule",
				"ArcMass",
				"ArcCore",
				"TraceLog"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcPersistence",
				"ArcTargeting",
				"MassSpawner",
				"TargetingSystem",
				"MassRepresentation",
				"MassActors",
				"MassCommon",
				"EnhancedInput",
				"CommonUI",
				"CommonGame"
			}
		);
	}
}
