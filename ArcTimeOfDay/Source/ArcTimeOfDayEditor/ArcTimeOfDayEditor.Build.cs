using UnrealBuildTool;

public class ArcTimeOfDayEditor : ModuleRules
{
	public ArcTimeOfDayEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"PropertyEditor",
				"WorkspaceMenuStructure",
				"DaySequence",
				"ArcTimeOfDay"
			}
		);
	}
}
