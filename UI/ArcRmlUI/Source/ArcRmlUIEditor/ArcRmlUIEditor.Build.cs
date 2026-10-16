// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcRmlUIEditor : ModuleRules
{
	public ArcRmlUIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcRmlUIEditor")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ArcRmlUI",
				"ArcRmlUIData"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"DirectoryWatcher",
				"ContentBrowserFileDataSource",
				"ContentBrowserData",
				"ContentBrowser",
				"PropertyEditor",
				"EditorWidgets",
				"Slate",
				"SlateCore",
				"AppFramework",
				"InputCore",
				"ModelViewViewModel",
				"FieldNotification",
				"GameplayTags",
				"GameplayTagsEditor",
				"Projects"
			}
		);
	}
}
