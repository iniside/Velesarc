// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcCraftEditor : ModuleRules
{
	public ArcCraftEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCraftEditor"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCraftEditor"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "UnrealEd"
				, "ContentBrowser"
				, "ContentBrowserData"
				, "ToolMenus"
				, "PropertyEditor"
				, "Projects"
				, "ArcCraft"
				, "ArcCore"
				, "ArcJsonAssetsEditor"
				, "GameplayTags"
				, "GameplayTagsEditor"
				, "EditorWidgets"
				, "StructViewer"
				, "InputCore"
				, "DeveloperSettings"
				// Required by debug-eval Apply path (FArcItemSpec, item fragment types).
				// ArcCraft exposes these as public deps but the DLL linker needs them listed
				// directly so the import lib is added to ArcCraftEditor's link command.
				, "ArcMassItemsRuntime"
				, "ArcAbilityKit"
				, "ArcMassAbilities"
				// RecipeGraph builder uses IAssetRegistry directly.
				, "AssetRegistry"
				// SArcRecipeGraphNode / FArcRecipeGraphNodeFactory (Step 4).
				, "GraphEditor"
				// Tab spawner group placement (WorkspaceMenu::GetMenuStructure()).
				, "WorkspaceMenuStructure"
			}
		);
	}
}
