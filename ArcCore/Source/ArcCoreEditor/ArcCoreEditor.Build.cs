// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcCoreEditor : ModuleRules
{
	public ArcCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCoreEditor"))
			}
		);
		
		PublicSystemIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCoreEditor"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCoreEditor"))
			}
		);

		// Get the engine path. Ends with "Engine/"
		string engineDir = Path.GetFullPath(EngineDirectory);
		// Now get the base of UE4's modules dir (could also be Developer, Editor, ThirdParty)
		string engineEditorDir = engineDir + "/Source/Editor/";
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(engineEditorDir, "PropertyEditor/Private"))
			}
		);
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				// ... add other public dependencies that you statically link with here ...
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "InputCore"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "GameplayTags"
				, "GameplayTasks"
				, "GameplayAbilities"
				, "UnrealEd"
				, "PropertyEditor"
				, "SceneOutliner"
				, "AssetTools"
				, "AssetRegistry"
				, "ContentBrowser"
				, "GameplayAbilitiesEditor"
				, "MainFrame"
				, "GameplayTags"
				, "GameplayTagsEditor"
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
				, "ArcCore"
				, "Projects"
				, "ApplicationCore"
				, "KismetCompiler"
				, "StructUtilsEditor"
				, "AssetDefinition"
				, "EngineAssetDefinitions"
				, "ContentBrowser"
				, "ContentBrowserData"
				, "AssetManagerEditor"
				, "AnimationModifiers"
				, "AnimGraph"
				, "AnimGraphRuntime"
				, "AnimationModifierLibrary"
				, "AnimationBlueprintLibrary"
				, "SignalProcessing"
				//dependencies that you statically link with here ...	
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