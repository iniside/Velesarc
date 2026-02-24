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
				, "AssetTools"
				, "AssetDefinition"
				, "EngineAssetDefinitions"
				, "ContentBrowser"
				, "ContentBrowserData"
				, "ToolMenus"
				, "PropertyEditor"
				, "ArcCraft"
				, "ArcCore"
				, "ArcJson"
				, "GameplayTags"
			}
		);
	}
}
