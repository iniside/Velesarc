// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;

public class ArcRmlUIMassGame : ModuleRules
{
	public ArcRmlUIMassGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameFeatures",
				"ModularGameplay",
				"ArcMassGameplayFramework",
				"ArcRmlUIData",
				"ArcGameplayInteraction",
				"StateTreeModule",
				"MassAIBehavior",
				"MassEntity",
				"MassCore",
				"MassActors"
			}
		);

		if (Target.Type != TargetType.Server)
		{
			PrivateDependencyModuleNames.Add("ArcRmlUI");
			PrivateDefinitions.Add("ARC_WITH_RMLUI=1");
		}
		else
		{
			PrivateDefinitions.Add("ARC_WITH_RMLUI=0");
		}
	}
}
