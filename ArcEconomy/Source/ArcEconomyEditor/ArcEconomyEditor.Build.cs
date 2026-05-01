using UnrealBuildTool;

public class ArcEconomyEditor : ModuleRules
{
	public ArcEconomyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetDefinition",
			"AssetTools",
			"ArcEconomy",
			"ArcMassEditor",
			"ArcCore",
			"ArcCraft",
			"ArcArea",
			"MassEntity",
			"MassCore",
			"MassSpawner",
			"SmartObjectsModule",
			"ArcGameplayInteraction",
			"StateTreeModule",
			"GameplayTags",
			"StructUtils",
			"EditorSubsystem",
			"Slate",
			"SlateCore",
			"InputCore",
			"LearningAgents",
			"LearningAgentsTraining",
			"LearningTraining",
			"PropertyEditor",
			"WorkspaceMenuStructure",
		});
	}
}
