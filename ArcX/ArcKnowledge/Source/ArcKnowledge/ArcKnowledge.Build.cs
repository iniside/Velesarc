// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcKnowledge : ModuleRules
{
	public ArcKnowledge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcKnowledge"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcKnowledge"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"GameplayTags",
				"StructUtils",
				"MassCommon",
				"MassEntity",
				"MassSignals",
				"MassSpawner",
				"MassAIBehavior",
				"StateTreeModule",
				"GameplayStateTreeModule",
				"ArcCore",
				"ArcMass",
				"ArcAI",
				"StateTreeModule",
				"MassAIBehavior",
				"MassActors"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AsyncMessageSystem"
			}
		);

		SetupGameplayDebuggerSupport(Target);
	}
}
