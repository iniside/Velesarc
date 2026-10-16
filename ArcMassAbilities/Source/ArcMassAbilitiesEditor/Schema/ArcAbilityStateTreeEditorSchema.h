// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "StateTreeEditorSchema.h"
#include "ArcAbilityStateTreeEditorSchema.generated.h"

/**
 * Editor-side schema paired with UArcAbilityStateTreeSchema.
 * Its only job in v0 is to ensure every ability StateTree carries a
 * UArcAbilityManifestEditorExtension on its editor data.
 */
UCLASS(MinimalAPI)
class UArcAbilityStateTreeEditorSchema : public UStateTreeEditorSchema
{
	GENERATED_BODY()

public:
	virtual void Validate(TNotNull<UStateTree*> StateTree) override;
};
