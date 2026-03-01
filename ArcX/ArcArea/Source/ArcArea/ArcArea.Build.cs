// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcArea : ModuleRules
{
	public ArcArea(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcArea"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcArea"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				"StructUtils",
				"MassEntity",
				"MassCommon",
				"SmartObjectsModule",
				"ArcCore",
				"ArcKnowledge"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"MassSignals",
				"MassSpawner",
				"MassSmartObjects",
				"MassAIBehavior",
				"StateTreeModule",
				"GameplayStateTreeModule",
				"ArcMass",
				"ArcAI"
			}
		);

		SetupGameplayDebuggerSupport(Target);
	}
}
