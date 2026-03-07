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

#include "ArcAbilityAction_SpawnVisualization.h"

#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "Targeting/ArcAoEVisualizationInterface.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Engine/World.h"

void FArcAbilityAction_SpawnVisualization::Execute(FArcAbilityActionContext& Context)
{
	UArcCoreGameplayAbility* Ability = Context.Ability;
	UWorld* World = Ability->GetWorld();
	if (!World)
	{
		return;
	}

	// Resolve targeting object and vis actor class — prefer overrides, fallback to item fragments
	UArcTargetingObject* TargetingObj = TargetingObjectOverride;
	TSoftClassPtr<AActor> VisActorClass = VisualizationActorClassOverride;

	if (UArcItemGameplayAbility* ItemAbility = Cast<UArcItemGameplayAbility>(Ability))
	{
		if (const FArcItemData* ItemData = ItemAbility->GetSourceItemEntryPtr())
		{
			CachedShapeData = FArcAoEShapeData::FromItemData(ItemData);

			if (!TargetingObj || VisActorClass.IsNull())
			{
				if (const auto* Fragment = ArcItemsHelper::FindFragment<FArcItemFragment_TargetingObjectPreset>(ItemData))
				{
					if (!TargetingObj)
					{
						TargetingObj = Fragment->TargetingObject;
					}
					if (VisActorClass.IsNull())
					{
						VisActorClass = Fragment->VisualizationActor;
					}
				}
			}
		}
	}

	// Spawn visualization actor
	if (!VisActorClass.IsNull())
	{
		if (UClass* LoadedClass = VisActorClass.LoadSynchronous())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* Actor = World->SpawnActor<AActor>(LoadedClass, FTransform::Identity, SpawnParams);
			SpawnedActor = Actor;

			if (Actor && Actor->Implements<UArcAoEVisualizationInterface>())
			{
				FArcAoEVisualizationContext VisContext;
				VisContext.ShapeData = CachedShapeData;
				VisContext.AvatarActor = Ability->GetAvatarActorFromActorInfo();
				VisContext.SourceAbility = Ability;
				IArcAoEVisualizationInterface::Execute_InitializeAoEShape(Actor, VisContext);
			}
		}
	}

	// Start async targeting with continuous re-queue
	if (TargetingObj && TargetingObj->TargetingPreset)
	{
		if (UTargetingSubsystem* TargetingSub = UTargetingSubsystem::Get(World))
		{
			FArcTargetingSourceContext SourceContext;
			SourceContext.SourceActor = Ability->GetAvatarActorFromActorInfo();
			SourceContext.InstigatorActor = Context.ActorInfo->OwnerActor.Get();
			SourceContext.SourceObject = Ability;

			AsyncTargetingHandle = Arcx::MakeTargetRequestHandle(TargetingObj->TargetingPreset, SourceContext);

			FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(AsyncTargetingHandle);
			AsyncTaskData.bRequeueOnCompletion = true;

			FGameplayTag CapturedTag = LatentTag;
			TargetingSub->StartAsyncTargetingRequestWithHandle(AsyncTargetingHandle,
				FTargetingRequestDelegate::CreateWeakLambda(Ability, [Ability, CapturedTag](FTargetingRequestHandle Handle)
				{
					if (FStructView* Found = Ability->LatentActions.Find(CapturedTag))
					{
						if (FArcAbilityAction_SpawnVisualization* Action = Found->GetPtr<FArcAbilityAction_SpawnVisualization>())
						{
							Action->HandleAsyncTargetingComplete(Handle, Ability);
						}
					}
				}));
		}
	}
}

void FArcAbilityAction_SpawnVisualization::CancelLatent(FArcAbilityActionContext& Context)
{
	// Destroy visualization actor
	if (AActor* VisActor = SpawnedActor.Get())
	{
		VisActor->SetActorHiddenInGame(true);
		VisActor->Destroy();
		SpawnedActor.Reset();
	}
	
	// Stop async targeting
	if (AsyncTargetingHandle.IsValid())
	{
		if (UTargetingSubsystem* TargetingSub = UTargetingSubsystem::Get(Context.Ability->GetWorld()))
		{
			TargetingSub->RemoveAsyncTargetingRequestWithHandle(AsyncTargetingHandle);
		}
		AsyncTargetingHandle = FTargetingRequestHandle();
	}
}

void FArcAbilityAction_SpawnVisualization::HandleAsyncTargetingComplete(FTargetingRequestHandle Handle, UArcCoreGameplayAbility* Ability)
{
	FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(Handle);
	if (Results.TargetResults.Num() == 0)
	{
		return;
	}

	const FHitResult& Hit = Results.TargetResults[0].HitResult;

	FVector Location = Hit.ImpactPoint;
	FRotator Rotation = FRotator::ZeroRotator;

	if (CachedShapeData.Shape == EArcAoEShape::Box)
	{
		AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
		FVector SourceLocation = AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
		FVector Direction = Hit.ImpactPoint - SourceLocation;
		FVector BoxCenter;
		FQuat BoxRotation;
		CachedShapeData.ComputeBoxTransform(Hit.ImpactPoint, Direction, BoxCenter, BoxRotation);
		Location = BoxCenter;
		Rotation = BoxRotation.Rotator();
	}

	if (AActor* VisActor = SpawnedActor.Get())
	{
		if (VisActor->Implements<UArcAoEVisualizationInterface>())
		{
			IArcAoEVisualizationInterface::Execute_UpdateAoELocation(VisActor, Location, Rotation);
		}
		else
		{
			VisActor->SetActorLocationAndRotation(Location, Rotation);
		}
	}
}
