// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class IDetailsView;
class UArcAbilityTreePreset;
class UStateTree;

struct FArcAbilityPresetResult
{
	TArray<FString> Errors;
	TArray<FString> Warnings;
	bool bSuccess = false;
	bool bCancelled = false;
};

namespace ArcAbilityPresetGenerator
{
	FArcAbilityPresetResult ConfirmAndBuild(UStateTree& StateTree, TSubclassOf<UArcAbilityTreePreset> PresetClass, TSharedPtr<IDetailsView> DetailsViewToRefresh = nullptr);

	FArcAbilityPresetResult ResetToPreset(UStateTree& StateTree, TSharedPtr<IDetailsView> DetailsViewToRefresh = nullptr);
}
