// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcDamageSystem : ModuleRules
{
	public ArcDamageSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "GameplayAbilities"
				, "GameplayTags"
				, "GameplayTasks"
				, "ArcCore"
				, "ArcConditionEffects"
				, "MassEntity"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "DeveloperSettings"
			}
			);
	}
}
