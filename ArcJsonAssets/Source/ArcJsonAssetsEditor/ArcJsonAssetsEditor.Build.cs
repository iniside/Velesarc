// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcJsonAssetsEditor : ModuleRules
{
	public ArcJsonAssetsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcJsonAssetsEditor")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ContentBrowserFileDataSource",
				"ContentBrowserData"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"ContentBrowser",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"DirectoryWatcher",
				"Projects",
				"GameplayTags",
				"Json",
				"ArcCore",
				"ArcCoreEditor"
			}
		);
	}
}
