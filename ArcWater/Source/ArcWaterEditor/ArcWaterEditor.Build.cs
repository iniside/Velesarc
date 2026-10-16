// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcWaterEditor : ModuleRules
{
	public ArcWaterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"ArcTerrain",
				"ArcWater",
				"CoreUObject",
				"DetailCustomizations",
				"Engine",
				"InputCore",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
