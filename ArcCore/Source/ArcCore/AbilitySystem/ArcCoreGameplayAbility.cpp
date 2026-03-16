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

#include "ArcCore/AbilitySystem/ArcAbilityCost.h"
#include "ArcCore/ArcCoreModule.h"
#include "Components/CapsuleComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ArcCoreCharacter.h"
#include "Player/ArcHeroComponentBase.h"

#include "Pawn/ArcCorePawn.h"
#include "Targeting/ArcTargetingObject.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

DEFINE_LOG_CATEGORY(LogArcAbility);

UArcCoreGameplayAbility::UArcCoreGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::Type::InstancedPerActor;
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
	Super::OnAvatarSet(ActorInfo, Spec);

	OnAvatarSetSafe(ActorInfo, Spec);

	BP_OnAvatarSet(*ActorInfo, Spec);
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

	// Cancel all latent actions before processing end actions
	if (LatentActions.Num() > 0)
	{
		FArcAbilityActionContext CancelContext;
		CancelContext.Ability = this;
		CancelContext.Handle = Handle;
		CancelContext.ActorInfo = ActorInfo;
		CancelContext.ActivationInfo = ActivationInfo;
		CancelContext.bWasCancelled = bWasCancelled;

		for (auto& [Tag, ActionView] : LatentActions)
		{
			if (FArcAbilityAction* Action = const_cast<FArcAbilityAction*>(ActionView.GetPtr<FArcAbilityAction>()))
			{
				Action->CancelLatent(CancelContext);
			}
		}
		LatentActions.Empty();
	}

	ProcessEndActions(bWasCancelled);

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

	ProcessActivateActions();
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
	return Super::MakeEffectContext(Handle, ActorInfo);
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

	NewSpecHandle.Data->GetContext().SetAbility(this);

	return NewSpecHandle;
}

FGameplayEffectContextHandle UArcCoreGameplayAbility::BP_MakeEffectContext() const
{
	return MakeEffectContext(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());
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

	ProcessLocalTargetActions(Hits, TargetingRequestHandle);

	OnLocalTargetResult(Hits);
}

void UArcCoreGameplayAbility::NativeOnAbilityTargetResult(const FGameplayAbilityTargetDataHandle& AbilityTargetData)
{
	UArcCoreAbilitySystemComponent* ArcASC = GetArcASC();
	if (!ArcASC || !ArcASC->GetOwner()) { return; }

	ENetMode NM = ArcASC->GetOwner()->GetNetMode();

	ProcessAbilityTargetActions(AbilityTargetData);

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

float UArcCoreGameplayAbility::GetAnimationPlayRate() const
{
	return 1.0f;
}

AActor* UArcCoreGameplayAbility::SpawnAbilityActor(TSubclassOf<AActor> ActorClass
	, const FGameplayAbilityTargetDataHandle& InTargetData
	, TOptional<FVector> TargetLocation
	, EArcAbilityActorSpawnOrigin SpawnOrigin
	, FVector CustomSpawnLocation)
{
	if (ActorClass == nullptr)
	{
		return nullptr;
	}

	if (!InTargetData.IsValid(0))
	{
		return nullptr;
	}

	FVector Location = InTargetData.Get(0)->GetEndPoint();
	FRotator Rotation = InTargetData.Get(0)->GetEndPointTransform().GetRotation().Rotator();

	switch (SpawnOrigin)
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
			if (InTargetData.Get(0)->HasOrigin())
			{
				FTransform OriginTM = InTargetData.Get(0)->GetOrigin();
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

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (SpawnedActor == nullptr)
	{
		return nullptr;
	}

	UArcAbilityActorComponent* AAC = SpawnedActor->FindComponentByClass<UArcAbilityActorComponent>();
	if (AAC)
	{
		UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
		AAC->Initialize(ArcASC, this, TargetLocation);
	}

	return SpawnedActor;
}

void UArcCoreGameplayAbility::ExecuteActionList(TArray<FInstancedStruct>& Actions, FArcAbilityActionContext& Context)
{
	for (FInstancedStruct& ActionStruct : Actions)
	{
		if (!ActionStruct.IsValid())
		{
			continue;
		}

		FArcAbilityAction* Action = ActionStruct.GetMutablePtr<FArcAbilityAction>();
		if (!Action)
		{
			continue;
		}

		if (Action->IsLatent())
		{
			if (LatentActions.Contains(Action->LatentTag))
			{
				continue;
			}
			Action->Execute(Context);
			LatentActions.Add(Action->LatentTag, FStructView(ActionStruct));
		}
		else
		{
			Action->Execute(Context);
		}
	}
}

void UArcCoreGameplayAbility::CancelLatentAction(FGameplayTag Tag, FArcAbilityActionContext& Context)
{
	if (FStructView* Found = LatentActions.Find(Tag))
	{
		if (FArcAbilityAction* Action = const_cast<FArcAbilityAction*>(Found->GetPtr<FArcAbilityAction>()))
		{
			Action->CancelLatent(Context);
		}
		LatentActions.Remove(Tag);
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
