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

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "Templates/SubclassOf.h"

#include "GameplayEffectExtension.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Abilities/GameplayAbility.h"

#include "ArcAbilitiesTypes.h"
#include "ArcCoreUtils.h"
#include "ArcGameplayAbilityActorInfo.h"
#include "ArcWorldDelegates.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Targeting/ArcTargetingObject.h"
#include "Tasks/ArcAbilityTask.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectComponents/ArcGEC_EffectAction.h"
#include "Items/ArcItemsHelpers.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "UObject/FastReferenceCollector.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY(LogArcAbilitySystem);

UArcCoreAbilitySystemComponent::UArcCoreAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickInterval = 0.033;
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void UArcCoreAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Look for DSO AttributeSets (note we are currently requiring all attribute sets to
	// be subobjects of the same owner. This doesn't *have* to be the case forever.
	InitializeAttributeSets();
}

void UArcCoreAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UArcCoreAbilitySystemComponent::InitializeAttributeSets()
{
	AActor* Owner = GetOwner();

	TArray<UObject*> ChildObjects;
	GetObjectsWithOuter(Owner
		, ChildObjects
		, false
		, RF_NoFlags
		, EInternalObjectFlags::Garbage);
	for (UObject* Obj : ChildObjects)
	{
		UArcAttributeSet* Set = Cast<UArcAttributeSet>(Obj);
		if (Set)
		{
			Set->SetArcASC(this);
		}
	}
}

void UArcCoreAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	OnAbilityGivenDelegate.Broadcast(AbilitySpec);

	UArcWorldDelegates::Get(GetWorld())->BroadcastActorOnAbilityGiven(GetOwner(), AbilitySpec, this);
}

void UArcCoreAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor
	, AActor* InAvatarActor)
{
	AActor* OldAvatar = AbilityActorInfo->AvatarActor.Get();

	if (OldAvatar && InAvatarActor != OldAvatar)
	{
		UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
		if (Targeting == nullptr)
		{
			return;
		}
	
		for (TPair<FGameplayTag, FArcCoreGlobalTargetingEntry>& Pair : TargetingPresets)
		{
			Pair.Value.EndTargeting(Targeting);
		}
	}
	
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	
	if (APawn* P = Cast<APawn>(InAvatarActor))
	{
		UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
		
		FArcTargetingSourceContext Context;
		Context.InstigatorActor = P->GetPlayerState();
		Context.SourceActor = P;
	
		for (TPair<FGameplayTag, FArcCoreGlobalTargetingEntry>& Pair : TargetingPresets)
		{
			Pair.Value.StartTargeting(P, Targeting, Context, this, Pair.Key);
		}
	}
}

void UArcCoreAbilitySystemComponent::PostAbilitySystemInit()
{
	if (APawn* P = Cast<APawn>(AbilityActorInfo->AvatarActor.Get()))
	{
		UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
		
		FArcTargetingSourceContext Context;
		Context.InstigatorActor = P->GetPlayerState();
		Context.SourceActor = P;
	
		for (TPair<FGameplayTag, FArcCoreGlobalTargetingEntry>& Pair : TargetingPresets)
		{
			Pair.Value.StartTargeting(P, Targeting, Context, this, Pair.Key);
		}
	}
}

void UArcCoreAbilitySystemComponent::ApplyModToAttributeCallback(const FGameplayAttribute& Attribute
																 , TEnumAsByte<EGameplayModOp::Type> ModifierOp
																 , float ModifierMagnitude
																 , const FGameplayEffectModCallbackData* ModData)
{
	// We can only apply loose mods on the authority. If we ever need to predict these,
	// they would need to be turned into GEs and be given a prediction key so that they
	// can be rolled back.
	if (IsOwnerActorAuthoritative())
	{
		ActiveGameplayEffects.ApplyModToAttribute(Attribute
			, ModifierOp
			, ModifierMagnitude
			, ModData);
	}
}

FActiveGameplayEffectHandle UArcCoreAbilitySystemComponent::ApplyGameplayEffectSpecToSelf(const FGameplayEffectSpec& GameplayEffect
	, FPredictionKey PredictionKey)
{
	FActiveGameplayEffectHandle Handle = Super::ApplyGameplayEffectSpecToSelf(GameplayEffect, PredictionKey);
	
	return Handle;
}

void UArcCoreAbilitySystemComponent::ClientNotifyTargetEffectRemoved_Implementation(AActor* Target, const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	float WorldTimeSeconds = ActiveGameplayEffects.GetWorldTime();
	float ServerWorldTime = ActiveGameplayEffects.GetServerWorldTime();
	
	const_cast<FArcActiveGameplayEffectForRPC&>(ActiveGE).StartWorldTime = WorldTimeSeconds - (ServerWorldTime - ActiveGE.StartServerWorldTime);
	UArcWorldDelegates::Get(this)->OnAnyGameplayEffectRemovedFromTarget.Broadcast(Target, ActiveGE);
}

void UArcCoreAbilitySystemComponent::ClientNotifyTargetEffectApplied_Implementation(AActor* Target, const FArcActiveGameplayEffectForRPC& ActiveGE)
{
	UArcWorldDelegates::Get(this)->OnAnyGameplayEffectAddedToTarget.Broadcast(Target, ActiveGE);
}

void UArcCoreAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);
	if (IsOwnerActorAuthoritative() == false)
	{
		ServerSetInputPressed(Spec.Handle);
	}
	if (Spec.IsActive())
	{
		// Invoke the InputPressed event. This is not replicated here. If someone is
		// listening, they may replicate the InputPressed event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed
			, Spec.Handle
			, Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey());
	}

	if (FArcAbilityInputDelegate* Delegate = OnInputPressedDelegateMap.Find(Spec.Handle))
	{
		Delegate->Broadcast(Spec);
	}
}

void UArcCoreAbilitySystemComponent::AbilitySpecInputTriggered(FGameplayAbilitySpec& Spec)
{
	if (FArcAbilityInputDelegate* Delegate = OnInputTriggeredDelegateMap.Find(Spec.Handle))
	{
		Delegate->Broadcast(Spec);
	}
}

void UArcCoreAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);
	if (IsOwnerActorAuthoritative() == false)
	{
		ServerSetInputReleased(Spec.Handle);
	}
	if (Spec.IsActive())
	{
		// Invoke the InputReleased event. This is not replicated here. If someone is
		// listening, they may replicate the InputReleased event to the server.
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased
			, Spec.Handle
			, Spec.GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
	if (FArcAbilityInputDelegate* Delegate = OnInputReleasedDelegateMap.Find(Spec.Handle))
	{
		Delegate->Broadcast(Spec);
	}
}

void UArcCoreAbilitySystemComponent::ProcessAbilityInput(float DeltaTime
														 , bool bGamePaused)
{
	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();

	//@TODO: See if we can use FScopedServerAbilityRPCBatcher ScopedRPCBatcher in some of
	// these loops

	//
	// Process all abilities that activate when the input is held.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->IsActive())
			{
				// const UArcCoreGameplayAbility* ArcAbilityCDO =
				// CastChecked<UArcCoreGameplayAbility>(AbilitySpec->Ability);
				//
				// if (ArcAbilityCDO->GetActivationPolicy() ==
				// EArcAbilityActivationPolicy::WhileInputActive)
				//{
				//	AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
				// }
			}
		}
	}

	//
	// Process all abilities that had their input pressed this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(AbilitySpec->Ability))
				{
					AbilitySpec->InputPressed = true;

					if (AbilitySpec->IsActive())
					{
						// Ability is active so pass along the input event.
						AbilitySpecInputPressed(*AbilitySpec);
					}
					else
					{
						// const UArcCoreGameplayAbility* ArcAbilityCDO =
						// CastChecked<UArcCoreGameplayAbility>(AbilitySpec->Ability);

						// if (ArcAbilityCDO->GetActivationPolicy() ==
						// EArcAbilityActivationPolicy::OnInputTriggered)
						if (ArcAbility->GetActivateOnTrigger() == false)
						{
							AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
						}
					}
				}
				else
				{
					AbilitySpec->InputPressed = true;

					if (AbilitySpec->IsActive())
					{
						// Ability is active so pass along the input event.
						AbilitySpecInputPressed(*AbilitySpec);
					}
					else
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& SpecHandle : InputTriggeredSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(AbilitySpec->Ability))
			{
				if (AbilitySpec->IsActive())
				{
					// The ability is active, so just pipe the input event to it
					TArray<UGameplayAbility*> Instances = AbilitySpec->GetAbilityInstances();
					for (UGameplayAbility* Instance : Instances)
					{
						UArcCoreGameplayAbility* ArcAbilityInstance = Cast<UArcCoreGameplayAbility>(Instance);
						ArcAbilityInstance->InputPressed(AbilitySpec->Handle
							, AbilityActorInfo.Get()
							, AbilitySpec->GetPrimaryInstance()->GetCurrentActivationInfo());
					}
					
				}
				else
				{
					if (ArcAbility->GetActivateOnTrigger() == true)
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}

				InputTriggeredAbilities.FindOrAdd(SpecHandle) = true;
			}
		}
	}

	//
	// Try to activate all the abilities that are from presses and holds.
	// We do it all at once so that held inputs don't activate the ability
	// and then also send a input event to the ability because of the press.
	//
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	//
	// Process all abilities that had their input released this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
				InputTriggeredAbilities.FindOrAdd(SpecHandle) = false;
			}
		}
	}

	//
	// Clear the cached ability handles.
	//
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UArcCoreAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UArcCoreAbilitySystemComponent::AbilityInputTagTriggered(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputTriggeredSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UArcCoreAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			}
		}
	}
}

void UArcCoreAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

FGameplayAbilitySpecHandle UArcCoreAbilitySystemComponent::GiveAbilityUnsafe(const FGameplayAbilitySpec& Spec)
{
	check(Spec.Ability);
	check(IsOwnerActorAuthoritative()); // Should be called on authority

	// If locked, add to pending list. The Spec.Handle is not regenerated when we receive,
	// so returning this is ok. if (AbilityScopeLockCount > 0)
	//{
	//	AbilityPendingAdds.Add(Spec);
	//	return Spec.Handle;
	//}

	// ABILITYLIST_SCOPE_LOCK();
	FGameplayAbilitySpec& OwnedSpec = ActivatableAbilities.Items[ActivatableAbilities.Items.Add(Spec)];

	if (OwnedSpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		// Create the instance at creation time
		CreateNewInstanceOfAbility(OwnedSpec
			, Spec.Ability);
	}

	OnGiveAbility(OwnedSpec);
	MarkAbilitySpecDirty(OwnedSpec
		, true);

	return OwnedSpec.Handle;
}

