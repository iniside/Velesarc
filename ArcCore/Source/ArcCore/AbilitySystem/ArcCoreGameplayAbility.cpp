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

#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"

#include "AbilitySystemGlobals.h"

#include "ArcGameplayAbilityActorInfo.h"
#include "ArcGameplayEffectContext.h"
#include "GameplayCueManager.h"
#include "UObject/Object.h"

#include "GameplayTagContainer.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ArcAbilitiesTypes.h"

#include "ArcCore/AbilitySystem/ArcAbilityTargetingComponent.h"
#include "ArcCore/AbilitySystem/ArcTargetingBPF.h"
#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemsComponent.h"
#include "ArcCore/Items/ArcItemsStoreComponent.h"

#include "ArcCore/AbilitySystem/ArcAbilityCost.h"
#include "ArcCore/ArcCoreModule.h"
#include "ArcCore/Camera/ArcCameraMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ArcCoreCharacter.h"
#include "Player/ArcHeroComponentBase.h"

#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Pawn/ArcCorePawn.h"
#include "Targeting/ArcTargetingObject.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

DEFINE_LOG_CATEGORY(LogArcAbility);

UArcCoreGameplayAbility::UArcCoreGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::Type::InstancedPerActor;
	bRequiresItem = true;
}

void UArcCoreGameplayAbility::BeginDestroy()
{
	SourceItemId.Reset();
	SourceItemPtr = nullptr;
	Super::BeginDestroy();
}

float UArcCoreGameplayAbility::GetActivationTime() const
{
	if (const FArcAbilityActivationTime* ActivationTimeType = ActivationTime.GetPtr<FArcAbilityActivationTime>())
	{
		FGameplayTagContainer OwnedGameplayTags;
		GetAbilitySystemComponentFromActorInfo()->GetOwnedGameplayTags(OwnedGameplayTags);
		return ActivationTimeType->UpdateActivationTime(this, OwnedGameplayTags);
	}

	return 0.0f;
}

void UArcCoreGameplayAbility::UpdateActivationTime(const FGameplayTagContainer& ActivationTimeTags)
{
	if (const FArcAbilityActivationTime* ActivationTimeType = ActivationTime.GetPtr<FArcAbilityActivationTime>())
	{
		const float NewTime = ActivationTimeType->UpdateActivationTime(this, ActivationTimeTags);

		OnActivationTimeChanged.Broadcast(this, NewTime);

		if (UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
		{
			ArcASC->OnAbilityActivationTimeChanged.Broadcast(GetSpecHandle(), this, NewTime);
		}
	}
}

void UArcCoreGameplayAbility::K2_ExecuteGameplayCueWithParams(FGameplayTag GameplayCueTag
															  , const FGameplayCueParameters& GameplayCueParameters)
{
	check(CurrentActorInfo);
	const_cast<FGameplayCueParameters&>(GameplayCueParameters).AbilityLevel = GetAbilityLevel();

	UAbilitySystemComponent* const AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo_Ensured();
	AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag
		, GameplayCueParameters);

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_WithParams(AbilitySystemComponent
		, GameplayCueTag
		, CurrentActivationInfo.GetActivationPredictionKey()
		, GameplayCueParameters);
}

