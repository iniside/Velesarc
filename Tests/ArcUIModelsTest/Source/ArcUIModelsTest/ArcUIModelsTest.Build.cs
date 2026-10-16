// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcUIModelsTest : ModuleRules
{
	public ArcUIModelsTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ArcCraftingViewModelCraftCommandTests: PIE game mode and secondary item store, craft record files.
				Path.GetFullPath(Path.Combine(PluginDirectory, "../ArcMassItemsTest/Source/ArcMassItemsTest")),
				Path.GetFullPath(Path.Combine(PluginDirectory, "../ArcCraftTest/Source/ArcCraftTest"))
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "Slate"
				, "SlateCore"
				, "ArcCore"
				, "ArcCraft"
				, "ArcBuilder"
				, "ArcUIModels"
				, "ArcFrontend"
				, "ArcMassItemsRuntime"
				, "ArcSharedTestHarness"
				, "ArcMassAbilities"
				, "ArcAbilityKit"
				, "GameplayTags"
				, "CQTest"
				, "MassEntity"
				, "MassCore"
				, "ModelViewViewModel"
				, "DeveloperSettings"
				, "ArcMassItemsTest"
				, "ArcMassGameplayFramework"
				, "MassActors"
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