void UArcCoreAbilitySystemComponent::AddAbilityDynamicTag(const FGameplayAbilitySpecHandle& InHandle
														  , const FGameplayTag& InTag)
{
	for (int32 Idx = 0; Idx < ActivatableAbilities.Items.Num(); Idx++)
	{
		if (ActivatableAbilities.Items[Idx].Handle == InHandle)
		{
			ActivatableAbilities.Items[Idx].GetDynamicSpecSourceTags().AddTag(InTag);
			ActivatableAbilities.MarkItemDirty(ActivatableAbilities.Items[Idx]);
			return;
		}
	}
}

void UArcCoreAbilitySystemComponent::RemoveAbilityDynamicTag(const FGameplayAbilitySpecHandle& InHandle
															 , const FGameplayTag& InTag)
{
	for (int32 Idx = 0; Idx < ActivatableAbilities.Items.Num(); Idx++)
	{
		if (ActivatableAbilities.Items[Idx].Handle == InHandle)
		{
			ActivatableAbilities.Items[Idx].GetDynamicSpecSourceTags().RemoveTag(InTag);
			ActivatableAbilities.MarkItemDirty(ActivatableAbilities.Items[Idx]);
			return;
		}
	}
}

void UArcCoreAbilitySystemComponent::AddActiveGameplayEffectAction(FActiveGameplayEffectHandle Handle
	, const TArray<FInstancedStruct>& InActions)
{
	TArray<FInstancedStruct>& Actions = ActiveGameplayEffectActions.FindOrAdd(Handle);
	Actions.Reserve(InActions.Num());

	for (const FInstancedStruct& A : InActions)
	{
		FInstancedStruct NewAction;
		NewAction.InitializeAs(A.GetScriptStruct(), A.GetMemory());

		Actions.Add(NewAction);	
	}
	
}

void UArcCoreAbilitySystemComponent::RemoveActiveGameplayEffectActions(FActiveGameplayEffectHandle Handle)
{
	ActiveGameplayEffectActions.Remove(Handle);
}

TArray<FInstancedStruct>& UArcCoreAbilitySystemComponent::GetActiveGameplayEffectActions(FActiveGameplayEffectHandle Handle)
{
	if (TArray<FInstancedStruct>* Actions = ActiveGameplayEffectActions.Find(Handle))
	{
		return *Actions;
	}
	
	static TArray<FInstancedStruct> ReturnValue;
	return ReturnValue;
}

void UArcCoreAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	OnAbilityRemovedDelegate.Broadcast(AbilitySpec);
	Super::OnRemoveAbility(AbilitySpec);
}

void UArcCoreAbilitySystemComponent::MulticastPlaySoftMontageUnreliable_Implementation(
	const TSoftObjectPtr<UAnimMontage>& InMontage)
{
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		USkeletalMeshComponent* SMC = GetAvatarActor()->FindComponentByClass<USkeletalMeshComponent>();

		SMC->GetAnimInstance()->Montage_Play(InMontage.LoadSynchronous());
	}
}

void UArcCoreAbilitySystemComponent::MulticastPlaySoftMontageReliable_Implementation(
	const TSoftObjectPtr<UAnimMontage>& InMontage)
{
	APawn* P = Cast<APawn>(GetAvatarActor());
	
	if (GetNetMode() != ENetMode::NM_DedicatedServer && P->IsLocallyControlled() == false)
	{
		USkeletalMeshComponent* SMC = GetAvatarActor()->FindComponentByClass<USkeletalMeshComponent>();

		SMC->GetAnimInstance()->Montage_Play(InMontage.LoadSynchronous());
	}
}

void UArcCoreAbilitySystemComponent::MulticastPlayMontageReliable_Implementation(UAnimMontage* InMontage)
{
	USkeletalMeshComponent* SMC = GetAvatarActor()->FindComponentByClass<USkeletalMeshComponent>();

	SMC->GetAnimInstance()->Montage_Play(InMontage);
}

void UArcCoreAbilitySystemComponent::MulticastStopMontageReliable_Implementation(UAnimMontage* InMontage)
{
	USkeletalMeshComponent* SMC = GetAvatarActor()->FindComponentByClass<USkeletalMeshComponent>();

	SMC->GetAnimInstance()->Montage_Stop(0.2, InMontage);
}

void UArcCoreAbilitySystemComponent::PostAttributeChanged(const FGameplayAttribute& Attribute
	, float OldValue
	, float NewValue)
{
	UArcWorldDelegates::Get(GetWorld())->OnAnyAttributeChangedDynamic.Broadcast(this, Attribute, OldValue, NewValue);
}

