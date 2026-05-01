// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "ArcEffectStateTreeSchema.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Effect"))
class ARCMASSABILITIES_API UArcEffectStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcEffectStateTreeSchema();

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
