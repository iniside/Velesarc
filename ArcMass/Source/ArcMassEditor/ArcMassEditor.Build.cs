// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassEditor : ModuleRules
{
    public ArcMassEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassEditor"))
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
                , "InputCore"
                , "Slate"
                , "SlateCore"
                , "UnrealEd"
                , "PropertyEditor"
                , "AssetTools"
                , "AssetRegistry"
                , "AssetDefinition"
                , "ClassViewer"
                , "ToolMenus"
                , "ArcMass"
                , "MassEntity"
                , "MassSpawner"
                , "StructUtils"
                , "StructUtilsEditor"
                , "StructViewer"
            }
        );
    }
}
