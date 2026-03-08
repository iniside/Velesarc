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

#include "AbilitySystem/Tasks/ArcAT_MeleeTraceWindow.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"
#include "AbilitySystem/ArcGameplayAbilityActorInfo.h"
#include "AbilitySystem/ArcAbilitiesBPF.h"
#include "AbilitySystem/Fragments/ArcItemFragment_MeleeSockets.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "Equipment/ArcSkeletalMeshOwnerInterface.h"
#include "Items/ArcItemDefinition.h"

UArcAT_MeleeTraceWindow::UArcAT_MeleeTraceWindow()
{
	bTickingTask = true;
}

UArcAT_MeleeTraceWindow* UArcAT_MeleeTraceWindow::MeleeTraceWindow(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	FGameplayTag InBeginEventTag,
	FGameplayTag InEndEventTag,
	FGameplayTag InHitEventTag,
	FGameplayTag InWindowEndEventTag)
{
	UArcAT_MeleeTraceWindow* Task = NewAbilityTask<UArcAT_MeleeTraceWindow>(OwningAbility, TaskInstanceName);
	Task->BeginEventTag = InBeginEventTag;
	Task->EndEventTag = InEndEventTag;
	Task->HitEventTag = InHitEventTag;
	Task->WindowEndEventTag = InWindowEndEventTag;
	return Task;
}

void UArcAT_MeleeTraceWindow::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndTask();
		return;
	}

	CachedEventTags.Reset();
	CachedEventTags.AddTag(BeginEventTag);
	CachedEventTags.AddTag(EndEventTag);

	EventDelegateHandle = ASC->AddGameplayEventTagContainerDelegate(
		CachedEventTags,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
			this, &UArcAT_MeleeTraceWindow::OnGameplayEvent));
}

void UArcAT_MeleeTraceWindow::TickTask(float DeltaTime)
{
	if (bIsTracing)
	{
		PerformTraces();
	}
}

void UArcAT_MeleeTraceWindow::OnDestroy(bool bInOwnerFinished)
{
	if (bIsTracing)
	{
		EndTraceWindow();
	}

	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->RemoveGameplayEventTagContainerDelegate(CachedEventTags, EventDelegateHandle);
	}

	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_MeleeTraceWindow::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (EventTag == BeginEventTag)
	{
		BeginTraceWindow();
	}
	else if (EventTag == EndEventTag)
	{
		EndTraceWindow();
	}
}

void UArcAT_MeleeTraceWindow::BeginTraceWindow()
{
	const UArcItemGameplayAbility* ItemAbility = Cast<UArcItemGameplayAbility>(Ability);
	if (!ItemAbility)
	{
		return;
	}

	const UArcItemDefinition* ItemDef = ItemAbility->NativeGetSourceItemData();
	if (!ItemDef)
	{
		return;
	}

	const FArcItemFragment_MeleeSockets* SocketFragment = ItemDef->FindFragment<FArcItemFragment_MeleeSockets>();
	if (!SocketFragment)
	{
		return;
	}

	CachedTipSocket = SocketFragment->TipSocketName;
	CachedBaseSocket = SocketFragment->BaseSocketName;
	CachedTraceRadius = SocketFragment->TraceRadius;
	CachedTraceChannel = SocketFragment->TraceChannel;

	CachedMeshComponent = ResolveMeshComponent();
	if (!CachedMeshComponent.IsValid())
	{
		return;
	}

	HitActors.Reset();
	AllWindowHits.Reset();
	bHasPreviousFrame = false;
	bIsTracing = true;
}

void UArcAT_MeleeTraceWindow::EndTraceWindow()
{
	if (!bIsTracing)
	{
		return;
	}

	bIsTracing = false;

	// Fire window-end gameplay event on ASC
	if (WindowEndEventTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
		{
			FGameplayEventData EventData;
			EventData.Instigator = GetAvatarActor();
			EventData.TargetData = UArcAbilitiesBPF::AbilityTargetDataFromHitResultArray(AllWindowHits);
			ASC->HandleGameplayEvent(WindowEndEventTag, &EventData);
		}
	}

	OnTraceWindowEnd.Broadcast(AllWindowHits);
}

