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

#include "AbilitySystemComponent.h"
#include "ArcCore/Items/ArcItemTypes.h"
#include "ArcMacroDefines.h"
#include "GameplayAbilitySpec.h"
#include "StructUtils/InstancedStruct.h"
#include "Types/TargetingSystemTypes.h"
#include "Targeting/ArcTargetingTypes.h"

#include "AbilitySystem/ArcActiveGameplayEffectForRPC.h"

#include "AbilitySystem/Targeting/ArcTargetDataId.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ArcCoreAbilitySystemComponent.generated.h"

class UArcItemDefinition;
class UArcTargetingObject;
DECLARE_LOG_CATEGORY_EXTERN(LogArcAbilitySystem
	, Log
	, All);

class UArcCoreGameplayAbility;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcGameplayAttributeChangedData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly)
	float Level;

	UPROPERTY(BlueprintReadOnly)
	float NewValue;

	UPROPERTY(BlueprintReadOnly)
	float OldValue;

	UPROPERTY(BlueprintReadOnly)
	TArray<FHitResult> Hits;
	
	FArcGameplayAttributeChangedData()
		: Target(nullptr)
		, Level(1.0f)
		, NewValue(-1.0f)
		, OldValue(-1.0f)
	{
	}

	FArcGameplayAttributeChangedData(AActor* InTarget
									 , float InLevel
									 , float InNewValue
									 , float InOldValue
									 , const TArray<FHitResult>& InHits)
		: Target(InTarget)
		, Level(InLevel)
		, NewValue(InNewValue)
		, OldValue(InOldValue)
		, Hits(InHits)
	{
	}
};

struct FArcTargetingSourceContext;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcAbilityInputDelegate, FGameplayAbilitySpec& /*AbilitySpec*/);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcAbilityCooldown
	, const FGameplayAbilitySpecHandle& /*AttributeData*/
	, UArcCoreGameplayAbility* /*InAbility*/
	, float /*InCooldownTime*/
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArcAbilityCooldownDynamic
	, const FGameplayAbilitySpecHandle&, AttributeData
	, UArcCoreGameplayAbility*, InAbility
	, float, InCooldownTime
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcGenericAbilityDynamicDelegate
	, const FGameplayAbilitySpecHandle&, AbilitySpecHandle
	, UArcCoreGameplayAbility*, InAbility
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArcAbilityActivationTimeDynamicDelegate
	, const FGameplayAbilitySpecHandle&, AbilitySpecHandle
	, UArcCoreGameplayAbility*, InAbility
	, float, InActivationTime
);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcTargetingDataDelegate, const FGameplayAbilityTargetDataHandle&);

DECLARE_MULTICAST_DELEGATE_OneParam(FArcGenericAbilitySpecDelegate, FGameplayAbilitySpec& /*AbilitySpec*/);

DECLARE_DELEGATE_OneParam(FArcTargetRequestHitResults, const TArray<FHitResult>&);

DECLARE_DELEGATE_OneParam(FArcGlobalTargetingResults, const TArray<FHitResult>&);

DECLARE_DELEGATE_OneParam(FArcTargetRequestAbilityTargetData, const FGameplayAbilityTargetDataHandle&);

UCLASS()
class ARCCORE_API UArcCoreAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

protected:
	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	/*
	 * Currently triggered events can only either activate ability
	 * or call delegates on clients side only.
	 */
	// Handles to abilities that had their input triggered this frame.
	TArray<FGameplayAbilitySpecHandle> InputTriggeredSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	TMap<FGameplayAbilitySpecHandle, bool> InputTriggeredAbilities;

	DEFINE_ARC_DELEGATE_MAP(FGameplayAbilitySpecHandle, FArcAbilityInputDelegate, OnInputPressed);

	DEFINE_ARC_DELEGATE_MAP(FGameplayAbilitySpecHandle, FArcAbilityInputDelegate, OnInputTriggered);

	DEFINE_ARC_DELEGATE_MAP(FGameplayAbilitySpecHandle, FArcAbilityInputDelegate, OnInputReleased);

	DEFINE_ARC_DELEGATE_MAP(FGameplayAbilitySpecHandle, FArcTargetingDataDelegate, OnTargetDataRequestComplete)

	DEFINE_ARC_DELEGATE(FArcGenericAbilitySpecDelegate, OnAbilityGiven)
	DEFINE_ARC_DELEGATE(FArcGenericAbilitySpecDelegate, OnAbilityRemoved)

