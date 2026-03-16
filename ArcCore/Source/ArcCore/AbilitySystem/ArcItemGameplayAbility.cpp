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

#include "ArcCore/AbilitySystem/ArcItemGameplayAbility.h"

#include "AbilitySystemGlobals.h"
#include "ArcAbilitiesBPF.h"
#include "ArcAbilitiesTypes.h"
#include "ArcGameplayEffectContext.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemsStoreComponent.h"

#include "Fragments/ArcItemFragment_AbilityActions.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

UArcItemGameplayAbility::UArcItemGameplayAbility()
{
}

void UArcItemGameplayAbility::BeginDestroy()
{
	SourceItemId.Reset();
	SourceItemPtr = nullptr;
	Super::BeginDestroy();
}

void UArcItemGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo
											, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (IsInstantiated())
	{
		if (UArcItemsStoreComponent* IC = Cast<UArcItemsStoreComponent>(GetCurrentSourceObject()))
		{
			SourceItemsStore = IC;
		}
	}
}

void UArcItemGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo
										  , const FGameplayAbilitySpec& Spec)
{
	if (IsInstantiated())
	{
		if (UArcItemsStoreComponent* IC = Cast<UArcItemsStoreComponent>(GetCurrentSourceObject()))
		{
			SourceItemsStore = IC;
		}

		// If we have an item store, defer BP_OnAvatarSet/OnAvatarSetSafe to OnAddedToItemSlot
		if (SourceItemsStore != nullptr)
		{
			UGameplayAbility::OnAvatarSet(ActorInfo, Spec);
			return;
		}
	}

	// No item store — fall through to base which calls OnAvatarSetSafe + BP_OnAvatarSet
	Super::OnAvatarSet(ActorInfo, Spec);
}

void UArcItemGameplayAbility::OnAddedToItemSlot(const FGameplayAbilityActorInfo* ActorInfo
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
			CachedLocalTargetActions = ActionsFragment->GetLocalTargetResultActions();
			CachedAbilityTargetActions = ActionsFragment->GetAbilityTargetResultActions();
			CachedActivateActions = ActionsFragment->GetActivateActions();
			CachedEndActions = ActionsFragment->GetEndActions();
		}
	}
}

bool UArcItemGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle
												 , const FGameplayAbilityActorInfo* ActorInfo
												 , const FGameplayTagContainer* GameplayTags
												 , const FGameplayTagContainer* TargetTags
												 , FGameplayTagContainer* OptionalRelevantTags) const
{
	if (GetSourceItemEntryPtr() == nullptr)
	{
		return false;
	}

	return Super::CanActivateAbility(Handle
		, ActorInfo
		, GameplayTags
		, TargetTags
		, OptionalRelevantTags);
}

FGameplayEffectContextHandle UArcItemGameplayAbility::MakeEffectContext(const FGameplayAbilitySpecHandle Handle
																		, const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayEffectContextHandle ContextHandle = Super::MakeEffectContext(Handle, ActorInfo);
	if (SourceItemsStore)
	{
		FArcGameplayEffectContext* Context = static_cast<FArcGameplayEffectContext*>(ContextHandle.Get());
		Context->AddSourceObject(SourceItemsStore);
		Context->SetSourceItemHandle(GetSourceItemHandle());
		Context->SetSourceItemPtr(GetSourceItemEntryPtr());
	}
	return ContextHandle;
}

FGameplayEffectSpecHandle UArcItemGameplayAbility::MakeOutgoingGameplayEffectSpec(
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

	return NewSpecHandle;
}

void UArcItemGameplayAbility::ProcessLocalTargetActions(const TArray<FHitResult>& Hits, FTargetingRequestHandle TargetingRequestHandle)
{
	if (CachedLocalTargetActions.Num() > 0)
	{
		FArcAbilityActionContext ActionContext;
		ActionContext.Ability = this;
		ActionContext.Handle = GetCurrentAbilitySpecHandle();
		ActionContext.ActorInfo = GetCurrentActorInfo();
		ActionContext.ActivationInfo = GetCurrentActivationInfo();
		ActionContext.HitResults = Hits;
		ActionContext.TargetData = UArcAbilitiesBPF::AbilityTargetDataFromHitResultArray(Hits);
		ActionContext.TargetingRequestHandle = TargetingRequestHandle;
		ExecuteActionList(CachedLocalTargetActions, ActionContext);
	}
}

