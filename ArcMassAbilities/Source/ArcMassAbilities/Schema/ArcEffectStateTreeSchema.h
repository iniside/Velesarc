// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "MassStateTreeDependency.h"
#include "ArcEffectStateTreeSchema.generated.h"

struct FStateTreeLinker;

namespace Arcx::MassAbilities::Effect
{
	extern ARCMASSABILITIES_API const FName OwnerEntityHandle;
	extern ARCMASSABILITIES_API const FName SourceEntityHandle;
	extern ARCMASSABILITIES_API const FName EffectTreeContext;
}

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Effect"))
class ARCMASSABILITIES_API UArcEffectStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcEffectStateTreeSchema();

	const TArray<FMassStateTreeDependency>& GetDependencies() const { return Dependencies; }

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }
	virtual bool Link(FStateTreeLinker& Linker) override;

	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;

	UPROPERTY(Transient)
	TArray<FMassStateTreeDependency> Dependencies;
};
