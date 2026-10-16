// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Fragments/ArcAbilityCustomData.h"

struct FMassEntityManager;
struct FMassExecutionContext;
struct FArcActiveAbility;
struct FArcEffectContext;
struct FArcCostCheckResult;
struct FArcCooldownCheckResult;
class UArcAbilityDefinition;
class UArcGameEventSubsystem;
class UArcMassAbilitySet;
class UArcAbilityStateTreeSubsystem;
enum class EArcAbilityEndReason : uint8;

namespace ArcAbilities
{
    namespace Private
    {
        // Stops the ability's StateTree (running ExitState on its tasks), frees the instance
        // data, and clears the handle. No-op when the instance handle is invalid.
        //
        // Stop() runs WITHOUT SetContextRequirements — identical to the shipped processor Stop
        // paths (tick-processor teardown, input-processor cancel). ExitState tasks must
        // therefore tolerate missing external context data (null-safe reads, not checked
        // getters).
        ARCMASSABILITIES_API void StopAndFreeAbilityTree(FArcActiveAbility& Ability,
                                                         UArcAbilityStateTreeSubsystem& Subsystem,
                                                         FMassEntityHandle Entity,
                                                         FMassExecutionContext& MassContext);

        ARCMASSABILITIES_API void QueueAbilityEnded(FMassEntityManager& EntityManager,
                                                    FMassEntityHandle Entity,
                                                    const FArcActiveAbility& Ability,
                                                    EArcAbilityEndReason Reason,
                                                    FMassEntityHandle Instigator);

        ARCMASSABILITIES_API void QueueAbilityActivated(UArcGameEventSubsystem& EventBus,
                                                        FMassEntityManager& EntityManager,
                                                        FMassEntityHandle Entity,
                                                        FArcAbilityHandle Handle);

        ARCMASSABILITIES_API void QueueCostFailed(UArcGameEventSubsystem& EventBus,
                                                  FMassEntityManager& EntityManager,
                                                  FMassEntityHandle Entity,
                                                  FArcAbilityHandle Handle,
                                                  const FArcCostCheckResult& Result);

        ARCMASSABILITIES_API void QueueCooldownFailed(UArcGameEventSubsystem& EventBus,
                                                      FMassEntityManager& EntityManager,
                                                      FMassEntityHandle Entity,
                                                      FArcAbilityHandle Handle,
                                                      const FArcCooldownCheckResult& Result);

        ARCMASSABILITIES_API void QueueCooldownExpired(UArcGameEventSubsystem& EventBus,
                                                       FMassEntityManager& EntityManager,
                                                       FMassEntityHandle Entity,
                                                       FArcAbilityHandle Handle);

        ARCMASSABILITIES_API void CancelActiveAbility(FMassEntityManager& EntityManager,
                                                      FMassEntityHandle Entity,
                                                      FArcActiveAbility& Ability,
                                                      UArcAbilityStateTreeSubsystem& Subsystem,
                                                      FMassExecutionContext& MassContext,
                                                      EArcAbilityEndReason Reason,
                                                      FMassEntityHandle Instigator);
    }

    ARCMASSABILITIES_API void AddOwnedTags(FMassEntityManager& EntityManager,
                                           FMassEntityHandle Entity,
                                           const FGameplayTagContainer& InTags);

    ARCMASSABILITIES_API void RemoveOwnedTags(FMassEntityManager& EntityManager,
                                              FMassEntityHandle Entity,
                                              const FGameplayTagContainer& InTags);

    ARCMASSABILITIES_API FArcAbilityHandle GrantAbility(FMassEntityManager& EntityManager,
                                   FMassEntityHandle Entity,
                                   const UArcAbilityDefinition* Definition,
                                   FGameplayTag InputTag,
                                   FMassEntityHandle SourceEntity = FMassEntityHandle(),
                                   FArcAbilityCustomData CustomData = FArcAbilityCustomData(),
                                   UWorld* World = nullptr);

    ARCMASSABILITIES_API void RemoveAbility(FMassEntityManager& EntityManager,
                       FMassEntityHandle Entity,
                       FArcAbilityHandle Handle,
                       UArcAbilityStateTreeSubsystem& Subsystem);

    ARCMASSABILITIES_API TArray<FArcAbilityHandle> GrantAbilitySet(FMassEntityManager& EntityManager,
                                              FMassEntityHandle Entity,
                                              const UArcMassAbilitySet* AbilitySet,
                                              UWorld* World = nullptr);

    ARCMASSABILITIES_API void RemoveAbilitySet(FMassEntityManager& EntityManager,
                          FMassEntityHandle Entity,
                          const TArray<FArcAbilityHandle>& Handles,
                          UArcAbilityStateTreeSubsystem& Subsystem);

    ARCMASSABILITIES_API const FArcActiveAbility* FindAbility(FMassEntityManager& EntityManager,
                                                FMassEntityHandle Entity,
                                                FArcAbilityHandle Handle);

    ARCMASSABILITIES_API FArcEffectContext MakeAbilityEffectContext(FMassEntityManager& EntityManager,
                                                                    FMassEntityHandle Entity,
                                                                    FArcAbilityHandle Handle);

    ARCMASSABILITIES_API FArcEffectContext MakeAbilityEffectContext(FMassEntityHandle Entity,
                                                                    const FArcActiveAbility& Ability);

    ARCMASSABILITIES_API bool IsAbilityActive(FMassEntityManager& EntityManager,
                         FMassEntityHandle Entity,
                         FArcAbilityHandle Handle);

    ARCMASSABILITIES_API bool TryActivateAbility(FMassEntityManager& EntityManager,
                            FMassEntityHandle Entity,
                            FArcAbilityHandle Handle);

    ARCMASSABILITIES_API void CancelAbility(FMassEntityManager& EntityManager,
                       FMassEntityHandle Entity,
                       FArcAbilityHandle Handle,
                       UArcAbilityStateTreeSubsystem& Subsystem,
                       EArcAbilityEndReason Reason,
                       FMassEntityHandle Instigator);

    ARCMASSABILITIES_API void PushInput(FMassEntityManager& EntityManager,
                   FMassEntityHandle Entity,
                   FGameplayTag InputTag,
                   const FGameplayTagContainer& OriginTags = FGameplayTagContainer());

    ARCMASSABILITIES_API void ReleaseInput(FMassEntityManager& EntityManager,
                      FMassEntityHandle Entity,
                      FGameplayTag InputTag);

    ARCMASSABILITIES_API TOptional<FArcAbilityHandle> TryGrantAbilitySafe(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const UArcAbilityDefinition* Definition,
        FGameplayTag InputTag,
        FMassEntityHandle SourceEntity = FMassEntityHandle(),
        FArcAbilityCustomData CustomData = FArcAbilityCustomData());

    ARCMASSABILITIES_API bool TryRemoveAbilitySafe(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcAbilityHandle Handle);
}
