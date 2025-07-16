/**
 * This file is part of ArcX.
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

#include "ArcAbilitiesBPF.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Abilities/GameplayAbility.h"
#include "ArcAbilitiesTypes.h"

#include "Components/PrimitiveComponent.h"

#include "ArcGameplayEffectContext.h"
#include "Items/ArcItemData.h"

FString UArcAbilitiesBPF::GetAttributeName(const FGameplayAttribute& Attribute)
{
	return Attribute.GetName();
}

FArcItemId UArcAbilitiesBPF::GetSourceItem(const FGameplayEffectSpec& Spec)
{
	const UArcCoreGameplayAbility* Ability = Cast<UArcCoreGameplayAbility>(
		Spec.GetContext().GetAbilityInstance_NotReplicated());

	return Ability->GetSourceItemHandle();
}

class UArcCoreAbilitySystemComponent* UArcAbilitiesBPF::GetInstigatorNxASC(const FGameplayEffectSpec& Spec)
{
	return Cast<UArcCoreAbilitySystemComponent>(Spec.GetEffectContext().GetInstigatorAbilitySystemComponent());
}

FString UArcAbilitiesBPF::GetSpecName(const FGameplayEffectSpecHandle& Spec)
{
	return Spec.Data->ToSimpleString();
}

const UArcItemDefinition* UArcAbilitiesBPF::GetOwnerItemDef(const FGameplayEffectContextHandle& Context, bool& bSuccess)
{
	const FArcGameplayEffectContext* Ctx = static_cast<const FArcGameplayEffectContext*>(Context.Get());
	const UArcItemDefinition* ItemDef = Ctx->GetSourceItem();
	if (ItemDef)
	{
		bSuccess = true;
		return ItemDef;
	}
	bSuccess = false;
	return nullptr;
}

FArcItemDataHandle UArcAbilitiesBPF::GetOwnerItemData(const FGameplayEffectContextHandle& Context, bool& bSucecss)
{
	const FArcGameplayEffectContext* Ctx = static_cast<const FArcGameplayEffectContext*>(Context.Get());

	TWeakPtr<FArcItemData> ItemData = Ctx->GetSourceItemWeakPtr();
	if (ItemData.IsValid())
	{
		bSucecss = true;
		return ItemData;	
	}

	bSucecss = false;
	return FArcItemDataHandle();
}

void UArcAbilitiesBPF::AddSpecTags(const FGameplayEffectSpecHandle& Spec
								   , const FGameplayTagContainer& Tags)
{
	Spec.Data->CapturedSourceTags.GetSpecTags().AppendTags(Tags);
}

void UArcAbilitiesBPF::AddSpecTag(const FGameplayEffectSpecHandle& Spec
								  , const FGameplayTag& Tag)
{
	Spec.Data->CapturedSourceTags.GetSpecTags().AddTag(Tag);
}

FGameplayEffectSpecHandle UArcAbilitiesBPF::MakeSpecHandle(TSubclassOf<UGameplayEffect> InGameplayEffect
														   , AActor* InInstigator
														   , AActor* InEffectCauser
														   , float InLevel)
{
	FGameplayEffectContext* EffectContext = UAbilitySystemGlobals::Get().AllocGameplayEffectContext();
	EffectContext->AddInstigator(InInstigator, InEffectCauser);

	return FGameplayEffectSpecHandle(new FGameplayEffectSpec(InGameplayEffect.GetDefaultObject()
		, FGameplayEffectContextHandle(EffectContext)
		, InLevel));
}

void UArcAbilitiesBPF::SetTargetHandleInContext(FGameplayEffectContextHandle Context
												, FArcTargetDataId TargetDataHandle)
{
	ArcAbility::SetTargetDataHandle(Context, TargetDataHandle);
}

void UArcAbilitiesBPF::SetContextInSpec(FGameplayEffectSpecHandle SpecHandle
										, FGameplayEffectContextHandle Context)
{
	SpecHandle.Data->SetContext(Context);
}

void UArcAbilitiesBPF::AddInstigator(FGameplayEffectContextHandle Context
									 , AActor* InInstigator
									 , AActor* InEffectCauser)
{
	Context.AddInstigator(InInstigator, InEffectCauser);
}

FGameplayEffectContextHandle UArcAbilitiesBPF::DuplicateEffectContext(FGameplayEffectContextHandle Context)
{
	return Context.Duplicate();
}

void UArcAbilitiesBPF::RemoveEffectsFromTarget(const TArray<FActiveGameplayEffectHandle>& Handles
											   , FGameplayAbilityTargetDataHandle Target)
{
	TArray<TWeakObjectPtr<AActor>> Actors = Target.Get(0)->GetActors();
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actors[0].Get());

	for (const FActiveGameplayEffectHandle& H : Handles)
	{
		ASI->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(H);
	}
}

void UArcAbilitiesBPF::GetCooldownTimeRemainingAndDuration(class UGameplayAbility* Ability
														   , float& TimeRemaining
														   , float& CooldownDuration)

{
	Ability->GetCooldownTimeRemainingAndDuration(Ability->GetCurrentAbilitySpecHandle()
		, Ability->GetCurrentActorInfo()
		, TimeRemaining
		, CooldownDuration);
}

FGameplayCueParameters UArcAbilitiesBPF::ArcMakeGameplayCueParametersFromHitResult(const FHitResult& HitResult)
{
	FGameplayCueParameters Params;
	Params.Location = HitResult.ImpactPoint;
	Params.Normal = HitResult.ImpactNormal;
	Params.PhysicalMaterial = HitResult.PhysMaterial;
	Params.TargetAttachComponent = HitResult.Component;

	return Params;
}

FGameplayAbilityTargetDataHandle UArcAbilitiesBPF::AbilityTargetDataFromHitResultArray(const TArray<FHitResult>& HitResults)
{
	FGameplayAbilityTargetDataHandle Handle;
	for (const FHitResult& HitResult : HitResults)
	{
		// Construct TargetData
		FGameplayAbilityTargetData_SingleTargetHit* TargetData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);
		Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData>(TargetData));
	}
	return Handle;
}

FActiveGameplayEffectHandle UArcAbilitiesBPF::GetActiveEffectHandle(const FActiveGameplayEffect& ActiveGE)
{
	return ActiveGE.Handle;
}

const UGameplayEffect* UArcAbilitiesBPF::GetEffectDef(const FActiveGameplayEffect& ActiveGE)
{
	return ActiveGE.Spec.Def;
}

float UArcAbilitiesBPF::GetEffectDuration(const FActiveGameplayEffect& ActiveGE)
{
	return ActiveGE.GetDuration();
}

float UArcAbilitiesBPF::GetEffectPeriod(const FActiveGameplayEffect& ActiveGE)
{
	return ActiveGE.GetPeriod();
}

float UArcAbilitiesBPF::GetEffectEndTime(const FActiveGameplayEffect& ActiveGE)
{
	return ActiveGE.GetEndTime();
}

float UArcAbilitiesBPF::GetEffectTimeRemaining(const FActiveGameplayEffect& ActiveGE, float WorldTime)
{
	return ActiveGE.GetTimeRemaining(WorldTime);
}

const UGameplayEffectComponent* UArcAbilitiesBPF::GetEffectComponent(const FActiveGameplayEffect& ActiveGE
	, TSubclassOf<UGameplayEffectComponent> ItemClass)
{
	return ActiveGE.Spec.Def->FindComponent(ItemClass);
}

FActiveGameplayEffectHandle UArcAbilitiesBPF::GetActiveEffectRPCHandle(const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	return ActiveGE.Handle;
}

const UGameplayEffect* UArcAbilitiesBPF::GetEffectRPCDef(const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	return ActiveGE.Spec.Def;
}

float UArcAbilitiesBPF::GetEffectRPCDuration(const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	return ActiveGE.GetDuration();
}

float UArcAbilitiesBPF::GetEffectRPCPeriod(const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	return ActiveGE.GetPeriod();
}

float UArcAbilitiesBPF::GetEffectRPCEndTime(const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	return ActiveGE.GetEndTime();
}

float UArcAbilitiesBPF::GetEffectRPCTimeRemaining(const FArcActiveGameplayEffectForRPC& ActiveGE
	, float WorldTime)
{
	return ActiveGE.GetTimeRemaining(WorldTime);
}
