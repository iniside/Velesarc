// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcTerrain : ModuleRules
{
	public ArcTerrain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat layout (no Public/): expose the module root so dependents and
		// internal cross-subdir includes resolve as "Foo/ArcTerrainXxx.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings", // UArcTerrainSettings : UDeveloperSettings (Project Settings surface)
				"RenderCore",
				"RHI",
				"DebugDraw", // terrain debug overlay; ArcTerrainDebugDraw.h includes its public header
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Renderer",
				"Projects",
				"Chaos",
				"ChaosCore", // TAABB/geometry exports pulled in by Chaos/HeightField.h (Step 7 collision)
				"PhysicsCore",
				"GeometryCore", // TMeshAABBTree3 — the mesh modifier's bed rasterizer
				"GeometryFramework", // UDynamicMeshComponent — the mesh modifier's baked collar
				"MeshConversion", // FStaticMeshLODResourcesMeshAdapter
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"DerivedDataCache",
					"Slate",
					"SlateCore",
				}
			);
		}
	}
}