void UArcCoreGameplayAbility::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	// AbilityTags

	{
		FString Tags;
		int32 Num = GetAssetTags().Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : GetAssetTags())
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("AbilityTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// CancelAbilitiesWithTag
	{
		FString Tags;
		int32 Num = CancelAbilitiesWithTag.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : CancelAbilitiesWithTag)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("CancelAbilitiesWithTag")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// BlockAbilitiesWithTag
	{
		FString Tags;
		int32 Num = BlockAbilitiesWithTag.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : BlockAbilitiesWithTag)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("BlockAbilitiesWithTag")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// ActivationOwnedTags
	{
		FString Tags;
		int32 Num = ActivationOwnedTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : ActivationOwnedTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("ActivationOwnedTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// ActivationRequiredTags
	{
		FString Tags;
		int32 Num = ActivationRequiredTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : ActivationRequiredTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("ActivationRequiredTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// ActivationBlockedTags
	{
		FString Tags;
		int32 Num = ActivationBlockedTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : ActivationBlockedTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("ActivationBlockedTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// SourceRequiredTags
	{
		FString Tags;
		int32 Num = SourceRequiredTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : SourceRequiredTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("SourceRequiredTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// SourceBlockedTags
	{
		FString Tags;
		int32 Num = SourceBlockedTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : SourceBlockedTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("SourceBlockedTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// TargetRequiredTags
	{
		FString Tags;
		int32 Num = TargetRequiredTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : TargetRequiredTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("TargetRequiredTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}

	// TargetBlockedTags
	{
		FString Tags;
		int32 Num = TargetBlockedTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : TargetBlockedTags)
		{
			Tags += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("TargetBlockedTags")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}
	// AbilityTriggers
	{
		FString Tags;
		int32 Num = AbilityTriggers.Num();
		int32 Idx = 0;
		for (const FAbilityTriggerData& T : AbilityTriggers)
		{
			Tags += T.TriggerTag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Tags += ",";
			}
		}
		OutTags.Add(FAssetRegistryTag(TEXT("AbilityTriggers")
			, Tags
			, FAssetRegistryTag::TT_Hidden));
	}
}

#if WITH_EDITOR
EDataValidationResult UArcCoreGameplayAbility::IsDataValid(class FDataValidationContext& Context)
{
	return EDataValidationResult::NotValidated;
}
#endif // #if WITH_EDITOR

void UArcCoreGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo
											, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo
		, Spec);
	
	if (IsInstantiated())
	{
		if (UArcItemsStoreComponent* IC = Cast<UArcItemsStoreComponent>(GetCurrentSourceObject()))
		{
			SourceItemsStore = IC;
		}

		UArcCoreAbilitySystemComponent* ArcASC = GetArcASC();
		{
			FArcTargetRequestAbilityTargetData Del = FArcTargetRequestAbilityTargetData::CreateUObject(this
					, &UArcCoreGameplayAbility::NativeOnAbilityTargetResult);
			ArcASC->RegisterCustomAbilityTargetRequest(GetCurrentAbilitySpecHandle(), Del);
		}
	}

	BP_OnGiveAbility(*ActorInfo
		, Spec);
}

void UArcCoreGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo
											  , const FGameplayAbilitySpec& AbilitySpec)
{
	if (GetArcASC() != nullptr)
	{
		GetArcASC()->UnregisterCustomTargetRequest(GetCurrentAbilitySpecHandle());
	}
	
	BP_OnRemoveAbility(AbilitySpec);
}

void UArcCoreGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo
										  , const FGameplayAbilitySpec& Spec)
{
	if (IsInstantiated())
	{
		if (UArcItemsStoreComponent* IC = Cast<UArcItemsStoreComponent>(GetCurrentSourceObject()))
		{
			SourceItemsStore = IC;
		}
		
		// In case we don't have ItemsComponent, we probably have not been granted at any point trough item.
		if (SourceItemsStore == nullptr)
		{
			Super::OnAvatarSet(ActorInfo, Spec);

			OnAvatarSetSafe(ActorInfo, Spec);
			
			BP_OnAvatarSet(*ActorInfo, Spec);
			
			return;
		}
	}

	Super::OnAvatarSet(ActorInfo
		, Spec);
}

void UArcCoreGameplayAbility::OnAddedToItemSlot(const FGameplayAbilityActorInfo* ActorInfo
	, const FGameplayAbilitySpec* Spec
	, const FGameplayTag& InItemSlot
	, const FArcItemData* InItemData)
{

	if (IsInstantiated())
	{
		SourceItemId = InItemData->GetItemId();
		SourceItemPtr = const_cast<FArcItemData*>(InItemData);

		if (bBPOnAvatarSetCalled == false)
		{
			OnAvatarSetSafe(ActorInfo, *Spec);
			BP_OnAvatarSet(*ActorInfo, *Spec);
			bBPOnAvatarSetCalled = true;
		}
		
		CurrentItemSlot = InItemSlot;

		const FArcItemFragment_AbilityTags* AbilityTagsFragment = ArcItems::FindFragment<FArcItemFragment_AbilityTags>(InItemData);
		if (AbilityTagsFragment)
		{
			AbilityTags.AppendTags(AbilityTagsFragment->AbilityTags);
			CancelAbilitiesWithTag.AppendTags(AbilityTagsFragment->CancelAbilitiesWithTag);
			BlockAbilitiesWithTag.AppendTags(AbilityTagsFragment->BlockAbilitiesWithTag);
			ActivationOwnedTags.AppendTags(AbilityTagsFragment->ActivationOwnedTags);
			ActivationRequiredTags.AppendTags(AbilityTagsFragment->ActivationRequiredTags);
			ActivationBlockedTags.AppendTags(AbilityTagsFragment->ActivationBlockedTags);
			SourceRequiredTags.AppendTags(AbilityTagsFragment->SourceRequiredTags);
			SourceBlockedTags.AppendTags(AbilityTagsFragment->SourceBlockedTags);
			TargetRequiredTags.AppendTags(AbilityTagsFragment->TargetRequiredTags);
			TargetBlockedTags.AppendTags(AbilityTagsFragment->TargetBlockedTags);
		}
	}
}

void UArcCoreGameplayAbility::PostNetInit()
{
	Super::PostNetInit();
}

void UArcCoreGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle
										 , const FGameplayAbilityActorInfo* ActorInfo
										 , const FGameplayAbilityActivationInfo ActivationInfo
										 , bool bReplicateEndAbility
										 , bool bWasCancelled)
{
	FString Role = "";
	switch (ActorInfo->AbilitySystemComponent->GetOwnerRole())
	{
		case ROLE_None:
			Role = "None";
			break;
		case ROLE_SimulatedProxy:
			Role = "Simulated Proxy";
			break;
		case ROLE_AutonomousProxy:
			Role = "Autonomous Proxy";
			break;
		case ROLE_Authority:
			Role = "Authority";
			break;
		case ROLE_MAX:
			Role = "MAX";
			break;
		default: ;
	}
	UE_LOG(LogTemp
		, Log
		, TEXT("[%s] EndAbility [%s]")
		, *Role
		, *GetName());
	
	Super::EndAbility(Handle
		, ActorInfo
		, ActivationInfo
		, bReplicateEndAbility
		, bWasCancelled);
}

bool UArcCoreGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle
												 , const FGameplayAbilityActorInfo* ActorInfo
												 , const FGameplayTagContainer* GameplayTags
												 , const FGameplayTagContainer* TargetTags
												 , FGameplayTagContainer* OptionalRelevantTags) const
{
	if (bRequiresItem)
	{
		if (GetSourceItemEntryPtr() == nullptr)
		{
			return false;
		}
	}
	
	return Super::CanActivateAbility(Handle
		, ActorInfo
		, GameplayTags
		, TargetTags
		, OptionalRelevantTags);
}

bool UArcCoreGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle
											, const FGameplayAbilityActorInfo* ActorInfo
											, FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	bool bOnCooldown = Super::CheckCooldown(Handle
		, ActorInfo
		, OptionalRelevantTags);
	if (bOnCooldown == false && CooldownTags != nullptr)
	{
		return false;
	}

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(
		ActorInfo->AbilitySystemComponent.Get());
	bOnCooldown = ArcASC->IsAbilityOnCooldown(Handle);;
	return !bOnCooldown;
}

void UArcCoreGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle
											, const FGameplayAbilityActorInfo* ActorInfo
											, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCooldown(Handle
		, ActorInfo
		, ActivationInfo);

	const FArcCustomAbilityCooldown* CooldownCalc = CustomCooldownDuration.GetPtr<FArcCustomAbilityCooldown>();
	if (CooldownCalc == nullptr)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(
		ActorInfo->AbilitySystemComponent.Get());
	float CooldownValue = CooldownCalc->GetCooldown(Handle
		, GetCurrentAbilitySpec()
		, ActorInfo
		, ActivationInfo);

	ArcASC->SetAbilityOnCooldown(Handle
		, CooldownValue);
}

void UArcCoreGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle
	, const FGameplayAbilityActorInfo* ActorInfo
	, const FGameplayAbilityActivationInfo ActivationInfo
	, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle
		, ActorInfo
		, ActivationInfo
		, TriggerEventData);

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ActorInfo->AbilitySystemComponent);

	ArcASC->OnAbilityActivatedDynamic.Broadcast(Handle, this);
}

