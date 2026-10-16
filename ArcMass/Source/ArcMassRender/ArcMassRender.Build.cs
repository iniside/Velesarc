// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcMassRender : ModuleRules
{
	public ArcMassRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		// Needed for the UE::ArcMassRender::Mesh namespace (mirrors MassEngine).
		bAllowUETypesInNamespaces = true;

		// Flat layout (no Public/): expose the module root so dependents and
		// internal cross-subdir includes resolve as "Mesh/ArcXxx.h" — mirrors how
		// MassEngine exposed "Mesh/MassEngineXxx.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Chaos",
				"Core",
				"CoreUObject",
				"Engine",
				"MassCore",
				"MassEntity",
				"MassSignals",
				"RenderCore",
				"RHI",
				// Retained public MassEngine dep (Phase 1): MassEngineTypes.h FNames
				// (ProcessorGroupNames / Signals) + public FMassPhysicsCollisionSettingsFragment.
				"MassEngine",
				// UHT-time need for FTypedElementHandle used in the ported fragments.
				"TypedElementFramework",
				"TypedElementRuntime",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PhysicsCore",
			}
		);

		// The ported ISM helper uses FInstanceDataHelpers::GenerateInstanceRandomIDs,
		// declared in Engine's Internal header InstanceData/InstanceDataHelpers.h.
		// Engine-tree modules (e.g. MassEngine) see this implicitly; a plugin must
		// add the Internal include path explicitly.
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Internal"));

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
