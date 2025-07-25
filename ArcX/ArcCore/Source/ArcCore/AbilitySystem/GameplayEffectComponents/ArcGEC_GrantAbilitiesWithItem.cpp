/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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



#include "ArcGEC_GrantAbilitiesWithItem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

#define LOCTEXT_NAMESPACE "AbilitiesGameplayEffectComponent"

bool operator==(const FArcGameplayAbilitySpecConfig& Lhs, const FArcGameplayAbilitySpecConfig& Rhs)
{
	return Lhs.Ability == Rhs.Ability &&
		Lhs.InputID == Rhs.InputID &&
		Lhs.LevelScalableFloat == Rhs.LevelScalableFloat &&
		Lhs.RemovalPolicy == Rhs.RemovalPolicy;
}

UArcGEC_GrantAbilitiesWithItem::UArcGEC_GrantAbilitiesWithItem()
{
#if WITH_EDITORONLY_DATA
	EditorFriendlyName = TEXT("Grant Abilities While Active");
#endif
}

void UArcGEC_GrantAbilitiesWithItem::AddGrantedAbilityConfig(const FArcGameplayAbilitySpecConfig& Config)
{
	GrantAbilityConfigs.AddUnique(Config);
}

// Reminder: this is happening on the authority only
void UArcGEC_GrantAbilitiesWithItem::OnInhibitionChanged(FActiveGameplayEffectHandle ActiveGEHandle, bool bIsInhibited) const
{
	if (bIsInhibited)
	{
		RemoveAbilities(ActiveGEHandle);
	}
	else
	{
		GrantAbilities(ActiveGEHandle);
	}
}

// Reminder: this is happening on the authority only
void UArcGEC_GrantAbilitiesWithItem::GrantAbilities(FActiveGameplayEffectHandle ActiveGEHandle) const
{
	UAbilitySystemComponent* ASC = ActiveGEHandle.GetOwningAbilitySystemComponent();
	if (!ensure(ASC))
	{
		//UE_LOG(LogGameplayEffects, Error, TEXT("%s was passed an ActiveGEHandle %s which did not have a valid associated AbilitySystemComponent"), ANSI_TO_TCHAR(__func__), *ActiveGEHandle.ToString());
		return;
	}

	if (ASC->bSuppressGrantAbility)
	{
		//UE_LOG(LogGameplayEffects, Warning, TEXT("%s suppressed by %s bSuppressGrantAbility"), *GetName(), *ASC->GetName());
		return;
	}

	const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveGEHandle);
	if (!ActiveGE)
	{
		//UE_LOG(LogGameplayEffects, Warning, TEXT("ActiveGEHandle %s did not corresponds to Active Gameplay Effect on %s. This could potentially happen if you remove the GE during the application of other GE's"), *ActiveGEHandle.ToString(), *ASC->GetName());
		return;
	}
	const FGameplayEffectSpec& ActiveGESpec = ActiveGE->Spec;

	const TArray<FGameplayAbilitySpec>& AllAbilities = ASC->GetActivatableAbilities();
	for (const FArcGameplayAbilitySpecConfig& AbilityConfig : GrantAbilityConfigs)
	{
		// Check that we're configured
		const UGameplayAbility* AbilityCDO = AbilityConfig.Ability.GetDefaultObject();
		if (!AbilityCDO)
		{
			continue;
		}

		// Only do this if we haven't assigned the ability yet! This prevents cases where stacking GEs
		// would regrant the ability every time the stack was applied
		const bool bAlreadyGrantedAbility = AllAbilities.ContainsByPredicate([ASC, AbilityCDO, &ActiveGEHandle](FGameplayAbilitySpec& Spec) { return Spec.Ability == AbilityCDO && Spec.GameplayEffectHandle == ActiveGEHandle; });
		if (bAlreadyGrantedAbility)
		{
			continue;
		}

		const FString ContextString = FString::Printf(TEXT("%s for %s from %s"), ANSI_TO_TCHAR(__func__), *AbilityCDO->GetName(), *GetNameSafe(ActiveGESpec.Def));
		const int32 Level = static_cast<int32>(AbilityConfig.LevelScalableFloat.GetValueAtLevel(ActiveGESpec.GetLevel(), &ContextString));

		// Now grant that ability to the owning actor
		FGameplayAbilitySpec AbilitySpec{ AbilityConfig.Ability, Level, AbilityConfig.InputID, ActiveGESpec.GetEffectContext().GetSourceObject() };
		AbilitySpec.SetByCallerTagMagnitudes = ActiveGESpec.SetByCallerTagMagnitudes;
		AbilitySpec.GameplayEffectHandle = ActiveGEHandle;

		FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);
		FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
		if (FoundSpec)
		{
			const UArcCoreGameplayAbility* SourceAbility = Cast<UArcCoreGameplayAbility>(ActiveGESpec.GetContext().GetAbilityInstance_NotReplicated());
			UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(FoundSpec->GetPrimaryInstance());
			if (ArcAbility)
			{
				ArcAbility->AssingItemFromAbility(SourceAbility);
				if (ArcAbility->IsInstantiated())
				{
					ASC->TryActivateAbility(FoundSpec->Handle);
				}
			}
		}
		else
		{
			UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ASC);
			FDelegateHandle DelegateHandle;
			DelegateHandle = ArcASC->AddOnAbilityGiven(FArcGenericAbilitySpecDelegate::FDelegate::CreateLambda([&DelegateHandle, Handle, ASC, ActiveGESpec](FGameplayAbilitySpec& AbilitySpec)
			{
				if (AbilitySpec.Handle != Handle)
				{
					return;
				}
				
				FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
				if (FoundSpec)
				{
					const UArcCoreGameplayAbility* SourceAbility = Cast<UArcCoreGameplayAbility>(ActiveGESpec.GetContext().GetAbilityInstance_NotReplicated());
					UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(FoundSpec->GetPrimaryInstance());
					if (ArcAbility)
					{
						ArcAbility->AssingItemFromAbility(SourceAbility);
						if (ArcAbility->IsInstantiated())
						{
							ASC->TryActivateAbility(FoundSpec->Handle);
						}
					}
					UArcCoreAbilitySystemComponent* ArcASCLocal = Cast<UArcCoreAbilitySystemComponent>(ASC);
					ArcASCLocal->RemoveOnAbilityGiven(DelegateHandle);
				}
			}));
		}
	}
}

