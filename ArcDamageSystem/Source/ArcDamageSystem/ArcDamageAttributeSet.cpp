// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcDamageAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectAggregator.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

#include "AbilitySystem/ArcGameplayAbilityActorInfo.h"
#include "ArcConditionEffects/ArcConditionFragments.h"
#include "ArcDamageResistanceData.h"
#include "ArcDamageResistanceMappingAsset.h"

// ---------------------------------------------------------------------------
// Constructor — register post-attribute handlers
// ---------------------------------------------------------------------------

UArcDamageAttributeSet::UArcDamageAttributeSet()
{
	MaxOutgoingDamageBonusPercent.SetBaseValue(1.f);
	MaxOutgoingDamageBonusPercent.SetCurrentValue(1.f);
	MaxOutgoingDamageReductionPercent.SetBaseValue(1.f);
	MaxOutgoingDamageReductionPercent.SetCurrentValue(1.f);
	MaxIncomingDamageBonusPercent.SetBaseValue(1.f);
	MaxIncomingDamageBonusPercent.SetCurrentValue(1.f);
	MaxDamageResistance.SetBaseValue(1.f);
	MaxDamageResistance.SetCurrentValue(1.f);

	ARC_REGISTER_PRE_ATTRIBUTE_HANDLER(OutgoingDamageBonusPercent);
	ARC_REGISTER_PRE_ATTRIBUTE_HANDLER(OutgoingDamageReductionPercent);
	ARC_REGISTER_PRE_ATTRIBUTE_HANDLER(IncomingDamageBonusPercent);
	ARC_REGISTER_PRE_ATTRIBUTE_HANDLER(DamageResistance);

	ARC_REGISTER_POST_ATTRIBUTE_HANDLER(DamageResistance);
}

// ---------------------------------------------------------------------------
// Pre-attribute handlers — clamp percentage attributes to [0, Max]
// ---------------------------------------------------------------------------

ARC_DECLARE_PRE_ATTRIBUTE_HANDLER(UArcDamageAttributeSet, OutgoingDamageBonusPercent)
{
	return [this](const FGameplayAttribute& Attribute, float& NewValue)
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxOutgoingDamageBonusPercent());
	};
}

ARC_DECLARE_PRE_ATTRIBUTE_HANDLER(UArcDamageAttributeSet, OutgoingDamageReductionPercent)
{
	return [this](const FGameplayAttribute& Attribute, float& NewValue)
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxOutgoingDamageReductionPercent());
	};
}

ARC_DECLARE_PRE_ATTRIBUTE_HANDLER(UArcDamageAttributeSet, IncomingDamageBonusPercent)
{
	return [this](const FGameplayAttribute& Attribute, float& NewValue)
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxIncomingDamageBonusPercent());
	};
}

ARC_DECLARE_PRE_ATTRIBUTE_HANDLER(UArcDamageAttributeSet, DamageResistance)
{
	return [this](const FGameplayAttribute& Attribute, float& NewValue)
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxDamageResistance());
	};
}

// ---------------------------------------------------------------------------
// Post-attribute handler for DamageResistance
// ---------------------------------------------------------------------------

ARC_DECLARE_POST_ATTRIBUTE_HANDLER(UArcDamageAttributeSet, DamageResistance)
{
	return [this](const FGameplayAttribute& Attribute, float OldValue, float NewValue)
	{
		SyncConditionResistances();
	};
}

// ---------------------------------------------------------------------------
// SyncConditionResistances — resistance bridge core logic
// ---------------------------------------------------------------------------

void UArcDamageAttributeSet::SyncConditionResistances()
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (!CachedMappingAsset)
	{
		if (ArcASC.IsValid())
		{
			const FArcDamageResistanceData* ResistanceData = ArcASC->FindAbilitySystemData<FArcDamageResistanceData>();
			CachedMappingAsset = ResistanceData ? ResistanceData->ResistanceMappingAsset : nullptr;
		}
	}
	if (!CachedMappingAsset || CachedMappingAsset->Mappings.IsEmpty())
	{
		return;
	}

	// Guard against simulated proxies — capture specs require authority/autonomous.
	AActor* Avatar = ASC->GetAvatarActor();
	if (!Avatar || Avatar->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}

	const FArcGameplayAbilityActorInfo* ActorInfo =
		static_cast<const FArcGameplayAbilityActorInfo*>(ASC->AbilityActorInfo.Get());
	if (!ActorInfo)
	{
		return;
	}

	const FMassEntityHandle EntityHandle = ActorInfo->EntityHandle;
	if (!EntityHandle.IsValid())
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = ActorInfo->MassEntitySubsystem;
	if (!EntitySubsystem)
	{
		return;
	}

	const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();

	// Cache the capture spec on first use.
	if (!CachedResistanceCaptureSpec.IsSet())
	{
		CachedResistanceCaptureSpec = GetDamageResistanceCaptureSpecTarget(ASC);
	}

	FGameplayEffectAttributeCaptureSpec& CaptureSpec = CachedResistanceCaptureSpec.GetValue();

	// Iterate each mapping entry: evaluate per-tag resistance and write to fragment.
	for (const FArcDamageConditionMapping& Mapping : CachedMappingAsset->Mappings)
	{
		if (!Mapping.DamageTypeTag.IsValid() || !Mapping.ConditionFragmentType)
		{
			continue;
		}

		// Build evaluate params with the damage-type tag as the source filter.
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(Mapping.DamageTypeTag);

		FAggregatorEvaluateParameters EvalParams;
		EvalParams.SourceTags = &TagContainer;

		float ResistanceValue = 0.f;
		CaptureSpec.AttemptCalculateAttributeMagnitude(EvalParams, ResistanceValue);

		// Retrieve the fragment for this entity and write Resistance.
		FStructView FragmentView = EntityManager.GetFragmentDataStruct(EntityHandle, Mapping.ConditionFragmentType);
		if (!FragmentView.IsValid())
		{
			continue;
		}

		// All condition fragments inherit from FArcConditionFragment which holds State.
		FArcConditionFragment* Fragment = FragmentView.GetPtr<FArcConditionFragment>();
		Fragment->State.Resistance = FMath::Clamp(ResistanceValue, 0.f, 1.f);
	}
}
