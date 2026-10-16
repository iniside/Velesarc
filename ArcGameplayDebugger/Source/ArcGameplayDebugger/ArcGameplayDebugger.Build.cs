// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArcGameplayDebugger : ModuleRules
{
	public ArcGameplayDebugger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		SetupIrisSupport(Target);
		PublicIncludePaths.Add(ModuleDirectory);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"SlateIM",
				"ArcCore",
				"ArcGun",
				"ArcCraft",
				"GameplayTags",
				"ImGui",
				"InputCore",
				"EnhancedInput",
				"TargetingSystem",
				"UMG",
				"ArcBuilder",
				"ArcAI",
				"ArcTactics",
				"MassEntity",
				"MassCore",
				"MassSimulation",
				"MassSpawner",
				"MassCommon",
				"MassActors",
				"MassAIBehavior",
				"MassNavigation",
				"MassNavMeshNavigation",
				"MassMovement",
				"StateTreeModule",
				"ArcMass",
				"ArcMassRender",
				"ArcKnowledge",
				"ArcArea",
				"SmartObjectsModule",
				"MassSmartObjects",
				"ArcPersistence",
				"AsyncMessageSystem",
				"MassSignals",
				"ArcGameplayInteraction",
				"ArcNeeds",
				"ArcWeather",
				"MassEngine",
				"InteractableInterface",
				"PhysicsCore",
				"ArcTargetQuery",
				"ArcTargeting",
				"DebugDraw",
				"ArcWorldSim",
				"ArcMassAbilities",
				"ArcAbilityKit",
				"ArcMassGameplayFramework",
				"ArcMassItemsRuntime",
				"NavigationSystem",
				"AssetRegistry",
				"GameplayBehaviorsModule",
				"GameFeatures"

				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"InputCore",
				"Iris",
				//"IrisCore",
				"Engine",
				"Slate",
				"SlateCore",
				"SlateIM",
				"ArcCore",
				"ArcGun",
				"TraceLog",
				"Json"
				// ... add private dependencies that you statically link with here ...
			}
			);

		if (Target.bBuildDeveloperTools)
		{
			PrivateDependencyModuleNames.Add("TraceAnalysis");
			PrivateDefinitions.Add("WITH_ARC_TACTICS_TRACE_ANALYSIS=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_ARC_TACTICS_TRACE_ANALYSIS=0");
		}

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelEditor",
					"UnrealEd"
					// ... add other public dependencies that you statically link with here ...
				}
			);
			
		
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelEditor",
					"UnrealEd"
					// ... add private dependencies that you statically link with here ...	
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
