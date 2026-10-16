// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassItemsEditor : ModuleRules
{
	public ArcMassItemsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassItemsEditor"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreUObject"
				, "Engine"
				, "ArcMassItemsRuntime"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects"
				, "UnrealEd"
				, "Slate"
				, "SlateCore"
				, "EditorStyle"
				, "PropertyEditor"
				, "AssetTools"
				, "AssetDefinition"
				, "ToolMenus"
				, "LevelEditor"
				, "ArcCore"
				, "ArcCoreEditor"
				, "ArcJsonAssetsEditor"
				, "GameplayTags"
				, "EditorScriptingUtilities"
				, "ContentBrowser"
				, "ContentBrowserData"
				, "Json"
				, "JsonUtilities"
				, "DataRegistry"
				, "StructUtilsEditor"
				, "DesktopWidgets"
				, "SharedSettingsWidgets"
				, "RenderCore"
				, "Kismet"
				, "InputCore"
				, "CustomizableObject"
			}
		);
	}
}
