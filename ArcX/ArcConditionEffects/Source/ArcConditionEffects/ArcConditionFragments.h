// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"

#include "ArcConditionFragments.generated.h"

// ---------------------------------------------------------------------------
// Per-condition fragment, config, and tag definitions.
//
// Each condition gets:
//   FArc{Name}ConditionFragment  — per-entity mutable state (FMassFragment)
//   FArc{Name}ConditionConfig    — per-archetype tuning (FMassConstSharedFragment)
//   FArc{Name}ConditionTag       — observer trigger tag (FMassTag)
//
// Processors query for specific fragments. Entities opt into only the
// conditions they can receive via individual traits.
// ---------------------------------------------------------------------------

// ===== MACRO: Declares a condition fragment + config + tag =================

#define ARC_DECLARE_CONDITION(Name, DefaultGroup, DefaultThreshold, DefaultDecay, DefaultOverload, DefaultBurnoutDur, DefaultBurnoutMult, DefaultBurnoutTarget) \
	USTRUCT() \
	struct ARCCONDITIONEFFECTS_API FArc##Name##ConditionFragment : public FMassFragment \
	{ \
		GENERATED_BODY() \
		FArcConditionState State; \
	}; \
	\
	USTRUCT(BlueprintType) \
	struct ARCCONDITIONEFFECTS_API FArc##Name##ConditionConfig : public FMassConstSharedFragment \
	{ \
		GENERATED_BODY() \
		UPROPERTY(EditAnywhere, Category = "Condition") \
		FArcConditionConfig Config = { DefaultGroup, DefaultThreshold, DefaultDecay, DefaultOverload, DefaultBurnoutDur, DefaultBurnoutMult, DefaultBurnoutTarget }; \
	}; \
	\
	template<> struct TMassFragmentTraits<FArc##Name##ConditionConfig> final \
	{ enum { AuthorAcceptsItsNotTriviallyCopyable = true }; }; \
	\
	USTRUCT() \
	struct ARCCONDITIONEFFECTS_API FArc##Name##ConditionTag : public FMassTag \
	{ \
		GENERATED_BODY() \
	};

// ===========================================================================
// Group A: Hysteresis conditions
// ===========================================================================

//                          Name        Group                                  Thresh  Decay  Overload  BurnoutDur  BurnoutMult  BurnoutTarget
USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcBurningConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcBleedingConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcChilledConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcShockedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcPoisonedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcDiseasedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcWeakenedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcOiledConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcWetConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcCorrodedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcBlindedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcSuffocatingConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
struct ARCCONDITIONEFFECTS_API FArcExhaustedConditionFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState State;
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