void UArcCoreAbilitySystemComponent::ClientNotifyTargetAttributeChanged_Implementation(AActor* Target
																					   , const FGameplayAttribute& Attribute
																					   , const FGameplayAbilitySpecHandle& AbilitySpec
																					   , const FArcTargetDataId& TargetDataHandle
																					   , uint8 InLevel
																					   , float NewValue)
{
	
	
	{
		TArray<FHitResult> TargetData = GetPredictedTargetData(AbilitySpec
										  , TargetDataHandle);

		uint8 CorrectLevel = InLevel == 0 ? 1 : InLevel;

		FArcGameplayAttributeChangedData AttributeData(Target
			, InLevel
			, NewValue / CorrectLevel
			, 0
			, TargetData);
		
		if (TargetData.Num() > 0)
		{
			UArcWorldDelegates::Get(GetWorld())->OnAnyTargetAttributeChanged.Broadcast(Attribute, AttributeData);
			UArcWorldDelegates::Get(GetWorld())->BroadcastOnTargetAttributeChangedMap(Attribute, AttributeData);
		}
		else if (Target)
		{
			TArray<FHitResult> HitArray;
			int32 Hits = static_cast<int32>(InLevel);
			for (int32 Idx = 0; Idx < Hits; Idx++)
			{
				FHitResult Hit;
				Hit.HitObjectHandle = FActorInstanceHandle(Target);
				Hit.Location = Target->GetActorLocation();
				Hit.ImpactPoint = Target->GetActorLocation();
				// we need to handle case where target data expired (ie. long running dot
				// effect).
				HitArray.Add(Hit);
			}
			FArcGameplayAttributeChangedData AttributeData2(Target
				, InLevel
				, NewValue / CorrectLevel
				, 0
				, HitArray);

			UArcWorldDelegates::Get(GetWorld())->OnAnyTargetAttributeChanged.Broadcast(Attribute, AttributeData2);
			UArcWorldDelegates::Get(GetWorld())->BroadcastOnTargetAttributeChangedMap(Attribute, AttributeData2);
		}
	}

	NotifyTargetAttributeChanged(Target
		, Attribute
		, NewValue
		, 0);
}

void UArcCoreAbilitySystemComponent::SetAbilityOnCooldown(FGameplayAbilitySpecHandle InAbility
														  , float Time)
{
	if (Time <= 0)
	{
		return;
	}

	// Clear existing cooldown timer if re-entering cooldown (e.g. cascading cost abilities)
	if (FTimerHandle* ExistingHandle = AbilitiesOnCooldown.Find(InAbility))
	{
		FTimerManager& TimerMgr = GetWorld()->GetTimerManager();
		TimerMgr.ClearTimer(*ExistingHandle);
		CooldownTimerToSpec.Remove(*ExistingHandle);
		AbilitiesOnCooldown.Remove(InAbility);
	}

	float Ping = 0;
	// add some value of ping to cooldown
	// to make some compensation on client, for the fact that server cooldown may actully
	// end quicker.
	if (GetOwner()->GetNetMode() == ENetMode::NM_Client)
	{
		APlayerState* PS = Cast<APlayerState>(GetOwner());
		Ping = PS->ExactPing;
		Time = Time + ((Ping / 2) / 1000);
	}
	FTimerManager& Timer = GetWorld()->GetTimerManager();
	FTimerHandle Handle;
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this
		, &UArcCoreAbilitySystemComponent::HandleCooldownTimeEnded
		, InAbility);
	
	Timer.SetTimer(Handle
		, Delegate
		, Time
		, false);
	
	AbilitiesOnCooldown.FindOrAdd(InAbility) = Handle;
	CooldownTimerToSpec.FindOrAdd(Handle) = InAbility;

	UArcCoreGameplayAbility* Ability = Cast<UArcCoreGameplayAbility>(FindAbilitySpecFromHandle(InAbility)->GetPrimaryInstance());

	OnGameplayAbilityCooldownStartedDelegate.Broadcast(InAbility
		, Ability
		, Time);
	
	OnGameplayAbilityCooldownStartedDynamic.Broadcast(InAbility
		, Ability
		, Time);
}

bool UArcCoreAbilitySystemComponent::IsAbilityOnCooldown(FGameplayAbilitySpecHandle InAbility)
{
	return AbilitiesOnCooldown.Contains(InAbility);
}

float UArcCoreAbilitySystemComponent::GetRemainingTime(FGameplayAbilitySpecHandle InAbility)
{
	if (AbilitiesOnCooldown.Contains(InAbility))
	{
		GetWorld()->GetTimerManager().GetTimerRemaining(AbilitiesOnCooldown[InAbility]);
	}
	return 0;
}

void UArcCoreAbilitySystemComponent::HandleCooldownTimeEnded(FGameplayAbilitySpecHandle InAbility)
{
	if (AbilitiesOnCooldown.Contains(InAbility))
	{
		GetWorld()->GetTimerManager().ClearTimer(AbilitiesOnCooldown[InAbility]);

		CooldownTimerToSpec.Remove(AbilitiesOnCooldown[InAbility]);
		AbilitiesOnCooldown.Remove(InAbility);

		UArcCoreGameplayAbility* Ability = Cast<UArcCoreGameplayAbility>(FindAbilitySpecFromHandle(InAbility)->GetPrimaryInstance());
		
		OnGameplayAbilityCooldownEndedDelegate.Broadcast(InAbility
			, Ability
			, 0);

		OnGameplayAbilityCooldownEndedDynamic.Broadcast(InAbility
			, Ability
			, 0);
	}
}

