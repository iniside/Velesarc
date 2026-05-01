using System.IO;
using UnrealBuildTool;

public class ArcMassReplicationRuntime : ModuleRules
{
    public ArcMassReplicationRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        SetupIrisSupport(Target);

        PublicIncludePaths.AddRange(
            new string[]
            {
                Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassReplicationRuntime"))
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                Path.Combine(GetModuleDirectory("IrisCore"), "Private"),
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GameplayTags",
                "StructUtils",
                "MassCore",
                "MassEntity",
                "MassSpawner",
                "MassSignals"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "NetCore",
                "IrisCore"
            }
        );
    }
}