public:
	friend class UGameplayAbility;

	TMap<UGameplayAbility*, TMap<FName, UAbilityTask*>> PooledTasks;

	UArcCoreAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeComponent() override;

	virtual void BeginPlay() override;
	
	void InitializeAttributeSets();


	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	virtual void PostAbilitySystemInit();
	
	void ApplyModToAttributeCallback(const FGameplayAttribute& Attribute
									 , TEnumAsByte<EGameplayModOp::Type> ModifierOp
									 , float ModifierMagnitude
									 , const FGameplayEffectModCallbackData* ModData);


	virtual FActiveGameplayEffectHandle ApplyGameplayEffectSpecToSelf(const FGameplayEffectSpec& GameplayEffect, FPredictionKey PredictionKey = FPredictionKey()) override;
	
	UFUNCTION(Client, Reliable)
    void ClientNotifyTargetEffectApplied(AActor* Target, const FArcActiveGameplayEffectForRPC& ActiveGE);

	UFUNCTION(Client, Reliable)
	void ClientNotifyTargetEffectRemoved(AActor* Target, const FArcActiveGameplayEffectForRPC& ActiveGE);
    	
	/** Allow events to be registered for specific gameplay tags being added or removed */
	FOnGameplayEffectTagCountChanged& RegisterTagStackChangedEvent(FGameplayTag Tag
																   , EGameplayTagEventType::Type EventType = EGameplayTagEventType::NewOrRemoved);

	/** Unregister previously added events */
	void UnregisterTagStackChangedEvent(FDelegateHandle DelegateHandle
										, FGameplayTag Tag
										, EGameplayTagEventType::Type EventType = EGameplayTagEventType::NewOrRemoved);

	/**
	 * Returns multicast delegate that is invoked whenever a tag is added or removed (but
	 * not if just count is increased. Only for 'new' and 'removed' events)
	 */
	FOnGameplayEffectTagCountChanged& RegisterGenericTagStackChangedEvent();

	/**
	 * Called on local player always. Called on server only if bReplicateInputDirectly is
	 * set on the GameplayAbility.
	 */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	virtual void AbilitySpecInputTriggered(FGameplayAbilitySpec& Spec);

	/**
	 * Called on local player always. Called on server only if bReplicateInputDirectly is
	 * set on the GameplayAbility.
	 */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	void ProcessAbilityInput(float DeltaTime
							 , bool bGamePaused);

	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	void AbilityInputTagTriggered(const FGameplayTag& InputTag);

	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	void ClearAbilityInput();

	// Same as original GiveAbility but does not scope lock.
	// use with caution !
	FGameplayAbilitySpecHandle GiveAbilityUnsafe(const FGameplayAbilitySpec& Spec);

	void AddAbilityDynamicTag(const FGameplayAbilitySpecHandle& InHandle
							  , const FGameplayTag& InTag);

	void RemoveAbilityDynamicTag(const FGameplayAbilitySpecHandle& InHandle
								 , const FGameplayTag& InTag);

	// Gameplay Effects
private:
	TMap<FActiveGameplayEffectHandle, TArray<FInstancedStruct>> ActiveGameplayEffectActions;

