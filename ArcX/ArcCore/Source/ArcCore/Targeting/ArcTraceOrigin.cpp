// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTraceOrigin.h"

#include "ArcTargetingSourceContext.h"
#include "ArcFloorLocationInterface.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

bool FArcTraceOrigin::Resolve(const FTargetingRequestHandle& Handle,
                              FVector& OutOrigin, FVector& OutDirection,
                              bool& bHasDirection) const
{
	bHasDirection = false;
	return false;
}

bool FArcTraceOrigin_EyesViewPoint::Resolve(const FTargetingRequestHandle& Handle,
                                            FVector& OutOrigin, FVector& OutDirection,
                                            bool& bHasDirection) const
{
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(Handle);
	if (!Ctx || !Ctx->SourceActor)
	{
		bHasDirection = false;
		return false;
	}

	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(OutOrigin, EyeRotation);
	OutDirection = EyeRotation.Vector();
	bHasDirection = true;
	return true;
}

bool FArcTraceOrigin_PreviousResult::Resolve(const FTargetingRequestHandle& Handle,
                                             FVector& OutOrigin, FVector& OutDirection,
                                             bool& bHasDirection) const
{
	FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(Handle);
	if (Results.TargetResults.IsEmpty())
	{
		bHasDirection = false;
		return false;
	}

	const FHitResult& PrevHit = Results.TargetResults[0].HitResult;
	OutOrigin = PrevHit.bBlockingHit ? PrevHit.ImpactPoint : PrevHit.TraceEnd;
	bHasDirection = false;
	return true;
}

bool FArcTraceOrigin_Socket::Resolve(const FTargetingRequestHandle& Handle,
                                     FVector& OutOrigin, FVector& OutDirection,
                                     bool& bHasDirection) const
{
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(Handle);
	if (!Ctx || !Ctx->SourceActor)
	{
		bHasDirection = false;
		return false;
	}

	USkeletalMeshComponent* SMC = Ctx->SourceActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SMC || !SMC->DoesSocketExist(SocketName))
	{
		bHasDirection = false;
		return false;
	}

	OutOrigin = SMC->GetSocketLocation(SocketName);
	bHasDirection = false;
	return true;
}

bool FArcTraceOrigin_WorldLocation::Resolve(const FTargetingRequestHandle& Handle,
                                            FVector& OutOrigin, FVector& OutDirection,
                                            bool& bHasDirection) const
{
	OutOrigin = Location;
	OutDirection = Rotation.Vector();
	bHasDirection = !Rotation.IsZero();
	return true;
}

bool FArcTraceOrigin_GlobalTargeting::Resolve(const FTargetingRequestHandle& Handle,
                                              FVector& OutOrigin, FVector& OutDirection,
                                              bool& bHasDirection) const
{
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(Handle);
	if (!Ctx || !Ctx->ArcASC || !TargetingTag.IsValid())
	{
		bHasDirection = false;
		return false;
	}

	bool bWasSuccessful = false;
	FHitResult HitResult = Ctx->ArcASC->GetHitResult(TargetingTag, bWasSuccessful);
	if (!bWasSuccessful)
	{
		bHasDirection = false;
		return false;
	}

	OutOrigin = HitResult.bBlockingHit ? HitResult.ImpactPoint : HitResult.TraceEnd;
	bHasDirection = false;
	return true;
}

bool FArcTraceOrigin_HitActorLocation::Resolve(const FTargetingRequestHandle& Handle,
                                               FVector& OutOrigin, FVector& OutDirection,
                                               bool& bHasDirection) const
{
	FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(Handle);
	if (Results.TargetResults.IsEmpty())
	{
		bHasDirection = false;
		return false;
	}

	const FHitResult& PrevHit = Results.TargetResults[0].HitResult;
	AActor* HitActor = PrevHit.GetActor();
	if (!HitActor)
	{
		bHasDirection = false;
		return false;
	}

	OutOrigin = HitActor->GetActorLocation();
	bHasDirection = false;
	return true;
}

