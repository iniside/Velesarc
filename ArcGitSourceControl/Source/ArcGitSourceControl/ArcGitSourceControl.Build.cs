using UnrealBuildTool;

public class ArcGitSourceControl : ModuleRules
{
    public ArcGitSourceControl(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Slate",
            "SlateCore",
            "SourceControl",
            "UnrealEd",
            "InputCore",
            "Json",
            "JsonUtilities",
            "Projects"
        });
    }
}
