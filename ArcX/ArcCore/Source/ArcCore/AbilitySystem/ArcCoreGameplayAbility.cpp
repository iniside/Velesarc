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

#include "ArcCore/AbilitySystem/ArcTargetingBPF.h"
#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemsComponent.h"
#include "ArcCore/Items/ArcItemsStoreComponent.h"

#include "ArcCore/AbilitySystem/ArcAbilityCost.h"
#include "ArcCore/ArcCoreModule.h"
#include "Components/CapsuleComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ArcCoreCharacter.h"
#include "Player/ArcHeroComponentBase.h"

#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Fragments/ArcItemFragment_AbilityActions.h"
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
		
		const float Current = ActivationTimeType->GetActivationTime(this);
		const float NewTime = ActivationTimeType->UpdateActivationTime(this, OwnedGameplayTags);
		
		if (Current != NewTime)
		{
			if (UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
			{
				ArcASC->OnAbilityActivationTimeChanged.Broadcast(GetSpecHandle(), const_cast<UArcCoreGameplayAbility*>(this), NewTime);
			}
		}
		
		return NewTime;
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

void UArcCoreGameplayAbility::CallOnActionTimeFinished()
{
	OnActivationTimeFinished.Broadcast(this, 0.f);
	
	if (UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		ArcASC->OnAbilityActivationTimeFinished.Broadcast(GetSpecHandle(), this);
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

static void AddTagContainerAsRegistryTag(FAssetRegistryTagsContext& Context, const TCHAR* Name, const FGameplayTagContainer& Tags)
{
	FString TagString;
	int32 Num = Tags.Num();
	int32 Idx = 0;
	for (const FGameplayTag& Tag : Tags)
	{
		TagString += Tag.ToString();
		if (++Idx < Num)
		{
			TagString += TEXT(",");
		}
	}
	Context.AddTag(UObject::FAssetRegistryTag(Name, TagString, UObject::FAssetRegistryTag::TT_Hidden));
}

void UArcCoreGameplayAbility::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	AddTagContainerAsRegistryTag(Context, TEXT("AbilityTags"), GetAssetTags());
	AddTagContainerAsRegistryTag(Context, TEXT("CancelAbilitiesWithTag"), CancelAbilitiesWithTag);
	AddTagContainerAsRegistryTag(Context, TEXT("BlockAbilitiesWithTag"), BlockAbilitiesWithTag);
	AddTagContainerAsRegistryTag(Context, TEXT("ActivationOwnedTags"), ActivationOwnedTags);
	AddTagContainerAsRegistryTag(Context, TEXT("ActivationRequiredTags"), ActivationRequiredTags);
	AddTagContainerAsRegistryTag(Context, TEXT("ActivationBlockedTags"), ActivationBlockedTags);
	AddTagContainerAsRegistryTag(Context, TEXT("SourceRequiredTags"), SourceRequiredTags);
	AddTagContainerAsRegistryTag(Context, TEXT("SourceBlockedTags"), SourceBlockedTags);
	AddTagContainerAsRegistryTag(Context, TEXT("TargetRequiredTags"), TargetRequiredTags);
	AddTagContainerAsRegistryTag(Context, TEXT("TargetBlockedTags"), TargetBlockedTags);

	// AbilityTriggers — different source type (FAbilityTriggerData)
	{
		FString Tags;
		int32 Num = AbilityTriggers.Num();
		int32 Idx = 0;
		for (const FAbilityTriggerData& T : AbilityTriggers)
		{
			Tags += T.TriggerTag.ToString();
			if (++Idx < Num)
			{
				Tags += TEXT(",");
			}
		}
		Context.AddTag(FAssetRegistryTag(TEXT("AbilityTriggers"), Tags, FAssetRegistryTag::TT_Hidden));
	}
}

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

		if (UArcCoreAbilitySystemComponent* ArcASC = GetArcASC())
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
	if (UArcCoreAbilitySystemComponent* ArcASC = GetArcASC())
	{
		ArcASC->UnregisterCustomTargetRequest(GetCurrentAbilitySpecHandle());
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

		const FArcItemFragment_AbilityTags* AbilityTagsFragment = ArcItemsHelper::FindFragment<FArcItemFragment_AbilityTags>(InItemData);
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

		// Cache action pipeline from item fragment
		const FArcItemFragment_AbilityActions* ActionsFragment = ArcItemsHelper::FindFragment<FArcItemFragment_AbilityActions>(InItemData);
		if (ActionsFragment)
		{
			CachedLocalTargetActions = ActionsFragment->OnLocalTargetResultActions;
			CachedAbilityTargetActions = ActionsFragment->OnAbilityTargetResultActions;
		}
	}
}

void UArcCoreGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle
										 , const FGameplayAbilityActorInfo* ActorInfo
										 , const FGameplayAbilityActivationInfo ActivationInfo
										 , bool bReplicateEndAbility
										 , bool bWasCancelled)
{
	UE_LOG(LogArcAbility
		, Verbose
		, TEXT("[%s] EndAbility [%s]")
		, *StaticEnum<ENetRole>()->GetValueAsString(ActorInfo->AbilitySystemComponent->GetOwnerRole())
		, *GetName());

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (ArcASC)
	{
		ArcASC->ClearAbilityActivationStartTime(Handle);
	}
	
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
	// Super returns true when NOT on cooldown (i.e. can activate)
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	const bool bPassedBaseCooldown = Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	if (!bPassedBaseCooldown && CooldownTags != nullptr)
	{
		return false;
	}

	// Also check custom timer-based cooldown
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(
		ActorInfo->AbilitySystemComponent.Get());
	if (!ArcASC)
	{
		return bPassedBaseCooldown;
	}
	return !ArcASC->IsAbilityOnCooldown(Handle);
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
	if (!ArcASC)
	{
		return;
	}

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
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ActorInfo->AbilitySystemComponent);
	if (ArcASC)
	{
		ArcASC->SetAbilityActivationStartTime(Handle, FPlatformTime::Seconds());
	}

	Super::ActivateAbility(Handle
		, ActorInfo
		, ActivationInfo
		, TriggerEventData);

	if (ArcASC)
	{
		ArcASC->OnAbilityActivatedDynamic.Broadcast(Handle, this);
	}
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
		FArcGameplayEffectContext* Context = static_cast<FArcGameplayEffectContext*>(ContextHandle.Get());
		Context->AddSourceObject(SourceItemsStore);
		Context->SetSourceItemHandle(GetSourceItemHandle());
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
	return Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

