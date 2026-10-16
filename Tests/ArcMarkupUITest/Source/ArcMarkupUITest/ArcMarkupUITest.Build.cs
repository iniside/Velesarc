// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMarkupUITest : ModuleRules
{
	public ArcMarkupUITest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UMG",
			"CQTest",
			"ArcMarkupUI"
		});
	}
}
