// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTT_ForwardPoint.h"

#include "ArcTargetingSourceContext.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "TargetingSystem/TargetingSubsystem.h"

void UArcTT_ForwardPoint::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx || !Ctx->SourceActor)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	const FVector ActorLocation = Ctx->SourceActor->GetActorLocation();
	FRotator ActorRotation = Ctx->SourceActor->GetActorRotation();
	ActorRotation.Pitch = 0.f;

	const FVector Forward = ActorRotation.Vector();
	const FVector Right = FRotationMatrix(ActorRotation).GetScaledAxis(EAxis::Y);
	const FVector Up = FVector::UpVector;

	const FVector TargetPoint = ActorLocation
		+ Forward * Distance.GetValue()
		+ Right * RightOffset.GetValue()
		+ Up * UpOffset.GetValue();

	FHitResult HitResult;
	HitResult.Location = TargetPoint;
	HitResult.ImpactPoint = TargetPoint;
	HitResult.TraceStart = ActorLocation;
	HitResult.TraceEnd = TargetPoint;

	FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
	ResultData->HitResult = HitResult;

#if ENABLE_DRAW_DEBUG
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		const float DrawTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
		UWorld* World = Ctx->SourceActor->GetWorld();
		DrawDebugLine(World, ActorLocation, TargetPoint, FColor::Cyan, false, DrawTime, 0, 1.f);
		DrawDebugSphere(World, TargetPoint, 16.f, 16, FColor::Green, false, DrawTime, 0, 1.f);
	}
#endif

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

#if ENABLE_DRAW_DEBUG
void UArcTT_ForwardPoint::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
	if (UTargetingSubsystem::IsTargetingDebugEnabled())
	{
		FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		FString DebugStr = FString::Printf(TEXT("ForwardPoint - Results: %d"), Results.TargetResults.Num());
		TargetingSubsystem->DebugLine(Info, DebugStr, XOffset, YOffset, MinTextRowsToAdvance);
	}
}
#endif
