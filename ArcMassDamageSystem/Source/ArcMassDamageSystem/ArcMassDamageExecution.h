// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectExecution.h"
#include "Attributes/ArcAttribute.h"

#include "ArcMassDamageExecution.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassTypedDamageExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef OutputAttribute;

	UPROPERTY(EditAnywhere)
	float BaseDamage = 0.f;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) override;

private:
	static float GetAttributeValue(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcAttributeRef& AttrRef);
};
