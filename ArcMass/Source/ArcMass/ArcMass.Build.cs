// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMass : ModuleRules
{
	public ArcMass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(GetModuleDirectory("IrisCore"), "Private"),
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "Engine"
				, "MassActors"
				, "MassCommon"
				, "MassCore"
				, "MassEntity"
				, "MassMovement"
				, "MassSimulation"
				, "MassSpawner"
				, "MassRepresentation"
				, "MassSignals"
				, "MassEngine"
				, "MassLOD"
				, "GameplayTags"
				, "StructUtils"
				, "DeveloperSettings"
				, "InstancedActors"
				, "AsyncMessageSystem"
				, "SmartObjectsModule"
				, "NavigationSystem"
				, "ArcPersistence"
				, "Niagara"
				, "NiagaraCore"
				, "RenderCore"
				, "RHI"
				, "RHICore"
				, "TypedElementRuntime"
				, "TypedElementFramework"
				, "FastGeoStreaming"
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
				"InputCore",
				"NetCore",
				"IrisCore",
				"PhysicsCore",
				"Chaos",
				"AssetRegistry",
				"UAF",
				"UAFMass"
				// ... add private dependencies that you statically link with here ...
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		// Access MassEngine Private headers for FMassRenderStateFragment, FMassStaticMeshRenderStateHelper
		string MassEnginePrivateMeshPath = System.IO.Path.Combine(EngineDirectory, "Source", "Runtime", "Mass", "MassEngine", "Private", "Mesh");
		PrivateIncludePaths.Add(MassEnginePrivateMeshPath);

		// FastGeoStreaming Internal headers — ESceneProxyCreationError is defined in both
		// FastGeoComponent.h and MassRenderStateHelper.h. Only files that don't transitively
		// include MassRenderStateHelper.h may include FastGeo headers.
		string FastGeoInternalPath = System.IO.Path.Combine(EngineDirectory, "Plugins", "Experimental", "FastGeoStreaming", "Source", "FastGeoStreaming", "Internal");
		PrivateIncludePaths.Add(FastGeoInternalPath);


		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		SetupGameplayDebuggerSupport(Target);
	}
}
