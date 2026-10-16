// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
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

// Edge-detector state for input-listen StateTree tasks. Tracks the last observed
// held-state of an input tag so a Tick can identify the rising or falling edge.
UENUM()
enum class EArcInputEdgeState : uint8
{
    NotObserved,
    Released,
    Pressed,
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityInputFragment : public FMassSparseFragment
{
    GENERATED_BODY()

    // Activation candidates — consumed and cleared by UArcAbilityInputProcessor.
    TArray<FGameplayTag> PressedThisFrame;
    TArray<FGameplayTag> ReleasedThisFrame;
    TMap<FGameplayTag, FGameplayTagContainer> PressedOriginTags;

    FGameplayTagContainer HeldInputs;
};

template <>
struct TMassFragmentTraits<FArcAbilityInputFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
