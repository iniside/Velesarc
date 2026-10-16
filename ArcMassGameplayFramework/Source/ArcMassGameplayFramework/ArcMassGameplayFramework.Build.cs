// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassGameplayFramework : ModuleRules
{
	public ArcMassGameplayFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;
		bUseUnity = false;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(ModuleDirectory))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NetCore",
				"GameplayTags",
				"DataRegistry",
				"ModularGameplay",
				"ModularGameplayActors",
				"GameFeatures",
				"GameplayMessageRuntime",
				"ArcCore",
				"MassCore",
				"MassEntity",
				"MassSpawner",
				"MassActors",
				"MassCommon",
				"MassReplication",
				"MassSimulation",
				"MassNavigation",
				"MassMovement",
				"Mover",
				"AIModule",
				"NavigationSystem",
				"EnhancedInput",
				"InputCore",
				"UMG",
				"CommonUI",
				"CommonGame",
				"CommonUser",
				"CommonLoadingScreen",
				"GameplayCameras",
				"UIExtension"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"DeveloperSettings",
				"AssetRegistry",
				"EngineSettings",
				"CoreOnline",
				"AsyncMessageSystem",
				"ArcMassAbilities",
				"ArcMassItemsRuntime",
				"ArcMass",
				"MassLOD"
			}
		);
	}
}
