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

#pragma once

#include "ArcCoreAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"

#include "Types/TargetingSystemTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/ArcItemId.h"
#include "ArcCoreGameplayAbility.generated.h"

class UArcCoreAbilitySystemComponent;
class UScriptStruct;
class UArcTargetingObject;
class UArcTargetingFilterPreset;
class UArcActorGameplayAbility;
struct FArcGameplayAbilityActorInfo;
struct FArcCustomTraceDataHandle;
struct FArcItemData;
struct FArcItemDataHandle;

DECLARE_LOG_CATEGORY_EXTERN(LogArcAbility, Log, Log);

USTRUCT(BlueprintType)
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

	virtual ~FArcGetItemValue()
	{
	}
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcAbilityActivationTimeDelegate
	, UArcCoreGameplayAbility* /*InAbility*/
	, const float /*In*ActivationTime*/
);

USTRUCT(BlueprintType)
struct ARCCORE_API FArcCustomAbilityCooldown
{
	GENERATED_BODY()

public:
	virtual float GetCooldown(const FGameplayAbilitySpecHandle Handle
							  , FGameplayAbilitySpec* InAbilitySpec
							  , const FGameplayAbilityActorInfo* ActorInfo
							  , const FGameplayAbilityActivationInfo ActivationInfo) const
	{
		return 0.0f;
	}

	virtual ~FArcCustomAbilityCooldown() = default;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityActivationTime
{
	GENERATED_BODY()

public:
	virtual float UpdateActivationTime(const UArcCoreGameplayAbility* InAbility
									   , FGameplayTagContainer ActivationTimeTags) const
	{
		return 0.0f;
	}

	virtual ~FArcAbilityActivationTime() = default;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityCooldown_ScalableFloat : public FArcCustomAbilityCooldown
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	FScalableFloat Cooldown;

public:
	virtual float GetCooldown(const FGameplayAbilitySpecHandle Handle
							  , FGameplayAbilitySpec* InAbilitySpec
							  , const FGameplayAbilityActorInfo* ActorInfo
							  , const FGameplayAbilityActivationInfo ActivationInfo) const override
	{
		return Cooldown.GetValueAtLevel(InAbilitySpec->Level);
	}

	virtual ~FArcAbilityCooldown_ScalableFloat() override
	{
	}
};

class UArcItemsStoreComponent;
class UArcHeroComponentBase;
class UArcCameraMode;

UENUM(BlueprintType)
enum class EArcClientServer : uint8
{
	Client
	, Server
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcCoreGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	/*
	 * Should ability be activate on Trigger Event ?
	 * Enhanced Input have three relevelant States. Started, Trigger and Completed.
	 * W treat Started and Completed as Pressed and Released. Trigger can happen on some special conditions like
	 * "hold button for X time, on press three buttons in order".
	 * For compatibility we disallow activation on trigger. Ability can still listen to for tirgger events
	 * after activation.
	 * It is mutually exclusive with activation on Started. If this is set to true ability will only activate on Triggered event.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bActivateOnTrigger = false;

public:
	bool GetActivateOnTrigger() const
	{
		return bActivateOnTrigger;
	}

protected:
	// If true ability won't activate if the item required by ability is not yet
	// accessible (mainly issue on replicated clients).
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	bool bRequiresItem = false;

	/**
	 * Only used internally to track if @link BP_OnAvatarSet Has already been called. This can prevent
	 * something set twice in blueprint by accident.
	 */
	bool bBPOnAvatarSetCalled = false;
	
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcAbilityCost"), Category = "Costs")
	TArray<FInstancedStruct> AdditionalCosts;

	/*
	 * Custom cooldown tags, which block ability activation
	 */
	UPROPERTY(EditAnywhere, Category = "Cooldown")
	FGameplayTagContainer CustomCooldownTags;

	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (BaseStruct = "/Script/ArcCore.ArcCustomAbilityCooldown"))
	FInstancedStruct CustomCooldownDuration;

	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (BaseStruct = "/Script/ArcCore.ArcAbilityActivationTime"))
	FInstancedStruct ActivationTime;

