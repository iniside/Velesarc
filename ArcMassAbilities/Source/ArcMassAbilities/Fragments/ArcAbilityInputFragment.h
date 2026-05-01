// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcAbilityInputFragment.generated.h"

UENUM()
enum class EArcInputState : uint8
{
    None,
    Pressed,
    Held,
    Released
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityInputFragment : public FMassFragment
{
    GENERATED_BODY()

    TArray<FGameplayTag> PressedThisFrame;
    TArray<FGameplayTag> ReleasedThisFrame;
    FGameplayTagContainer HeldInputs;
};

template <>
struct TMassFragmentTraits<FArcAbilityInputFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
