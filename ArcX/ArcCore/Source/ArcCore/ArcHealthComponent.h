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

#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "ArcMacroDefines.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"

#include "Pawn/ArcPawnComponent.h"
#include "ArcHealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcAttributeComponentChanged
	, float /*NewValue*/
	, float /*OldValue*/
	, const FGameplayEffectModCallbackData* /*ModData*/
)

UCLASS(Abstract
	, Blueprintable)
class ARCCORE_API UArcAttributeHandlingComponent : public UArcPawnComponent
{
	GENERATED_BODY()

private:
	mutable bool bHealthInitialized = false;
	DEFINE_ARC_DELEGATE(FArcAttributeComponentChanged, OnAttributeChanged);

protected:
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	FGameplayAttribute TrackedAttribute;

	FDelegateHandle AttributeChangedHandle;

	TWeakObjectPtr<class UArcCoreAbilitySystemComponent> ArcASC;

	virtual void OnRegister() override;

	virtual void BeginPlay() override;

public:
	virtual bool IsPawnComponentReadyToInitialize() override;
	virtual void OnPawnReady() override;
	
protected:
	void HandleAttributeChangedDelegate(const FOnAttributeChangeData& InChangeData)
	{
		OnAttributeChangedDelegate.Broadcast(InChangeData.NewValue
			, InChangeData.OldValue
			, InChangeData.GEModData);
		HandleAttributeChanged(InChangeData);
	};

	virtual void HandleAttributeChanged(const FOnAttributeChangeData& InChangeData)
	{
	};
};

UENUM(BlueprintType)
enum class EArcDeathState : uint8
{
	NotDead = 0
	, DeathStarted
	, DeathFinished
};

USTRUCT()
struct FArcDeathMessage
{
	GENERATED_BODY()
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcHealth_DeathEvent, AActor*, OwningActor);

DECLARE_LOG_CATEGORY_EXTERN(LogArcDeath
	, Log
	, Log);

/**
 * Refactor It later into more generic Attribute Handling Component;
 */
UCLASS(ClassGroup = (Arc), meta = (BlueprintSpawnableComponent), Blueprintable)
class ARCCORE_API UArcHealthComponent : public UArcAttributeHandlingComponent
{
	GENERATED_BODY()

protected:
	TWeakObjectPtr<AController> CachedController = nullptr;
	
	UPROPERTY(BlueprintAssignable)
	FArcHealth_DeathEvent OnDeathStarted;

	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FArcHealth_DeathEvent OnDeathFinished;

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EArcDeathState DeathState;

	/*
	 * When Health reaches zero actor have collision disabled and is hidden.
	 * This sets the remaning lifetime before Actor will be destroyed (or recycled if it
	 * is pooled) from world.
	 */
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	float TimeToDie = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Arc Core")
	bool bDestroyOnDeath = false;

	UFUNCTION()
	virtual void OnRep_DeathState(EArcDeathState OldDeathState);

	UFUNCTION(BlueprintCallable)
	void SetDeathState(EArcDeathState InNewState);
	
public:
	UArcHealthComponent();

	// Begins the death sequence for the owner.
	virtual void StartDeath();

	// Ends the death sequence for the owner.
	virtual void FinishDeath();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnDeathStart(APawn* OutPawn
					  , AController* OutController);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnDeathFinish(APawn* OutPawn
					   , AController* OutController);

	virtual void HandleAttributeChanged(const FOnAttributeChangeData& InChangeData) override;

	UFUNCTION()
	void HandleOwnerEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

public:
	EArcDeathState GetDeathState() const
	{
		return DeathState;
	}

	static UArcHealthComponent* FindHealthComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UArcHealthComponent>() : nullptr);
	}
};

// ability might not be the solution here. Maybe it's better to subclass component, and
// customize here ?

UCLASS(Abstract)
class UArcGameplayAbility_Death : public UArcCoreGameplayAbility
{
	GENERATED_BODY()

public:
	UArcGameplayAbility_Death();

protected:
	void DoneAddingNativeTags();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle
								 , const FGameplayAbilityActorInfo* ActorInfo
								 , const FGameplayAbilityActivationInfo ActivationInfo
								 , const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle
							, const FGameplayAbilityActorInfo* ActorInfo
							, const FGameplayAbilityActivationInfo ActivationInfo
							, bool bReplicateEndAbility
							, bool bWasCancelled) override;

	// Starts the death sequence.
	UFUNCTION(BlueprintCallable
		, Category = "Lyra|Ability")
	void StartDeath();

	// Finishes the death sequence.
	UFUNCTION(BlueprintCallable
		, Category = "Lyra|Ability")
	void FinishDeath();

protected:
	// If enabled, the ability will automatically call StartDeath.  FinishDeath is always
	// called when the ability ends if the death was started.
	UPROPERTY(EditDefaultsOnly
		, BlueprintReadOnly
		, Category = "Lyra|Death")
	bool bAutoStartDeath;
};
