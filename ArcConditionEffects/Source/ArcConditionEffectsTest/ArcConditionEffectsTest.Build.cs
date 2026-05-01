// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcConditionEffectsTest : ModuleRules
{
    public ArcConditionEffectsTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcConditionEffects"))
            }
        );

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "CQTest",
            "ArcConditionEffects",
            "MassEntity",
            "MassCore",
            "MassSignals",
            "MassSimulation",
            "MassSpawner",
            "MassCommon",
            "GameplayTags",
            "StructUtils",
            "GameplayAbilities",
            "ArcMassAbilities"
        });
    }
}
