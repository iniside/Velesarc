// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "GameplayTagContainer.h"
#include "ArcAreaGetSlotDefinitionPropertyFunction.generated.h"

USTRUCT()
struct FArcAreaGetSlotDefinitionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayTag RoleTag;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bAutoPostVacancy = false;

	UPROPERTY(EditAnywhere, Category = Output)
	float VacancyRelevance = 0.0f;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 SmartObjectSlotIndex = 0;
};

/**
 * Reads the design-time slot definition (role tag, vacancy config) for a specific slot by index.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Slot Definition", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetSlotDefinitionPropertyFunction : public FMassStateTreePropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetSlotDefinitionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
