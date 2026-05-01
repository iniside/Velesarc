// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "Attributes/ArcAggregator.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "GameplayTagContainer.h"

struct FMassEntityManager;
struct FArcAggregatorFragment;
struct FArcEffectContext;
struct FArcEffectSpec;
struct FArcEffectStackFragment;
struct FArcActiveEffect;
class UArcEffectDefinition;
struct FArcEffectComponent;
struct FArcEffectExecutionEntry;

namespace ArcEffects
{
	namespace Private
	{
		ARCMASSABILITIES_API void RunExecutions(FMassEntityManager& EntityManager, FMassEntityHandle Target,
		                                         const TArray<FArcEffectExecutionEntry>& Executions,
		                                         const FArcEffectSpec& Spec,
		                                         const FSharedStruct& SpecStruct,
		                                         const FGameplayTagContainer& PassedInTags = FGameplayTagContainer());

		ARCMASSABILITIES_API void RunComponentHook(const TArray<FInstancedStruct>& Components,
		                                            TFunctionRef<void(const FArcEffectComponent&)> Func);

		ARCMASSABILITIES_API void RemoveModsFromAggregator(FArcAggregatorFragment& AggFragment,
		                                                    const TArray<FArcModifierHandle>& Handles);
	}

	ARCMASSABILITIES_API FArcActiveEffectHandle TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         UArcEffectDefinition* Effect, FMassEntityHandle Source,
	                                         const FInstancedStruct& SourceData = FInstancedStruct());

	ARCMASSABILITIES_API FArcActiveEffectHandle TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         UArcEffectDefinition* Effect, const FArcEffectContext& Context);

	ARCMASSABILITIES_API FArcActiveEffectHandle TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         const FSharedStruct& Spec);

	ARCMASSABILITIES_API void RemoveEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                        UArcEffectDefinition* Effect);

	ARCMASSABILITIES_API void RemoveEffectBySource(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                                UArcEffectDefinition* Effect, FMassEntityHandle Source);

	ARCMASSABILITIES_API void RemoveAllEffects(FMassEntityManager& EntityManager, FMassEntityHandle Target);

	ARCMASSABILITIES_API void RemoveEffectsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                              const FGameplayTagContainer& Tags);

	// Tears down a single active effect — runs OnRemoved on tasks and components, removes modifiers.
	// Does NOT remove the effect from the ActiveEffects array or send recalculate signals.
	ARCMASSABILITIES_API void CleanupActiveEffect(FArcActiveEffect& Active, FArcAggregatorFragment* AggFragment,
	                                               FMassEntityManager& EntityManager, FMassEntityHandle Target);

	ARCMASSABILITIES_API FArcActiveEffect* FindActiveEffect(FArcEffectStackFragment& EffectStack, FArcActiveEffectHandle Handle);

	ARCMASSABILITIES_API bool RemoveEffectByHandle(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                                FArcActiveEffectHandle Handle);
}