bool UArcCoreGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle
										, const FGameplayAbilityActorInfo* ActorInfo
										, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle
			, ActorInfo
			, OptionalRelevantTags) || !ActorInfo)
	{
		return false;
	}

	// Verify we can afford any additional costs
	for (const FInstancedStruct& AdditionalCost : AdditionalCosts)
	{
		if (AdditionalCost.IsValid())
		{
			if (!AdditionalCost.GetPtr<FArcAbilityCost>()->CheckCost(this
				, Handle
				, ActorInfo
				, /*inout*/ OptionalRelevantTags))
			{
				return false;
			}
		}
	}

	return true;
}

void UArcCoreGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle
										, const FGameplayAbilityActorInfo* ActorInfo
										, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle
		, ActorInfo
		, ActivationInfo);

	check(ActorInfo);
	
	for (const FInstancedStruct& AdditionalCost : AdditionalCosts)
	{
		if (AdditionalCost.IsValid())
		{
			AdditionalCost.GetPtr<FArcAbilityCost>()->ApplyCost(this
				, Handle
				, ActorInfo
				, ActivationInfo);
		}
	}
}

FGameplayEffectContextHandle UArcCoreGameplayAbility::MakeEffectContext(const FGameplayAbilitySpecHandle Handle
																		, const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayEffectContextHandle ContextHandle = Super::MakeEffectContext(Handle, ActorInfo);
	if (SourceItemsStore)
	{
		// it shouldn't actually fail..
		FArcGameplayEffectContext* Context = static_cast<FArcGameplayEffectContext*>(ContextHandle.Get());
		Context->AddSourceObject(SourceItemsStore);

		Context->SetSourceItemHandle(GetSourceItemHandle());

		return ContextHandle;
	}
	
	return ContextHandle;
}

FGameplayEffectSpecHandle UArcCoreGameplayAbility::MakeOutgoingGameplayEffectSpec(
	const FGameplayAbilitySpecHandle Handle
	, const FGameplayAbilityActorInfo* ActorInfo
	, const FGameplayAbilityActivationInfo ActivationInfo
	, TSubclassOf<UGameplayEffect> GameplayEffectClass
	, float Level) const
{
	FGameplayEffectSpecHandle NewSpecHandle = Super::MakeOutgoingGameplayEffectSpec(Handle
		, ActorInfo
		, ActivationInfo
		, GameplayEffectClass
		, Level);

	NewSpecHandle.Data->GetContext().AddSourceObject(SourceItemsStore);
	NewSpecHandle.Data->GetContext().SetAbility(this);

	return NewSpecHandle;
}

FGameplayEffectContextHandle UArcCoreGameplayAbility::BP_MakeEffectContext() const
{
	return MakeEffectContext(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());
}

void UArcCoreGameplayAbility::AssingItemFromAbility(const UArcCoreGameplayAbility* Other) const
{
	if (Other->SourceItemsStore)
	{
		SourceItemsStore = Other->SourceItemsStore;
		SourceItemId = Other->GetSourceItemHandle();
		SourceItemPtr = Other->SourceItemPtr;
	}
}

