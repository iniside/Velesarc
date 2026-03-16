using System.IO;
using UnrealBuildTool;

public class ArcGameSettings : ModuleRules
{
    public ArcGameSettings(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGameSettings"))
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGameSettings"))
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ModelViewViewModel",
                "InputCore",
                "PropertyPath",
                "GameplayTags",
                "GameFeatures",
                "EnhancedInput",
                "UMG",
                "StructUtils"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "CommonUI",
                "CommonGame"
            }
        );
    }
}
