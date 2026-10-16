// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcCraftTest : ModuleRules
{
	public ArcCraftTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// Expose UArcMassItemTestsSettings (real asset-backed item defs for tests that need
				// AddItem→InitializeMass to succeed — transient defs crash the delivery path).
				Path.GetFullPath(Path.Combine(PluginDirectory, "../ArcMassItemsTest/Source/ArcMassItemsTest"))
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
				, "GameplayTags"
				, "GameplayTasks"
				, "DeveloperSettings"
				, "CQTest"
				, "ModularGameplay"
				, "GameFeatures"
				, "ArcMassItemsRuntime"
				, "ArcSharedTestHarness"
				, "ArcMassAbilities"
				, "ArcAbilityKit"
				, "MassEntity"
				, "MassCore"
				, "MassSignals"
				// Real asset-backed item defs for tests requiring AddItem→InitializeMass to succeed.
				, "ArcMassItemsTest"
			}
			);
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"EngineSettings",
					"LevelEditor",
					"UnrealEd",
					// ArcRecipeGraphBuilderTests.cpp tests FArcRecipeGraphBuilder (editor-only).
					"ArcCraftEditor"
				});
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
