// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ArcAbilityCostValidator.generated.h"

UCLASS(meta = (ToolTip = "Fails an ability definition whose Cost holds an empty entry or a Cost Set with an empty entry. Costs nested in Cost Sets are checked too."))
class UArcAbilityCostValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
