// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcKnowledgeTest : ModuleRules
{
	public ArcKnowledgeTest(ReadOnlyTargetRules Target) : base(Target)
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
				"ArcKnowledge",
				"GameplayTags",
				"StructUtils",
				"MassEntity",
				"CQTest"
			}
		);
	}
}
