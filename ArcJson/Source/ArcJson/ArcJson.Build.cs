// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ArcJson : ModuleRules
{
	public ArcJson(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.GetFullPath(ModuleDirectory));

		PublicDependencyModuleNames.Add("Core");

		if (Target.LinkType != TargetLinkType.Monolithic)
		{
			PrivateDefinitions.Add("YYJSON_EXPORTS=1");
			PublicDefinitions.Add("YYJSON_IMPORTS=1");
		}
	}
}