public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	float GetActivationTime() const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void UpdateActivationTime(const FGameplayTagContainer& ActivationTimeTags);
	
	FArcAbilityActivationTimeDelegate OnActivationTimeChanged;

protected:
	/* Item from which this ability has been created. Might be null, but shouldn't be. */
	UPROPERTY(Transient)
	mutable TObjectPtr<UArcItemsStoreComponent> SourceItemsStore;

protected:
	/** Id of source item. */
	UPROPERTY(Transient)
	mutable FArcItemId SourceItemId;

	/** @brief Item which is source of this ability. Ie item component or base item. */
	mutable FArcItemData* SourceItemPtr = nullptr;

	virtual void K2_ExecuteGameplayCueWithParams(FGameplayTag GameplayCueTag
												 , const FGameplayCueParameters& GameplayCueParameters) override;
	// Current camera mode set by the ability.
	TSubclassOf<UArcCameraMode> ActiveCameraMode;

	FGameplayTag CurrentItemSlot;

public:
	const FGameplayTag& GetCurrentItemSlot() const
	{
		return CurrentItemSlot;
	}
	
public:
	UArcCoreGameplayAbility();

	virtual void BeginDestroy() override;

	//FAssetRegistryTagsContext
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;
#endif // #if WITH_EDITOR

	/**
	 * Called when ability is added to Ability System. Overriden it, to also initialize
	 * ability with targeting system by default.
	 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo
							   , const FGameplayAbilitySpec& Spec) override;

	/**
	 * Called when ability is being removed from Ability System. Overriden to
	 * unregister from targeting system.
	 */
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo
								 , const FGameplayAbilitySpec& Spec) override;

	/**
	 * We set here Source Item in case it was not catch up in OnGiveAbility.
	 * We also called @link BP_OnAvatarSet from here, is can get our source item.
	 *
	 * We also try to register with targeting system here, in case it failed with @link OnGiveAbility;
	 */
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo
							 , const FGameplayAbilitySpec& Spec) override;

	/**
	 * Version, which should be overriden by subclasses if needed. It is called when Item source have been set correctly.
	 */
	virtual void OnAvatarSetSafe(const FGameplayAbilityActorInfo* ActorInfo
							 , const FGameplayAbilitySpec& Spec) {}
	/** 
	 * Called from @link FArcSlotData::OnAddedToSlotFromReplication
	 * When source item is attached to another item, the owner of attachment is treated as source of this ability.
	 * We also try to call @link BP_OnAvatarSet from here in case we failed to call it from @link OnAvatarSet 
	 */
	virtual void OnAddedToItemSlot(const FGameplayAbilityActorInfo* ActorInfo
								  , const FGameplayAbilitySpec* Spec
								  , const FGameplayTag& InItemSlot
								  , const FArcItemData* InItemData);
	
	virtual void PostNetInit() override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle
							, const FGameplayAbilityActorInfo* ActorInfo
							, const FGameplayAbilityActivationInfo ActivationInfo
							, bool bReplicateEndAbility
							, bool bWasCancelled) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle
									, const FGameplayAbilityActorInfo* ActorInfo
									, const FGameplayTagContainer* SourceTags = nullptr
									, const FGameplayTagContainer* TargetTags = nullptr
									, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle
							   , const FGameplayAbilityActorInfo* ActorInfo
							   , OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle
							   , const FGameplayAbilityActorInfo* ActorInfo
							   , const FGameplayAbilityActivationInfo ActivationInfo) const override;


	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
protected:
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle
						   , const FGameplayAbilityActorInfo* ActorInfo
						   , OUT FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle
						   , const FGameplayAbilityActorInfo* ActorInfo
						   , const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle
														   , const FGameplayAbilityActorInfo* ActorInfo) const override;

	virtual FGameplayEffectSpecHandle MakeOutgoingGameplayEffectSpec(const FGameplayAbilitySpecHandle Handle
																	 , const FGameplayAbilityActorInfo* ActorInfo
																	 , const FGameplayAbilityActivationInfo ActivationInfo
																	 , TSubclassOf<UGameplayEffect> GameplayEffectClass
																	 , float Level = 1.f) const override;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	FGameplayEffectContextHandle BP_MakeEffectContext() const;

