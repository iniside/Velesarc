// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcPCGExtensionTest : ModuleRules
{
	public ArcPCGExtensionTest(ReadOnlyTargetRules Target) : base(Target)
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
				// PCG for the headless smoke test: the PCG element, its data types, and the shipped
				// PCGTestsCommon::FTestData harness that builds a runnable FPCGContext.
				"PCG",
				// The unit under test — the pure forest solver + PCG settings/element.
				"ArcPCGExtension",
				// FArcTerrainSurfaceCache + ArcTerrainSurface::BuildCellCache, the CPU surface sampler
				// the terrain PCG data reads through.
				"ArcTerrain",
			}
		);

		// PCG public headers (PCGDataVisualization.h) transitively pull editor-only headers
		// (Editor/UnrealEdTypes.h) under WITH_EDITOR. Mirror ArcPCGExtension's own Build.cs so the
		// include paths resolve in editor builds — this does not make the test module editor-only.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"UnrealEd",
				}
			);
		}
	}
}
