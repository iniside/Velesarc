using System.IO;
using UnrealBuildTool;

public class ArcInstancedWorldEditor : ModuleRules
{
	public ArcInstancedWorldEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcInstancedWorldEditor"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"LevelEditor",
				"ArcInstancedWorld",
				"ArcMass",
				"StructUtils",
				"MassSpawner",
				"ToolMenus"
			}
		);
	}
}
