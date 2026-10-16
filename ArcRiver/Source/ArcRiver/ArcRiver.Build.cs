// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcRiver : ModuleRules
{
	public ArcRiver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ArcTerrain",
				"Water",
			}
		);
	}
}
