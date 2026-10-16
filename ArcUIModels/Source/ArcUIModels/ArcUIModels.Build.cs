// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcUIModels : ModuleRules
{
	public ArcUIModels(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcUIModels"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MassCore",
				"MassEntity",
				"ArcMassAbilities",
				"ArcMassDamageSystem",
				"ArcMassItemsRuntime",
				"ArcFrontend",
				"ModelViewViewModel",
				"GameplayTags",
				"MassSignals",
				"SlateCore",
				"ArcCraft",
				"ArcMassReplicationRuntime",
				"ArcBuilder",
				"ArcRmlUIData"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"ArcCore",
				"MassActors",
				"ArcAbilityKit",
				"ArcTargeting",
				"AssetRegistry",
				"ArcMassMechanics"
			}
		);
	}
}
