// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassConfigTest : ModuleRules
{
	public ArcMassConfigTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcMass",
			"MassEntity",
			"MassCore",
			"MassSpawner",
		});
	}
}
