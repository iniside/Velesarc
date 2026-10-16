// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsTags.h"

namespace ArcMassMechanicsTags
{
	UE_DEFINE_GAMEPLAY_TAG(Condition_Burning, "ArcCondition.Burning");
	UE_DEFINE_GAMEPLAY_TAG(Condition_Bleeding, "ArcCondition.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(Condition_Oiled, "ArcCondition.Oiled");
	UE_DEFINE_GAMEPLAY_TAG(Condition_Weakened, "ArcCondition.Weakened");
	UE_DEFINE_GAMEPLAY_TAG(Condition_Blinded, "ArcCondition.Blinded");
	UE_DEFINE_GAMEPLAY_TAG(Condition_Crippled, "ArcCondition.Crippled");
	UE_DEFINE_GAMEPLAY_TAG(Condition_Dazed, "ArcCondition.Dazed");

	UE_DEFINE_GAMEPLAY_TAG(State_Stun, "ArcState.Stun");
	UE_DEFINE_GAMEPLAY_TAG(State_Stun_Frozen, "ArcState.Stun.Frozen");
	UE_DEFINE_GAMEPLAY_TAG(State_Knockdown, "ArcState.Knockdown");
	UE_DEFINE_GAMEPLAY_TAG(State_Root, "ArcState.Root");
	UE_DEFINE_GAMEPLAY_TAG(State_Silence, "ArcState.Silence");
	UE_DEFINE_GAMEPLAY_TAG(State_Fear, "ArcState.Fear");
	UE_DEFINE_GAMEPLAY_TAG(State_Sleep, "ArcState.Sleep");

	UE_DEFINE_GAMEPLAY_TAG(EffectType_Condition, "ArcEffect.Type.Condition");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Hex, "ArcEffect.Type.Hex");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Enchantment, "ArcEffect.Type.Enchantment");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Boon, "ArcEffect.Type.Boon");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Poison, "ArcEffect.Type.Poison");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Disease, "ArcEffect.Type.Disease");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Injury, "ArcEffect.Type.Injury");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Curse, "ArcEffect.Type.Curse");
	UE_DEFINE_GAMEPLAY_TAG(EffectType_Blessing, "ArcEffect.Type.Blessing");



	UE_DEFINE_GAMEPLAY_TAG(SBC_TrueDamage, "ArcMech.SBC.TrueDamage");
	UE_DEFINE_GAMEPLAY_TAG(SBC_TrueHeal, "ArcMech.SBC.TrueHeal");
	UE_DEFINE_GAMEPLAY_TAG(HitZone, "ArcMech.Hit.Zone");

	UE_DEFINE_GAMEPLAY_TAG(HitOutcome, "ArcMech.Hit.Outcome");
	UE_DEFINE_GAMEPLAY_TAG(HitOutcome_Hit, "ArcMech.Hit.Outcome.Hit");
	UE_DEFINE_GAMEPLAY_TAG(HitOutcome_Miss, "ArcMech.Hit.Outcome.Miss");
	UE_DEFINE_GAMEPLAY_TAG(HitOutcome_Dodge, "ArcMech.Hit.Outcome.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(HitOutcome_Parry, "ArcMech.Hit.Outcome.Parry");
	UE_DEFINE_GAMEPLAY_TAG(HitOutcome_Block, "ArcMech.Hit.Outcome.Block");

	UE_DEFINE_GAMEPLAY_TAG(DamageType, "ArcEffect.Damage");
	UE_DEFINE_GAMEPLAY_TAG(DamageDealt, "ArcMech.Damage.Dealt");
	UE_DEFINE_GAMEPLAY_TAG(DamageCrit, "ArcMech.Damage.Crit");

	UE_DEFINE_GAMEPLAY_TAG(TriggerOrigin, "ArcMech.Trigger.Origin");
}
