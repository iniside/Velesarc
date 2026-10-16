// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcEcotopeEditor : ModuleRules
{
	public ArcEcotopeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcEcotope",
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcMass",
				"ArcTerrain",
				"CoreUObject",
				"Engine",
				"InputCore",
				// Required to link: UMassEntityConfigAsset::FindTrait is header-inline and calls the
				// dllimported FMassEntityConfig::FindTraitInternal; ArcMass's public MassSpawner dependency
				// does not carry that import into this module.
				"MassSpawner",
				"UnrealEd",
				"PropertyEditor",
				"LevelEditor",
				"Kismet",
				"Slate",
				"SlateCore",
				"AssetTools",
				"AssetDefinition",
				"StructUtilsEditor",          // step 5: FInstancedStructProvider (STRUCTUTILSEDITOR_API)
				"EditorViewport",             // FEditorViewportClientBase, the base FEditorViewportClient links against
				"AdvancedPreviewScene",       // step 6: FAdvancedPreviewScene
				"EditorFramework",
				"InteractiveToolsFramework",  // step 6: UInteractiveToolManager
				"EditorInteractiveToolsFramework",
			}
		);
	}
}