UArcHeroComponentBase* UArcCoreGameplayAbility::GetHeroComponentFromActorInfo() const
{
	return (CurrentActorInfo ? UArcHeroComponentBase::FindHeroComponent(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}

UArcCoreAbilitySystemComponent* UArcCoreGameplayAbility::GetArcASC() const
{
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());

	return ArcASC;
}

FActiveGameplayEffectHandle UArcCoreGameplayAbility::GetGrantedEffectHandle()
{
	UAbilitySystemComponent* const AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo_Ensured();
	FActiveGameplayEffectHandle ActiveHandle = AbilitySystemComponent->FindActiveGameplayEffectHandle(
		GetCurrentAbilitySpecHandle());

	return ActiveHandle;
}

FGameplayAbilitySpecHandle UArcCoreGameplayAbility::GetSpecHandle() const
{
	return GetCurrentAbilitySpecHandle();
}

float UArcCoreGameplayAbility::GetItemStat(FGameplayAttribute Attribute) const
{
	const FArcItemInstance_ItemStats* ItemStats = ArcItems::FindInstance<FArcItemInstance_ItemStats>(GetSourceItemEntryPtr());
	return ItemStats->GetStatValue(Attribute);
}

FArcItemId UArcCoreGameplayAbility::GetSourceItemHandle() const
{
	return SourceItemId;
}

bool UArcCoreGameplayAbility::BP_GetSourceItem(FArcItemDataHandle& OutItem)
{
	OutItem = SourceItemsStore->GetWeakItemPtr(SourceItemId);
	return OutItem.IsValiD();;
}

FArcItemData* UArcCoreGameplayAbility::GetSourceItemEntryPtr() const
{
	if (SourceItemsStore == nullptr)
	{
		return nullptr;
	}
	/**
	 * Abilities sources (items) are kept inside ItemsComponent, and are added here when
	 * Item is added to slot, and when that slot is replicated.
	 * This definetly can happen out of order, Abilities may arrive sooner than replicated slots, or vice versa.
	 * We lazy assign SourceItem when trying to get it.
	 */
	if (SourceItemPtr == nullptr)
	{
		SourceItemPtr = SourceItemsStore->GetItemPtr(SourceItemId);
	}
	return SourceItemPtr;
}

const FArcItemData* UArcCoreGameplayAbility::GetOwnerItemEntryPtr() const
{
	FArcItemData* ItemPtr = GetSourceItemEntryPtr();
	if (ItemPtr && ItemPtr->GetOwnerPtr())
	{
		return ItemPtr->GetOwnerPtr();
	}

	return ItemPtr;
}

DEFINE_FUNCTION(UArcCoreGameplayAbility::execFindItemFragment)
{
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	
	P_FINISH;
	bool bSuccess = false;

	FArcItemData* ItemData = P_THIS->GetSourceItemEntryPtr();
	if (ItemData)
	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = ArcItems::FindFragment(ItemData, InFragmentType);
		//ItemData->FindFragment(InFragmentType);
		UScriptStruct* OutputStruct = OutItemProp->Struct;
		// Make sure the type we are trying to get through the blueprint node matches the
		// type of the message payload received.
		if ((OutItemProp != nullptr) && (OutItemProp->Struct != nullptr) && (Fragment != nullptr) && (
				OutItemProp->Struct == InFragmentType))
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Fragment);
			bSuccess = true;
		}
		P_NATIVE_END;
	}
	*(bool*)RESULT_PARAM = bSuccess;
}

DEFINE_FUNCTION(UArcCoreGameplayAbility::execFindItemFragmentPure)
{
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	
	P_FINISH;
	bool bSuccess = false;

	FArcItemData* ItemData = P_THIS->GetSourceItemEntryPtr();
	if (ItemData)
	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = ArcItems::FindFragment(ItemData, InFragmentType);
		//ItemData->FindFragment(InFragmentType);
		UScriptStruct* OutputStruct = OutItemProp->Struct;
		// Make sure the type we are trying to get through the blueprint node matches the
		// type of the message payload received.
		if ((OutItemProp != nullptr) && (OutItemProp->Struct != nullptr) && (Fragment != nullptr) && (
				OutItemProp->Struct == InFragmentType))
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Fragment);
			bSuccess = true;
		}
		P_NATIVE_END;
	}
	*(bool*)RESULT_PARAM = bSuccess;
}

float UArcCoreGameplayAbility::FindItemScalableValue(UScriptStruct* InFragmentType
	, FName PropName) const
{
	FStructProperty* Prop = FindFProperty<FStructProperty>(InFragmentType, PropName);
	FArcScalableCurveFloat Data(Prop, InFragmentType);
	
	return GetOwnerItemEntryPtr()->GetValue(Data);
}

DEFINE_FUNCTION(UArcCoreGameplayAbility::execFindScalableItemFragment)
{
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	
	P_FINISH;
	bool bSuccess = false;

	FArcItemData* ItemData = P_THIS->GetSourceItemEntryPtr();
	if (ItemData)
	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = (uint8*)ArcItems::FindScalableItemFragment<FArcScalableFloatItemFragment>(ItemData);
		//ItemData->FindFragment(InFragmentType);
		UScriptStruct* OutputStruct = OutItemProp->Struct;
		// Make sure the type we are trying to get through the blueprint node matches the
		// type of the message payload received.
		if ((OutItemProp != nullptr) && (OutItemProp->Struct != nullptr) && (Fragment != nullptr) && (
				OutItemProp->Struct == InFragmentType))
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Fragment);
			bSuccess = true;
		}
		P_NATIVE_END;
	}
	*(bool*)RESULT_PARAM = bSuccess;
}

