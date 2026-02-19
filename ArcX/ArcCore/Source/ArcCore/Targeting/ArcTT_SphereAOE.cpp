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

#include "ArcTT_SphereAOE.h"


#include "ArcTargetingSourceContext.h"
#include "CollisionDebugDrawingPublic.h"

#include "CollisionQueryParams.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UArcTT_SphereAOE::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	FVector SourceLocation = FVector::ZeroVector;
	if (GlobalTargetingSource.IsValid())
	{
		UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(Ctx->SourceAbility->GetAbilitySystemComponentFromActorInfo());
		if (ArcASC)
		{
			bool bWasSuccessful = false;
			SourceLocation = ArcASC->GetGlobalHitLocation(ArcASC, GlobalTargetingSource, bWasSuccessful);
		}
	}
	else
	{
		if (TargetingResults.TargetResults.Num() > 0)
		{
			FTargetingDefaultResultData Data = TargetingResults.TargetResults[0];
			
			SourceLocation = Data.HitResult.ImpactPoint;
		}
		
	}
	UWorld* World = Ctx->SourceActor->GetWorld();

	FVector StartLocation = SourceLocation;
	FVector EndLocation = StartLocation + FVector(0,0,1);
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, CollisionChannel,
		FCollisionShape::MakeSphere(Radius.GetValue()), Params, ResponseParams);
	
	FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
	ResultData->HitResult = HitResult;
	
	DrawDebugSphere(World, EndLocation, Radius.GetValue(), 32, FColor::Yellow, false, 2, 0, 1.f);
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
