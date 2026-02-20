// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcConditionEffects : ModuleRules
{
	public ArcConditionEffects(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "MassEntity"
				, "MassCommon"
				, "MassSignals"
				, "MassSimulation"
				, "MassSpawner"
				, "StructUtils"
				, "GameplayAbilities"
				, "GameplayTags"
				, "GameplayTasks"
				, "ArcCore"
				, "MassActors"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
			}
			);
	}
}
