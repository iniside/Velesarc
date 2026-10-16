// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "MassStateTreeSubsystem.h"
#include "StateTreeExecutionTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "ArcAbilityCollectionFragment.generated.h"

class UArcAbilityDefinition;

USTRUCT()
struct ARCMASSABILITIES_API FArcActiveAbility
{
    GENERATED_BODY()

    FArcAbilityHandle Handle;

    UPROPERTY()
    TObjectPtr<const UArcAbilityDefinition> Definition = nullptr;

    FGameplayTag InputTag;

    FMassEntityHandle SourceEntity;

    UPROPERTY(EditAnywhere)
    FArcAbilityCustomData CustomData;

    FGameplayTagContainer OriginTags;

    FMassStateTreeInstanceHandle InstanceHandle;

    double LastUpdateTimeInSeconds = 0.0;

    double NextWakeTimeInSeconds = -1.0;

    EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;

    bool bIsActive = false;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCollectionFragment : public FMassSparseFragment
{
    GENERATED_BODY()

    TArray<FArcActiveAbility> Abilities;

    int32 NextGeneration = 0;
};

template <>
struct TMassFragmentTraits<FArcAbilityCollectionFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
