// Copyright Lukasz Baran. All Rights Reserved.

#include "Actions/ArcMassAction_ApplyEffects.h"

#include "ArcMass/Projectile/ArcProjectileFragments.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassAction_ApplyEffects)

bool FArcMassAction_ApplyEffects::Prepare(const FArcMassActionPreparationContext& Context)
{
	PreparedEffects.Reset();

	const FArcEffectContext* Payload = Context.Payload.GetPtr<FArcEffectContext>();
	if (!Payload)
	{
		return false;
	}

	for (const TObjectPtr<UArcEffectDefinition>& Effect : Effects)
	{
		if (!Effect)
		{
			return false;
		}
	}

	PreparedEffects.Reserve(Effects.Num());
	for (const TObjectPtr<UArcEffectDefinition>& Effect : Effects)
	{
		FArcEffectContext EffectContext = *Payload;
		EffectContext.EntityManager = &Context.EntityManager;
		EffectContext.TargetEntity = Context.TargetEntity;
		EffectContext.SourceEntity = Context.SourceEntity;
		EffectContext.EffectDefinition = Effect;
		EffectContext.StackCount = 1;

		PreparedEffects.Add(ArcEffects::MakeSpec(Effect, EffectContext));
	}

	return true;
}

void FArcMassAction_ApplyEffects::Execute(FArcMassActionContext& Context)
{
	const FArcCollisionHitContext* HitContext = Context.EventData.GetPtr<FArcCollisionHitContext>();
	if (!HitContext || HitContext->bExpired)
	{
		return;
	}

	for (const FSharedStruct& PreparedEffect : PreparedEffects)
	{
		ArcEffects::TryApplyEffect(Context.EntityManager, HitContext->TargetEntity, PreparedEffect);
	}
}