UArcItemsStoreComponent* UArcCoreGameplayAbility::GetItemsStoreComponent(TSubclassOf<UArcItemsStoreComponent> IC) const
{
	return SourceItemsStore;
}

const UArcItemDefinition* UArcCoreGameplayAbility::GetSourceItemData() const
{
	return GetSourceItemEntryPtr()->GetItemDefinition();
}

const UArcItemDefinition* UArcCoreGameplayAbility::NativeGetSourceItemData() const
{
	return GetSourceItemEntryPtr()->GetItemDefinition();
}

const UArcItemDefinition* UArcCoreGameplayAbility::NativeGetOwnerItemData() const
{
	return GetOwnerItemEntryPtr()->GetItemDefinition();
}

const UArcItemDefinition* UArcCoreGameplayAbility::GetOwnerItemData() const
{
	return GetOwnerItemEntryPtr()->GetItemDefinition();
}

const FArcGameplayAbilityActorInfo& UArcCoreGameplayAbility::GetArcActorInfo()
{
	const FArcGameplayAbilityActorInfo* NxActorInfo = static_cast<const FArcGameplayAbilityActorInfo*>(
		GetCurrentActorInfo());
	return *NxActorInfo;
}

AArcCoreCharacter* UArcCoreGameplayAbility::GetArcCharacterFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<AArcCoreCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}

bool UArcCoreGameplayAbility::HasAuthority() const
{
	return GetCurrentActorInfo()->OwnerActor->HasAuthority();
}

float UArcCoreGameplayAbility::GetItemValue(FInstancedStruct InMagnitudeCalculation) const
{
	checkf(InMagnitudeCalculation.IsValid(), TEXT("Instanced Struct Is Not Valid"));
	
	checkf(InMagnitudeCalculation.GetScriptStruct()->IsChildOf( FArcGetItemValue::StaticStruct()), TEXT("Instanced Struct Is Not Child of FArcGetItemValue"));
	
	return InMagnitudeCalculation.GetPtr<FArcGetItemValue>()->Calculate(GetSourceItemEntryPtr()
		, this
		, GetCurrentActorInfo());
}

void UArcCoreGameplayAbility::SetEffectMagnitude(const FGameplayTag& DataTag
												 , FInstancedStruct InMagnitudeCalculation
												 , const FGameplayTag& EffectTag)
{
	checkf(InMagnitudeCalculation.IsValid(), TEXT("Instanced Struct Is Not Valid"));
	checkf(InMagnitudeCalculation.GetScriptStruct()->IsChildOf( FArcGetItemValue::StaticStruct()), TEXT("Instanced Struct Is Not Child of FArcGetItemValue"));

	float Value = InMagnitudeCalculation.GetPtr<FArcGetItemValue>()->Calculate(GetSourceItemEntryPtr()
		, this
		, GetCurrentActorInfo());

	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);
	
	for (const FArcEffectSpecItem* E : Effects)
	{
		for (const FGameplayEffectSpecHandle& SpecHandle : E->Specs)
		{
			FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

			checkf(Spec != nullptr, TEXT("Invalid GameplayEffectSpec"));
			
			Spec->SetSetByCallerMagnitude(DataTag
				, Value);
		}
	}
}


void UArcCoreGameplayAbility::ExecuteLocalTargeting(UArcTargetingObject* InTrace)
{
	ENetMode NM = GetArcASC()->GetOwner()->GetNetMode();
	EGameplayAbilityNetExecutionPolicy::Type ExecPolicy = GetNetExecutionPolicy();

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
	
	bool bExecuteTrace = (NM == ENetMode::NM_DedicatedServer && ExecPolicy ==
							   EGameplayAbilityNetExecutionPolicy::Type::ServerOnly)
								|| (NM == NM_Standalone) || (NM == NM_Client);

	if (bExecuteTrace)
	{
		NativeExecuteLocalTargeting(InTrace);	
	}
}

void UArcCoreGameplayAbility::SendTargetingResult(const FGameplayAbilityTargetDataHandle& TargetData
												  , UArcTargetingObject* InTrace)
{
	ENetMode NM = GetArcASC()->GetOwner()->GetNetMode();
	EGameplayAbilityNetExecutionPolicy::Type ExecPolicy = GetNetExecutionPolicy();
	
	bool bServerExecuteTrace = (NM == ENetMode::NM_DedicatedServer && ExecPolicy ==
							   EGameplayAbilityNetExecutionPolicy::Type::ServerOnly)
								|| NM == NM_Standalone;
	
	FGameplayAbilitySpecHandle AbilityHandle = GetCurrentAbilitySpecHandle();
	if (NM == ENetMode::NM_Client || bServerExecuteTrace)
	{
		NativeOnAbilityTargetResult(TargetData);
	}

	if (NM == ENetMode::NM_Client)
	{
		GetArcASC()->SendRequestCustomTargets(TargetData
		, InTrace
		, GetCurrentAbilitySpecHandle());	
	}
}

