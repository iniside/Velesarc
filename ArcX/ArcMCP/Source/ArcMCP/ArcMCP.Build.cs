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
			"Json",
			"JsonUtilities",
			"ECABridge"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"ArcCore",
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"StructUtils",
			"GameplayTags"
		});
	}
}
