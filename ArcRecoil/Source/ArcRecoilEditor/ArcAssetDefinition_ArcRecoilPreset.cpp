// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAssetDefinition_ArcRecoilPreset.h"

#include "ArcRecoilPresetEditor.h"

EAssetCommandResult UArcAssetDefinition_ArcRecoilPreset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	if (!GetAssetOpenSupport(FAssetOpenSupportArgs(OpenArgs.OpenMethod)).IsSupported)
	{
		return EAssetCommandResult::Unhandled;
	}

	for (UArcRecoilPreset* Preset : OpenArgs.LoadObjects<UArcRecoilPreset>())
	{
		FArcRecoilPresetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, Preset);
	}

	return EAssetCommandResult::Handled;
}
