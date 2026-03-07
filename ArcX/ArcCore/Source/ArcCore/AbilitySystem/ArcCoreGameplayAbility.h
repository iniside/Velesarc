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

#include "ArcCoreAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"

#include "Types/TargetingSystemTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilityAction.h"
#include "ArcCoreGameplayAbility.generated.h"

class UArcCoreAbilitySystemComponent;
class UScriptStruct;
class UArcTargetingObject;
class UArcTargetingFilterPreset;
class UArcActorGameplayAbility;
struct FArcGameplayAbilityActorInfo;

DECLARE_LOG_CATEGORY_EXTERN(LogArcAbility, Log, Log);

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
	virtual float GetActivationTime(const UArcCoreGameplayAbility* InAbility) const
	{
		return 0.0f;
	}

	virtual float UpdateActivationTime(const UArcCoreGameplayAbility* InAbility
									   , FGameplayTagContainer ActivationTimeTags) const
	{
		return 0.0f;
	}

	virtual ~FArcAbilityActivationTime() = default;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityActivationTime_Static : public FArcAbilityActivationTime
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float ActivationTime = 4;

	virtual float GetActivationTime(const UArcCoreGameplayAbility* InAbility) const
	{
		return ActivationTime;
	}

	virtual float UpdateActivationTime(const UArcCoreGameplayAbility* InAbility
									   , FGameplayTagContainer ActivationTimeTags) const override
	{
		return ActivationTime;
	}

	virtual ~FArcAbilityActivationTime_Static() override = default;
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

	virtual ~FArcAbilityCooldown_ScalableFloat() override = default;
};

class UArcHeroComponentBase;
class UArcCameraMode;

UENUM(BlueprintType)
enum class EArcClientServer : uint8
{
	Client
	, Server
};

UENUM(BlueprintType)
enum class EArcAbilityActorSpawnOrigin : uint8
{
	ImpactPoint,
	ActorLocation,
	Origin,
	Custom
};

/**
 * Base gameplay ability class. Does not depend on item system.
 * For item-aware abilities, use UArcItemGameplayAbility.
 */
UCLASS()
class ARCCORE_API UArcCoreGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

	friend struct FArcAbilityAction_ApplyCost;
	friend struct FArcAbilityAction_ExecuteGlobalTargeting;
	friend struct FArcGameplayAbilityApplyCostTask;
	friend struct FArcGameplayAbilityCheckCostCondition;

protected:
	/*
	 * Should ability be activate on Trigger Event ?
	 * Enhanced Input have three relevant States. Started, Trigger and Completed.
	 * We treat Started and Completed as Pressed and Released. Trigger can happen on some special conditions like
	 * "hold button for X time, on press three buttons in order".
	 * For compatibility we disallow activation on trigger. Ability can still listen to for trigger events
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

	FArcAbilityActivationTimeDelegate OnActivationTimeFinished;

	void CallOnActionTimeFinished();

protected:
	virtual void K2_ExecuteGameplayCueWithParams(FGameplayTag GameplayCueTag
												 , const FGameplayCueParameters& GameplayCueParameters) override;
	// Current camera mode set by the ability.
	TSubclassOf<UArcCameraMode> ActiveCameraMode;

public:
	UArcCoreGameplayAbility();

	//FAssetRegistryTagsContext
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

	/**
	 * Called when ability is added to Ability System. Overridden to also initialize
	 * ability with targeting system by default.
	 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo
							   , const FGameplayAbilitySpec& Spec) override;

	/**
	 * Called when ability is being removed from Ability System. Overridden to
	 * unregister from targeting system.
	 */
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo
								 , const FGameplayAbilitySpec& AbilitySpec) override;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo
							 , const FGameplayAbilitySpec& Spec) override;

	/**
	 * Version which should be overridden by subclasses if needed. Called when ability source has been set correctly.
	 */
	virtual void OnAvatarSetSafe(const FGameplayAbilityActorInfo* ActorInfo
							 , const FGameplayAbilitySpec& Spec) {}

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

public:
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

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const FArcGameplayAbilityActorInfo& GetArcActorInfo();

	template <typename T>
	const T* GetTActorInfo() const
	{
		return static_cast<const T*>(GetCurrentActorInfo());
	}

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	class AArcCoreCharacter* GetArcCharacterFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	bool HasAuthority() const;

	void NativeExecuteLocalTargeting(UArcTargetingObject* InTrace);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void ExecuteLocalTargeting(UArcTargetingObject* InTrace);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	void SendTargetingResult(const FGameplayAbilityTargetDataHandle& TargetData
							 , UArcTargetingObject* InTrace);

	void ExecuteActionList(const TArray<FInstancedStruct>& Actions, FArcAbilityActionContext& Context);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnLocalTargetResult(const TArray<FHitResult>& OutHits);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData
							 , EArcClientServer Mode);

	virtual void NativeOnLocalTargetResult(FTargetingRequestHandle TargetingRequestHandle);

	virtual void NativeOnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData);

	/** Virtual hook for subclasses to execute actions during local target result. Called before OnLocalTargetResult. */
	virtual void ProcessLocalTargetActions(const TArray<FHitResult>& Hits, FTargetingRequestHandle TargetingRequestHandle) {}

	/** Virtual hook for subclasses to execute actions during ability target result. Called before OnAbilityTargetResult. */
	virtual void ProcessAbilityTargetActions(const FGameplayAbilityTargetDataHandle& AbilityTargetData) {}

public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	bool RemoveGameplayEffectFromTarget(FGameplayAbilityTargetDataHandle TargetData, FActiveGameplayEffectHandle Handle, int32 Stacks = -1);

protected:
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
						   , EArcAbilityActorSpawnOrigin SpawnLocationType
						   , FVector CustomSpawnLocation
						   , TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility
						   , FArcAbilityActorHandle& OutActorHandle);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability", meta=(ExpandBoolAsExecs = "ReturnValue"))
	bool GetSpawnedActor(const FArcAbilityActorHandle& OutActorHandle
						   , AActor*& OutActor);

public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability")
	const FGameplayTagContainer& GetAbilityTags() const
	{
		return GetAssetTags();
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
