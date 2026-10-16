// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassMechanicsEditor : ModuleRules
{
	public ArcMassMechanicsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DataValidation"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"GameplayTags",
				"ArcMassAbilities",
				"ArcMassMechanics"
			}
		);
	}
}
