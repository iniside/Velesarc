// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectTypes.h"
#include "StructUtils/SharedStruct.h"
#include "ArcEffectSpec.generated.h"

class UArcEffectDefinition;

USTRUCT()
struct ARCMASSABILITIES_API FArcResolvedScopedMod
{
	GENERATED_BODY()

	FArcAttributeCaptureDef CaptureDef;
	EArcModifierOp Operation = EArcModifierOp::Add;
	float Magnitude = 0.f;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectSpec
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcEffectDefinition> Definition = nullptr;

	FArcEffectContext Context;

	TArray<float> ResolvedMagnitudes;

	TMap<FGameplayTag, float> SetByCallerMagnitudes;

	FGameplayTagContainer CapturedSourceTags;
	FGameplayTagContainer CapturedTargetTags;

	TArray<FArcCapturedAttribute> CapturedAttributes;

	TArray<FArcResolvedScopedMod> ResolvedScopedModifiers;
};

namespace ArcEffects
{
	ARCMASSABILITIES_API FSharedStruct MakeSpec(UArcEffectDefinition* Definition, const FArcEffectContext& Context);

	ARCMASSABILITIES_API void CaptureTagsFromEntities(FArcEffectSpec& Spec, FMassEntityManager& EntityManager,
	                                                    FMassEntityHandle Source, FMassEntityHandle Target);

	ARCMASSABILITIES_API void CaptureTagsFromDefinition(FArcEffectSpec& Spec, UArcEffectDefinition* Definition);

	ARCMASSABILITIES_API void CaptureAttributes(FArcEffectSpec& Spec, FMassEntityManager& EntityManager,
	                                              FMassEntityHandle Source, FMassEntityHandle Target);

	ARCMASSABILITIES_API void ResolveScopedModifiers(FArcEffectSpec& Spec);
}
