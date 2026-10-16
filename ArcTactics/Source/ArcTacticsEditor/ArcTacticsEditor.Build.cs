// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcTacticsEditor : ModuleRules
{
	public ArcTacticsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(ModuleDirectory));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ApplicationCore",
				"ArcAI",
				"ArcCore",
				"ArcCoreEditor",
				"ArcTactics",
				"ArcTargetQuery",
				"ArcTargetQueryEditor",
				"AssetTools",
				"BlueprintGraph",
				"EditorFramework",
				"EditorStyle",
				"GameplayTags",
				"InputCore",
				"Json",
				"JsonUtilities",
				"PropertyAccessEditor",
				"PropertyBindingUtils",
				"PropertyBindingUtilsEditor",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"StructUtilsEditor",
				"StructViewer",
				"ToolsetRegistry",
				"UnrealEd"
			}
		);
	}
}