void UArcCoreGameplayAbility::NativeExecuteLocalTargeting(UArcTargetingObject* InTrace)
{
	ENetMode NM = GetArcASC()->GetOwner()->GetNetMode();
	EGameplayAbilityNetExecutionPolicy::Type ExecPolicy = GetNetExecutionPolicy();

	UTargetingSubsystem* TargetingSystem = UTargetingSubsystem::Get(GetWorld());
	FArcTargetingSourceContext Context;
	Context.SourceActor = GetAvatarActorFromActorInfo();
	Context.InstigatorActor = GetActorInfo().OwnerActor.Get();
	Context.SourceObject = this;
	
	FTargetingRequestHandle RequestHandle = Arcx::MakeTargetRequestHandle(InTrace->TargetingPreset, Context);
	TargetingSystem->ExecuteTargetingRequestWithHandle(RequestHandle
		, FTargetingRequestDelegate::CreateUObject(this, &UArcCoreGameplayAbility::NativeOnLocalTargetResult));
}

void UArcCoreGameplayAbility::NativeOnLocalTargetResult(FTargetingRequestHandle TargetingRequestHandle)
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	TArray<FHitResult> Hits;
	Hits.Reserve(TargetingResults.TargetResults.Num());
	
	for (auto Result : TargetingResults.TargetResults)
	{
		Hits.Add(MoveTemp(Result.HitResult));
	}

	OnLocalTargetResult(Hits);
}

void UArcCoreGameplayAbility::NativeOnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData)
{
	ENetMode NM = GetArcASC()->GetOwner()->GetNetMode();
	if (NM == NM_Client)
	{
		OnAbilityTargetResult(AbilityTargetData
			, EArcClientServer::Client);
	}
	else
	{
		OnAbilityTargetResult(AbilityTargetData
			, EArcClientServer::Server);
	}
}

TArray<FActiveGameplayEffectHandle> UArcCoreGameplayAbility::ApplyEffectsFromItemByTags(const FGameplayTagContainer& EffectTagContainer
	, const FGameplayAbilityTargetDataHandle& InHitResults)
{
	TArray<FActiveGameplayEffectHandle> OutHandles;
	OutHandles.Reserve(EffectTagContainer.Num() * 4);

	for (const FGameplayTag& Tag : EffectTagContainer)
	{
		OutHandles.Append(ApplyEffectsFromItem(Tag, InHitResults));
	}

	return OutHandles;
}

TArray<FActiveGameplayEffectHandle> UArcCoreGameplayAbility::ApplyEffectsFromItem(const FGameplayTag& EffectTag
																				  , const FGameplayAbilityTargetDataHandle& InHitResults)
{
	TArray<FActiveGameplayEffectHandle> ReturnHandles;

	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	
	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);

	FGameplayAbilitySpecHandle AbilitySpecHandle = GetCurrentAbilitySpecHandle();
	FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();

	FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

	static FGameplayTagContainer AbilitySystemComponentTags;
	AbilitySystemComponentTags.Reset();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo_Ensured();
	ASC->GetOwnedGameplayTags(AbilitySystemComponentTags);

	for (const FArcEffectSpecItem* E : Effects)
	{
		if (E->SourceIgnoreTags.Num() > 0 && AbilitySystemComponentTags.HasAny(E->SourceIgnoreTags))
		{
			continue;
		}

		if (E->SourceRequiredTags.Num() > 0 && !AbilitySystemComponentTags.HasAll(E->SourceRequiredTags))
		{
			continue;
		}

		for (const FGameplayEffectSpecHandle& Effect : E->Specs)
		{
			TArray<FActiveGameplayEffectHandle> EffectHandles;
			FPredictionKey PredKey = ActivInfo.GetActivationPredictionKey();
			bool IsValid = PredKey.IsValidForMorePrediction();

			if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
			{
				// Early out to avoid making effect specs that we can't apply
				return ReturnHandles; // EffectHandles;
			}

			if (Effect == nullptr)
			{
				// ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability
				// %s with no GameplayEffect."), *GetName());
			}
			else if (HasAuthorityOrPredictionKey(GetCurrentActorInfo()
						 , &ActivInfo) || IsValid)
			{
				Effect.Data->CapturedSourceTags.GetSpecTags().AppendTags(GetAssetTags());
				Effect.Data->GetContext().Get()->SetAbility(this);
				// ArcAbility::SetTargetDataHandle(Effect.Data->GetContext().Get(),
				// InHitResults);
				ReturnHandles = ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle
					, *Effect.Data.Get()
					, ASC->GetPredictionKeyForNewAction());
			}
		}
	}
	return ReturnHandles;
}

