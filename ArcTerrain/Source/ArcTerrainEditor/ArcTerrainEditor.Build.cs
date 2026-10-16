// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcTerrainEditor : ModuleRules
{
	public ArcTerrainEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat layout (no Public/): expose the module root so dependents and
		// internal cross-subdir includes resolve as "Foo/ArcTerrainEditorXxx.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"ComponentVisualizers",
				"MaterialEditor",
				"ArcTerrain",
			}
		);

		// Invariant: ArcTerrainEditor must NEVER depend on ArcEditorTools.
	}
}
