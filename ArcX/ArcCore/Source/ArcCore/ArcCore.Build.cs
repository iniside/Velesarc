// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcCore : ModuleRules
{
	public ArcCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;

		var enginePath = Path.GetFullPath(Target.RelativeEnginePath);
		// Now get the base of UE4's modules dir (could also be Developer, Editor, ThirdParty)
		var pluginPath = enginePath + "Plugins/Runtime/";
		PublicIncludePaths.Add(pluginPath + "GameplayInteractions/Source/GameplayInteractionsModule/Private");
		PublicIncludePaths.Add(enginePath + "Source/Runtime/Experimental/Iris/Core/Private");
		SetupIrisSupport(Target);
				
		PublicDefinitions.Add("BOOST_ALL_NO_LIB");
				
		bUseUnity = false;
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(GetModuleDirectory("IrisCore"), "Private"),
			}
		);
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCore"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCore"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcCore"))
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreOnline"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "NetCore"
				, "GameplayAbilities"
				, "GameplayTasks"
				, "GameplayTags"
				, "GameplayDebugger"
				, "DeveloperSettings"
				, "GameFeatures"
				, "ModularGameplay"
				, "EnhancedInput"
				, "AudioMixer"
				, "InputCore"
				, "AssetRegistry"
				, "DeveloperSettings"
				, "DataRegistry"
				, "UIExtension"
				, "Niagara"
				, "NiagaraAnimNotifies"
				, "AsyncMixin"
				, "ChaosCore"
				, "Chaos"
				, "ChaosSolverEngine"
				, "PhysicsCore"
				, "WorldConditions"
				, "EngineSettings"
				, "GameplayCameras"
				, "HeadMountedDisplay"
				, "MovieScene"
				, "MovieSceneTracks"
				, "TemplateSequence"
				, "Mover"
				, "DaySequence"
				, "PoseSearch"
				, "ChaosClothAssetEngine"
				, "CustomizableObject"
				, "StateTreeModule"
				, "GameplayStateTreeModule"
				, "OnlineServicesInterface"
				, "Chooser"
				, "HierarchyTableAnimationRuntime"
				// ... add other public dependencies that you statically link with here ...
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AIModule"
				, "NavigationSystem"
				, "ArcJson"
			});

		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NetCore",
			});
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcPersistence"
			});
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"RHI"
			});
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"MassEntity", "AIModule", "MassCommon", "StateTreeModule", "SmartObjectsModule",
				"GameplayInteractionsModule", "MassActors", "MassSpawner", "MassLOD", "MassRepresentation", "InstancedActors"
			});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"UMG"
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"TargetingSystem"
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationLocomotionLibraryRuntime", "AnimGraphRuntime"
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonUser", "ControlFlows", "CommonGame", "GameplayMessageRuntime", "GameSettings",
				"ModularGameplayActors", "CommonLoadingScreen", "CommonStartupLoadingScreen", "GameSubtitles",
				"CommonUI", "CommonInput"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Json", "JsonUtilities"

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