// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsEditor/Validation/ArcTriggerValidator.h"

#include "ArcMassMechanics/Triggers/ArcTriggerIf.h"
#include "ArcMassMechanics/Triggers/ArcTriggerOn.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcMassMechanics/Triggers/ArcTriggerWhile.h"
#include "Effects/ArcEffectDefinition.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcTriggerValidator"

namespace ArcTriggerValidatorPrivate
{
	bool IsTriggerTask(const FInstancedStruct& Task)
	{
		return Task.GetPtr<FArcEffectTask_TriggerOn>() || Task.GetPtr<FArcEffectTask_TriggerWhile>() || Task.GetPtr<FArcEffectTask_TriggerIf>();
	}

	void ValidateCharges(EArcTriggerChargeMode ChargeMode, int32 Charges, TArray<FText>& OutErrors)
	{
		if (ChargeMode == EArcTriggerChargeMode::Trigger && Charges < 1)
		{
			OutErrors.Add(LOCTEXT("TriggerChargesNone", "Charge Mode Trigger needs at least one charge."));
		}
	}

	template<typename TValidate>
	void ValidateConditions(TConstArrayView<FInstancedStruct> Conditions, TArray<FText>& OutErrors, TValidate&& Validate)
	{
		for (const FInstancedStruct& ConditionStruct : Conditions)
		{
			if (const FArcTriggerCondition* Condition = ConditionStruct.GetPtr<FArcTriggerCondition>())
			{
				Validate(*Condition);
			}
			else
			{
				OutErrors.Add(LOCTEXT("EmptyCondition", "A condition entry is empty."));
			}
		}
	}

	template<typename TValidate>
	void ValidateEffects(TConstArrayView<FInstancedStruct> Effects, TArray<FText>& OutErrors, TValidate&& Validate)
	{
		for (const FInstancedStruct& EffectStruct : Effects)
		{
			if (const FArcTriggerEffect* Effect = EffectStruct.GetPtr<FArcTriggerEffect>())
			{
				Validate(*Effect);
			}
			else
			{
				OutErrors.Add(LOCTEXT("EmptyEffect", "An effect entry is empty."));
			}
		}
	}

	void ValidateOn(const FArcEffectTask_TriggerOn& Task, TArray<FText>& OutErrors)
	{
		if (Task.Chance <= 0.f || Task.Chance > 1.f)
		{
			OutErrors.Add(LOCTEXT("OnChance", "An On trigger needs an authored Chance in (0, 1]."));
		}
		const bool bTagChannel = ArcTriggers::IsTagChangeChannel(Task.EventTag);
		if (bTagChannel && !Task.Tag.IsValid())
		{
			OutErrors.Add(LOCTEXT("OnTagMissing", "An On trigger on ArcEvent.Tag.Added or ArcEvent.Tag.Removed needs Tag: the owned tag whose gain or loss fires it."));
		}
		if (!bTagChannel && Task.Tag.IsValid())
		{
			OutErrors.Add(LOCTEXT("OnTagUnused", "Tag is read only with EventTag ArcEvent.Tag.Added or ArcEvent.Tag.Removed; clear it."));
		}
		ValidateCharges(Task.ChargeMode, Task.Charges, OutErrors);
		ValidateConditions(Task.Conditions, OutErrors, [&OutErrors](const FArcTriggerCondition& Condition) { Condition.ValidateInOn(OutErrors); });
		ValidateEffects(Task.Effects, OutErrors, [&OutErrors](const FArcTriggerEffect& Effect) { Effect.ValidateInOn(OutErrors); });
	}

	void ValidateWhile(const FArcEffectTask_TriggerWhile& Task, TArray<FText>& OutErrors)
	{
		if (Task.Conditions.IsEmpty())
		{
			OutErrors.Add(LOCTEXT("WhileNoConditions", "A While trigger needs at least one condition."));
		}
		ValidateConditions(Task.Conditions, OutErrors, [&OutErrors](const FArcTriggerCondition& Condition) { Condition.ValidateInWhile(OutErrors); });
		ValidateEffects(Task.Effects, OutErrors, [&OutErrors](const FArcTriggerEffect& Effect) { Effect.ValidateInWhile(OutErrors); });
	}