public:
	void AssingItemFromAbility(const UArcCoreGameplayAbility* Other) const;

	virtual void InputTriggered(const FGameplayAbilitySpecHandle Handle
								, const FGameplayAbilityActorInfo* ActorInfo
								, const FGameplayAbilityActivationInfo ActivationInfo)
	{
	};

protected:
	UArcHeroComponentBase* GetHeroComponentFromActorInfo() const;

	UArcCoreAbilitySystemComponent* GetArcASC() const;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core", meta = (DisplayName = "On Avatar Set"))
	void BP_OnAvatarSet(const FGameplayAbilityActorInfo& ActorInfo
						, const FGameplayAbilitySpec& Spec);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core", meta = (DisplayName = "On Give Ability"))
	void BP_OnGiveAbility(const FGameplayAbilityActorInfo& ActorInfo
						  , const FGameplayAbilitySpec& Spec);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void BP_OnRemoveAbility(FGameplayAbilitySpec AbilitySpec);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void BP_OnCanActivateAbilitySuccess() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void BP_OnCanActivateAbilityFailed() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void BP_OnCheckCostSuccess() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void BP_OnCheckCostFailed() const;

	UFUNCTION(BlueprintPure, Category = "Arc Core")
	FActiveGameplayEffectHandle GetGrantedEffectHandle();
	