// Reminder: this is happening on the authority only and the ActiveGEHandle can be considered 'inactive' by this time.
void UArcGEC_GrantAbilitiesWithItem::RemoveAbilities(FActiveGameplayEffectHandle ActiveGEHandle) const
{
	UAbilitySystemComponent* ASC = ActiveGEHandle.GetOwningAbilitySystemComponent();
	if (!ensure(ASC))
	{
		//UE_LOG(LogGameplayEffects, Error, TEXT("%s was passed an ActiveGEHandle %s which did not have a valid associated AbilitySystemComponent"), ANSI_TO_TCHAR(__func__), *ActiveGEHandle.ToString());
		return;
	}

	FScopedAbilityListLock ScopedAbilityListLock(*ASC);
	const TArray<const FGameplayAbilitySpec*> GrantedAbilities = ASC->FindAbilitySpecsFromGEHandle(ScopedAbilityListLock, ActiveGEHandle, EConsiderPending::All);
	for (const FArcGameplayAbilitySpecConfig& AbilityConfig : GrantAbilityConfigs)
	{
		// Check that we're configured
		const UGameplayAbility* AbilityCDO = AbilityConfig.Ability.GetDefaultObject();
		if (!AbilityCDO)
		{
			continue;
		}

		// See if we were granted, and if so we can remove it
		const FGameplayAbilitySpec* const* AbilitySpecItem = GrantedAbilities.FindByPredicate([AbilityCDO](const FGameplayAbilitySpec* Spec) { return Spec->Ability == AbilityCDO; });
		if (!AbilitySpecItem || !(*AbilitySpecItem))
		{
			continue;
		}
		const FGameplayAbilitySpec& AbilitySpecDef = (**AbilitySpecItem);

		switch (AbilityConfig.RemovalPolicy)
		{
			case EGameplayEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately:
			{
				ASC->ClearAbility(AbilitySpecDef.Handle);
				break;
			}
			case EGameplayEffectGrantedAbilityRemovePolicy::RemoveAbilityOnEnd:
			{
				ASC->SetRemoveAbilityOnEnd(AbilitySpecDef.Handle);
				break;
			}
			default:
			{
				// Do nothing to granted ability
				break;
			}
		}
	}
}

bool UArcGEC_GrantAbilitiesWithItem::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const
{
	if (ActiveGEContainer.IsNetAuthority())
	{
		ActiveGE.EventSet.OnEffectRemoved.AddUObject(this, &UArcGEC_GrantAbilitiesWithItem::OnActiveGameplayEffectRemoved);
		ActiveGE.EventSet.OnInhibitionChanged.AddUObject(this, &UArcGEC_GrantAbilitiesWithItem::OnInhibitionChanged);
	}

	return true;
}

void UArcGEC_GrantAbilitiesWithItem::OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const
{
	const FActiveGameplayEffect* ActiveGE = RemovalInfo.ActiveEffect;
	if (!ensure(ActiveGE))
	{
		//UE_LOG(LogGameplayEffects, Error, TEXT("FGameplayEffectRemovalInfo::ActiveEffect was not populated in %s"), ANSI_TO_TCHAR(__func__));
		return;
	}

	RemoveAbilities(ActiveGE->Handle);
}

#if WITH_EDITOR
EDataValidationResult UArcGEC_GrantAbilitiesWithItem::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (GetOwner()->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		if (GrantAbilityConfigs.Num() > 0)
		{
			//Context.AddError(FText::FormatOrdered(LOCTEXT("InstantDoesNotWorkWithGrantAbilities", "GrantAbilityConfigs does not work with Instant Effects: {0}."), FText::FromString(GetClass()->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int Index = 0; Index < GrantAbilityConfigs.Num(); ++Index)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = GrantAbilityConfigs[Index].Ability;
		for (int CheckIndex = Index + 1; CheckIndex < GrantAbilityConfigs.Num(); ++CheckIndex)
		{
			if (AbilityClass == GrantAbilityConfigs[CheckIndex].Ability)
			{
				//Context.AddError(FText::FormatOrdered(LOCTEXT("GrantAbilitiesMustBeUnique", "Multiple Abilities of the same type cannot be granted by {0}."), FText::FromString(GetClass()->GetName())));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE