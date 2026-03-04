// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassPlayerPersistence : ModuleRules
{
	public ArcMassPlayerPersistence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MassEntity",
				"ArcMass",
				"ArcPersistence",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"CoreOnline",
				"Engine",
				"MassActors",
				"ArcCore",
				"MassSpawner"
			}
		);
	}
}
