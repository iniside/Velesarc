// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcEcotopeSamplerEditor : ModuleRules
{
	public ArcEcotopeSamplerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcEcotope",
				"ArcEcotopeEditor",
				"CoreUObject",
				"Engine",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
