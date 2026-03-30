// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcEditorTools : ModuleRules
{
	public ArcEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcEditorTools"))
			}
		);
		
		PublicSystemIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcEditorTools"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcEditorTools"))
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "UnrealEd"
				, "Slate"
				, "SlateCore"
				, "InputCore"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "InputCore"
				, "UnrealEd"
				, "PropertyEditor"
				, "SceneOutliner"
				, "AssetTools"
				, "AssetRegistry"
				, "ContentBrowser"
				, "GameplayAbilitiesEditor"
				, "MainFrame"
				, "EditorScriptingUtilities"
				, "ClassViewer"
				, "EditorStyle"
				, "GraphEditor"
				, "BlueprintGraph"
				, "Settings"
				, "SettingsEditor"
				, "SharedSettingsWidgets"
				, "Kismet"
				, "DesktopWidgets"
				, "DeveloperSettings"
				, "DataRegistry"
				, "DataRegistryEditor"
				, "ToolMenus"
				, "Projects"
				, "ApplicationCore"
				, "KismetCompiler"
				, "StructUtilsEditor"
				, "AssetDefinition"
				, "EngineAssetDefinitions"
				, "ContentBrowser"
				, "ContentBrowserData"
				, "AssetManagerEditor"
				, "ArcInstancedWorld"
				, "MassEntity"
				, "WorkspaceMenuStructure"
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
