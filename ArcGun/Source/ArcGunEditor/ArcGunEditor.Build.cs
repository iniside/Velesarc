using System.IO;
using UnrealBuildTool;

public class ArcGunEditor : ModuleRules
{
    public ArcGunEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGunEditor"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGunEditor"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGunEditor"))
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
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
				, "ArcGun"
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