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

#pragma once

#include "ArcCoreGameplayAbility.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/ArcItemId.h"
#include "ArcItemGameplayAbility.generated.h"

class UArcItemsStoreComponent;
struct FArcItemData;
struct FArcItemDataHandle;

USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_AbilityTags : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = Tags, DisplayName="AssetTags (Default AbilityTags)", meta=(Categories="AbilityTagCategory"))
	FGameplayTagContainer AbilityTags;

	/** Abilities with these tags are cancelled when this ability is executed */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="AbilityTagCategory"))
	FGameplayTagContainer CancelAbilitiesWithTag;

	/** Abilities with these tags are blocked while this ability is active */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="AbilityTagCategory"))
	FGameplayTagContainer BlockAbilitiesWithTag;

	/** Tags to apply to activating owner while this ability is active. These are replicated if ReplicateActivationOwnedTags is enabled in AbilitySystemGlobals. */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="OwnedTagsCategory"))
	FGameplayTagContainer ActivationOwnedTags;

	/** This ability can only be activated if the activating actor/component has all of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="OwnedTagsCategory"))
	FGameplayTagContainer ActivationRequiredTags;

	/** This ability is blocked if the activating actor/component has any of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="OwnedTagsCategory"))
	FGameplayTagContainer ActivationBlockedTags;

	/** This ability can only be activated if the source actor/component has all of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="SourceTagsCategory"))
	FGameplayTagContainer SourceRequiredTags;

	/** This ability is blocked if the source actor/component has any of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="SourceTagsCategory"))
	FGameplayTagContainer SourceBlockedTags;

	/** This ability can only be activated if the target actor/component has all of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="TargetTagsCategory"))
	FGameplayTagContainer TargetRequiredTags;

	/** This ability is blocked if the target actor/component has any of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="TargetTagsCategory"))
	FGameplayTagContainer TargetBlockedTags;
};

USTRUCT()
struct ARCCORE_API FArcGetItemValue
{
	GENERATED_BODY()

public:
	virtual float Calculate(FArcItemData* InItem
							, const class UArcCoreGameplayAbility* InAbility
							, const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return 0;
	}

	float operator()(FArcItemData* InItem
					 , const class UArcCoreGameplayAbility* InAbility
					 , const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return Calculate(InItem
			, InAbility
			, ActorInfo);
	}

	virtual ~FArcGetItemValue() = default;
};

/**
 * Item-aware gameplay ability subclass. This ability requires an item source
 * (ArcItemData/ArcItemDefinition) and provides all item interop functionality:
 * fragment access, effect application from items, item stat retrieval, etc.
 *
 * Use UArcCoreGameplayAbility as base when no item dependency is needed.
 */
UCLASS()
class ARCCORE_API UArcItemGameplayAbility : public UArcCoreGameplayAbility
{
	GENERATED_BODY()

	friend struct FArcAbilityAction_ApplyEffectsFromItem;

protected:
	/* Item from which this ability has been created. */
	UPROPERTY(Transient)
	mutable TObjectPtr<UArcItemsStoreComponent> SourceItemsStore;

	/** Id of source item. */
	UPROPERTY(Transient)
	mutable FArcItemId SourceItemId;

	/** @brief Item which is source of this ability. Ie item component or base item. */
	mutable FArcItemData* SourceItemPtr = nullptr;

	UPROPERTY(Transient)
	TArray<FInstancedStruct> CachedLocalTargetActions;

	UPROPERTY(Transient)
	TArray<FInstancedStruct> CachedAbilityTargetActions;

	UPROPERTY(Transient)
	TArray<FInstancedStruct> CachedActivateActions;

	UPROPERTY(Transient)
	TArray<FInstancedStruct> CachedEndActions;

	FGameplayTag CurrentItemSlot;

	bool bBPOnAvatarSetCalled = false;

public:
	UArcItemGameplayAbility();

	virtual void BeginDestroy() override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo
							   , const FGameplayAbilitySpec& Spec) override;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo
							 , const FGameplayAbilitySpec& Spec) override;

	/**
	 * Called when source item is attached to a slot.
	 * Sets up item references, tags from item fragment, and caches action pipelines.
	 */
	virtual void OnAddedToItemSlot(const FGameplayAbilityActorInfo* ActorInfo
								  , const FGameplayAbilitySpec* Spec
								  , const FGameplayTag& InItemSlot
								  , const FArcItemData* InItemData);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle
									, const FGameplayAbilityActorInfo* ActorInfo
									, const FGameplayTagContainer* SourceTags = nullptr
									, const FGameplayTagContainer* TargetTags = nullptr
									, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle
														   , const FGameplayAbilityActorInfo* ActorInfo) const override;

	virtual FGameplayEffectSpecHandle MakeOutgoingGameplayEffectSpec(const FGameplayAbilitySpecHandle Handle
																	 , const FGameplayAbilityActorInfo* ActorInfo
																	 , const FGameplayAbilityActivationInfo ActivationInfo
																	 , TSubclassOf<UGameplayEffect> GameplayEffectClass
																	 , float Level = 1.f) const override;

	virtual void ProcessLocalTargetActions(const TArray<FHitResult>& Hits, FTargetingRequestHandle TargetingRequestHandle) override;
	virtual void ProcessAbilityTargetActions(const FGameplayAbilityTargetDataHandle& AbilityTargetData) override;
	virtual void ProcessActivateActions() override;
	virtual void ProcessEndActions(bool bWasCancelled) override;