void UArcAT_MeleeTraceWindow::PerformTraces()
{
	UMeshComponent* Mesh = CachedMeshComponent.Get();
	if (!Mesh)
	{
		bIsTracing = false;
		return;
	}

	const FVector CurrentTip = Mesh->GetSocketLocation(CachedTipSocket);
	const FVector CurrentBase = Mesh->GetSocketLocation(CachedBaseSocket);

	if (bHasPreviousFrame)
	{
		AActor* AvatarActor = GetAvatarActor();
		UWorld* World = GetWorld();
		if (!World || !AvatarActor)
		{
			return;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeTrace), false, AvatarActor);

		const FVector TraceEndpoints[] = { PrevTipLocation, CurrentTip, PrevBaseLocation, CurrentBase };

		for (int32 i = 0; i < 2; ++i)
		{
			const FVector& Start = TraceEndpoints[i * 2];
			const FVector& End = TraceEndpoints[i * 2 + 1];

			TArray<FHitResult> Hits;

			if (CachedTraceRadius > 0.f)
			{
				World->SweepMultiByChannel(
					Hits, Start, End,
					FQuat::Identity,
					CachedTraceChannel,
					FCollisionShape::MakeSphere(CachedTraceRadius),
					QueryParams);
			}
			else
			{
				World->LineTraceMultiByChannel(Hits, Start, End, CachedTraceChannel, QueryParams);
			}

			for (const FHitResult& Hit : Hits)
			{
				AActor* HitActor = Hit.GetActor();
				if (!HitActor)
				{
					continue;
				}

				TWeakObjectPtr<AActor> WeakHit(HitActor);
				if (HitActors.Contains(WeakHit))
				{
					continue;
				}

				HitActors.Add(WeakHit);
				AllWindowHits.Add(Hit);

				DispatchHit(Hit);
				OnMeleeHit.Broadcast(Hit);
			}
		}
	}

	PrevTipLocation = CurrentTip;
	PrevBaseLocation = CurrentBase;
	bHasPreviousFrame = true;
}

void UArcAT_MeleeTraceWindow::DispatchHit(const FHitResult& Hit)
{
	TArray<FHitResult> SingleHit;
	SingleHit.Add(Hit);
	FGameplayAbilityTargetDataHandle TargetData = UArcAbilitiesBPF::AbilityTargetDataFromHitResultArray(SingleHit);

	// Fire gameplay event for CachedEventActions dispatch
	if (HitEventTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
		{
			FGameplayEventData EventData;
			EventData.Instigator = GetAvatarActor();
			EventData.Target = Hit.GetActor();
			EventData.TargetData = TargetData;
			ASC->HandleGameplayEvent(HitEventTag, &EventData);
		}
	}

	// Send through ability targeting pipeline for ProcessAbilityTargetActions + replication
	if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability))
	{
		ArcAbility->SendTargetingResult(TargetData, nullptr);
	}
}

UMeshComponent* UArcAT_MeleeTraceWindow::ResolveMeshComponent() const
{
	const UArcItemGameplayAbility* ItemAbility = Cast<UArcItemGameplayAbility>(Ability);
	if (!ItemAbility)
	{
		return nullptr;
	}

	const UArcItemDefinition* ItemDef = ItemAbility->NativeGetSourceItemData();
	if (!ItemDef)
	{
		return nullptr;
	}

	const FArcItemFragment_MeleeSockets* SocketFragment = ItemDef->FindFragment<FArcItemFragment_MeleeSockets>();
	if (!SocketFragment)
	{
		return nullptr;
	}

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor)
	{
		return nullptr;
	}

	if (SocketFragment->bUseCharacterMesh)
	{
		if (IArcSkeletalMeshOwnerInterface* MeshOwner = Cast<IArcSkeletalMeshOwnerInterface>(AvatarActor))
		{
			return MeshOwner->GetSkeletalMesh();
		}
		return nullptr;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return nullptr;
	}

	const FArcGameplayAbilityActorInfo* ArcActorInfo = static_cast<const FArcGameplayAbilityActorInfo*>(ActorInfo);
	UArcItemAttachmentComponent* AttachmentComp = ArcActorInfo->ItemAttachmentComponent.Get();
	if (!AttachmentComp)
	{
		return nullptr;
	}

	const FArcItemId SourceItemId = ItemAbility->GetSourceItemHandle();

	if (AActor* WeaponActor = AttachmentComp->GetAttachedActor(SourceItemId))
	{
		if (UMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<UMeshComponent>())
		{
			return WeaponMesh;
		}
	}

	return AttachmentComp->FindFirstAttachedObject<UMeshComponent>(ItemDef);
}
