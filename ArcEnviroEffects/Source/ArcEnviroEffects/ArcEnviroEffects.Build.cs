// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcEnviroEffects : ModuleRules
{
	public ArcEnviroEffects(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(ModuleDirectory));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreUObject"
				, "Engine"
				, "MassEntity"
				, "MassCore"
				, "GameplayTags"
				, "ArcMassAbilities"
				, "ArcAbilityKit"
				, "ArcMassMechanics"
			}
			);
	}
}