public:
	void AddActiveGameplayEffectAction(FActiveGameplayEffectHandle Handle, const TArray<FInstancedStruct>& InActions);

	void RemoveActiveGameplayEffectActions(FActiveGameplayEffectHandle Handle);

	TArray<FInstancedStruct>& GetActiveGameplayEffectActions(FActiveGameplayEffectHandle Handle);

	const FGameplayTagCountContainer& GetGameplayTagCountContainer() const
	{
		return GameplayTagCountContainer;
	}

	const FMinimalReplicationTagCountMap& GetMinimalReplicationCountTags() const
	{
		return GetMinimalReplicationTags();
	}

	const FMinimalReplicationTagCountMap& GetReplicatedLooseCountTags() const
	{
		return GetReplicatedLooseTags();
	}
	
	// Attributes
public:
	UFUNCTION(Client, Reliable)
	void ClientNotifyTargetAttributeChanged(AActor* Target
											, const FGameplayAttribute& Attribute
											, const FGameplayAbilitySpecHandle& AbilitySpec
											, const FArcTargetDataId& TargetDataHandle
											, uint8 InLevel
											, float NewValue);

	virtual void NotifyTargetAttributeChanged(AActor* Target
											  , const FGameplayAttribute& Attribute
											  , float NewValue
											  , float OldValue)
	{
	};

	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;



	virtual void PreAttributeChanged(const FGameplayAttribute& Attribute, float NewValue)
	{
	}
	
	virtual void PostAttributeChanged(const FGameplayAttribute& Attribute, float OldValue, float NewValue);

	// Cooldowns and resources
public:
	DEFINE_ARC_DELEGATE(FArcAbilityCooldown, OnGameplayAbilityCooldownStarted);
	DEFINE_ARC_DELEGATE(FArcAbilityCooldown, OnGameplayAbilityCooldownEnded);

public:
	UPROPERTY(BlueprintAssignable)
	FArcAbilityCooldownDynamic OnGameplayAbilityCooldownStartedDynamic;

	UPROPERTY(BlueprintAssignable)
	FArcAbilityCooldownDynamic OnGameplayAbilityCooldownEndedDynamic;

	UPROPERTY(BlueprintAssignable)
	FArcGenericAbilityDynamicDelegate OnAbilityActivatedDynamic;

	UPROPERTY(BlueprintAssignable)
	FArcAbilityActivationTimeDynamicDelegate OnAbilityActivationTimeChanged;
	
	UPROPERTY(BlueprintAssignable)
	FArcGenericAbilityDynamicDelegate OnAbilityActivationTimeFinished;
	
protected:
	TMap<FGameplayAbilitySpecHandle, FTimerHandle> AbilitiesOnCooldown;
	TMap<FTimerHandle, FGameplayAbilitySpecHandle> CooldownTimerToSpec;

public:
	void SetAbilityOnCooldown(FGameplayAbilitySpecHandle InAbility
							  , float Time);

	bool IsAbilityOnCooldown(FGameplayAbilitySpecHandle InAbility);

	float GetRemainingTime(FGameplayAbilitySpecHandle InAbility);

private:
	void HandleCooldownTimeEnded(FGameplayAbilitySpecHandle InAbility);

	TMap<FGameplayAbilitySpecHandle, double> AbilityActivationStartTimes;

public:
	void SetAbilityActivationStartTime(FGameplayAbilitySpecHandle InAbility, double StartTime)
	{
		AbilityActivationStartTimes.FindOrAdd(InAbility) = StartTime;
	}

	void ClearAbilityActivationStartTime(FGameplayAbilitySpecHandle InAbility)
	{
		AbilityActivationStartTimes.Remove(InAbility);
	}

	double GetAbilityActivationStartTime(FGameplayAbilitySpecHandle InAbility) const
	{
		if (const double* StartTime = AbilityActivationStartTimes.Find(InAbility))
		{
			return *StartTime;
		}
		return -1;
	}
	// Animations

	bool PlayAnimMontage(UGameplayAbility* InAnimatingAbility
						 , UAnimMontage* NewAnimMontage
						 , float InPlayRate
						 , FName StartSectionName
						 , float StartTimeSeconds
						 , bool bClientOnlyMontage
						 , const UArcItemDefinition* SourceItemDef = nullptr);

	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastPlayMontageReliable(UAnimMontage* InMontage);

	/** Takes SoftPtr to AnimMontage and DOES NOT play it or load on dedicated server. */
	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastPlaySoftMontageReliable(const TSoftObjectPtr<UAnimMontage>& InMontage);

	/** Takes SoftPtr to AnimMontage and DOES NOT play it or load on dedicated server. */
	UFUNCTION(NetMulticast, Unreliable)
	virtual void MulticastPlaySoftMontageUnreliable(const TSoftObjectPtr<UAnimMontage>& InMontage);
	
	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastStopMontageReliable(UAnimMontage* InMontage);
	