public:
	const FGameplayTag& GetCurrentItemSlot() const
	{
		return CurrentItemSlot;
	}

	void AssingItemFromAbility(const UArcItemGameplayAbility* Other) const;

	UArcItemsStoreComponent* GetSourceItemStore() const
	{
		return SourceItemsStore;
	}

	/*
	 * An actual source of this ability (the item which granted it)
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	FArcItemId GetSourceItemHandle() const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta = (ExpandEnumAsExecs = "ReturnValue"))
	bool BP_GetSourceItem(FArcItemDataHandle& OutItem);

	/**
	 * @return Item which is source of this ability.
	 */
	FArcItemData* GetSourceItemEntryPtr() const;

	/**
	 * @return Item which is owner of this ability. Can be the same as source, or Owner of Source item if source is attached to other item.
	 */
	const FArcItemData* GetOwnerItemEntryPtr() const;

	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core|Ability", meta = (CustomStructureParam = "OutFragment", ExpandBoolAsExecs = "ReturnValue"))
	bool FindItemFragment(UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcItemFragment")) UScriptStruct* InFragmentType
						  , int32& OutFragment);

	DECLARE_FUNCTION(execFindItemFragment);

	UFUNCTION(BlueprintPure, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core|Ability", meta = (CustomStructureParam = "OutFragment"))
	bool FindItemFragmentPure(UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcItemFragment")) UScriptStruct* InFragmentType
						  , int32& OutFragment);

	DECLARE_FUNCTION(execFindItemFragmentPure);

	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core|Ability", meta = (CustomStructureParam = "OutFragment", ExpandBoolAsExecs = "ReturnValue"))
	bool FindScalableItemFragment(UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcScalableFloatItemFragment")) UScriptStruct* InFragmentType
						  , int32& OutFragment);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	float FindItemScalableValue(UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcScalableFloatItemFragment")) UScriptStruct* InFragmentType, FName PropName) const;

	DECLARE_FUNCTION(execFindScalableItemFragment);

	/**
	 * \brief Get Stat value of item which was source of this ability.
	 * \param Attribute Stat to take attribute from.
	 * \return Current stat value.
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	float GetItemStat(FGameplayAttribute Attribute) const;

	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Ability"
		, meta = (ItemClass = "/Script/ArcCore.ArcItemsStoreComponent", ScriptName = "GetItemsComponent",
			DeterminesOutputType = "IC"))
	UArcItemsStoreComponent* GetItemsStoreComponent(TSubclassOf<UArcItemsStoreComponent> IC) const;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	FArcItemDataHandle GetItemDataHandle() const;

	/**
	 * @brief Get Item Definition which is source of this ability.
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const UArcItemDefinition* GetSourceItemData() const;

	const UArcItemDefinition* NativeGetSourceItemData() const;
	const UArcItemDefinition* NativeGetOwnerItemData() const;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const UArcItemDefinition* GetOwnerItemData() const;

	/*
	 * Calculate value from either Source Item or Owner item, using provided Getter class
	 * which should implement custom calculated to return value
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	float GetItemValue(
		UPARAM(meta = (BaseStruct = "/Script/ArcCore.ArcGetItemValue")) FInstancedStruct InMagnitudeCalculation) const;

	/*
	 * Similar to above but set magnitude directly by using SetByCaller on Gameplay
	 * Effect. Shouldn't be used to calculate damage unless you don't care about
	 * target/source attributes. As only owner is known within this calculation
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	void SetEffectMagnitude(const FGameplayTag& DataTag
							, UPARAM(meta = (BaseStruct = "/Script/ArcCore.ArcGetItemValue")) FInstancedStruct InMagnitudeCalculation
							, const FGameplayTag& EffectTag);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	TArray<FActiveGameplayEffectHandle> ApplyEffectsFromItemByTags(const FGameplayTagContainer& EffectTagContainer
																   , const FGameplayAbilityTargetDataHandle& InHitResults);

	/*
	 * Apply effects from Source Item of this ability.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	TArray<FActiveGameplayEffectHandle> ApplyEffectsFromItem(const FGameplayTag& EffectTag
															 , const FGameplayAbilityTargetDataHandle& InHitResults);

	/*
	 * Apply all effects from Source Item of this ability.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	TArray<FActiveGameplayEffectHandle> ApplyAllEffectsFromItem(const FGameplayAbilityTargetDataHandle& InHitResults);

	/**
	 * Get effects from associated ItemData which match Tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	TArray<FGameplayEffectSpecHandle> GetEffectSpecFromItem(const FGameplayTag& EffectTag) const;

protected:
	void ForEachEffectSpec(const FGameplayTag& EffectTag, TFunctionRef<void(FGameplayEffectSpec&)> Func) const;

	/**
	 * Get Effect with given tag from Item with which this ability is associated, and sets SetByCallerMagnitude
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	void SetByCallerMagnitude(const FGameplayTag& SetByCallerTag
							  , float Magnitude
							  , const FGameplayTag& EffectTag);

	/*
	 * Get Effect with given tag from Item with which this ability is associated, and sets duration
	 * for it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	void SetEffectDuration(float Duration
						   , const FGameplayTag& EffectTag);

	/*
	 * Get Effect with given tag from Item with which this ability is associated, and sets period
	 * for it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	void SetEffectPeriod(float Period
						 , const FGameplayTag& EffectTag);
};
