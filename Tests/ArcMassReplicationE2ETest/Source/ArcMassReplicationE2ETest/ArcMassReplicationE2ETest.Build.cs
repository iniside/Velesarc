using UnrealBuildTool;

public class ArcMassReplicationE2ETest : ModuleRules
{
    public ArcMassReplicationE2ETest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        SetupIrisSupport(Target);

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ModularTestFramework",
            "ArcMassReplicationRuntime",
            "MassCore",
            "MassEntity",
            "MassSpawner",
            "NetCore",
            "IrisCore"
        });
    }
}