private:
	UFUNCTION(Server, Reliable)
	void ServerMulticastPlayMontage(UGameplayAbility* InAnimatingAbility
									, UAnimMontage* NewAnimMontage
									, float InPlayRate
									, FName StartSectionName
									, float StartTimeSeconds
									, const UArcItemDefinition* SourceItemDef);

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastPlayMontage(UGameplayAbility* InAnimatingAbility
								 , UAnimMontage* NewAnimMontage
								 , float InPlayRate
								 , FName StartSectionName
								 , float StartTimeSeconds
								 , const UArcItemDefinition* SourceItemDef);

	bool InternalPlayAnimMontage(UGameplayAbility* InAnimatingAbility
								 , UAnimMontage* NewAnimMontage
								 , float InPlayRate
								 , FName StartSectionName
								 , float StartTimeSeconds
								 , const UArcItemDefinition* SourceItemDef);

public:
	void StopAnimMontage(UAnimMontage* NewAnimMontage);

public:
	TMap<TWeakObjectPtr<UAnimMontage>, const FArcItemData*> MontageToItem;

	TMap<TWeakObjectPtr<UAnimMontage>, TWeakObjectPtr<const UArcItemDefinition>> MontageToItemDef;
	
public:
	void AddMontageToItem(UAnimMontage* NewAnimMontage, const FArcItemData* ItemId);
	void RemoveMontageFromItem(UAnimMontage* NewAnimMontage);
	const FArcItemData* GetItemFromMontage(UAnimMontage* NewAnimMontage) const;
	
	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastStopAnimMontage(UAnimMontage* NewAnimMontage);

	void InternalStopAnimMontage(UAnimMontage* NewAnimMontage);


	void AnimMontageJumpToSection(UAnimMontage* NewAnimMontage
								  , FName SectionName);

private:
	UFUNCTION(Server, Reliable)
	void ServerAnimMontageJumpToSection(UAnimMontage* NewAnimMontage
										, FName SectionName);

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastAnimMontageJumpToSection(UAnimMontage* NewAnimMontage
											  , FName SectionName);

	void InternalAnimMontageJumpToSection(UAnimMontage* NewAnimMontage
										  , FName SectionName);

private:
	// Targeting

	/**
	 * Contains current target requests.
	 *
	 * They are removed as they are either confirmed or rejected by server.
	 */
	TMap<FGameplayAbilitySpecHandle, FArcTargetRequestHitResults> WaitingCustomTargetRequests;

	TMap<FGameplayAbilitySpecHandle, FArcTargetRequestAbilityTargetData> WaitingCustomAbilityTargetRequests;

	TMap<FGameplayAbilitySpecHandle, TMap<FArcTargetDataId, TArray<FHitResult>>> PredictedTargetData;
	
