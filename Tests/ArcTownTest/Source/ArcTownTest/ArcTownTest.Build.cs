// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcTownTest : ModuleRules
{
	public ArcTownTest(ReadOnlyTargetRules Target) : base(Target)
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
				"ArcTown",
			}
		);

		if (Target.bBuildEditor)
		{
			// MakeTownGridGuid is ARCTOWNEDITOR_API; the pad fold lives in the runtime module.
			PrivateDependencyModuleNames.Add("ArcTownEditor");
		}
	}
}
