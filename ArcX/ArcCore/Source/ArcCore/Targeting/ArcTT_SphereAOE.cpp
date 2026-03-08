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
#include "ArcTraceOrigin.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TargetingSystem/TargetingSubsystem.h"

void UArcTT_SphereAOE::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx || !Ctx->SourceActor)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	FVector SourceLocation;

	const FArcTraceOrigin* Origin = TraceOriginOverride.GetPtr<FArcTraceOrigin>();
	if (Origin)
	{
		FVector Direction;
		bool bHasDirection = false;
		if (!Origin->Resolve(TargetingHandle, SourceLocation, Direction, bHasDirection))
		{
			SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
			return;
		}
	}
	else
	{
		if (TargetingResults.TargetResults.Num() > 0)
		{
			const FHitResult& PrevHit = TargetingResults.TargetResults[0].HitResult;
			SourceLocation = PrevHit.bBlockingHit ? PrevHit.ImpactPoint : PrevHit.TraceEnd;
		}
		else
		{
			SourceLocation = Ctx->SourceLocation;
		}
	}

	UWorld* World = Ctx->SourceActor->GetWorld();

	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, SourceLocation, SourceLocation + FVector(0, 0, 1),
		FQuat::Identity, CollisionChannel,
		FCollisionShape::MakeSphere(Radius.GetValue()), Params, ResponseParams);

	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = Hit;
	}

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		DrawDebugSphere(World, SourceLocation, Radius.GetValue(), 16, FColor::Red, false, DrawTime, 0, 1.f);
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, 8.f, 8, FColor::Green, false, DrawTime, 0, 1.f);
		}
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_SphereAOE::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("SphereAOE - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif
