// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMCP : ModuleRules
{
	public ArcMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"ToolsetRegistry"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"ArcCore",
			"ArcCoreEditor",
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"GameplayTags",
			"SemanticSearch",
			"TraceAnalysis",
			"TraceServices",
			"ArcGameplayDebugger"
		});
	}
}