bool UArcCoreAbilitySystemComponent::PlayAnimMontage(UGameplayAbility* InAnimatingAbility
													 , UAnimMontage* NewAnimMontage
													 , float InPlayRate
													 , FName StartSectionName
													 , float StartTimeSeconds
													 , bool bClientOnlyMontage
													 , const UArcItemDefinition* InItemDef)
{
	if (bClientOnlyMontage)
	{
		ServerMulticastPlayMontage(InAnimatingAbility
			, NewAnimMontage
			, InPlayRate
			, StartSectionName
			, StartTimeSeconds
			, InItemDef);
	}
	else
	{
		NetMulticastPlayMontage(InAnimatingAbility
			, NewAnimMontage
			, InPlayRate
			, StartSectionName
			, StartTimeSeconds
			, InItemDef);
	}

	return InternalPlayAnimMontage(InAnimatingAbility
		, NewAnimMontage
		, InPlayRate
		, StartSectionName
		, StartTimeSeconds
		, InItemDef);
}

void UArcCoreAbilitySystemComponent::ServerMulticastPlayMontage_Implementation(UGameplayAbility* InAnimatingAbility
																			   , UAnimMontage* NewAnimMontage
																			   , float InPlayRate
																			   , FName StartSectionName
																			   , float StartTimeSeconds
																			   , const UArcItemDefinition* SourceItemDef)
{
	NetMulticastPlayMontage(InAnimatingAbility
		, NewAnimMontage
		, InPlayRate
		, StartSectionName
		, StartTimeSeconds
		, SourceItemDef);
	
	InternalPlayAnimMontage(InAnimatingAbility
		, NewAnimMontage
		, InPlayRate
		, StartSectionName
		, StartTimeSeconds
		, SourceItemDef);
}

void UArcCoreAbilitySystemComponent::NetMulticastPlayMontage_Implementation(UGameplayAbility* InAnimatingAbility
																			, UAnimMontage* NewAnimMontage
																			, float InPlayRate
																			, FName StartSectionName
																			, float StartTimeSeconds
																			, const UArcItemDefinition* SourceItemDef)
{
	if (AbilityActorInfo->PlayerController.IsValid())
	{
		return;
	}
	
	InternalPlayAnimMontage(InAnimatingAbility
		, NewAnimMontage
		, InPlayRate
		, StartSectionName
		, StartTimeSeconds
		,SourceItemDef);
}

bool UArcCoreAbilitySystemComponent::InternalPlayAnimMontage(UGameplayAbility* InAnimatingAbility
															 , UAnimMontage* NewAnimMontage
															 , float InPlayRate
															 , FName StartSectionName
															 , float StartTimeSeconds
															 , const UArcItemDefinition* SourceItemDef)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	float Duration = AnimInstance->Montage_Play(NewAnimMontage
		, InPlayRate
		, EMontagePlayReturnType::MontageLength
		, StartTimeSeconds);

	bool bPlayed = Duration > 0.0f;
	if (bPlayed)
	{
		if (StartSectionName != NAME_None)
		{
			AnimInstance->Montage_JumpToSection(StartSectionName
				, NewAnimMontage);
		}
	}

	MontageToItemDef.FindOrAdd(NewAnimMontage) = SourceItemDef;
	
	return bPlayed;
}

void UArcCoreAbilitySystemComponent::StopAnimMontage(UAnimMontage* NewAnimMontage)
{
	NetMulticastStopAnimMontage(NewAnimMontage);
	InternalStopAnimMontage(NewAnimMontage);
}

void UArcCoreAbilitySystemComponent::AddMontageToItem(UAnimMontage* NewAnimMontage
	, const FArcItemData* ItemId)
{
	MontageToItem.FindOrAdd(NewAnimMontage) = ItemId;
}

void UArcCoreAbilitySystemComponent::RemoveMontageFromItem(UAnimMontage* NewAnimMontage)
{
	MontageToItem.Remove(NewAnimMontage);
}

const FArcItemData* UArcCoreAbilitySystemComponent::GetItemFromMontage(UAnimMontage* NewAnimMontage) const
{
	if (const FArcItemData* const* Id = MontageToItem.Find(NewAnimMontage))
	{
		return *Id;
	}

	return nullptr;
}

void UArcCoreAbilitySystemComponent::NetMulticastStopAnimMontage_Implementation(UAnimMontage* NewAnimMontage)
{
	if (AbilityActorInfo->PlayerController.IsValid())
	{
		return;
	}
	InternalStopAnimMontage(NewAnimMontage);
}

void UArcCoreAbilitySystemComponent::InternalStopAnimMontage(UAnimMontage* NewAnimMontage)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;

	bool bShouldStopMontage = !AnimInstance->Montage_GetIsStopped(NewAnimMontage);

	if (bShouldStopMontage)
	{
		const float BlendOutTime = NewAnimMontage->BlendOut.GetBlendTime();

		AnimInstance->Montage_Stop(BlendOutTime
			, NewAnimMontage);
	}
}

