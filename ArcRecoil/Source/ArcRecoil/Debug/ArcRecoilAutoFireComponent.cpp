// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcRecoilAutoFireComponent.h"

#include "ArcRecoilApi.h"
#include "ArcRecoilEntityResolve.h"
#include "ArcRecoilModule.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "Mass/EntityManager.h"

namespace ArcRecoilAutoFirePrivate
{
	constexpr float BindWarningDelay = 2.0f;
	constexpr float MaxCatchUpSeconds = 0.5f;
	constexpr float MaxDiscardReportSeconds = 10.0f;
}

UArcRecoilAutoFireComponent::UArcRecoilAutoFireComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UArcRecoilAutoFireComponent::BeginPlay()
{
	Super::BeginPlay();

	TryBindInput();
}

void UArcRecoilAutoFireComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInputBound)
	{
		UnboundTime += DeltaTime;
		TryBindInput();
	}

	if (!bTriggerHeld || !bAutoFire)
	{
		return;
	}

	const float SecondsPerShot = 60.0f / FMath::Max(RoundsPerMinute, 1.0f);
	const float CatchUpCapSeconds = FMath::Max(ArcRecoilAutoFirePrivate::MaxCatchUpSeconds, SecondsPerShot);
	const int32 MaxShots = FMath::Max(1, FMath::FloorToInt32(CatchUpCapSeconds / SecondsPerShot));

	ShotAccumulator = FMath::Min(ShotAccumulator + DeltaTime,
		CatchUpCapSeconds + ArcRecoilAutoFirePrivate::MaxDiscardReportSeconds);

	const int32 AvailableShots = FMath::FloorToInt32(ShotAccumulator / SecondsPerShot);
	const int32 ShotCount = FMath::Min(AvailableShots, MaxShots);
	ShotAccumulator -= static_cast<float>(ShotCount) * SecondsPerShot;

	const int32 DroppedShots = FMath::FloorToInt32(ShotAccumulator / SecondsPerShot);
	if (DroppedShots > 0)
	{
		ShotAccumulator -= static_cast<float>(DroppedShots) * SecondsPerShot;

		UE_LOGF(LogArcRecoil, Verbose,"AutoFire: '%ls' discarded %d shot(s) of catch-up fire after a %.3fs frame; firing %d.",
			*GetNameSafe(GetOwner()), DroppedShots, DeltaTime, ShotCount);
	}

	if (ShotCount > 0)
	{
		FireShots(ShotCount);
	}
}

bool UArcRecoilAutoFireComponent::TryBindInput()
{
	if (bInputBound)
	{
		return true;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UEnhancedInputComponent* EnhancedInput = OwnerPawn != nullptr
		? Cast<UEnhancedInputComponent>(OwnerPawn->InputComponent)
		: nullptr;

	const TCHAR* FailureCause = nullptr;
	if (FireAction == nullptr)
	{
		FailureCause = TEXT("FireAction is not set on the component");
	}
	else if (OwnerPawn == nullptr)
	{
		FailureCause = TEXT("owner is not an APawn");
	}
	else if (EnhancedInput == nullptr)
	{
		FailureCause = TEXT("pawn has no UEnhancedInputComponent (unpossessed, or a legacy input component)");
	}

	if (FailureCause != nullptr)
	{
		if (!bWarnedInputUnbound && UnboundTime > ArcRecoilAutoFirePrivate::BindWarningDelay)
		{
			bWarnedInputUnbound = true;
			UE_LOGF(LogArcRecoil, Warning,"AutoFire: '%ls' has no input binding after %.1fs - %ls. Holding fire will do nothing.",
				*GetNameSafe(GetOwner()), UnboundTime, FailureCause);
		}
		return false;
	}

	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &UArcRecoilAutoFireComponent::OnFireStarted);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &UArcRecoilAutoFireComponent::OnFireStopped);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Canceled, this, &UArcRecoilAutoFireComponent::OnFireStopped);

	bInputBound = true;

	UE_LOGF(LogArcRecoil, Log,"AutoFire: bound '%ls' on '%ls' (RoundsPerMinute=%.0f, AutoFire=%ls).",
		*GetNameSafe(FireAction), *GetNameSafe(OwnerPawn), RoundsPerMinute, bAutoFire ? TEXT("on") : TEXT("off"));

	return true;
}

void UArcRecoilAutoFireComponent::OnFireStarted()
{
	bTriggerHeld = true;
	ShotAccumulator = 0.0f;

	FireShots(1);
}

void UArcRecoilAutoFireComponent::OnFireStopped()
{
	bTriggerHeld = false;
	ShotAccumulator = 0.0f;
}

void UArcRecoilAutoFireComponent::FireShots(const int32 ShotCount)
{
	ArcRecoil::FResolvedRecoilEntity Resolved;
	const ArcRecoil::EEntityResolveResult Result = ArcRecoil::ResolveRecoilEntity(GetOwner(), Resolved);
	if (Result != ArcRecoil::EEntityResolveResult::Resolved)
	{
		if (!bWarnedUnresolvedEntity)
		{
			bWarnedUnresolvedEntity = true;
			UE_LOGF(LogArcRecoil, Warning,"AutoFire: '%ls' has no recoil entity - %ls. Shots ignored.",
				*GetNameSafe(GetOwner()), ArcRecoil::DescribeResolveResult(Result));
		}
		return;
	}

	bWarnedUnresolvedEntity = false;

	ArcRecoil::NotifyShotFired(*Resolved.EntityManager, Resolved.Entity, ShotCount);

	UE_LOGF(LogArcRecoil, Verbose,"AutoFire: entity %d fired %d shot(s).", Resolved.Entity.Index, ShotCount);
}
