// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArcMass : ModuleRules
{
	public ArcMass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
				, "Engine"
				, "MassActors"
				, "MassCommon"
				, "MassEntity"
				, "MassMovement"
				, "MassSimulation"
				, "MassSpawner"
				, "MassRepresentation"
				, "MassSignals"
				, "MassLOD"
				, "GameplayTags"
				, "StructUtils"
				, "DeveloperSettings"
				, "InstancedActors"
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
				"PhysicsCore",
				"Chaos",
				"InputCore"
				// ... add private dependencies that you statically link with here ...
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		SetupGameplayDebuggerSupport(Target);
	}
}
