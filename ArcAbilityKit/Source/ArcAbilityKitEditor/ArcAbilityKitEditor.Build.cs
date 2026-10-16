// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcAbilityKitEditor : ModuleRules
{
	public ArcAbilityKitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ArcAbilityKit",
				"StateTreeModule",
				"StateTreeEditorModule",
			}
		);
	}
}
