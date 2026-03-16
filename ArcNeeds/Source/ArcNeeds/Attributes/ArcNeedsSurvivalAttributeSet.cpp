/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcNeedsSurvivalAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "MassEntitySubsystem.h"
#include "AbilitySystem/ArcGameplayAbilityActorInfo.h"
#include "Engine/World.h"
#include "Fragments/ArcNeedFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNeedsSurvivalAttributeSet)

// ---------------------------------------------------------------------------
// File-scoped helpers: entity handle and entity manager access via
// FArcGameplayAbilityActorInfo, avoiding the old MassAgentComponent path.
// ---------------------------------------------------------------------------

namespace ArcNeedsSurvivalAttributes
{
	static FMassEntityHandle GetEntityHandle(const UAbilitySystemComponent* ASC)
	{
		if (const FArcGameplayAbilityActorInfo* ArcActorInfo =
			static_cast<const FArcGameplayAbilityActorInfo*>(ASC->AbilityActorInfo.Get()))
		{
			return ArcActorInfo->EntityHandle;
		}
		return FMassEntityHandle();
	}

	static FMassEntityManager* GetEntityManager(const UAbilitySystemComponent* ASC)
	{
		if (UWorld* World = ASC->GetWorld())
		{
			if (UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>())
			{
				return &MES->GetMutableEntityManager();
			}
		}
		return nullptr;
	}
} // namespace ArcNeedsSurvivalAttributes

// ---------------------------------------------------------------------------
// Internal helper -- retrieves a mutable Mass fragment pointer for the entity.
// Returns nullptr if the entity is invalid or the fragment is absent.
// ---------------------------------------------------------------------------

template <typename FragmentType>
static FragmentType* GetNeedFragment(UAbilitySystemComponent* ASC)
{
	using namespace ArcNeedsSurvivalAttributes;

	FMassEntityHandle Entity = GetEntityHandle(ASC);
	if (!Entity.IsValid())
	{
		return nullptr;
	}

	FMassEntityManager* EntityManager = GetEntityManager(ASC);
	if (!EntityManager)
	{
		return nullptr;
	}

	return EntityManager->GetFragmentDataPtr<FragmentType>(Entity);
}

// ---------------------------------------------------------------------------
// Constructor -- initialise base values and register all handlers
// ---------------------------------------------------------------------------

UArcNeedsSurvivalAttributeSet::UArcNeedsSurvivalAttributeSet()
{
	AddHunger.SetBaseValue(0.f);
	AddHunger.SetCurrentValue(0.f);

	RemoveHunger.SetBaseValue(0.f);
	RemoveHunger.SetCurrentValue(0.f);

	AddThirst.SetBaseValue(0.f);
	AddThirst.SetCurrentValue(0.f);

	RemoveThirst.SetBaseValue(0.f);
	RemoveThirst.SetCurrentValue(0.f);

	AddFatigue.SetBaseValue(0.f);
	AddFatigue.SetCurrentValue(0.f);

	RemoveFatigue.SetBaseValue(0.f);
	RemoveFatigue.SetCurrentValue(0.f);

	Fatigue.SetBaseValue(0.f);
	Fatigue.SetCurrentValue(0.f);

	ARC_REGISTER_ATTRIBUTE_HANDLER(AddHunger)
	ARC_REGISTER_ATTRIBUTE_HANDLER(RemoveHunger)
	ARC_REGISTER_ATTRIBUTE_HANDLER(AddThirst)
	ARC_REGISTER_ATTRIBUTE_HANDLER(RemoveThirst)
	ARC_REGISTER_ATTRIBUTE_HANDLER(AddFatigue)
	ARC_REGISTER_ATTRIBUTE_HANDLER(RemoveFatigue)
}

// ---------------------------------------------------------------------------
// Hunger handlers
// ---------------------------------------------------------------------------

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcNeedsSurvivalAttributeSet, AddHunger)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float Value = GetAddHunger();
		SetAddHunger(0.f);

		FArcHungerNeedFragment* Fragment = GetNeedFragment<FArcHungerNeedFragment>(GetOwningAbilitySystemComponent());
		if (!Fragment)
		{
			return;
		}

		Fragment->CurrentValue = FMath::Clamp(Fragment->CurrentValue + Value, 0.f, 100.f);
	};
}

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcNeedsSurvivalAttributeSet, RemoveHunger)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float Value = GetRemoveHunger();
		SetRemoveHunger(0.f);

		FArcHungerNeedFragment* Fragment = GetNeedFragment<FArcHungerNeedFragment>(GetOwningAbilitySystemComponent());
		if (!Fragment)
		{
			return;
		}

		Fragment->CurrentValue = FMath::Clamp(Fragment->CurrentValue - Value, 0.f, 100.f);
	};
}

// ---------------------------------------------------------------------------
// Thirst handlers
// ---------------------------------------------------------------------------

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcNeedsSurvivalAttributeSet, AddThirst)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float Value = GetAddThirst();
		SetAddThirst(0.f);

		FArcThirstNeedFragment* Fragment = GetNeedFragment<FArcThirstNeedFragment>(GetOwningAbilitySystemComponent());
		if (!Fragment)
		{
			return;
		}

		Fragment->CurrentValue = FMath::Clamp(Fragment->CurrentValue + Value, 0.f, 100.f);
	};
}

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcNeedsSurvivalAttributeSet, RemoveThirst)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float Value = GetRemoveThirst();
		SetRemoveThirst(0.f);

		FArcThirstNeedFragment* Fragment = GetNeedFragment<FArcThirstNeedFragment>(GetOwningAbilitySystemComponent());
		if (!Fragment)
		{
			return;
		}

		Fragment->CurrentValue = FMath::Clamp(Fragment->CurrentValue - Value, 0.f, 100.f);
	};
}

// ---------------------------------------------------------------------------
// Fatigue handlers
// ---------------------------------------------------------------------------

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcNeedsSurvivalAttributeSet, AddFatigue)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float Value = GetAddFatigue();
		SetAddFatigue(0.f);

		FArcFatigueNeedFragment* Fragment = GetNeedFragment<FArcFatigueNeedFragment>(GetOwningAbilitySystemComponent());
		if (!Fragment)
		{
			return;
		}

		Fragment->CurrentValue = FMath::Clamp(Fragment->CurrentValue + Value, 0.f, 100.f);
	};
}

ARC_DECLARE_ATTRIBUTE_HANDLER(UArcNeedsSurvivalAttributeSet, RemoveFatigue)
{
	return [&](const struct FGameplayEffectModCallbackData& Data)
	{
		const float Value = GetRemoveFatigue();
		SetRemoveFatigue(0.f);

		FArcFatigueNeedFragment* Fragment = GetNeedFragment<FArcFatigueNeedFragment>(GetOwningAbilitySystemComponent());
		if (!Fragment)
		{
			return;
		}

		Fragment->CurrentValue = FMath::Clamp(Fragment->CurrentValue - Value, 0.f, 100.f);
	};
}