FActiveGameplayEffectHandle UArcCoreGameplayAbility::GetGrantedEffectHandle()
{
	return GetAbilitySystemComponentFromActorInfo_Ensured()->FindActiveGameplayEffectHandle(GetCurrentAbilitySpecHandle());
}

FGameplayAbilitySpecHandle UArcCoreGameplayAbility::GetSpecHandle() const
{
	return GetCurrentAbilitySpecHandle();
}

float UArcCoreGameplayAbility::GetItemStat(FGameplayAttribute Attribute) const
{
	const FArcItemInstance_ItemStats* ItemStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(GetSourceItemEntryPtr());
	if (!ItemStats)
	{
		return 0.0f;
	}
	return ItemStats->GetStatValue(Attribute);
}

FArcItemId UArcCoreGameplayAbility::GetSourceItemHandle() const
{
	return SourceItemId;
}

bool UArcCoreGameplayAbility::BP_GetSourceItem(FArcItemDataHandle& OutItem)
{
	if (!SourceItemsStore)
	{
		return false;
	}
	OutItem = SourceItemsStore->GetWeakItemPtr(SourceItemId);
	return OutItem.IsValid();
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

static bool FindItemFragmentImpl(UArcCoreGameplayAbility* Ability, UScriptStruct* InFragmentType, void* OutItemDataPtr, FStructProperty* OutItemProp)
{
	FArcItemData* ItemData = Ability->GetSourceItemEntryPtr();
	if (!ItemData)
	{
		return false;
	}

	const uint8* Fragment = ArcItemsHelper::FindFragment(ItemData, InFragmentType);
	if (OutItemProp && OutItemProp->Struct && Fragment && OutItemProp->Struct == InFragmentType)
	{
		OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Fragment);
		return true;
	}
	return false;
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
	P_NATIVE_BEGIN;
	bSuccess = FindItemFragmentImpl(P_THIS, InFragmentType, OutItemDataPtr, OutItemProp);
	P_NATIVE_END;
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
	P_NATIVE_BEGIN;
	bSuccess = FindItemFragmentImpl(P_THIS, InFragmentType, OutItemDataPtr, OutItemProp);
	P_NATIVE_END;
	*(bool*)RESULT_PARAM = bSuccess;
}

