// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassPlacedEntityTest : ModuleRules
{
	public ArcMassPlacedEntityTest(ReadOnlyTargetRules Target) : base(Target)
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
			"MassCommon",
			"MassSpawner",
			"MassActors",
			"MassSignals",
		});
	}
}
