// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcCues : ModuleRules
{
	public ArcCues(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(ModuleDirectory))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"MassEntity",
				"MassCore",
				"StateTreeModule",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MassSpawner",
				"MassActors",
				"AssetRegistry",
				"ArcMass",
				"Niagara",
				"MetasoundEngine"
			}
		);
	}
}
