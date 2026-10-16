// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcGameEventTags.h"

namespace ArcGameEventTags
{
	UE_DEFINE_GAMEPLAY_TAG(EffectApplied,  "ArcEvent.Effect.Applied");
	UE_DEFINE_GAMEPLAY_TAG(EffectExecuted, "ArcEvent.Effect.Executed");
	UE_DEFINE_GAMEPLAY_TAG(EffectRemoved,  "ArcEvent.Effect.Removed");
	UE_DEFINE_GAMEPLAY_TAG(EffectRejected, "ArcEvent.Effect.Rejected");
	UE_DEFINE_GAMEPLAY_TAG(EffectRejectedImmunity, "ArcEvent.Effect.Rejected.Immunity");
	UE_DEFINE_GAMEPLAY_TAG(EffectRejectedGate,     "ArcEvent.Effect.Rejected.Gate");
	UE_DEFINE_GAMEPLAY_TAG(EffectRejectedDenyNew,  "ArcEvent.Effect.Rejected.DenyNew");
	UE_DEFINE_GAMEPLAY_TAG(EffectRejectedOverflow, "ArcEvent.Effect.Rejected.Overflow");
	UE_DEFINE_GAMEPLAY_TAG(EffectRejectedChance,   "ArcEvent.Effect.Rejected.Chance");
	UE_DEFINE_GAMEPLAY_TAG(AbilityActivated,       "ArcEvent.Ability.Activated");
	UE_DEFINE_GAMEPLAY_TAG(AbilityEnded,           "ArcEvent.Ability.Ended");
	UE_DEFINE_GAMEPLAY_TAG(AbilityCostFailed,      "ArcEvent.Ability.CostFailed");
	UE_DEFINE_GAMEPLAY_TAG(AbilityCooldownFailed,  "ArcEvent.Ability.CooldownFailed");
	UE_DEFINE_GAMEPLAY_TAG(AbilityCooldownExpired, "ArcEvent.Ability.CooldownExpired");
	UE_DEFINE_GAMEPLAY_TAG(TagAdded,               "ArcEvent.Tag.Added");
	UE_DEFINE_GAMEPLAY_TAG(TagRemoved,             "ArcEvent.Tag.Removed");
	UE_DEFINE_GAMEPLAY_TAG(AttributeChanged,       "ArcEvent.Attribute.Changed");
}
