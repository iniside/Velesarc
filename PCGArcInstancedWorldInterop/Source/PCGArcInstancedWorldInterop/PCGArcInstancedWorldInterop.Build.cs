using UnrealBuildTool;

public class PCGArcInstancedWorldInterop : ModuleRules
{
	public PCGArcInstancedWorldInterop(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PCG",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ArcInstancedWorld",
				"MassSpawner",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("ArcInstancedWorldEditor");
		}
	}
}