void UArcCoreAbilitySystemComponent::AnimMontageJumpToSection(UAnimMontage* NewAnimMontage
															  , FName SectionName)
{
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		ServerAnimMontageJumpToSection(NewAnimMontage, SectionName);
	}
	NetMulticastAnimMontageJumpToSection(NewAnimMontage, SectionName);
	InternalAnimMontageJumpToSection(NewAnimMontage, SectionName);
}

void UArcCoreAbilitySystemComponent::ServerAnimMontageJumpToSection_Implementation(UAnimMontage* NewAnimMontage
																				   , FName SectionName)
{
	NetMulticastAnimMontageJumpToSection(NewAnimMontage, SectionName);
	InternalAnimMontageJumpToSection(NewAnimMontage, SectionName);
}

void UArcCoreAbilitySystemComponent::NetMulticastAnimMontageJumpToSection_Implementation(UAnimMontage* NewAnimMontage
																						 , FName SectionName)
{
	if (AbilityActorInfo->PlayerController.IsValid())
	{
		return;
	}
	InternalAnimMontageJumpToSection(NewAnimMontage, SectionName);
}

void UArcCoreAbilitySystemComponent::InternalAnimMontageJumpToSection(UAnimMontage* NewAnimMontage
																	  , FName SectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	AnimInstance->Montage_JumpToSection(SectionName, NewAnimMontage);
}



void UArcCoreAbilitySystemComponent::RegisterCustomTargetRequest(const FGameplayAbilitySpecHandle& RequestHandle
																 , FArcTargetRequestHitResults InDelegate)
{
	if (InDelegate.IsBound())
	{
		WaitingCustomTargetRequests.FindOrAdd(RequestHandle) = InDelegate;
	}
}

void UArcCoreAbilitySystemComponent::RegisterCustomAbilityTargetRequest(const FGameplayAbilitySpecHandle& RequestHandle
	, FArcTargetRequestAbilityTargetData InDelegate)
{
	if (InDelegate.IsBound())
	{
		WaitingCustomAbilityTargetRequests.FindOrAdd(RequestHandle) = InDelegate;
	}
}

void UArcCoreAbilitySystemComponent::UnregisterCustomTargetRequest(const FGameplayAbilitySpecHandle& RequestHandle)
{
	WaitingCustomTargetRequests.Remove(RequestHandle);
}

void UArcCoreAbilitySystemComponent::RequestCustomTarget(UArcCoreGameplayAbility* InSourceAbility
	, UArcTargetingObject* InTrace
	, FArcTargetRequestHitResults OnCompletedDelegate) const
{
	UTargetingSubsystem* TargetingSystem = UTargetingSubsystem::Get(GetWorld());
	FArcTargetingSourceContext Context;
	Context.SourceActor = GetAvatarActor();
	Context.InstigatorActor = GetOwner();
	Context.SourceObject = GetOwner();
	
	FTargetingRequestHandle RequestHandle = Arcx::MakeTargetRequestHandle(InTrace->TargetingPreset, Context);
	TargetingSystem->ExecuteTargetingRequestWithHandle(RequestHandle
		, FTargetingRequestDelegate::CreateUObject(this, &UArcCoreAbilitySystemComponent::HandleOnRequestTargetComplete,
			InSourceAbility->GetCurrentAbilitySpecHandle(), OnCompletedDelegate));
 }

void UArcCoreAbilitySystemComponent::HandleOnRequestTargetComplete(FTargetingRequestHandle TargetingRequestHandle, FGameplayAbilitySpecHandle InRequestHandle, FArcTargetRequestHitResults OnCompletedDelegate) const
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	TArray<FHitResult> Hits;
	Hits.Reserve(TargetingResults.TargetResults.Num());
		
	if (const FArcTargetRequestHitResults* Delegate = WaitingCustomTargetRequests.Find(InRequestHandle))
	{
		for (FTargetingDefaultResultData& Result : TargetingResults.TargetResults)
		{
			Hits.Add(MoveTemp(Result.HitResult));
		}
		
		Delegate->Execute(Hits);
	}
	
    if (OnCompletedDelegate.IsBound())
    {
    	OnCompletedDelegate.Execute(Hits);
    }
}

void UArcCoreAbilitySystemComponent::SendRequestCustomTargets(const FGameplayAbilityTargetDataHandle& ClientResult
															 , UArcTargetingObject* InTrace
															 , const FGameplayAbilitySpecHandle& RequestHandle)
{
	ServerRequestCustomTargets(ClientResult
		, InTrace
		, RequestHandle);
}

void UArcCoreAbilitySystemComponent::ServerRequestCustomTargets_Implementation(
	const FGameplayAbilityTargetDataHandle& ClientResult
	, class UArcTargetingObject* InTrace
	, const FGameplayAbilitySpecHandle& RequestHandle)
{
	//bool bValid = InTrace->Verify(this
	//	, ClientResult);

	if (WaitingCustomAbilityTargetRequests.Contains(RequestHandle))
	{
		WaitingCustomAbilityTargetRequests[RequestHandle].ExecuteIfBound(ClientResult);
	}
}

