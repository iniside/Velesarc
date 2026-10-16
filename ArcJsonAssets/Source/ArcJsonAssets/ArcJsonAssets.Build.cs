// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcJsonAssets : ModuleRules
{
	public ArcJsonAssets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcJsonAssets")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcCore"
			}
		);
	}
}
