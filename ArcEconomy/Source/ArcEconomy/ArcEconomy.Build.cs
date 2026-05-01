using System.IO;
using UnrealBuildTool;

public class ArcEconomy : ModuleRules
{
    public ArcEconomy(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
	        new string[]
	        {
		        // ... add public include paths required here ...
		        Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcEconomy"))
	        }
        );
        PublicSystemIncludePaths.AddRange(
	        new string[]
	        {
		        Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcEconomy"))
	        }
        );

        PrivateIncludePaths.AddRange(
	        new string[]
	        {
		        // ... add other private include paths required here ...
		        Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcEconomy"))
	        }
        );
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "StructUtils",
            "MassCore",
            "MassEntity",
            "MassCommon",
            "MassSpawner",
            "StateTreeModule",
            "MassSignals",
            "MassAIBehavior",
            "ArcCore",
            "ArcCraft",
            "ArcTargetQuery",
            "ArcKnowledge",
            "ArcMass",
            "ArcAI",
            "ArcArea",
            "ArcNeeds",
            "SmartObjectsModule",
            "MassSmartObjects",
            "Learning",
            "LearningAgents"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "LearningAgentsTraining",
                "LearningTraining",
            });
        }
    }
}
