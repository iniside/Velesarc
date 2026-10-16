// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "Attributes/ArcAggregator.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcScalableFloat.h"
#include "GameplayTagContainer.h"
#include "Misc/Optional.h"

struct FMassEntityManager;
struct FArcAggregatorFragment;
struct FArcEffectContext;
struct FArcEffectApplyRequest;
struct FArcEffectSpec;
struct FArcEffectStackFragment;
struct FArcActiveEffect;
class UArcEffectDefinition;
struct FArcEffectComponent;
struct FArcEffectExecutionEntry;
struct FArcEffectComp_Inhibition;
struct FArcTagCountContainer;
enum class EArcEffectExecutionTrigger : uint8;

/**
 * Result of ArcEffects::TryApplyEffect.
 *
 * bApplied — the effect passed all gates (validity, immunity, pre-application, stacking)
 *            and its application ran. True for instant effects too.
 * Handle   — valid ONLY for stored (duration/infinite) effects; default-invalid for instant
 *            applications and rejections. Guard handle use (e.g. a RemoveEffects Handle query) on
 *            Handle.IsValid(), never on bApplied.
 *
 * Deliberately no IsValid()/operator bool sugar — "applied?" and "do I have a removable
 * active effect?" are different questions and each call site must pick one explicitly.
 */
struct ARCMASSABILITIES_API FArcEffectApplyResult
{
	bool bApplied = false;
	FArcActiveEffectHandle Handle;
};

struct ARCMASSABILITIES_API FArcEffectRemovalQuery
{
	const UArcEffectDefinition* Definition = nullptr;
	TOptional<FMassEntityHandle> Source;
	FArcActiveEffectHandle Handle;
	FGameplayTagQuery TagQuery;
	bool bAll = false;

	FArcActiveEffectHandle ExcludeHandle;
	int32 MaxCount = 0;
	EArcEffectRemovalOrder Order = EArcEffectRemovalOrder::OldestFirst;
	EArcEffectRemovalReason Reason = EArcEffectRemovalReason::Removed;
};

namespace ArcEffects
{
	namespace Private
	{
		ARCMASSABILITIES_API void RunExecutions(FMassEntityManager& EntityManager, FMassEntityHandle Target,
		                                         const TArray<FArcEffectExecutionEntry>& Executions,
		                                         const FArcEffectSpec& Spec,
		                                         const FSharedStruct& SpecStruct,
		                                         EArcEffectExecutionTrigger Trigger,
		                                         const FGameplayTagContainer& PassedInTags = FGameplayTagContainer());

		ARCMASSABILITIES_API void RunComponentHook(const TArray<FInstancedStruct>& Components,
		                                            TFunctionRef<void(const FArcEffectComponent&)> Func);

		ARCMASSABILITIES_API void RemoveModsFromAggregator(FArcAggregatorFragment& AggFragment,
		                                                    const TArray<FArcModifierHandle>& Handles);

		ARCMASSABILITIES_API void ExtractActiveEffects(TArray<FArcActiveEffect>& ActiveEffects,
		                                                TConstArrayView<int32> Indices,
		                                                TArray<FArcActiveEffect>& OutExtracted);

		ARCMASSABILITIES_API void QueueEffectExecuted(FMassEntityManager& EntityManager, FMassEntityHandle Target,
		                                               const FSharedStruct& SharedSpec, FArcActiveEffectHandle Handle);

		ARCMASSABILITIES_API void RebuildEffectMods(FArcAggregatorFragment& AggFragment, FArcActiveEffect& Effect);

		ARCMASSABILITIES_API const FArcEffectComp_Inhibition* FindInhibitionComponent(const FArcActiveEffect& Effect);

		ARCMASSABILITIES_API bool ShouldInhibitEffect(const FArcActiveEffect& Effect, const FArcTagCountContainer& OwnedTags);

		ARCMASSABILITIES_API void NotifyInhibitionChanged(FArcActiveEffect& Effect, bool bInhibited);
	}

	ARCMASSABILITIES_API FArcEffectContext MakeChildContext(const FArcEffectContext& Parent,
	                                                        const FGameplayTagContainer& ExtraOriginTags = FGameplayTagContainer(),
	                                                        const FGameplayTagContainer& ExcludedOriginTags = FGameplayTagContainer());

	ARCMASSABILITIES_API FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         UArcEffectDefinition* Effect, FMassEntityHandle Source,
	                                         const FInstancedStruct& SourceData = FInstancedStruct());

	ARCMASSABILITIES_API FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         UArcEffectDefinition* Effect, const FArcEffectContext& Context);

	ARCMASSABILITIES_API FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         const FArcEffectApplyRequest& Request);

	// The spec's target half is rebound to Target before immunity and pre-application; the source half stays as built.
	ARCMASSABILITIES_API FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         const FSharedStruct& Spec);

	ARCMASSABILITIES_API int32 RemoveEffects(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                         const FArcEffectRemovalQuery& Query);

	ARCMASSABILITIES_API int32 ConsumeStack(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                        FArcActiveEffectHandle Handle, int32 Count);

	ARCMASSABILITIES_API bool SetEffectDecayRate(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                             FArcActiveEffectHandle Handle, float Rate);

	ARCMASSABILITIES_API void CleanupActiveEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
	                                               FArcActiveEffect&& Effect, EArcEffectRemovalReason Reason);

	ARCMASSABILITIES_API FArcActiveEffect* FindActiveEffect(FArcEffectStackFragment& EffectStack, FArcActiveEffectHandle Handle);

	ARCMASSABILITIES_API float ResolveScalableMagnitude(const FArcScalableFloat& Mag, EArcRangeResolution Mode);
}
