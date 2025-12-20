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

#include "GameplayEffectExecutionCalculation.h"
#include "GameplayEffectExtension.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"

#include "ArcCore/AbilitySystem/ArcTargetingBPF.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "ArcCoreGameplayAbility.h"

#include "ArcCore/AbilitySystem/ArcAbilitiesTypes.h"
#include "Targeting/ArcTargetingSourceContext.h"

FGameplayAbilityTargetDataHandle UArcTargetingBPF::SingleTargetHitFromHitArray(UGameplayAbility* SourceAbility
																			   , const TArray<FHitResult>& InHits)
{
	FGameplayAbilityTargetData_SingleTargetHit* Hit = new FGameplayAbilityTargetData_SingleTargetHit(InHits[0]);

	FGameplayAbilityTargetDataHandle Handle(Hit);
	return Handle;
}

AActor* UArcTargetingBPF::GetActorFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData)
{
	TArray<AActor*> Result;
	for (int32 TargetDataIndex = 0; TargetDataIndex < TargetData.Data.Num(); ++TargetDataIndex)
	{
		if (TargetData.Data.IsValidIndex(TargetDataIndex))
		{
			const FGameplayAbilityTargetData* DataAtIndex = TargetData.Data[TargetDataIndex].Get();
			if (DataAtIndex)
			{
				TArray<TWeakObjectPtr<AActor>> WeakArray = DataAtIndex->GetActors();
				for (TWeakObjectPtr<AActor>& WeakPtr : WeakArray)
				{
					return WeakPtr.Get();
				}
			}
		}
	}
	return nullptr;
}

FGameplayAbilityTargetDataHandle UArcTargetingBPF::HitResultArray(const TArray<FHitResult>& HitResults)
{
	FGameplayAbilityTargetDataHandle Handle;
	Handle.Data.Reserve(HitResults.Num());
	
	for (const FHitResult& Hit : HitResults)
	{
		FGameplayAbilityTargetData_SingleTargetHit*	NewData = new FGameplayAbilityTargetData_SingleTargetHit();
		NewData->HitResult = Hit;
		Handle.Add(NewData);
	}
	

	return Handle;
}

FGameplayAbilityTargetDataHandle UArcTargetingBPF::MakeHitResultTargetData(FTargetingRequestHandle InHandle)
{
	FGameplayAbilityTargetDataHandle Handle;
	
	FTargetingDefaultResultsSet* TargetingResults = FTargetingDefaultResultsSet::Find(InHandle);
	if (!TargetingResults || TargetingResults->TargetResults.IsEmpty())
	{
		return Handle;
	}
	FGameplayAbilityTargetData_SingleTargetHit*	NewData = new FGameplayAbilityTargetData_SingleTargetHit();
	NewData->HitResult = TargetingResults->TargetResults[0].HitResult;
	Handle.Add(NewData);
	
	return Handle;
}

FGameplayAbilityTargetDataHandle UArcTargetingBPF::MakeHitResultArrayTargetData(FTargetingRequestHandle InHandle)
{
	FGameplayAbilityTargetDataHandle Handle;
	
	FTargetingDefaultResultsSet* TargetingResults = FTargetingDefaultResultsSet::Find(InHandle);
	
	Handle.Data.Reserve(TargetingResults->TargetResults.Num());
	
	for (const FTargetingDefaultResultData& Hit : TargetingResults->TargetResults)
	{
		FGameplayAbilityTargetData_SingleTargetHit*	NewData = new FGameplayAbilityTargetData_SingleTargetHit();
		NewData->HitResult = Hit.HitResult;
		Handle.Add(NewData);
	}
	
	return Handle;
}

FGameplayAbilityTargetDataHandle UArcTargetingBPF::MakeProjectileTargetData(FTargetingRequestHandle InHandle)
{
	FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(InHandle);
	FTargetingDefaultResultsSet* TargetingResults = FTargetingDefaultResultsSet::Find(InHandle);
	if (!SourceContext || !TargetingResults || TargetingResults->TargetResults.IsEmpty())
	{
		// Log a warning if the source context is null
		// This can happen if the targeting request was not properly initialized or has been cleared
		// Handle is used to identify the request, so we log it for debugging purposes
		if (SourceContext)
		{
			UE_LOG(LogArcAbility, Warning, TEXT("MakeProjectileTargetData: TargetingResults is null for handle %d"), InHandle.Handle);
		}
		else
		{
			UE_LOG(LogArcAbility, Warning, TEXT("MakeProjectileTargetData: SourceContext is null for handle %d"), InHandle.Handle);
		}
		
		return FGameplayAbilityTargetDataHandle();
	}
	
	FGameplayAbilityTargetDataHandle Handle;

	FArcGameplayAbilityTargetData_ProjectileTargetHit*	NewData = new FArcGameplayAbilityTargetData_ProjectileTargetHit();
	NewData->Origin = SourceContext->SourceLocation;
	NewData->HitResult = TargetingResults->TargetResults[0].HitResult;
	Handle.Add(NewData);
	
	return Handle;
}

FGameplayAbilityTargetDataHandle UArcTargetingBPF::MakeActorSpawnLocation(FTargetingRequestHandle InHandle)
{
	FGameplayAbilityTargetDataHandle Handle;

	return Handle;
}
