// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcTargetQueryEditor : ModuleRules
{
	public ArcTargetQueryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(ModuleDirectory));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AdvancedPreviewScene",
				"ArcCore",
				"ArcMass",
				"ArcTargetQuery",
				"AssetDefinition",
				"AssetRegistry",
				"AssetTools",
				"BlueprintGraph",
				"EditorFramework",
				"EditorInteractiveToolsFramework",
				"EditorViewport",
				"InputCore",
				"InteractiveToolsFramework",
				"Json",
				"JsonUtilities",
				"MassCore",
				"MassEntity",
				"MessageLog",
				"PropertyAccessEditor",
				"PropertyBindingUtils",
				"PropertyBindingUtilsEditor",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"StructUtilsEditor",
				"StructViewer",
				"ToolMenus",
				"ToolsetRegistry",
				"UnrealEd"
			}
		);
	}
}
