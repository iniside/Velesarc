// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArcTextureGraphTerrain : ModuleRules
{
	public ArcTextureGraphTerrain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat module (no Public/ folder): expose the module root so consumers
		// (e.g. ArcEditorTools nodes) can include the transform headers directly.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
				, "Engine"
				, "RenderCore"
				, "RHI"
				, "Renderer"
				, "Projects"
				, "TextureGraphEngine"
			}
			);
	}
}
