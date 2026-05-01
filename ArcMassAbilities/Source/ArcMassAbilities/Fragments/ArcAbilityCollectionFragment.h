// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "MassStateTreeSubsystem.h"
#include "StateTreeExecutionTypes.h"
#include "StructUtils/InstancedStruct.h"
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
    FInstancedStruct SourceData;

    FMassStateTreeInstanceHandle InstanceHandle;

    double LastUpdateTimeInSeconds = 0.0;

    EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;

    bool bIsActive = false;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCollectionFragment : public FMassFragment
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
