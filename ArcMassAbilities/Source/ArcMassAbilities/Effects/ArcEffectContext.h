// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "StructUtils/InstancedStruct.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcEffectContext.generated.h"


class UArcEffectDefinition;
class UArcAbilityDefinition;
struct FMassEntityManager;
struct FArcEffectSpec;

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectContext
{
	GENERATED_BODY()

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle TargetEntity;
	FMassEntityHandle SourceEntity;
	UArcEffectDefinition* EffectDefinition = nullptr;
	int32 StackCount = 0;
	FInstancedStruct SourceData;

	UPROPERTY()
	TObjectPtr<UArcAbilityDefinition> SourceAbility = nullptr;

	FArcAbilityHandle SourceAbilityHandle;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectExecutionContext
{
	GENERATED_BODY()

	const FArcEffectSpec* OwningSpec = nullptr;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle TargetEntity;
	FGameplayTagContainer PassedInTags;

	const FArcEffectSpec& GetOwningSpec() const;
	FMassEntityHandle GetSourceEntity() const;
	int32 GetStackCount() const;

	FArcAttribute GetAttribute(FMassEntityHandle Entity, const FArcAttributeRef& Ref) const;

	bool AttemptCalculateCapturedAttributeMagnitude(
		const FArcAttributeCaptureDef& CaptureDef,
		const FArcEvaluateParameters& EvalParams,
		float& OutMagnitude) const;
};
