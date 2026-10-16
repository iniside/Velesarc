using UnrealBuildTool;
using System.IO;

public class Lore : ModuleRules
{
	public Lore(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win64", "lore.lib"));

			string DllName = "lore.dll";
			RuntimeDependencies.Add(
				Path.Combine("$(TargetOutputDir)", DllName),
				Path.Combine(ModuleDirectory, "bin", "Win64", DllName));
			PublicDelayLoadDLLs.Add(DllName);
		}
	}
}
