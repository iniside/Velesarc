// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcGameplayInteraction : ModuleRules
{
	public ArcGameplayInteraction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGameplayInteraction"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGameplayInteraction"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcGameplayInteraction"))
			}
		);
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreUObject"
				, "Engine"
				, "AIModule"
				, "MassCommon"
				, "MassEntity"
				, "MassSpawner"
				, "MassSignals"
				, "MassActors"
				, "MassAIBehavior"
				, "MassNavigation"
				, "MassNavMeshNavigation"
				, "MassMovement"
				, "MassEQS"
				, "MassLOD"
				, "StateTreeModule"
				, "GameplayStateTreeModule" 
				, "PropertyPath"
				, "GameplayAbilities"
				, "PropertyBindingUtils"
				, "ArcMass"
				, "NavCorridor"
				, "NavigationSystem"
				, "Navmesh"
				, "SmartObjectsModule"
				, "MassSmartObjects"
				, "ArcCore"
				, "GameplayBehaviorSmartObjectsModule"
				, "GameplayBehaviorsModule"
				, "GameplayInteractionsModule"
				, "EnhancedInput"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
