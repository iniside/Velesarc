// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStateTreeEditorData;

#include "ArcAbilityTreePreset.generated.h"

UCLASS(Abstract)
class UArcAbilityTreePreset : public UObject
{
	GENERATED_BODY()

public:
	virtual FText GetDisplayName() const PURE_VIRTUAL(UArcAbilityTreePreset::GetDisplayName, return FText::GetEmpty(); );

	/** EditorData arrives clean: no SubTrees, Evaluators, or GlobalTasks. Do not call Compile. */
	virtual void BuildTree(UStateTreeEditorData& EditorData) const PURE_VIRTUAL(UArcAbilityTreePreset::BuildTree, );
};
