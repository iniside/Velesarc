// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Statuses/ArcStatusTasks.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "Attributes/ArcInstantModifier.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Modifiers/ArcModifierFunctions.h"

namespace ArcStatusTasksPrivate
{
	FMassEntityManager* GetEntityManager(const UWorld* World)
	{
		UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
	}

	void RemoveEffectDeferred(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle Handle)
	{
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>([Owner, Handle](FMassEntityManager& DeferredManager)
		{
			FArcEffectRemovalQuery Query;
			Query.Handle = Handle;
			Query.Reason = EArcEffectRemovalReason::Removed;
			ArcEffects::RemoveEffects(DeferredManager, Owner, Query);
		});
	}
}

float FArcEffectTask_Upkeep::GetAmount(const FArcActiveEffect& OwningEffect) const
{
	return Amount;
}

void FArcEffectTask_Upkeep::OnExecute(FArcActiveEffect& OwningEffect)
{
	if (!OwningEffect.Handle.IsValid())
	{
		return;
	}

	FMassEntityManager* EntityManager = ArcStatusTasksPrivate::GetEntityManager(OwningEffect.OwnerWorld);
	if (!EntityManager)
	{
		return;
	}

	const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
	const FMassEntityHandle PayerEntity = Payer == EArcUpkeepPayer::Source
		? (Spec ? Spec->Context.SourceEntity : FMassEntityHandle())
		: OwningEffect.OwnerEntity;

	const float Required = GetAmount(OwningEffect);
	const FArcAttribute* ResourceAttribute = EntityManager->IsEntityValid(PayerEntity)
		? ArcAttributes::ResolveAttribute(*EntityManager, PayerEntity, Resource)
		: nullptr;
	const FArcAttribute* MetaAttribute = ResourceAttribute
		? ArcAttributes::ResolveAttribute(*EntityManager, PayerEntity, Meta)
		: nullptr;

	if (!ResourceAttribute || !MetaAttribute || ResourceAttribute->GetCurrentValue() < Required)
	{
		UE_LOGF(LogArcMassMechanics, Verbose, "Upkeep %ls payer %ls: unsatisfied, needs %.3f of %ls, has %.3f; removing the effect",
			*GetNameSafe(Spec ? Spec->Definition.Get() : nullptr), *PayerEntity.DebugGetDescription(), Required, *Resource.PropertyName.ToString(),
			ResourceAttribute ? ResourceAttribute->GetCurrentValue() : 0.f);
		ArcStatusTasksPrivate::RemoveEffectDeferred(*EntityManager, OwningEffect.OwnerEntity, OwningEffect.Handle);
		return;
	}

	UE_LOGF(LogArcMassMechanics, Verbose, "Upkeep %ls payer %ls: paid %.3f of %ls (had %.3f)",
		*GetNameSafe(Spec ? Spec->Definition.Get() : nullptr), *PayerEntity.DebugGetDescription(), Required, *Resource.PropertyName.ToString(),
		ResourceAttribute->GetCurrentValue());
	if (Required > 0.f)
	{
		ArcAttributes::EnqueueInstantModifier(*EntityManager, PayerEntity, Meta, EArcModifierOp::Add, Required,
			EArcAttributeOpType::Execute, OwningEffect.OwnerEntity);
	}
}
