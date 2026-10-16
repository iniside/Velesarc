// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcTownEditor : ModuleRules
{
	public ArcTownEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcTown",
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AdvancedPreviewScene",
				"ArcMass",
				"ArcTerrain",
				"AssetDefinition",
				"CoreUObject",
				"EditorFramework",
				"EditorInteractiveToolsFramework",
				"EditorViewport",
				"Engine",
				"InputCore",
				"InteractiveToolsFramework",
				// Required to link: UMassEntityConfigAsset::FindTrait is header-inline and calls the
				// dllimported FMassEntityConfig::FindTraitInternal; ArcMass's public MassSpawner dependency
				// does not carry that import into this module.
				"MassSpawner",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
