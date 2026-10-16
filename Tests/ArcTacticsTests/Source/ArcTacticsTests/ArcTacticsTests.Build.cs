using UnrealBuildTool;

public class ArcTacticsTests : ModuleRules
{
	public ArcTacticsTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CQTest",
			"GameplayTags",
			"Json",
			"MassCommon",
			"MassCore",
			"MassEntity",
			"MassMovement",
			"MassNavigation",
			"MassNavMeshNavigation",
			"MassSignals",
			"MassSpawner",
			"ArcAI",
			"ArcMass",
			"ArcCore",
			"ArcTargetQuery",
			"ArcTactics",
			"PropertyBindingUtils",
			"StateTreeModule"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("ArcTacticsEditor");
			PrivateDependencyModuleNames.Add("ArcTargetQueryEditor");
			PrivateDependencyModuleNames.Add("ApplicationCore");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
