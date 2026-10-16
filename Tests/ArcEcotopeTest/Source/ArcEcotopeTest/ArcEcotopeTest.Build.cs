// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcEcotopeTest : ModuleRules
{
	public ArcEcotopeTest(ReadOnlyTargetRules Target) : base(Target)
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
				"ArcEcotope",
			}
		);

		if (Target.bBuildEditor)
		{
			// The site field generators live in the editor module; the grid family does not.
			PrivateDependencyModuleNames.Add("ArcEcotopeEditor");
		}
	}
}
