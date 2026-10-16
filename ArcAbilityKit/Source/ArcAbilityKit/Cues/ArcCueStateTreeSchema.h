// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"

#include "ArcCueStateTreeSchema.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Cue"))
class ARCABILITYKIT_API UArcCueStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcCueStateTreeSchema();

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
