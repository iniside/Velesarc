// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcRecoil : ModuleRules
{
	public ArcRecoil(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcRecoil"))
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcRecoil"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MassEntity",
				"MassCore",
				"MassSpawner",
				"MassCommon",
				"GameplayCameras"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MassActors",
				"EnhancedInput"
			}
		);
	}
}
