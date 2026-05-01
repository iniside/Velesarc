// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

struct FMassEntityManager;
struct FArcActiveAbility;
class UArcAbilityDefinition;
class UArcMassAbilitySet;
class UArcAbilityStateTreeSubsystem;

namespace ArcAbilities
{
    ARCMASSABILITIES_API FArcAbilityHandle GrantAbility(FMassEntityManager& EntityManager,
                                   FMassEntityHandle Entity,
                                   const UArcAbilityDefinition* Definition,
                                   FGameplayTag InputTag,
                                   FMassEntityHandle SourceEntity = FMassEntityHandle(),
                                   const FInstancedStruct& SourceData = FInstancedStruct());

    ARCMASSABILITIES_API void RemoveAbility(FMassEntityManager& EntityManager,
                       FMassEntityHandle Entity,
                       FArcAbilityHandle Handle,
                       UArcAbilityStateTreeSubsystem& Subsystem);

    ARCMASSABILITIES_API TArray<FArcAbilityHandle> GrantAbilitySet(FMassEntityManager& EntityManager,
                                              FMassEntityHandle Entity,
                                              const UArcMassAbilitySet* AbilitySet);

    ARCMASSABILITIES_API void RemoveAbilitySet(FMassEntityManager& EntityManager,
                          FMassEntityHandle Entity,
                          const TArray<FArcAbilityHandle>& Handles,
                          UArcAbilityStateTreeSubsystem& Subsystem);

    ARCMASSABILITIES_API const FArcActiveAbility* FindAbility(FMassEntityManager& EntityManager,
                                                FMassEntityHandle Entity,
                                                FArcAbilityHandle Handle);

    ARCMASSABILITIES_API bool IsAbilityActive(FMassEntityManager& EntityManager,
                         FMassEntityHandle Entity,
                         FArcAbilityHandle Handle);

    ARCMASSABILITIES_API bool TryActivateAbility(FMassEntityManager& EntityManager,
                            FMassEntityHandle Entity,
                            FArcAbilityHandle Handle);

    ARCMASSABILITIES_API void CancelAbility(FMassEntityManager& EntityManager,
                       FMassEntityHandle Entity,
                       FArcAbilityHandle Handle,
                       UArcAbilityStateTreeSubsystem& Subsystem);

    ARCMASSABILITIES_API void PushInput(FMassEntityManager& EntityManager,
                   FMassEntityHandle Entity,
                   FGameplayTag InputTag);

    ARCMASSABILITIES_API void ReleaseInput(FMassEntityManager& EntityManager,
                      FMassEntityHandle Entity,
                      FGameplayTag InputTag);

    ARCMASSABILITIES_API TOptional<FArcAbilityHandle> TryGrantAbilitySafe(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const UArcAbilityDefinition* Definition,
        FGameplayTag InputTag,
        FMassEntityHandle SourceEntity = FMassEntityHandle(),
        const FInstancedStruct& SourceData = FInstancedStruct());

    ARCMASSABILITIES_API bool TryRemoveAbilitySafe(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcAbilityHandle Handle);
}
