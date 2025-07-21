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



#include "ArcAT_WaitPerformTargetingWithVisualization.h"

#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/Actors/ArcTargetingVisualizationActor.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "Engine/World.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

UArcAT_WaitPerformTargetingWithVisualization* UArcAT_WaitPerformTargetingWithVisualization::WaitPerformTargetingWithVisualization(
	UGameplayAbility* OwningAbility
	, UArcTargetingObject* InTargetingObject)
{
	UArcAT_WaitPerformTargetingWithVisualization* Task = NewAbilityTask<UArcAT_WaitPerformTargetingWithVisualization>(OwningAbility);
	Task->TargetingObject = InTargetingObject;

	return Task;
}

void UArcAT_WaitPerformTargetingWithVisualization::Activate()
{
	Super::Activate();

	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability);
	
	if (AActor* SourceActor = GetAvatarActor())
	{
		if (UTargetingSubsystem* TargetingSubsystem = UTargetingSubsystem::Get(SourceActor->GetWorld()))
		{
			if (!TargetingObject->VisualizationActor.IsNull())
			{
				UClass* VisualizationActorClass = TargetingObject->VisualizationActor.LoadSynchronous();
				if (VisualizationActorClass)
				{
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					
					VisualizationActor = SourceActor->GetWorld()->SpawnActor<AArcTargetingVisualizationActor>(VisualizationActorClass, FTransform::Identity, SpawnParameters);
				}
				
				FArcTargetingSourceContext Context;
				Context.SourceActor = ArcAbility->GetAvatarActorFromActorInfo();
				Context.InstigatorActor = ArcAbility->GetActorInfo().OwnerActor.Get();
				Context.SourceObject = ArcAbility;
	
				AsyncTargetingHandle = Arcx::MakeTargetRequestHandle(TargetingObject->TargetingPreset, Context);
				
				FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcAT_WaitPerformTargetingWithVisualization::HandleTargetingCompleted);
				
				FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(AsyncTargetingHandle);
				AsyncTaskData.bRequeueOnCompletion = true;
				
				TargetingSubsystem->StartAsyncTargetingRequestWithHandle(AsyncTargetingHandle, CompletionDelegate);
			}
		}
	}
}

void UArcAT_WaitPerformTargetingWithVisualization::ExecuteTargeting()
{
	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability);
	if (VisualizationActor)
	{
		VisualizationActor->SetActorHiddenInGame(true);
		VisualizationActor->Destroy();
		VisualizationActor = nullptr;
	}
	
	if (AActor* SourceActor = GetAvatarActor())
	{
		if (UTargetingSubsystem* TargetingSubsystem = UTargetingSubsystem::Get(SourceActor->GetWorld()))
		{
			if (AsyncTargetingHandle.IsValid())
			{
				TargetingSubsystem->RemoveAsyncTargetingRequestWithHandle(AsyncTargetingHandle);	
			}

			FArcTargetingSourceContext Context;
			Context.SourceActor = ArcAbility->GetAvatarActorFromActorInfo();
			Context.InstigatorActor = ArcAbility->GetActorInfo().OwnerActor.Get();
			Context.SourceObject = ArcAbility;
			Context.ArcASC = Cast<UArcCoreAbilitySystemComponent>(ArcAbility->GetAbilitySystemComponentFromActorInfo());
			
			ExecuteTargetingHandle = Arcx::MakeTargetRequestHandle(TargetingObject->TargetingPreset, Context);
			
			TargetingSubsystem->ExecuteTargetingRequestWithHandle(ExecuteTargetingHandle
				, FTargetingRequestDelegate::CreateUObject(this, &UArcAT_WaitPerformTargetingWithVisualization::HandleExecuteTargetingComplete));
			
		}
	}
}

void UArcAT_WaitPerformTargetingWithVisualization::HandleExecuteTargetingComplete(FTargetingRequestHandle TargetingRequestHandle)
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	TArray<FHitResult> Hits;
	Hits.Reset(TargetingResults.TargetResults.Num());
	
	for (FTargetingDefaultResultData& Result : TargetingResults.TargetResults)
	{
		Hits.Add(Result.HitResult);
	}

	OnTargetingExcuted.Broadcast(Hits, TargetingRequestHandle);
}

void UArcAT_WaitPerformTargetingWithVisualization::HandleTargetingCompleted(FTargetingRequestHandle TargetingRequestHandle)
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);
	if (VisualizationActor)
	{
		if (TargetingResults.TargetResults.Num())
		{
			VisualizationActor->SetActorLocation(TargetingResults.TargetResults[0].HitResult.ImpactPoint);
		}
	}
}