// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassDamageSystem : ModuleRules
{
	public ArcMassDamageSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				"ArcMassDamageSystem"
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "MassCore"
				, "MassEntity"
				, "MassSpawner"
				, "ArcMassAbilities"
				, "ArcConditionEffects"
				, "GameplayTags"
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
