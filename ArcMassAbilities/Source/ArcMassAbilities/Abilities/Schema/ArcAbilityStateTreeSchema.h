// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "ArcAbilityStateTreeSchema.generated.h"

UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Ability State Tree"))
class UArcAbilityStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcAbilityStateTreeSchema();

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual bool IsScheduledTickAllowed() const override;
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
