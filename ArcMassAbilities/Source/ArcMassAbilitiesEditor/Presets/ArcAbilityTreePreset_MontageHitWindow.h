// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presets/ArcAbilityTreePreset.h"

#include "ArcAbilityTreePreset_MontageHitWindow.generated.h"

UCLASS()
class UArcAbilityTreePreset_MontageHitWindow : public UArcAbilityTreePreset
{
	GENERATED_BODY()

public:
	virtual FText GetDisplayName() const override;
	virtual void BuildTree(UStateTreeEditorData& EditorData) const override;
};