TArray<FActiveGameplayEffectHandle> UArcCoreGameplayAbility::ApplyAllEffectsFromItem(
	const FGameplayAbilityTargetDataHandle& InHitResults)
{
	TArray<FActiveGameplayEffectHandle> ReturnHandles;
	if(SourceItemsStore == nullptr)
	{
		return ReturnHandles;
	}

	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	
	const TArray<FArcEffectSpecItem>& Effects = Instance->GetAllEffectSpecHandles();

	FGameplayAbilitySpecHandle AbilitySpecHandle = GetCurrentAbilitySpecHandle();
	FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();

	FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

	static FGameplayTagContainer AbilitySystemComponentTags;
	AbilitySystemComponentTags.Reset();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo_Ensured();
	ASC->GetOwnedGameplayTags(AbilitySystemComponentTags);

	for (const FArcEffectSpecItem& SpecItem : Effects)
	{
		if (SpecItem.SourceIgnoreTags.Num() > 0 && AbilitySystemComponentTags.HasAny(SpecItem.SourceIgnoreTags))
		{
			continue;
		}

		if (SpecItem.SourceRequiredTags.Num() > 0 && !AbilitySystemComponentTags.HasAll(SpecItem.SourceRequiredTags))
		{
			continue;
		}
		for (const FGameplayEffectSpecHandle& Effect : SpecItem.Specs)
		{
			TArray<FActiveGameplayEffectHandle> EffectHandles;
			FPredictionKey PredKey = ActivInfo.GetActivationPredictionKey();
			bool IsValid = PredKey.IsValidForMorePrediction();

			if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
			{
				// Early out to avoid making effect specs that we can't apply
				return ReturnHandles; // EffectHandles;
			}

			if (Effect == nullptr)
			{
				// ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability
				// %s with no GameplayEffect."), *GetName());
			}
			else if (HasAuthorityOrPredictionKey(GetCurrentActorInfo()
						 , &ActivInfo) || IsValid)
			{
				Effect.Data->CapturedSourceTags.GetSpecTags().AppendTags(GetAssetTags());
				Effect.Data->GetContext().Get()->SetAbility(this);
				// ArcAbility::SetTargetDataHandle(Effect.Data->GetContext().Get(),
				// InHitResults);
				ReturnHandles = ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle
					, *Effect.Data.Get()
					, ASC->GetPredictionKeyForNewAction());
			}
		}
	}
	return ReturnHandles;
}

TArray<FGameplayEffectSpecHandle> UArcCoreGameplayAbility::GetEffectSpecFromItem(const FGameplayTag& EffectTag) const
{
	TArray<FGameplayEffectSpecHandle> Specs;

	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	
	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);
	for (const FArcEffectSpecItem* E : Effects)
	{
		Specs.Append(E->Specs);
	}
	return Specs;
}

bool UArcCoreGameplayAbility::RemoveGameplayEffectFromTarget(FGameplayAbilityTargetDataHandle TargetData
	, FActiveGameplayEffectHandle Handle
	, int32 Stacks)
{
	bool bAllSuccessfull = true;
	
	const int32 NumTargets = TargetData.Num();
	for (int32 Idx = 0; Idx < NumTargets; Idx++)
	{
		TArray<TWeakObjectPtr<AActor>> Actors = TargetData.Get(Idx)->GetActors();
		for (TWeakObjectPtr<AActor> A : Actors)
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(A.Get()))
			{
				bAllSuccessfull = ASC->RemoveActiveGameplayEffect(Handle, Stacks);
			}
		}
	}

	return bAllSuccessfull;
}

void UArcCoreGameplayAbility::SetByCallerMagnitude(const FGameplayTag& SetByCallerTag
												   , float Magnitude
												   , const FGameplayTag& EffectTag)
{
	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());

	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);
	
	for (const FArcEffectSpecItem* E : Effects)
	{
		for (const FGameplayEffectSpecHandle& SpecHandle : E->Specs)
		{
			FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
			checkf(Spec != nullptr, TEXT("Invalid GameplayEffectSpec"));
			Spec->SetSetByCallerMagnitude(SetByCallerTag, Magnitude);
		}
	}
}

void UArcCoreGameplayAbility::SetEffectDuration(float Duration
												, const FGameplayTag& EffectTag)
{
	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);
	for (const FArcEffectSpecItem* E : Effects)
	{
		for (const FGameplayEffectSpecHandle& SpecHandle : E->Specs)
		{
			FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
			checkf(Spec != nullptr
				, TEXT("Invalid GameplayEffectSpec"));
			Spec->SetDuration(Duration
				, false);
		}
	}
}

void UArcCoreGameplayAbility::SetEffectPeriod(float Period
											  , const FGameplayTag& EffectTag)
{
	const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);
	for (const FArcEffectSpecItem* E : Effects)
	{
		for (const FGameplayEffectSpecHandle& SpecHandle : E->Specs)
		{
			FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
			checkf(Spec != nullptr
				, TEXT("Invalid GameplayEffectSpec"));
			Spec->Period = Period;
		}
	}
}

float UArcCoreGameplayAbility::GetAnimationPlayRate() const
{
	return 1.0f;
}

