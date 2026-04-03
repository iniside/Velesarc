// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcPersistenceTest : ModuleRules
{
	public ArcPersistenceTest(ReadOnlyTargetRules Target) : base(Target)
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
				"ArcPersistence",
				"ArcJson",
				"GameplayTags",
				"StructUtils",
				"ArcMass",
				"MassEntity",
				"MassCore",
				"SQLiteCore",
			}
		);
	}
}
