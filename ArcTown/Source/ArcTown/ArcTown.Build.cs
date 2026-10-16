// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcTown : ModuleRules
{
	public ArcTown(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"ArcTerrain",
				"DebugDraw",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"GeometryCore",
				"GeometryAlgorithms",
			}
		);
	}
}
