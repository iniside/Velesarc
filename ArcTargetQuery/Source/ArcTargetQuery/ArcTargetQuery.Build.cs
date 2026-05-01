using System.IO;
using UnrealBuildTool;

public class ArcTargetQuery : ModuleRules
{
	public ArcTargetQuery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcTargetQuery"))
			}
		);
		PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcTargetQuery"))
			}
		);
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcTargetQuery"))
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
				, "CoreUObject"
				, "Engine"
				, "MassEntity"
				, "MassCommon"
				, "MassCore"
				, "MassActors"
				, "MassSignals"
				, "ArcCore"
				, "ArcMass"
				, "ArcMassDamageSystem"
				, "ArcMassAbilities"
				, "GameplayAbilities"
				, "NavigationSystem"
				, "Navmesh"
				, "SmartObjectsModule"
				, "WorldConditions"
				, "StructUtils"
				, "TraceLog"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate"
				, "SlateCore"
				, "InputCore"
				, "GameplayTags"
				, "MassGameplayDebug"
				, "StateTreeModule"
			}
		);

		SetupGameplayDebuggerSupport(Target);
	}
}
