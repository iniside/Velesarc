// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcAI : ModuleRules
{
	public ArcAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcAI"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcAI"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcAI"))
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
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "InputCore"
				, "NavigationSystem"
				, "SmartObjectsModule"
				, "AIModule"
				, "GameplayTasks"
				, "GameplayTags"
				, "MassCommon"
				, "MassEntity"
				, "MassSpawner"
				, "MassEQS"
				, "ArcCore"
				, "GameplayBehaviorSmartObjectsModule"
				, "ArcGameplayInteraction"
				, "MassGameplayDebug"
				, "MoverMassIntegration"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		SetupGameplayDebuggerSupport(Target);
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
