using System.IO;
using UnrealBuildTool;

public class ArcWorldSim : ModuleRules
{
    public ArcWorldSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
	        new string[]
	        {
		        // ... add public include paths required here ...
		        Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcWorldSim"))
	        }
        );
        PublicSystemIncludePaths.AddRange(
	        new string[]
	        {
		        Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcWorldSim"))
	        }
        );

        PrivateIncludePaths.AddRange(
	        new string[]
	        {
		        // ... add other private include paths required here ...
		        Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcWorldSim"))
	        }
        );
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "MassCore",
            "MassEntity",
            "MassCommon",
            "MassSpawner",
            "MassSignals",
            "MassMovement",
            "MassNavigation",
            "MassNavMeshNavigation",
            "SmartObjectsModule",
            "StructUtils",
            "MassSmartObjects",
            "ArcCore",
            "ArcMass",
            "TraceLog",
            "ArcMassItemsRuntime",
            "ArcCraft",
            "ArcAI",
            "ArcNeeds"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry"
        });
    }
}
