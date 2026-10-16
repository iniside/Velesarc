// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMarkupUI : ModuleRules
{
	public ArcMarkupUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG"
			}
		);
	}
}
