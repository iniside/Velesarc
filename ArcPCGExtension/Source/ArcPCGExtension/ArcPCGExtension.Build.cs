// Copyright Lukasz Baran. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ArcPCGExtension : ModuleRules
{
	public ArcPCGExtension(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(PluginDirectory, "Source/ArcPCGExtension")));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"PCG",
			"ArcTerrain",
			"RenderCore",
			"RHI",
			"Renderer"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"ComputeFramework", // UComputeKernel, base of the native GPU kernels
			"Projects" // IPluginManager, for the shader source directory mapping
		});

		// PCG public headers (via PCGModule.h -> PCGDataVisualizationRegistry.h) transitively pull
		// editor-only headers (Editor/UnrealEdTypes.h) under WITH_EDITOR. Mirror PCG's own Build.cs
		// so the include paths resolve in editor builds — this does NOT make the module editor-only.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"EditorFramework",
				"UnrealEd"
			});
		}
	}
}
