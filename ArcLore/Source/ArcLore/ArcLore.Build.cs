using UnrealBuildTool;

public class ArcLore : ModuleRules
{
    public ArcLore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "SourceControl",
            "UnrealEd",
            "InputCore",
            "Json",
            "JsonUtilities",
            "Projects",
            "Lore",
            "ToolMenus",
            "WorkspaceMenuStructure",
            "ContentBrowser",
            "AssetRegistry",
            "PropertyEditor"
        });

        PrivateIncludePaths.Add(ModuleDirectory);
    }
}
