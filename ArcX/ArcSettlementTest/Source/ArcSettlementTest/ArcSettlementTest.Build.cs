// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcSettlementTest : ModuleRules
{
	public ArcSettlementTest(ReadOnlyTargetRules Target) : base(Target)
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
				"ArcSettlement",
				"GameplayTags",
				"StructUtils",
				"MassEntity",
				"CQTest"
			}
		);
	}
}
