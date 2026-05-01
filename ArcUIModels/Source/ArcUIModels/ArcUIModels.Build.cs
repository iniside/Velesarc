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
				"MassEntity",
				"ArcMassAbilities",
				"ArcMassDamageSystem",
				"ArcFrontend",
				"ModelViewViewModel",
				"GameplayTags",
				"StructUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"ArcCore",
				"MassActors"
			}
		);
	}
}