void FArcCoreGlobalTargetingEntry::HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle)
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	if (!ArcASC->GetWorld())
	{
		return;
	}
	
	if (TargetingResults.TargetResults.Num() == 1)
	{
		HitResult = TargetingResults.TargetResults[0].HitResult;
		UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(ArcASC->GetWorld());
		if (WorldDelegates)
		{
			WorldDelegates->OnAnyGlobalHitResultChaged.Broadcast(TargetingTag, HitResult);
			WorldDelegates->BroadcastOnGlobalHitResultChangedMap(TargetingTag, HitResult);	
		}
		
		
		if (HitResult.bBlockingHit)
		{
			HitLocation = HitResult.ImpactPoint;
		}
		else
		{
			HitLocation = HitResult.TraceEnd;
		}
	}
	else
	{
		HitResults.Reset(TargetingResults.TargetResults.Num());
		for (const FTargetingDefaultResultData& Result : TargetingResults.TargetResults)
		{
			HitResults.Add(Result.HitResult);
		}

		if (HitResults.Num() > 0)
		{
			HitResult = HitResults[0];	
		}
		
		UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(ArcASC->GetWorld());
		if (WorldDelegates)
		{
			WorldDelegates->OnAnyGlobalHitResultChaged.Broadcast(TargetingTag, HitResult);
			WorldDelegates->BroadcastOnGlobalHitResultChangedMap(TargetingTag, HitResult);	
		}
	}
}

void FArcCoreGlobalTargetingEntry::StartTargeting(APawn* InPawn
	, UTargetingSubsystem* Targeting
	, const FArcTargetingSourceContext& Context
	, UArcCoreAbilitySystemComponent* InArcASC
	, const FGameplayTag& InTargetingTag)
{
	ArcASC = InArcASC;
	TargetingTag = InTargetingTag;

	ENetMode NM = InArcASC->GetNetMode();
	bool bCanActivate = false;
	
	if (NM == ENetMode::NM_Standalone)
	{
		bCanActivate = true;
	}
	else if (NM == ENetMode::NM_Client)
	{
		if (bIsClient == true)
		{
			bCanActivate = true;
		}
	}
	else if (NM == ENetMode::NM_DedicatedServer)
	{
		if (bIsServer == true)
		{
			bCanActivate = true;
		}
	}
	
	if (TargetingPreset && bCanActivate)
	{
		FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(ArcASC.Get(), &UArcCoreAbilitySystemComponent::HandleTargetingCompleted, InTargetingTag);
		TargetingHandle = Arcx::MakeTargetRequestHandle(TargetingPreset, Context);
		FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(TargetingHandle);
		
		AsyncTaskData.bRequeueOnCompletion = true;
		Targeting->StartAsyncTargetingRequestWithHandle(TargetingHandle, CompletionDelegate);
	}
}

void FArcCoreGlobalTargetingEntry::EndTargeting(UTargetingSubsystem* Targeting)
{
	Targeting->RemoveAsyncTargetingRequestWithHandle(TargetingHandle);
}

void UArcCoreAbilitySystemComponent::HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle
	, FGameplayTag TargetingTag)
{
	if (FArcCoreGlobalTargetingEntry* Targeting = TargetingPresets.Find(TargetingTag))
	{
		Targeting->HandleTargetingCompleted(TargetingRequestHandle);
	}
}

void UArcCoreAbilitySystemComponent::GiveGlobalTargetingPreset(const TMap<FGameplayTag, FArcCoreGlobalTargetingEntry>& Preset)
{
	TMap<FGameplayTag, FArcCoreGlobalTargetingEntry> OldPreset = TargetingPresets;
	TargetingPresets = Preset;
	
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting == nullptr)
	{
		return;
	}
	
	for (TPair<FGameplayTag, FArcCoreGlobalTargetingEntry>& Pair : OldPreset)
	{
		Pair.Value.EndTargeting(Targeting);
	}

	
	if (APawn* P = Cast<APawn>(AbilityActorInfo->AvatarActor.Get()))
	{		
		FArcTargetingSourceContext Context;
		Context.InstigatorActor = P->GetPlayerState();
		Context.SourceActor = P;
	
		for (TPair<FGameplayTag, FArcCoreGlobalTargetingEntry>& Pair : TargetingPresets)
		{
			Pair.Value.StartTargeting(P, Targeting, Context, this, Pair.Key);
		}
	}
}

FHitResult UArcCoreAbilitySystemComponent::GetHitResult(FGameplayTag Tag, bool& bWasSuccessful)
{
	if (const FArcCoreGlobalTargetingEntry* Entry = TargetingPresets.Find(Tag))
	{
		bWasSuccessful = true;
		return Entry->HitResult;
	}

	bWasSuccessful = false;
	return FHitResult();
}

FVector UArcCoreAbilitySystemComponent::GetHitLocation(FGameplayTag Tag, bool& bWasSuccessful)
{
	if (const FArcCoreGlobalTargetingEntry* Entry = TargetingPresets.Find(Tag))
	{
		bWasSuccessful = true;
		return Entry->HitLocation;
	}

	bWasSuccessful = false;
	return FVector();
}

