// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassAbilitiesTest : ModuleRules
{
	public ArcMassAbilitiesTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMassAbilities"))
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"ArcMassAbilities",
			"MassEntity",
			"MassCore",
			"GameplayTags",
			"StructUtils",
			"GameplayAbilities",
			"MassSignals",
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils"
		});
	}
}