float UArcCoreGameplayAbility::FindItemScalableValue(UScriptStruct* InFragmentType
	, FName PropName) const
{
	const FArcItemData* Item = GetOwnerItemEntryPtr();
	if (!Item) { return 0.0f; }

	FStructProperty* Prop = FindFProperty<FStructProperty>(InFragmentType, PropName);
	FArcScalableCurveFloat Data(Prop, InFragmentType);

	return Item->GetValue(Data);
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
		const uint8* Fragment = (uint8*)ArcItemsHelper::FindScalableItemFragment<FArcScalableFloatItemFragment>(ItemData);
		if (OutItemProp && OutItemProp->Struct && Fragment && OutItemProp->Struct == InFragmentType)
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

FArcItemDataHandle UArcCoreGameplayAbility::GetItemDataHandle() const
{
	if (!SourceItemId.IsValid() || SourceItemsStore == nullptr)
	{
		return FArcItemDataHandle();
	}
	return FArcItemDataHandle(SourceItemsStore->GetWeakItemPtr(SourceItemId));
}

const UArcItemDefinition* UArcCoreGameplayAbility::GetSourceItemData() const
{
	return NativeGetSourceItemData();
}

const UArcItemDefinition* UArcCoreGameplayAbility::NativeGetSourceItemData() const
{
	if (bRequiresItem)
	{
		const FArcItemData* Item = GetSourceItemEntryPtr();
		return Item ? Item->GetItemDefinition() : nullptr;
	}
	return nullptr;
}

const UArcItemDefinition* UArcCoreGameplayAbility::NativeGetOwnerItemData() const
{
	const FArcItemData* Item = GetOwnerItemEntryPtr();
	return Item ? Item->GetItemDefinition() : nullptr;
}

const UArcItemDefinition* UArcCoreGameplayAbility::GetOwnerItemData() const
{
	return NativeGetOwnerItemData();
}

const FArcGameplayAbilityActorInfo& UArcCoreGameplayAbility::GetArcActorInfo()
{
	return *static_cast<const FArcGameplayAbilityActorInfo*>(GetCurrentActorInfo());
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

	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.SetSetByCallerMagnitude(DataTag, Value);
	});
}


void UArcCoreGameplayAbility::ExecuteLocalTargeting(UArcTargetingObject* InTrace)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetArcASC();
	if (!ArcASC || !ArcASC->GetOwner()) { return; }

	ENetMode NM = ArcASC->GetOwner()->GetNetMode();
	EGameplayAbilityNetExecutionPolicy::Type ExecPolicy = GetNetExecutionPolicy();

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
	UArcCoreAbilitySystemComponent* ArcASC = GetArcASC();
	if (!ArcASC || !ArcASC->GetOwner()) { return; }

	ENetMode NM = ArcASC->GetOwner()->GetNetMode();
	EGameplayAbilityNetExecutionPolicy::Type ExecPolicy = GetNetExecutionPolicy();

	bool bServerExecuteTrace = (NM == ENetMode::NM_DedicatedServer && ExecPolicy ==
							   EGameplayAbilityNetExecutionPolicy::Type::ServerOnly)
								|| NM == NM_Standalone;

	if (NM == ENetMode::NM_Client || bServerExecuteTrace)
	{
		NativeOnAbilityTargetResult(TargetData);
	}

	if (NM == ENetMode::NM_Client)
	{
		ArcASC->SendRequestCustomTargets(TargetData
		, InTrace
		, GetCurrentAbilitySpecHandle());
	}
}

