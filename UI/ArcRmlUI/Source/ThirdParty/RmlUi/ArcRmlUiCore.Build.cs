// Copyright Lukasz Baran. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcRmlUiCore : ModuleRules
{
	public ArcRmlUiCore(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Off;

		PublicDependencyModuleNames.Add("Core");

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Include"));

		PrivateIncludePaths.AddRange(
			new string[]
			{
				ModuleDirectory,
				Path.Combine(ModuleDirectory, "ThirdParty/lunasvg/include"),
				Path.Combine(ModuleDirectory, "ThirdParty/lunasvg/source"),
				Path.Combine(ModuleDirectory, "ThirdParty/plutovg/include"),
				Path.Combine(ModuleDirectory, "ThirdParty/plutovg/source"),
			}
		);

		PublicDefinitions.Add("ITLIB_FLAT_MAP_NO_THROW");

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicDefinitions.Add("RMLUI_ARC_PERF");
		}

		if (Target.LinkType == TargetLinkType.Monolithic)
		{
			PublicDefinitions.Add("RMLUI_STATIC_LIB");
		}
		else
		{
			PrivateDefinitions.Add("RMLUI_CORE_EXPORTS");
		}

		PrivateDefinitions.AddRange(
			new string[]
			{
				"RMLUI_FONT_ENGINE_FREETYPE",
				"RMLUI_SVG_PLUGIN",
				"LUNASVG_BUILD_STATIC",
				"PLUTOVG_BUILD_STATIC",
				"PLUTOVG_BUILD",
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "FreeType2");
	}
}
