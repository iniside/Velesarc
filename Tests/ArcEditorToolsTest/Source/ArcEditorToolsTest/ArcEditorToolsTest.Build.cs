// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcEditorToolsTest : ModuleRules
{
	public ArcEditorToolsTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"CQTest",
				"GeometryCore",
				"DynamicMesh",
				"SkeletalMeshDescription",
				"RenderCore",
				"RHI",
				"TextureGraph",
				"TextureGraphEngine",
				"ArcEditorTools",
				"ArcTextureGraphTerrain",
			}
		);
	}
}