void UArcCoreGameplayAbility::NativeExecuteLocalTargeting(UArcTargetingObject* InTrace)
{
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
	
	for (auto& Result : TargetingResults.TargetResults)
	{
		Hits.Add(MoveTemp(Result.HitResult));
	}

	// Execute cached action pipeline for local target result
	if (CachedLocalTargetActions.Num() > 0)
	{
		FArcAbilityActionContext ActionContext;
		ActionContext.Ability = this;
		ActionContext.Handle = GetCurrentAbilitySpecHandle();
		ActionContext.ActorInfo = GetCurrentActorInfo();
		ActionContext.ActivationInfo = GetCurrentActivationInfo();
		ActionContext.HitResults = Hits;
		ActionContext.TargetingRequestHandle = TargetingRequestHandle;
		ExecuteActionList(CachedLocalTargetActions, ActionContext);
	}
	
	OnLocalTargetResult(Hits);
}

void UArcCoreGameplayAbility::NativeOnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetArcASC();
	if (!ArcASC || !ArcASC->GetOwner()) { return; }

	ENetMode NM = ArcASC->GetOwner()->GetNetMode();
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

	// Execute cached action pipeline for ability target result
	if (CachedAbilityTargetActions.Num() > 0)
	{
		FArcAbilityActionContext ActionContext;
		ActionContext.Ability = this;
		ActionContext.Handle = GetCurrentAbilitySpecHandle();
		ActionContext.ActorInfo = GetCurrentActorInfo();
		ActionContext.ActivationInfo = GetCurrentActivationInfo();
		ActionContext.TargetData = AbilityTargetData;
		ExecuteActionList(CachedAbilityTargetActions, ActionContext);
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

	const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	if (!Instance) { return ReturnHandles; }

	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);

	FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();

	FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

	FGameplayTagContainer AbilitySystemComponentTags;
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
			FPredictionKey PredKey = ActivInfo.GetActivationPredictionKey();
			bool IsValid = PredKey.IsValidForMorePrediction();

			if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
			{
				return ReturnHandles;
			}

			if (Effect != nullptr
				&& (HasAuthorityOrPredictionKey(GetCurrentActorInfo(), &ActivInfo) || IsValid))
			{
				Effect.Data->CapturedSourceTags.GetSpecTags().AppendTags(GetAssetTags());
				Effect.Data->GetContext().Get()->SetAbility(this);
				ReturnHandles.Append(ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle
					, *Effect.Data.Get()
					, ASC->GetPredictionKeyForNewAction()));
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

	const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	if (!Instance) { return ReturnHandles; }

	const TArray<FArcEffectSpecItem>& Effects = Instance->GetAllEffectSpecHandles();

	FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();

	FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

	FGameplayTagContainer AbilitySystemComponentTags;
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
			FPredictionKey PredKey = ActivInfo.GetActivationPredictionKey();
			bool IsValid = PredKey.IsValidForMorePrediction();

			if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
			{
				return ReturnHandles;
			}

			if (Effect != nullptr
				&& (HasAuthorityOrPredictionKey(GetCurrentActorInfo(), &ActivInfo) || IsValid))
			{
				Effect.Data->CapturedSourceTags.GetSpecTags().AppendTags(GetAssetTags());
				Effect.Data->GetContext().Get()->SetAbility(this);
				ReturnHandles.Append(ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle
					, *Effect.Data.Get()
					, ASC->GetPredictionKeyForNewAction()));
			}
		}
	}
	return ReturnHandles;
}

TArray<FGameplayEffectSpecHandle> UArcCoreGameplayAbility::GetEffectSpecFromItem(const FGameplayTag& EffectTag) const
{
	TArray<FGameplayEffectSpecHandle> Specs;

	const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	if (!Instance) { return Specs; }

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
				bAllSuccessfull &= ASC->RemoveActiveGameplayEffect(Handle, Stacks);
			}
		}
	}

	return bAllSuccessfull;
}

void UArcCoreGameplayAbility::ForEachEffectSpec(const FGameplayTag& EffectTag, TFunctionRef<void(FGameplayEffectSpec&)> Func) const
{
	const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(GetSourceItemEntryPtr());
	if (!Instance) { return; }
	TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);
	for (const FArcEffectSpecItem* E : Effects)
	{
		for (const FGameplayEffectSpecHandle& SpecHandle : E->Specs)
		{
			FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
			checkf(Spec != nullptr, TEXT("Invalid GameplayEffectSpec"));
			Func(*Spec);
		}
	}
}

