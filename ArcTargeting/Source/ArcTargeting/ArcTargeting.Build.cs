using System.IO;
using UnrealBuildTool;

public class ArcTargeting : ModuleRules
{
	public ArcTargeting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
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
				"MassSpawner",
				"MassActors",
				"ArcMass",
				"DebugDraw"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry"
			}
		);
	}
}