bool FArcTraceOrigin_HitActorFloorLocation::Resolve(const FTargetingRequestHandle& Handle,
                                                     FVector& OutOrigin, FVector& OutDirection,
                                                     bool& bHasDirection) const
{
	FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(Handle);
	if (Results.TargetResults.IsEmpty())
	{
		bHasDirection = false;
		return false;
	}

	const FHitResult& PrevHit = Results.TargetResults[0].HitResult;
	AActor* HitActor = PrevHit.GetActor();
	if (!HitActor)
	{
		bHasDirection = false;
		return false;
	}

	if (HitActor->Implements<UArcFloorLocationInterface>())
	{
		OutOrigin = IArcFloorLocationInterface::Execute_GetFloorLocation(HitActor);
		bHasDirection = false;
		return true;
	}

	bHasDirection = false;
	return false;
}

bool FArcTraceOrigin_GlobalTargetingHitActorLocation::Resolve(const FTargetingRequestHandle& Handle,
                                                              FVector& OutOrigin, FVector& OutDirection,
                                                              bool& bHasDirection) const
{
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(Handle);
	if (!Ctx || !Ctx->ArcASC || !TargetingTag.IsValid())
	{
		bHasDirection = false;
		return false;
	}

	bool bWasSuccessful = false;
	FHitResult HitResult = Ctx->ArcASC->GetHitResult(TargetingTag, bWasSuccessful);
	if (!bWasSuccessful)
	{
		bHasDirection = false;
		return false;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		bHasDirection = false;
		return false;
	}

	OutOrigin = HitActor->GetActorLocation();
	bHasDirection = false;
	return true;
}

bool FArcTraceOrigin_GlobalTargetingHitActorFloorLocation::Resolve(const FTargetingRequestHandle& Handle,
                                                                    FVector& OutOrigin, FVector& OutDirection,
                                                                    bool& bHasDirection) const
{
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(Handle);
	if (!Ctx || !Ctx->ArcASC || !TargetingTag.IsValid())
	{
		bHasDirection = false;
		return false;
	}

	bool bWasSuccessful = false;
	FHitResult HitResult = Ctx->ArcASC->GetHitResult(TargetingTag, bWasSuccessful);
	if (!bWasSuccessful)
	{
		bHasDirection = false;
		return false;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		bHasDirection = false;
		return false;
	}

	if (HitActor->Implements<UArcFloorLocationInterface>())
	{
		OutOrigin = IArcFloorLocationInterface::Execute_GetFloorLocation(HitActor);
		bHasDirection = false;
		return true;
	}

	bHasDirection = false;
	return false;
}

bool FArcTraceOrigin_DirectionToPreviousResult::Resolve(const FTargetingRequestHandle& Handle,
                                                        FVector& OutOrigin, FVector& OutDirection,
                                                        bool& bHasDirection) const
{
	const FArcTraceOrigin* Inner = OriginProvider.GetPtr<FArcTraceOrigin>();
	if (!Inner)
	{
		bHasDirection = false;
		return false;
	}

	FVector InnerDirection;
	bool bInnerHasDir;
	if (!Inner->Resolve(Handle, OutOrigin, InnerDirection, bInnerHasDir))
	{
		bHasDirection = false;
		return false;
	}

	FTargetingDefaultResultsSet& Results = FTargetingDefaultResultsSet::FindOrAdd(Handle);
	if (Results.TargetResults.IsEmpty())
	{
		bHasDirection = false;
		return true;
	}

	const FHitResult& PrevHit = Results.TargetResults[0].HitResult;
	FVector Target = PrevHit.bBlockingHit ? PrevHit.ImpactPoint : PrevHit.TraceEnd;
	OutDirection = (Target - OutOrigin).GetSafeNormal();
	bHasDirection = !OutDirection.IsZero();
	return true;
}
