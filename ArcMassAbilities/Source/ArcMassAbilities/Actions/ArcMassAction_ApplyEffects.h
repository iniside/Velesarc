// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Actions/ArcMassAction.h"
#include "StructUtils/SharedStruct.h"

#include "ArcMassAction_ApplyEffects.generated.h"

class UArcEffectDefinition;

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcMassAction_ApplyEffects : public FArcMassStatefulAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Effects")
	TArray<TObjectPtr<UArcEffectDefinition>> Effects;

	UPROPERTY(Transient)
	TArray<FSharedStruct> PreparedEffects;

	virtual bool Prepare(const FArcMassActionPreparationContext& Context) override;
	virtual void Execute(FArcMassActionContext& Context) override;
};
