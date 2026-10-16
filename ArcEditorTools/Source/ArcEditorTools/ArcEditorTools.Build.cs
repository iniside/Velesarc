// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcEditorTools : ModuleRules
{
	public ArcEditorTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		OptimizeCode = CodeOptimization.Always;
		
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
				// ... add public include paths required here ...+
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
				, "StructUtilsEditor"
				, "AssetDefinition"
				, "EngineAssetDefinitions"
				, "ContentBrowser"
				, "ContentBrowserData"
				, "AssetManagerEditor"
				, "MassEntity"
				, "MassCore"
				, "MassSignals"
				, "WorkspaceMenuStructure"
				, "ArcArea"
				, "ArcCues"
				, "ArcMassDamageSystem"
				, "ArcKnowledge"
				, "ArcNeeds"
				, "ArcTargetQuery"
				, "ModelingComponents"
				, "GeometryCore"
				, "DynamicMesh"
				, "ZoneGraph"
				, "Blutility"
				, "MeshDescription"
				, "SkeletalMeshDescription"
				, "AnimationCore"
				, "Json"
				, "JsonUtilities"
				, "ArcMassItemsRuntime"
				, "DerivedDataCache"
				, "TextureGraph"
				, "TextureGraphEngine"
				, "ArcTextureGraphTerrain"
				, "ArcTerrain"
				, "RenderCore"
				, "RHI"
				, "GeometryAlgorithms"
				, "ModelingComponentsEditorOnly"
				, "MeshConversion"
				, "PCG"
				, "ImageCore"
				, "ToolWidgets"
				, "DynamicWind"
				, "DynamicWindEditor"
				, "TraceLog"
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
