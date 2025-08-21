using UnrealBuildTool;

public class ArcAIEditor : ModuleRules
{
    public ArcAIEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ArcAI"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );
        
        PublicDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "Core",
		        "CoreUObject",
		        "Engine",
		        "InputCore",
		        "AssetTools",
		        "EditorFramework",
		        "UnrealEd",
		        "Slate",
		        "SlateCore",
		        "EditorStyle",
		        "PropertyEditor",
		        "StateTreeModule",
		        "Projects",
		        "BlueprintGraph",
		        "PropertyAccessEditor",
		        "GameplayTags",
				
		        // ... add other public dependencies that you statically link with here ...
	        }
        );
			
		
        PrivateDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "CoreUObject",
		        "Engine",
		        "Slate",
		        "SlateCore",
		        "ArcGameSettings",
		        "AssetDefinition",
		        "RenderCore",
		        "GraphEditor",
		        "KismetWidgets",
		        "PropertyPath",
		        "ToolMenus",
		        "ToolWidgets",
		        "ApplicationCore",
		        "DeveloperSettings",
		        "RewindDebuggerInterface",
		        "DetailCustomizations",
		        "AppFramework",
		        "KismetCompiler",
		        "ArcEditorTools"
				
		        // ... add private dependencies that you statically link with here ...	
	        }
        );
    }
}