	void ValidateIf(const FArcEffectTask_TriggerIf& Task, TArray<FText>& OutErrors)
	{
		ValidateCharges(Task.ChargeMode, Task.Charges, OutErrors);

		const bool bHasEffect = Task.Effects.ContainsByPredicate([](const FInstancedStruct& Effect)
		{
			return Effect.GetPtr<FArcTriggerEffect>() != nullptr;
		});
		if (!bHasEffect && Task.Modifiers.IsEmpty())
		{
			OutErrors.Add(LOCTEXT("IfNothing", "An If trigger needs Modifiers or Effects."));
		}

		bool bHasCritChance = false;
		for (const FArcIfModifier& Modifier : Task.Modifiers)
		{
			bHasCritChance |= Modifier.Channel == EArcIfChannel::CritChance;

			const bool bTaken = Modifier.Channel == EArcIfChannel::DamageTakenMultiplier || Modifier.Channel == EArcIfChannel::DamageTakenFlat;
			if (!bTaken && Task.Role == EArcEventRole::Target)
			{
				OutErrors.Add(LOCTEXT("DoneOnTarget", "A Done channel needs the owner to be the hit's source; Role Target never applies it. Use Instigator or Any."));
			}
			if (bTaken && Task.Role == EArcEventRole::Instigator)
			{
				OutErrors.Add(LOCTEXT("TakenOnInstigator", "A Taken channel needs the owner to be the struck target; Role Instigator never applies it. Use Target or Any."));
			}

			const bool bMultiplier = Modifier.Channel == EArcIfChannel::DamageDoneMultiplier || Modifier.Channel == EArcIfChannel::DamageTakenMultiplier;
			if (bMultiplier && Modifier.Value < -1.f)
			{
				OutErrors.Add(LOCTEXT("MultiplierBelowMinusOne", "A multiplier Value below -1 would flip the sign; the lowest reduction is -1 (damage x0)."));
			}
		}

		if (bHasCritChance)
		{
			for (const FInstancedStruct& ConditionStruct : Task.Conditions)
			{
				const FArcTriggerCondition_HitResult* HitResult = ConditionStruct.GetPtr<FArcTriggerCondition_HitResult>();
				if (HitResult && (HitResult->Crit != EArcTriggerCritFilter::Any || !HitResult->Zones.IsEmpty()))
				{
					OutErrors.Add(LOCTEXT("CritChanceNeedsRoll", "A Crit Chance modifier is read before the crit and zone rolls; a Hit Result condition on the same If cannot filter on Crit or Zones."));
				}
			}
		}

		ValidateConditions(Task.Conditions, OutErrors, [&OutErrors](const FArcTriggerCondition& Condition) { Condition.ValidateInIf(OutErrors); });
		ValidateEffects(Task.Effects, OutErrors, [&OutErrors](const FArcTriggerEffect& Effect) { Effect.ValidateInIf(OutErrors); });
	}
}

bool UArcTriggerValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	const UArcEffectDefinition* Definition = Cast<UArcEffectDefinition>(InObject);
	return Definition && Definition->Tasks.ContainsByPredicate(&ArcTriggerValidatorPrivate::IsTriggerTask);
}

EDataValidationResult UArcTriggerValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UArcEffectDefinition* Definition = CastChecked<UArcEffectDefinition>(InAsset);
	bool bFailed = false;

	if (Definition->StackingPolicy.DurationType == EArcEffectDuration::Instant)
	{
		AssetFails(InAsset, LOCTEXT("InstantHost", "A trigger task only runs on a stored effect; make the effect Duration or Infinite."));
		bFailed = true;
	}

	for (int32 TaskIndex = 0; TaskIndex < Definition->Tasks.Num(); ++TaskIndex)
	{
		const FInstancedStruct& TaskStruct = Definition->Tasks[TaskIndex];
		TArray<FText> Errors;
		if (const FArcEffectTask_TriggerOn* On = TaskStruct.GetPtr<FArcEffectTask_TriggerOn>())
		{
			ArcTriggerValidatorPrivate::ValidateOn(*On, Errors);
		}
		else if (const FArcEffectTask_TriggerWhile* While = TaskStruct.GetPtr<FArcEffectTask_TriggerWhile>())
		{
			ArcTriggerValidatorPrivate::ValidateWhile(*While, Errors);
		}
		else if (const FArcEffectTask_TriggerIf* If = TaskStruct.GetPtr<FArcEffectTask_TriggerIf>())
		{
			ArcTriggerValidatorPrivate::ValidateIf(*If, Errors);
		}

		for (const FText& Error : Errors)
		{
			AssetFails(InAsset, FText::Format(LOCTEXT("TriggerError", "Task {0}: {1}"), FText::AsNumber(TaskIndex), Error));
			bFailed = true;
		}
	}

	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