public:
	const TMap<FGameplayAbilitySpecHandle, TMap<FArcTargetDataId, TArray<FHitResult>>>& GetPredictedTargetDatA() const
	{
		return PredictedTargetData;
	}
	
	/**
     * @brief Adds currently predicted hits.
     * @param SpecHandle Ability which wants to add predicted target data
     * @param TargetData Locally created target data
     */
	void AddPredictedTargetData(FGameplayAbilitySpecHandle SpecHandle
								, const FArcTargetDataId& TargetDataId
								, const TArray<FHitResult>& TargetData)
	{
		PredictedTargetData.FindOrAdd(SpecHandle).FindOrAdd(TargetDataId).Append(TargetData);
	}

	/*
	 * Do Not remove by default. PredictedTargetData is cleared automatically. How often
	 * it happens can be configured in project settings inside Nx Targeting category. This
	 * data can be requested by multiple system over span of several frames.
	 */
	TArray<FHitResult> GetPredictedTargetData(FGameplayAbilitySpecHandle SpecHandle
											, const FArcTargetDataId& TargetDataHandle
											, bool bRemove = false)
	{
		if (TArray<FHitResult>* Data = PredictedTargetData.FindOrAdd(SpecHandle).Find(TargetDataHandle))
		{
			TArray<FHitResult> D = *Data;
			if (bRemove)
			{
				PredictedTargetData.FindOrAdd(SpecHandle).Remove(TargetDataHandle);
			}
			return D;
		}
		
		return TArray<FHitResult>();
	}
	
	/**
	 * Adds specific target request to list of waiting for confirmation.
	 *
	 * You don't need to do it for every attempt at calling of @SendRequestCustomTargets and can just register once.
	 * It's preferable way to do it once and as soon as possible on both client and server, before attempting at sending any
	 * targeting data to server.
	 */
	void RegisterCustomTargetRequest(const FGameplayAbilitySpecHandle& RequestHandle
									 , FArcTargetRequestHitResults InDelegate);

	void RegisterCustomAbilityTargetRequest(const FGameplayAbilitySpecHandle& RequestHandle
									 , FArcTargetRequestAbilityTargetData InDelegate);
	
	void UnregisterCustomTargetRequest(const FGameplayAbilitySpecHandle& RequestHandle);

	/**
	 * @brief Requests target from provided targeting preset. Does not automatically send them to server. Only works locally.
	 * @param InTrace Targeting Preset to use for target acquisition
	 * @param FilterPreset Filter preset, to filter targeting result if null it is ignored.
	 * @param CustomData Any custom data needed by targeting
	 * @return list of target
	 */
	void RequestCustomTarget(UArcCoreGameplayAbility* InSourceAbility
										 , UArcTargetingObject* InTrace
										 , FArcTargetRequestHitResults OnCompletedDelegate = FArcTargetRequestHitResults()) const;
	
	void HandleOnRequestTargetComplete(FTargetingRequestHandle TargetingRequestHandle, FGameplayAbilitySpecHandle InRequestHandle
										, FArcTargetRequestHitResults OnCompletedDelegate) const;
	/**
	 * Sends targets from client to server.
	 */
	void SendRequestCustomTargets(const FGameplayAbilityTargetDataHandle& ClientResult
								  , UArcTargetingObject* InTrace
								  , const FGameplayAbilitySpecHandle& RequestHandle);
	
	UFUNCTION(Server, Reliable)
	void ServerRequestCustomTargets(const FGameplayAbilityTargetDataHandle& ClientResult
									, UArcTargetingObject* InTrace
									, const FGameplayAbilitySpecHandle& RequestHandle);

	TMap<FGameplayTag, FArcCoreGlobalTargetingEntry> TargetingPresets;

	const TMap<FGameplayTag, FArcCoreGlobalTargetingEntry>& GetTargetingPresets() const
	{
		return TargetingPresets;
	}

	// Debug accessors for protected engine members
