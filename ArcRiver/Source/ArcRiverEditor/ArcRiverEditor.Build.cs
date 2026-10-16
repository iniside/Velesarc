// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcRiverEditor : ModuleRules
{
	public ArcRiverEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"ArcRiver",
				"ArcTerrain",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"TypedElementFramework",
				"TypedElementRuntime",
				"UnrealEd",
				"Water",
				"WaterEditor",
			}
		);
	}
}
