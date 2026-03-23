// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"

#include "ArcConditionFragments.generated.h"

// ---------------------------------------------------------------------------
// Per-condition fragment, config, and tag definitions.
//
// Each condition gets:
//   FArc{Name}ConditionFragment  — per-entity mutable state (FArcConditionFragment)
//   FArc{Name}ConditionConfig    — per-archetype tuning (FMassConstSharedFragment)
//   FArc{Name}ConditionTag       — observer trigger tag (FMassTag)
//
// Processors query for specific fragments. Entities opt into only the
// conditions they can receive via individual traits.
// ---------------------------------------------------------------------------

/**
 * Base fragment for all conditions. Holds the shared FArcConditionState.
 * Named condition fragments inherit from this instead of FMassFragment directly.
 */
USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcConditionFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FArcConditionState State;
};

// ===========================================================================
// Group A: Hysteresis conditions
// ===========================================================================

//                          Name        Group                                  Thresh  Decay  Overload  BurnoutDur  BurnoutMult  BurnoutTarget
USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcBurningConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcBurningConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ShowOnlyInnerProperties))
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 20.f, 3.f, 6.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcBurningConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcBurningConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcBleedingConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcBleedingConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ShowOnlyInnerProperties))
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 25.f, 2.f, 8.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcBleedingConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcBleedingConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcChilledConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcChilledConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 30.f, 2.f, 5.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcChilledConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcChilledConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcShockedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcShockedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 30.f, 4.f, 5.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcShockedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcShockedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcPoisonedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcPoisonedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 20.f, 1.f, 5.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcPoisonedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcPoisonedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcDiseasedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcDiseasedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 25.f, 0.5f, 10.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcDiseasedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcDiseasedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcWeakenedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcWeakenedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupA_Hysteresis, 20.f, 2.f, 10.f, 2.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcWeakenedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcWeakenedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

// ===========================================================================
// Group B: Linear catalysts (no hysteresis, no overload by default)
// ===========================================================================

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcOiledConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcOiledConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupB_Linear, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcOiledConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcOiledConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcWetConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcWetConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupB_Linear, 0.f, 2.f, 0.f, 0.f, 1.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcWetConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcWetConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcCorrodedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcCorrodedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupB_Linear, 0.f, 0.5f, 0.f, 0.f, 1.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcCorrodedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcCorrodedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

// ===========================================================================
// Group C: Environmental
// ===========================================================================

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcBlindedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcBlindedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupC_Environmental, 0.f, 5.f, 4.f, 1.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcBlindedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcBlindedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcSuffocatingConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcSuffocatingConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupC_Environmental, 0.f, 10.f, 3.f, 1.f, 5.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcSuffocatingConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcSuffocatingConditionTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcExhaustedConditionFragment : public FArcConditionFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcExhaustedConditionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcConditionConfig Config = {EArcConditionGroup::GroupC_Environmental, 0.f, 3.f, 0.f, 0.f, 1.f, 0.f};
};

template <>
struct TMassFragmentTraits<FArcExhaustedConditionConfig> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcExhaustedConditionTag : public FMassTag
{
	GENERATED_BODY()
};

#undef ARC_DECLARE_CONDITION