void UArcCoreGameplayAbility::SetByCallerMagnitude(const FGameplayTag& SetByCallerTag
												   , float Magnitude
												   , const FGameplayTag& EffectTag)
{
	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.SetSetByCallerMagnitude(SetByCallerTag, Magnitude);
	});
}

void UArcCoreGameplayAbility::SetEffectDuration(float Duration
												, const FGameplayTag& EffectTag)
{
	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.SetDuration(Duration, false);
	});
}

void UArcCoreGameplayAbility::SetEffectPeriod(float Period
											  , const FGameplayTag& EffectTag)
{
	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.Period = Period;
	});
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
	, EArcAbilityActorSpawnOrigin SpawnLocationType
	, FVector CustomSpawnLocation
	, TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility
	, FArcAbilityActorHandle& OutActorHandle)
{
	if (ActorClass == nullptr)
	{
		return false;
	}

	if (!InTargetData.IsValid(0))
	{
		return false;
	}
	
	FTransform OriginTM = InTargetData.Get(0)->GetOrigin();
	
	FVector Location = InTargetData.Get(0)->GetEndPoint();
	FRotator Rotation = InTargetData.Get(0)->GetEndPointTransform().GetRotation().Rotator();

	switch (SpawnLocationType)
	{
		case EArcAbilityActorSpawnOrigin::ImpactPoint:
		{
			Location = InTargetData.Get(0)->GetEndPoint();
			
			if (const FHitResult* Hit = InTargetData.Get(0)->GetHitResult())
			{
				Location = Hit->ImpactPoint;
				Rotation = Hit->ImpactNormal.Rotation();
			}
			
			break;
		}
		case EArcAbilityActorSpawnOrigin::ActorLocation:
		{
			AActor* Actor = nullptr;
			if (InTargetData.Data.Num() > 0 && InTargetData.Data[0]->HasHitResult())
			{
				Actor = InTargetData.Data[0].Get()->GetHitResult()->GetActor();
			}
			if (!Actor)
			{
				if (const FGameplayAbilityTargetData* TargetData = InTargetData.Get(0))
				{
					if (TargetData->GetActors().Num() > 0)
					{
						Actor = TargetData->GetActors()[0].Get();
					}
				}
			}
			
			if (Actor)
			{
				Location = Actor->GetActorLocation();
			}
			break;
		}
		case EArcAbilityActorSpawnOrigin::Origin:
		{
			const bool bHasOrigin = InTargetData.Get(0)->HasOrigin();
			if (bHasOrigin)
			{
				
				OriginTM = InTargetData.Get(0)->GetOrigin();
				Location = OriginTM.GetLocation();
				Rotation = OriginTM.GetRotation().Rotator();
			}
			break;
		}
		case EArcAbilityActorSpawnOrigin::Custom:
		{
			Location = CustomSpawnLocation;
			break;
		}
	}
	
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

		if (Handle.IsValid())
		{
			TWeakObjectPtr<AActor> SpawnedActor = ArcASC->SpawnedActors[Handle];
			UArcAbilityActorComponent* AAC = SpawnedActor->FindComponentByClass<UArcAbilityActorComponent>();

			AAC->Initialize(ArcASC, this, Handle, InTargetData,  ActorGrantedAbility);	
		}
	}
	
	OutActorHandle = Handle;
	return true;
}

bool UArcCoreGameplayAbility::GetSpawnedActor(const FArcAbilityActorHandle& OutActorHandle, AActor*& OutActor)
{
	UArcCoreAbilitySystemComponent* ArcASC =  Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (ArcASC == nullptr)
	{
		OutActor = nullptr;
		return false;
	}
	
	if (ArcASC->SpawnedActors.Contains(OutActorHandle))
	{
		OutActor = ArcASC->SpawnedActors[OutActorHandle].Get();
		return true;
	}
	
	OutActor = nullptr;
	return false;
}

void UArcCoreGameplayAbility::ExecuteActionList(const TArray<FInstancedStruct>& Actions, FArcAbilityActionContext& Context)
{
	for (const FInstancedStruct& ActionStruct : Actions)
	{
		if (ActionStruct.IsValid())
		{
			const FArcAbilityAction* Action = ActionStruct.GetPtr<FArcAbilityAction>();
			if (Action)
			{
				Action->Execute(Context);
			}
		}
	}
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
