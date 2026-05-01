// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcFrontend : ModuleRules
{
	public ArcFrontend(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add("BOOST_ALL_NO_LIB");
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcFrontend"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcFrontend"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcFrontend"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreUObject"
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
				, "AIModule"
				, "AudioMixer"
				, "InputCore"
				, "AssetRegistry"
				, "DeveloperSettings"
				, "DataRegistry"
				, "UIExtension"
				, "Niagara"
				, "AsyncMixin"
				, "ArcCore"
				, "ArcAudio"
				, "ApplicationCore"
				, "TargetingSystem"
				// ... add other public dependencies that you statically link with here ...
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcGameSettings"
			});
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"RHI"
			});

		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"UMG"
				, "ModelViewViewModel"
				, "FieldNotification"
				, "StructUtils"
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonUser"
				, "ControlFlows"
				, "CommonGame"
				, "GameplayMessageRuntime"
				, "GameSettings"
				, "ModularGameplayActors"
				, "CommonLoadingScreen"
				, "CommonStartupLoadingScreen"
				, "GameSubtitles"
				, "CommonUI"
				, "CommonInput"
				, "OnlineServicesInterface"
				, "OnlineSubsystemUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcGun", "AudioModulation"
				, "MassCore"
				// ... add private dependencies that you statically link with here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"MassEntity"
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
