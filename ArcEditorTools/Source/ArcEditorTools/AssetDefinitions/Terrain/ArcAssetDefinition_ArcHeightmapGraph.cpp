// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Terrain/ArcAssetDefinition_ArcHeightmapGraph.h"

#include "AssetDefinition.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapGraphEditor.h"
#include "Toolkits/IToolkitHost.h"

EAssetCommandResult UArcAssetDefinition_ArcHeightmapGraph::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcHeightmapGraph* Graph : OpenArgs.LoadObjects<UArcHeightmapGraph>())
	{
		if (Graph != nullptr)
		{
			const TSharedRef<FArcHeightmapGraphEditor> Editor = MakeShared<FArcHeightmapGraphEditor>();
			Editor->InitHeightmapGraphEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Graph);
		}
	}
	return EAssetCommandResult::Handled;
}
