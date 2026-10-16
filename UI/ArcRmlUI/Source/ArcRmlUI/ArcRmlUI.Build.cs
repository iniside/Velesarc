// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcRmlUI : ModuleRules
{
	public ArcRmlUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcRmlUI")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"GameplayTags",
				"ArcRmlUiCore",
				"ArcRmlUIData"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RenderCore",
				"RHI",
				"SlateCore",
				"Slate",
				"ApplicationCore",
				"InputCore",
				"ImageWrapper",
				"UMG",
				"CommonUI",
				"CommonInput",
				"CommonGame",
				"EnhancedInput",
				"ModelViewViewModel",
				"FieldNotification",
				"Json",
				"Projects",
				"ArcGameSettings",
				"ArcRmlUIShaders"
			}
		);
	}
}
