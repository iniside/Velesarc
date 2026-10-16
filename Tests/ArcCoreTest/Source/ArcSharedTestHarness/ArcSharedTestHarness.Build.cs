// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcSharedTestHarness : ModuleRules
{
	public ArcSharedTestHarness(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(ModuleDirectory))
			}
		);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"FunctionalTesting",
			"ApplicationCore",
			"Slate",
			"SlateCore",
			"ArcCore",
			"ArcMassItemsRuntime",
			"StructUtils"
		});

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"LevelEditor"
			});
		}
	}
}
