// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassAbilitiesEditor : ModuleRules
{
	public ArcMassAbilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassAbilitiesEditor"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ArcMassAbilities"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"ArcCore",
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"PropertyEditor",
				"ToolMenus",
				"MassEntity",
				"AssetDefinition",
				"StateTreeModule",
				"StateTreeEditorModule",
				"PropertyBindingUtils",
				"EnhancedInput",
				"GameplayTags",
				"MassSpawner",
				"TraceLog",
				"TraceAnalysis",
				"TraceServices",
				"RewindDebuggerInterface",
				"RewindDebuggerRuntimeInterface",
				"GameplayInsights",
				"Projects"
			}
		);
	}
}