public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	FGameplayAbilitySpecHandle GetSpecHandle() const;
	
	/**
	 * \brief Get Stat value of item which was source of this ability.
	 * \param Attribute Stat to take attribute from.
	 * \return Current stat value.
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	float GetItemStat(FGameplayAttribute Attribute) const;


	UArcItemsStoreComponent* GetSourceItemStore() const
	{
		return SourceItemsStore;
	}

	/*
	 * An actruall source of this ability (the item which granted it)
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
	
	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Ability"
		, meta = (ItemClass = "/Script/ArcCore.ArcItemsStoreComponent", ScriptName = "GetItemsComponent",
			DeterminesOutputType = "IC"))
	UArcItemsStoreComponent* GetItemsStoreComponent(TSubclassOf<UArcItemsStoreComponent> IC) const;

	/**
	 * @brief Get Item Definition which is source of this ability.
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const UArcItemDefinition* GetSourceItemData() const;

	const UArcItemDefinition* NativeGetSourceItemData() const;
	const UArcItemDefinition* NativeGetOwnerItemData() const;
	
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const UArcItemDefinition* GetOwnerItemData() const;
	
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const FArcGameplayAbilityActorInfo& GetArcActorInfo();

	template <typename T>
	const T* GetTActorInfo() const
	{
		return static_cast<const T*>(GetCurrentActorInfo());
	}

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	class AArcCoreCharacter* GetArcCharacterFromActorInfo() const;

	/*
	 * Calculate value from either Source Item or Owner item, using provided Getter class
	 * which should implement custom calculated to return value
	 */
	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	float GetItemValue(
		UPARAM(meta = (BaseStruct = "/Script/ArcCore.ArcGetItemValue")) FInstancedStruct InMagnitudeCalculation) const;

	/*
	 * Similiar to above but set magnited directly by using SetByCaller on Gameplay
	 * Effect. Shouldn't be used to calculate dmage unless you don't care about
	 * target/source attributes attributes. As only owner is known within this calulcation
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta = (HidePin = "Target", DefaultToSelf = "Target", AdvancedDisplay = "bUseOwnerItem", BlueprintProtected))
	void SetEffectMagnitude(const FGameplayTag& DataTag
							, UPARAM(meta = (BaseStruct = "/Script/ArcCore.ArcGetItemValue")) FInstancedStruct InMagnitudeCalculation
							, const FGameplayTag& EffectTag);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	bool HasAuthority() const;

	void NativeExecuteLocalTargeting(UArcTargetingObject* InTrace);

	/**
	 * @brief Perform targeting, but does not convert hits to GameplayAbilityTargetData or
	 * send them to server. This will trigger OnLocalTargetResult, from where you can use static function to convert HitResults into
	 * GameplayAbilityTargetData and then use SendTargetingResult to Send them to server
	 * and trigger OnAbilityTargetResult
	 * 
	 * @param CustomDataHandle Required. Handle to store custom required by this
	 * TargetingObject
	 * 
	 * @param FilterHandle Option. Filter Handle which will perform HitResult filtering
	 * 
	 * @param InTrace Required. Targeting object used to perform targeting
	 * 
	 * @param CustomData Required. Custom FArcCustomTraceData which will be used by
	 * 
	 * targeting
	 * @return Array of HitResults.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void ExecuteLocalTargeting(UArcTargetingObject* InTrace);

	/**
	 * @brief Send GameplayAbilityTargetData to server and trigger OnAbilityTargetResult.
	 * @param CustomDataHandle Required. Handle to custom data handle used by targeting
	 * @param TargetData Required. AbilityTargetData send to the server
	 * @param InTrace Required. TargetingObject used to perform targeting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void SendTargetingResult(const FGameplayAbilityTargetDataHandle& TargetData
							 , UArcTargetingObject* InTrace);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnLocalTargetResult(const TArray<FHitResult>& OutHits);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData
							 , EArcClientServer Mode);

	virtual void NativeOnLocalTargetResult(FTargetingRequestHandle TargetingRequestHandle);

	virtual void NativeOnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData);

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

public:
	/**
	 * Get effects from associated ItemData which match Tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	TArray<FGameplayEffectSpecHandle> GetEffectSpecFromItem(const FGameplayTag& EffectTag) const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	bool RemoveGameplayEffectFromTarget(FGameplayAbilityTargetDataHandle TargetData, FActiveGameplayEffectHandle Handle, int32 Stacks = -1);
	
protected:
	/**
	 * Get Effect with given tag from Item with which this ability is associated, and sets SetByCallerMagnitde
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

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	virtual float GetAnimationPlayRate() const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta=(ExpandBoolAsExecs = "ReturnValue"))
	bool SpawnAbilityActor(TSubclassOf<AActor> ActorClass
						   , FGameplayAbilityTargetingLocationInfo TargetLocation
						   , const FGameplayAbilityTargetDataHandle& InTargetData
						   , TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta=(ExpandBoolAsExecs = "ReturnValue"))
	bool SpawnAbilityActorTargetData(TSubclassOf<AActor> ActorClass
						   , const FGameplayAbilityTargetDataHandle& InTargetData
						   , TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility);
	
public:
	const FGameplayTagContainer* GetAbilityTags() const
	{
		return &GetAssetTags();
	};

	const FGameplayTagContainer& GetCustomCooldownTags() const
	{
		return CustomCooldownTags;
	};

	const FGameplayTagContainer& GetCancelAbilitiesWithTag() const
	{
		return CancelAbilitiesWithTag;
	};

	const FGameplayTagContainer& GetBlockAbilitiesWithTag() const
	{
		return BlockAbilitiesWithTag;
	};

	const FGameplayTagContainer& GetActivationOwnedTags() const
	{
		return ActivationOwnedTags;
	};

	const FGameplayTagContainer& GetActivationRequiredTags() const
	{
		return ActivationRequiredTags;
	};

	const FGameplayTagContainer& GetActivationBlockedTags() const
	{
		return ActivationBlockedTags;
	};

	const FGameplayTagContainer& GetSourceRequiredTags() const
	{
		return SourceRequiredTags;
	};

	const FGameplayTagContainer& GetSourceBlockedTags() const
	{
		return SourceBlockedTags;
	};

	const FGameplayTagContainer& GetTargetRequiredTags() const
	{
		return TargetRequiredTags;
	};

	const FGameplayTagContainer& GetTargetBlockedTags() const
	{
		return TargetBlockedTags;
	};

	UGameplayTask* GetTaskByName(FName TaskName) const;
};
