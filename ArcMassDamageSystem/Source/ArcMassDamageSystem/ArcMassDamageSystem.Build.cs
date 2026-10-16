// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcMassDamageSystem : ModuleRules
{
	public ArcMassDamageSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "MassCore"
				, "MassEntity"
				, "MassSpawner"
				, "ArcMassAbilities"
				, "GameplayTags"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "ArcMass"
				, "StructUtils"
			}
			);
	}
}
