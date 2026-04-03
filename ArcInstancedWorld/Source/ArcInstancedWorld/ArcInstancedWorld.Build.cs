using UnrealBuildTool;

public class ArcInstancedWorld : ModuleRules
{
	public ArcInstancedWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"CoreUObject",
				"MassEntity",
				"MassCore",
				"MassSpawner",
				"MassCommon",
				"MassActors",
				"MassSignals",
				"StructUtils",
				"DeveloperSettings",
				"PCG",
				"ArcMass"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"ArcPersistence",
				"MassEngine",
				"PhysicsCore",
				"Chaos"
			}
		);

		// Access MassEngine Private headers for FMassRenderStateFragment, FMassISMRenderStateHelper
		// Required by AArcIWMassISMPartitionActor to create ISM holder entities with render state.
		string MassEnginePrivateMeshPath = System.IO.Path.Combine(
			EngineDirectory, "Source", "Runtime", "Mass", "MassEngine", "Private", "Mesh");
		PrivateIncludePaths.Add(MassEnginePrivateMeshPath);
	}
}