bool UArcCoreGameplayAbility::SpawnAbilityActor(TSubclassOf<AActor> ActorClass
	, FGameplayAbilityTargetingLocationInfo TargetLocation
	, const FGameplayAbilityTargetDataHandle& InTargetData
	, TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility)
{
	if (ActorClass == nullptr)
	{
		return false;
	}

	FVector Location = TargetLocation.LiteralTransform.GetLocation();
	FRotator Rotation = TargetLocation.LiteralTransform.GetRotation().Rotator();

	const bool bIsServer = ActorClass->ImplementsInterface(UArcAbilityServerActorInterface::StaticClass());
	const bool bIsClientAuth = ActorClass->ImplementsInterface(UArcAbilityClientAuthActorInterface::StaticClass());
	const bool bIsClientPredicted = ActorClass->ImplementsInterface(UArcAbilityClientPredictedActorInterface::StaticClass());

	if (!bIsServer && !bIsClientAuth && !bIsClientPredicted)
	{
		checkf(false, TEXT("Actor spawned by this function needs to implement one of interfaces, ArcAbilityServerActorInterface, ArcAbilityClientAuthActorInterface, ArcAbilityClientPredictedActorInterface:"));
		return false;
	}
	
	UArcCoreAbilitySystemComponent* ArcASC =  Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (ArcASC == nullptr)
	{
		return false;
	}

	ENetMode NM = ArcASC->GetNetMode();
	FArcAbilityActorHandle Handle;
	if (bIsClientAuth && (NM != ENetMode::NM_DedicatedServer))
	{
		Handle = ArcASC->SpawnClientAuthoritativeActor(ActorClass
			, GetSpecHandle()
			, InTargetData
			, Location
			, Rotation
			, ActorGrantedAbility);

		TWeakObjectPtr<AActor> SpawnedActor = ArcASC->SpawnedActors[Handle];
		UArcAbilityActorComponent* AAC = SpawnedActor->FindComponentByClass<UArcAbilityActorComponent>();

		AAC->Initialize(ArcASC, this, Handle, InTargetData,  ActorGrantedAbility);
	}
	else if (bIsServer && (NM != ENetMode::NM_Client))
	{
		Handle =  ArcASC->SpawnActor(ActorClass
			, GetSpecHandle()
			, InTargetData
			, Location
			, Rotation);

		TWeakObjectPtr<AActor> SpawnedActor = ArcASC->SpawnedActors[Handle];
		UArcAbilityActorComponent* AAC = SpawnedActor->FindComponentByClass<UArcAbilityActorComponent>();

		AAC->Initialize(ArcASC, this, Handle, InTargetData,  ActorGrantedAbility);
	}
	
	return true;
}

bool UArcCoreGameplayAbility::SpawnAbilityActorTargetData(TSubclassOf<AActor> ActorClass
	, const FGameplayAbilityTargetDataHandle& InTargetData
	, TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility)
{
	if (ActorClass == nullptr)
	{
		return false;
	}

	if (!InTargetData.IsValid(0))
	{
		return false;
	}
	
	const bool bHasOrigin = InTargetData.Get(0)->HasOrigin();
	if (!bHasOrigin)
	{
		return false;
	}
	FTransform OriginTM = InTargetData.Get(0)->GetOrigin();
	FVector Location = OriginTM.GetLocation();
	FRotator Rotation = OriginTM.GetRotation().Rotator();

	const bool bIsServer = ActorClass->ImplementsInterface(UArcAbilityServerActorInterface::StaticClass());
	const bool bIsClientAuth = ActorClass->ImplementsInterface(UArcAbilityClientAuthActorInterface::StaticClass());
	const bool bIsClientPredicted = ActorClass->ImplementsInterface(UArcAbilityClientPredictedActorInterface::StaticClass());

	if (!bIsServer && !bIsClientAuth && !bIsClientPredicted)
	{
		checkf(false, TEXT("Actor spawned by this function needs to implement one of interfaces, ArcAbilityServerActorInterface, ArcAbilityClientAuthActorInterface, ArcAbilityClientPredictedActorInterface:"));
		return false;
	}
	
	UArcCoreAbilitySystemComponent* ArcASC =  Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (ArcASC == nullptr)
	{
		return false;
	}

	ENetMode NM = ArcASC->GetNetMode();
	FArcAbilityActorHandle Handle;
	if (bIsClientAuth && (NM != ENetMode::NM_DedicatedServer))
	{
		Handle = ArcASC->SpawnClientAuthoritativeActor(ActorClass
			, GetSpecHandle()
			, InTargetData
			, Location
			, Rotation
			, ActorGrantedAbility);

		TWeakObjectPtr<AActor> SpawnedActor = ArcASC->SpawnedActors[Handle];
		UArcAbilityActorComponent* AAC = SpawnedActor->FindComponentByClass<UArcAbilityActorComponent>();

		AAC->Initialize(ArcASC, this, Handle, InTargetData,  ActorGrantedAbility);
	}
	else if (bIsServer && (NM != ENetMode::NM_Client))
	{
		Handle =  ArcASC->SpawnActor(ActorClass
			, GetSpecHandle()
			, InTargetData
			, Location
			, Rotation);

		TWeakObjectPtr<AActor> SpawnedActor = ArcASC->SpawnedActors[Handle];
		UArcAbilityActorComponent* AAC = SpawnedActor->FindComponentByClass<UArcAbilityActorComponent>();

		AAC->Initialize(ArcASC, this, Handle, InTargetData,  ActorGrantedAbility);
	}
	
	return true;
}

UGameplayTask* UArcCoreGameplayAbility::GetTaskByName(FName TaskName) const
{
	for (UGameplayTask* Task : ActiveTasks)
	{
		if (Task->GetInstanceName() == TaskName)
		{
			return Task;
		}
	}
	return nullptr;
}
