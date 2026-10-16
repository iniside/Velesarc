// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ArcTriggerValidator.generated.h"

UCLASS(meta = (ToolTip = "Fails an effect definition whose Trigger Host is on an instant effect, or has a trigger that breaks its shape: On without an event tag, an event tag outside On, While with charges, an On trigger without a Chance in (0, 1], While on the other party, an If that neither has modifiers nor fires effects, a trigger without effects, or conditions and effects that reject the shape."))
class UArcTriggerValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
