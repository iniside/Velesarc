// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMarkupUIEditor : ModuleRules
{
	public ArcMarkupUIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcMarkupUIEditor"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreUObject"
				, "Engine"
				, "ArcMarkupUI"
				, "ToolsetRegistry"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects"
				, "UnrealEd"
				, "Slate"
				, "SlateCore"
				, "AssetTools"
				, "AssetRegistry"
				, "AssetDefinition"
				, "RenderCore"
				, "PropertyEditor"
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Nanosvg");
	}
}
