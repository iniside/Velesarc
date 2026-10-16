// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcWater : ModuleRules
{
	public ArcWater(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcTerrain",
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"Renderer",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("DebugDraw");
		}
	}
}
