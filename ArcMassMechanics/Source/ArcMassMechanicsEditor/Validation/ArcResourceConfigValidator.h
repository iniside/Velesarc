// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ArcResourceConfigValidator.generated.h"

UCLASS(meta = (ToolTip = "Fails a resource config with a resource that has no Value, no Rate or no Target, or a Pips Rate without RegenPips or PerPip. Warns when a resource has no Delta, since its steps are then dropped. Does not check the Delta meta's handler: that lives in the entity's handler config (ArcAbilityKit), which this module does not depend on."))
class UArcResourceConfigValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