UTargetingPreset* UArcCoreAbilitySystemComponent::GetGlobalTargetingPreset(FGameplayTag Tag
	, bool& bWasSuccessful)
{
	if (const FArcCoreGlobalTargetingEntry* Entry = TargetingPresets.Find(Tag))
	{
		bWasSuccessful = true;
		return Entry->TargetingPreset;
	}

	bWasSuccessful = false;
	return nullptr;
}

FHitResult UArcCoreAbilitySystemComponent::GetGlobalHitResult(UArcCoreAbilitySystemComponent* InASC
															  , FGameplayTag TargetingTag, bool& bWasSuccessful)
{
	return InASC->GetHitResult(TargetingTag, bWasSuccessful);
}

FHitResult UArcCoreAbilitySystemComponent::GetGlobalHitResult(UAbilitySystemComponent* InASC
	, FGameplayTag TargetingTag, bool& bWasSuccessful)
{
	if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(InASC))
	{
		return GetGlobalHitResult(ASC, TargetingTag, bWasSuccessful);
	}

	return FHitResult();
}

FVector UArcCoreAbilitySystemComponent::GetGlobalHitLocation(UArcCoreAbilitySystemComponent* InASC
	, FGameplayTag TargetingTag, bool& bWasSuccessful)
{
	return InASC->GetHitLocation(TargetingTag, bWasSuccessful);
}

FVector UArcCoreAbilitySystemComponent::GetGlobalHitLocation(UAbilitySystemComponent* InASC
	, FGameplayTag TargetingTag, bool& bWasSuccessful)
{
	if (UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(InASC))
	{
		return GetGlobalHitLocation(ASC, TargetingTag, bWasSuccessful);
	}

	return FVector();
}

UTargetingPreset* UArcCoreAbilitySystemComponent::GetGlobalTargetingPreset(UArcCoreAbilitySystemComponent* InASC
	, FGameplayTag TargetingTag
	, bool& bWasSuccessful)
{
	return InASC->GetGlobalTargetingPreset(TargetingTag, bWasSuccessful);
}

FHitResult UArcCoreAbilitySystemComponent::BP_GetGlobalHitResult(UAbilitySystemComponent* InASC
																 , FGameplayTag TargetingTag, bool& bWasSuccessful)
{
	return GetGlobalHitResult(InASC, TargetingTag, bWasSuccessful);
}

FVector UArcCoreAbilitySystemComponent::BP_GetGlobalHitLocation(UAbilitySystemComponent* InASC
	, FGameplayTag TargetingTag, bool& bWasSuccessful)
{
	return GetGlobalHitLocation(InASC, TargetingTag, bWasSuccessful);
}

UTargetingPreset* UArcCoreAbilitySystemComponent::BP_GetGlobalTargetingPreset(UArcCoreAbilitySystemComponent* InASC
	, FGameplayTag TargetingTag
	, bool& bWasSuccessful)
{
	return GetGlobalTargetingPreset(InASC, TargetingTag, bWasSuccessful);
}


#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "ArcCore/AbilitySystem/ArcItemGameplayAbility.h"



void UArcAbilityActorComponent::Initialize(UArcCoreAbilitySystemComponent* OwningASC
                                           , UArcCoreGameplayAbility* InInstigatorAbility
                                           , TOptional<FVector> InTargetLocation)
{
	ASC = OwningASC;
	InstigatorAbility = InInstigatorAbility;
	if (InTargetLocation.IsSet())
	{
		TargetLocation = InTargetLocation.GetValue();
		bHaveTargetLocation = true;
	}
	else
	{
		bHaveTargetLocation = false;
	}
	RootPrim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	OnInitializedDelegate.Broadcast();
	OnInitialized();
}

AActor* UArcAbilityActorComponent::GetAvatar() const
{
	if (!ASC.IsValid())
	{
		return nullptr;
	}
	
	return ASC->GetAvatarActor();
}

AActor* UArcAbilityActorComponent::GetOwnerActor() const
{
	if (!ASC.IsValid())
	{
		return nullptr;
	}
	
	return ASC->GetAvatarActor();
}


const UArcItemDefinition* UArcAbilityActorComponent::GetSourceItemData() const
{
	UArcItemGameplayAbility* Ability = Cast<UArcItemGameplayAbility>(InstigatorAbility);
	if (!Ability)
	{
		return nullptr;
	}
	
	return Ability->GetSourceItemData();
}

DEFINE_FUNCTION(UArcAbilityActorComponent::execFindItemFragment)
{
	P_GET_OBJECT(UScriptStruct, InFragmentType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	
	P_FINISH;
	bool bSuccess = false;

	if (P_THIS->InstigatorAbility == nullptr)
	{
		*(bool*)RESULT_PARAM = false;
		return;
	}
	UArcItemGameplayAbility* Ability = Cast<UArcItemGameplayAbility>(P_THIS->InstigatorAbility);
	if (!Ability)
	{
		*(bool*)RESULT_PARAM = false;
		return;
	}
	
	FArcItemData* ItemData = Ability->GetSourceItemEntryPtr();
	if (ItemData)
	{
		P_NATIVE_BEGIN;
		const uint8* Fragment = ArcItemsHelper::FindFragment(ItemData, InFragmentType);
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