void UArcItemGameplayAbility::ProcessAbilityTargetActions(const FGameplayAbilityTargetDataHandle& AbilityTargetData)
{
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

void UArcItemGameplayAbility::ProcessActivateActions()
{
	if (CachedActivateActions.Num() > 0)
	{
		FArcAbilityActionContext ActionContext;
		ActionContext.Ability = this;
		ActionContext.Handle = GetCurrentAbilitySpecHandle();
		ActionContext.ActorInfo = GetCurrentActorInfo();
		ActionContext.ActivationInfo = GetCurrentActivationInfo();
		ExecuteActionList(CachedActivateActions, ActionContext);
	}
}

void UArcItemGameplayAbility::ProcessEndActions(bool bWasCancelled)
{
	if (CachedEndActions.Num() > 0)
	{
		FArcAbilityActionContext ActionContext;
		ActionContext.Ability = this;
		ActionContext.Handle = GetCurrentAbilitySpecHandle();
		ActionContext.ActorInfo = GetCurrentActorInfo();
		ActionContext.ActivationInfo = GetCurrentActivationInfo();
		ActionContext.bWasCancelled = bWasCancelled;
		ExecuteActionList(CachedEndActions, ActionContext);
	}
}

void UArcItemGameplayAbility::AssingItemFromAbility(const UArcItemGameplayAbility* Other) const
{
	if (Other->SourceItemsStore)
	{
		SourceItemsStore = Other->SourceItemsStore;
		SourceItemId = Other->GetSourceItemHandle();
		SourceItemPtr = Other->SourceItemPtr;
	}
}

FArcItemId UArcItemGameplayAbility::GetSourceItemHandle() const
{
	return SourceItemId;
}

bool UArcItemGameplayAbility::BP_GetSourceItem(FArcItemDataHandle& OutItem)
{
	if (!SourceItemsStore)
	{
		return false;
	}
	OutItem = SourceItemsStore->GetWeakItemPtr(SourceItemId);
	return OutItem.IsValid();
}

FArcItemData* UArcItemGameplayAbility::GetSourceItemEntryPtr() const
{
	if (SourceItemsStore == nullptr)
	{
		return nullptr;
	}
	/**
	 * Abilities sources (items) are kept inside ItemsComponent, and are added here when
	 * Item is added to slot, and when that slot is replicated.
	 * This definitely can happen out of order, Abilities may arrive sooner than replicated slots, or vice versa.
	 * We lazy assign SourceItem when trying to get it.
	 */
	if (SourceItemPtr == nullptr)
	{
		SourceItemPtr = SourceItemsStore->GetItemPtr(SourceItemId);
	}
	return SourceItemPtr;
}

const FArcItemData* UArcItemGameplayAbility::GetOwnerItemEntryPtr() const
{
	FArcItemData* ItemPtr = GetSourceItemEntryPtr();
	if (ItemPtr && ItemPtr->GetOwnerPtr())
	{
		return ItemPtr->GetOwnerPtr();
	}

	return ItemPtr;
}

static bool FindItemFragmentImpl(UArcItemGameplayAbility* Ability, UScriptStruct* InFragmentType, void* OutItemDataPtr, FStructProperty* OutItemProp)
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

DEFINE_FUNCTION(UArcItemGameplayAbility::execFindItemFragment)
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

DEFINE_FUNCTION(UArcItemGameplayAbility::execFindItemFragmentPure)
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

float UArcItemGameplayAbility::FindItemScalableValue(UScriptStruct* InFragmentType
	, FName PropName) const
{
	const FArcItemData* Item = GetOwnerItemEntryPtr();
	if (!Item) { return 0.0f; }

	FStructProperty* Prop = FindFProperty<FStructProperty>(InFragmentType, PropName);
	FArcScalableCurveFloat Data(Prop, InFragmentType);

	return Item->GetValue(Data);
}

DEFINE_FUNCTION(UArcItemGameplayAbility::execFindScalableItemFragment)
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

float UArcItemGameplayAbility::GetItemStat(FGameplayAttribute Attribute) const
{
	const FArcItemInstance_ItemStats* ItemStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(GetSourceItemEntryPtr());
	if (!ItemStats)
	{
		return 0.0f;
	}
	return ItemStats->GetStatValue(Attribute);
}

UArcItemsStoreComponent* UArcItemGameplayAbility::GetItemsStoreComponent(TSubclassOf<UArcItemsStoreComponent> IC) const
{
	return SourceItemsStore;
}

FArcItemDataHandle UArcItemGameplayAbility::GetItemDataHandle() const
{
	if (!SourceItemId.IsValid() || SourceItemsStore == nullptr)
	{
		return FArcItemDataHandle();
	}
	return FArcItemDataHandle(SourceItemsStore->GetWeakItemPtr(SourceItemId));
}

const UArcItemDefinition* UArcItemGameplayAbility::GetSourceItemData() const
{
	return NativeGetSourceItemData();
}

const UArcItemDefinition* UArcItemGameplayAbility::NativeGetSourceItemData() const
{
	const FArcItemData* Item = GetSourceItemEntryPtr();
	return Item ? Item->GetItemDefinition() : nullptr;
}

const UArcItemDefinition* UArcItemGameplayAbility::NativeGetOwnerItemData() const
{
	const FArcItemData* Item = GetOwnerItemEntryPtr();
	return Item ? Item->GetItemDefinition() : nullptr;
}

const UArcItemDefinition* UArcItemGameplayAbility::GetOwnerItemData() const
{
	return NativeGetOwnerItemData();
}

float UArcItemGameplayAbility::GetItemValue(FInstancedStruct InMagnitudeCalculation) const
{
	checkf(InMagnitudeCalculation.IsValid(), TEXT("Instanced Struct Is Not Valid"));

	checkf(InMagnitudeCalculation.GetScriptStruct()->IsChildOf( FArcGetItemValue::StaticStruct()), TEXT("Instanced Struct Is Not Child of FArcGetItemValue"));

	return InMagnitudeCalculation.GetPtr<FArcGetItemValue>()->Calculate(GetSourceItemEntryPtr()
		, this
		, GetCurrentActorInfo());
}

void UArcItemGameplayAbility::SetEffectMagnitude(const FGameplayTag& DataTag
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

TArray<FActiveGameplayEffectHandle> UArcItemGameplayAbility::ApplyEffectsFromItemByTags(const FGameplayTagContainer& EffectTagContainer
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

TArray<FActiveGameplayEffectHandle> UArcItemGameplayAbility::ApplyEffectsFromItem(const FGameplayTag& EffectTag
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

TArray<FActiveGameplayEffectHandle> UArcItemGameplayAbility::ApplyAllEffectsFromItem(
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

TArray<FGameplayEffectSpecHandle> UArcItemGameplayAbility::GetEffectSpecFromItem(const FGameplayTag& EffectTag) const
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

void UArcItemGameplayAbility::ForEachEffectSpec(const FGameplayTag& EffectTag, TFunctionRef<void(FGameplayEffectSpec&)> Func) const
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

void UArcItemGameplayAbility::SetByCallerMagnitude(const FGameplayTag& SetByCallerTag
												   , float Magnitude
												   , const FGameplayTag& EffectTag)
{
	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.SetSetByCallerMagnitude(SetByCallerTag, Magnitude);
	});
}

void UArcItemGameplayAbility::SetEffectDuration(float Duration
												, const FGameplayTag& EffectTag)
{
	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.SetDuration(Duration, false);
	});
}

void UArcItemGameplayAbility::SetEffectPeriod(float Period
											  , const FGameplayTag& EffectTag)
{
	ForEachEffectSpec(EffectTag, [&](FGameplayEffectSpec& Spec)
	{
		Spec.Period = Period;
	});
}
