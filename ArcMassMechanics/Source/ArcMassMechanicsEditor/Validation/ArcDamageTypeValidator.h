// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ArcDamageTypeValidator.generated.h"

UCLASS(meta = (ToolTip = "Fails an effect definition that carries more than one ArcEffect.Damage tag, carries one with child types, has a damage execution without a damage type, or has more than one damage execution; warns with each damage execution's data warnings. One damage effect is one hit of one concrete (leaf) damage type."))
class UArcDamageTypeValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
