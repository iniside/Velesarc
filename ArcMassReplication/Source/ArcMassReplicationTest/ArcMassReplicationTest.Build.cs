using System.IO;
using UnrealBuildTool;

public class ArcMassReplicationTest : ModuleRules
{
    public ArcMassReplicationTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        SetupIrisSupport(Target);

        PrivateIncludePaths.AddRange(
            new string[]
            {
                Path.Combine(GetModuleDirectory("IrisCore"), "Private"),
            }
        );

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "CQTest",
            "ArcMassReplicationRuntime",
            "MassEntity",
            "MassCore",
            "MassSpawner",
            "MassSignals",
            "GameplayTags",
            "StructUtils",
            "NetCore",
            "IrisCore"
        });
    }
}