#if !UE_BUILD_SHIPPING
	int32 GetDebug_AbilityScopeLockCount() const { return AbilityScopeLockCount; }
	float GetDebug_AbilityLastActivatedTime() const { return AbilityLastActivatedTime; }
	const FGameplayAbilityLocalAnimMontage& GetDebug_LocalAnimMontageInfo() const { return LocalAnimMontageInfo; }
	const TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>>& GetDebug_GameplayEventTriggeredAbilities() const { return GameplayEventTriggeredAbilities; }
	const TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle>>& GetDebug_OwnedTagTriggeredAbilities() const { return OwnedTagTriggeredAbilities; }
	const TMap<FGameplayAbilitySpecHandle, FTimerHandle>& GetDebug_AbilitiesOnCooldown() const { return AbilitiesOnCooldown; }
	const TMap<FGameplayAbilitySpecHandle, double>& GetDebug_AbilityActivationStartTimes() const { return AbilityActivationStartTimes; }
	const TArray<FGameplayAbilitySpecHandle>& GetDebug_InputPressedSpecHandles() const { return InputPressedSpecHandles; }
	const TArray<FGameplayAbilitySpecHandle>& GetDebug_InputHeldSpecHandles() const { return InputHeldSpecHandles; }
#endif
	
	void HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle, FGameplayTag TargetingTag);
	void GiveGlobalTargetingPreset(const TMap<FGameplayTag, FArcCoreGlobalTargetingEntry>& Preset);
	
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Targeting")
	FHitResult GetHitResult(FGameplayTag Tag, bool& bWasSuccessful);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Targeting")
	FVector GetHitLocation(FGameplayTag Tag, bool& bWasSuccessful);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Targeting")
	UTargetingPreset* GetGlobalTargetingPreset(FGameplayTag Tag, bool& bWasSuccessful);
	
	static FHitResult GetGlobalHitResult(UArcCoreAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);
	static FHitResult GetGlobalHitResult(UAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);
	
	static FVector GetGlobalHitLocation(UArcCoreAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);
	static FVector GetGlobalHitLocation(UAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);

	static UTargetingPreset* GetGlobalTargetingPreset(UArcCoreAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting", DisplayName = "Get Global HitResult")
	static FHitResult BP_GetGlobalHitResult(UAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting", DisplayName = "Get Global Hit Location")
	static FVector BP_GetGlobalHitLocation(UAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Targeting", DisplayName = "Get Global Targeting Preset")
	static UTargetingPreset* BP_GetGlobalTargetingPreset(UArcCoreAbilitySystemComponent* InASC, FGameplayTag TargetingTag, bool& bWasSuccessful);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcAbilityActorDelegate);

/**
 * Lightweight component for actors spawned by abilities.
 * Tracks the instigator (ASC + ability) and provides item data access.
 * Add this to any actor that should be spawnable via the ability system.
 */
UCLASS(Blueprintable, BlueprintType, DefaultToInstanced, ClassGroup = (ArcCore), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcAbilityActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> RootPrim;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UArcCoreGameplayAbility> InstigatorAbility;

	TWeakObjectPtr<UArcCoreAbilitySystemComponent> ASC;

	UPROPERTY(BlueprintReadOnly, Category = "Arc Core|Ability System")
	FVector TargetLocation;
	
	UPROPERTY(BlueprintReadOnly, Category = "Arc Core|Ability System")
	bool bHaveTargetLocation = false;
	
	void Initialize(UArcCoreAbilitySystemComponent* OwningASC
		, UArcCoreGameplayAbility* InInstigatorAbility
		, TOptional<FVector> InTargetLocation = {});

	UPROPERTY(BlueprintAssignable)
	FArcAbilityActorDelegate OnInitializedDelegate;
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnInitialized();
	
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability System")
	AActor* GetAvatar() const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability System")
	AActor* GetOwnerActor() const;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	const UArcItemDefinition* GetSourceItemData() const;

	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core|Ability", meta = (CustomStructureParam = "OutFragment", ExpandBoolAsExecs = "ReturnValue"))
	bool FindItemFragment(UPARAM(meta = (MetaStruct = "/Script/ArcCore.ArcItemFragment")) UScriptStruct* InFragmentType
						  , int32& OutFragment);

	DECLARE_FUNCTION(execFindItemFragment);
};

USTRUCT(BlueprintType)
struct FArcItemFragment_ProjectileSettings : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxVelocity = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float GravityScale = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Lifetime = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bUseSuggestVelocity = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ArcParam = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bVelocityInLocalSpace = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsHoming = false;
};