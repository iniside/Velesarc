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
				"StructUtils",
				"MassCore",
				"MassEntity",
				"MassSignals",
				"MassSpawner",
				"MassAIBehavior",
				"GameplayAbilities",
				"StateTreeModule",
				"TargetingSystem",
				"MassRepresentation",
				"MassActors",
				"MassCommon",
				"ArcMass"
			}
		);
	}
}
