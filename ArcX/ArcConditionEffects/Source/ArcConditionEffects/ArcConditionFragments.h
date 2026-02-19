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
ARC_DECLARE_CONDITION(Burning,   EArcConditionGroup::GroupA_Hysteresis,        20.f,   3.f,   6.f,      2.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Bleeding,  EArcConditionGroup::GroupA_Hysteresis,        25.f,   2.f,   8.f,      2.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Chilled,   EArcConditionGroup::GroupA_Hysteresis,        30.f,   2.f,   5.f,      2.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Shocked,   EArcConditionGroup::GroupA_Hysteresis,        30.f,   4.f,   5.f,      2.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Poisoned,  EArcConditionGroup::GroupA_Hysteresis,        20.f,   1.f,   5.f,      2.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Diseased,  EArcConditionGroup::GroupA_Hysteresis,        25.f,   0.5f,  10.f,     2.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Weakened,  EArcConditionGroup::GroupA_Hysteresis,        20.f,   2.f,   10.f,     2.f,        5.f,         0.f)

// ===========================================================================
// Group B: Linear catalysts (no hysteresis, no overload by default)
// ===========================================================================

ARC_DECLARE_CONDITION(Oiled,     EArcConditionGroup::GroupB_Linear,            0.f,    1.f,   0.f,      0.f,        1.f,         0.f)
ARC_DECLARE_CONDITION(Wet,       EArcConditionGroup::GroupB_Linear,            0.f,    2.f,   0.f,      0.f,        1.f,         0.f)
ARC_DECLARE_CONDITION(Corroded,  EArcConditionGroup::GroupB_Linear,            0.f,    0.5f,  0.f,      0.f,        1.f,         0.f)

// ===========================================================================
// Group C: Environmental
// ===========================================================================

ARC_DECLARE_CONDITION(Blinded,      EArcConditionGroup::GroupC_Environmental,  0.f,    5.f,   4.f,      1.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Suffocating,  EArcConditionGroup::GroupC_Environmental,  0.f,    10.f,  3.f,      1.f,        5.f,         0.f)
ARC_DECLARE_CONDITION(Exhausted,    EArcConditionGroup::GroupC_Environmental,  0.f,    3.f,   0.f,      0.f,        1.f,         0.f)

#undef ARC_DECLARE_CONDITION
