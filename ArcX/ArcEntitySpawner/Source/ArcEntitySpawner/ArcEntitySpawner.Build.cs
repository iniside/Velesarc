// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcEntitySpawner : ModuleRules
{
	public ArcEntitySpawner(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "Engine"
				, "MassActors"
				, "MassCommon"
				, "MassEntity"
				, "MassSimulation"
				, "MassSpawner"
				, "MassSignals"
				, "MassAIBehavior"
				, "GameplayTags"
				, "StructUtils"
				, "StateTreeModule"
				, "GameplayStateTreeModule"
				, "ArcAI"
				, "ArcMass"
				, "ArcPersistence"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "MassRepresentation"
				, "AsyncMessageSystem"
			}
		);
	}
}
