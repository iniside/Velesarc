// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassEditor : ModuleRules
{
    public ArcMassEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // We deliberately consume experimental TEDS APIs (TedsOutliner / TedsEditorCompatibility
        // columns and the FTedsOutlinerExpansionStateBridge), all marked UE_EXPERIMENTAL(5.8).
        // Opt the whole module in so UE_EXPERIMENTAL expands to nothing instead of [[deprecated]].
        bValidateExperimentalApi = false;

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
                , "EditorViewport"
                , "PropertyEditor"
                , "AssetTools"
                , "AssetRegistry"
                , "AssetDefinition"
                , "ClassViewer"
                , "ToolMenus"
                , "ArcMass"
                , "MassEntity"
                , "MassCore"
                , "MassSpawner"
                , "StructUtilsEditor"
                , "StructViewer"
                , "PhysicsCore"
                , "AdvancedPreviewScene"
                , "Niagara"
                , "EditorFramework"
                , "LevelEditor"
                , "InteractiveToolsFramework"
                , "EditorInteractiveToolsFramework"
                , "EditorSubsystem"
                , "TypedElementRuntime"
                , "TypedElementFramework"
                , "TedsOutliner"
                , "TedsEditorCompatibility"
                , "PCG"
                , "ContentBrowser"
                , "ContentBrowserData"
                , "EditorWidgets"
                , "DebugDraw"
                , "RenderCore"
            }
        );
    }
}
