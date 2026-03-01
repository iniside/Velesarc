// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcAITests : ModuleRules
{
	public ArcAITests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcAI",
			"MassEntity",
			"GameplayTags",
			"SmartObjectsModule",
			"MassSmartObjects"
		});
	}
}
