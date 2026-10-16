// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcRecoilTestLibrary.h"

#include "ArcRecoilApi.h"
#include "ArcRecoilEntityResolve.h"
#include "ArcRecoilImpulse.h"
#include "ArcRecoilModule.h"
#include "Config/ArcRecoilPreset.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mass/EntityManager.h"

namespace ArcRecoilTestLibraryPrivate
{
	void LogUnresolved(const AActor* Shooter, const ArcRecoil::EEntityResolveResult Result, const TCHAR* Context)
	{
		UE_LOGF(LogArcRecoil, Warning,"%ls: '%ls' has no recoil entity - %ls.",
			Context, *GetNameSafe(Shooter), ArcRecoil::DescribeResolveResult(Result));
	}

	void TestFireConsoleCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		const APlayerController* PlayerController = World->GetFirstPlayerController();
		APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			UE_LOGF(LogArcRecoil, Warning,"arc.Recoil.TestFire: no local player pawn in world '%ls'.", *World->GetName());
			return;
		}

		int32 ShotCount = 1;
		if (Args.Num() > 0)
		{
			ShotCount = FCString::Atoi(*Args[0]);
		}

		UArcRecoilTestLibrary::ArcRecoilTestFire(Pawn, ShotCount);
	}

	FAutoConsoleCommandWithWorldAndArgs GTestFireCommand(
		TEXT("arc.Recoil.TestFire"),
		TEXT("arc.Recoil.TestFire <N> - fires N recoil shots on the local player pawn's Mass entity."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestFireConsoleCommand));
}

void UArcRecoilTestLibrary::ArcRecoilTestFire(AActor* Shooter, const int32 ShotCount)
{
	ArcRecoil::FResolvedRecoilEntity Resolved;
	const ArcRecoil::EEntityResolveResult Result = ArcRecoil::ResolveRecoilEntity(Shooter, Resolved);
	if (Result != ArcRecoil::EEntityResolveResult::Resolved)
	{
		ArcRecoilTestLibraryPrivate::LogUnresolved(Shooter, Result, TEXT("ArcRecoilTestFire"));
		return;
	}

	ArcRecoil::NotifyShotFired(*Resolved.EntityManager, Resolved.Entity, ShotCount);

	UE_LOGF(LogArcRecoil, Log,"ArcRecoilTestFire: '%ls' entity %d fired %d shot(s).",
		*GetNameSafe(Shooter), Resolved.Entity.Index, ShotCount);
}

bool UArcRecoilTestLibrary::ArcRecoilGetState(AActor* Shooter,
	FVector2D& OutAngularOffsetDeg,
	float& OutSpreadHalfAngleDeg,
	float& OutHeat,
	float& OutShotIndex)
{
	OutAngularOffsetDeg = FVector2D::ZeroVector;
	OutSpreadHalfAngleDeg = 0.0f;
	OutHeat = 0.0f;
	OutShotIndex = 0.0f;

	ArcRecoil::FResolvedRecoilEntity Resolved;
	const ArcRecoil::EEntityResolveResult Result = ArcRecoil::ResolveRecoilEntity(Shooter, Resolved);
	if (Result != ArcRecoil::EEntityResolveResult::Resolved)
	{
		ArcRecoilTestLibraryPrivate::LogUnresolved(Shooter, Result, TEXT("ArcRecoilGetState"));
		return false;
	}

	FVector2f AngularOffsetDeg = FVector2f::ZeroVector;
	float SpreadExponent = 1.0f;
	if (!ArcRecoil::GetAimPerturbation(*Resolved.EntityManager, Resolved.Entity, AngularOffsetDeg, OutSpreadHalfAngleDeg, SpreadExponent))
	{
		return false;
	}

	const FArcRecoilStateFragment* State = Resolved.EntityManager->GetFragmentDataPtr<FArcRecoilStateFragment>(Resolved.Entity);
	if (State == nullptr)
	{
		return false;
	}

	OutAngularOffsetDeg = FVector2D(AngularOffsetDeg.X, AngularOffsetDeg.Y);
	OutHeat = State->Heat;
	OutShotIndex = State->ShotIndex;

	UE_LOGF(LogArcRecoil, Log,"ArcRecoilGetState: '%ls' entity %d Offset=(%.4f, %.4f) SpreadHalf=%.4f Heat=%.3f ShotIndex=%.3f",
		*GetNameSafe(Shooter), Resolved.Entity.Index,
		OutAngularOffsetDeg.X, OutAngularOffsetDeg.Y, OutSpreadHalfAngleDeg, OutHeat, OutShotIndex);

	return true;
}

bool UArcRecoilTestLibrary::ArcRecoilApplyProfile(AActor* Shooter, UArcRecoilPreset* Preset, FArcRecoilStats Stats)
{
	if (Preset == nullptr)
	{
		UE_LOGF(LogArcRecoil, Warning,"ArcRecoilApplyProfile: '%ls' called with a null preset.", *GetNameSafe(Shooter));
		return false;
	}

	ArcRecoil::FResolvedRecoilEntity Resolved;
	const ArcRecoil::EEntityResolveResult Result = ArcRecoil::ResolveRecoilEntity(Shooter, Resolved);
	if (Result != ArcRecoil::EEntityResolveResult::Resolved)
	{
		ArcRecoilTestLibraryPrivate::LogUnresolved(Shooter, Result, TEXT("ArcRecoilApplyProfile"));
		return false;
	}

	if (!ArcRecoil::ApplyProfile(*Resolved.EntityManager, Resolved.Entity, *Preset, Stats))
	{
		return false;
	}

	UE_LOGF(LogArcRecoil, Log,"ArcRecoilApplyProfile: '%ls' entity %d preset '%ls' Stability=%.1f Control=%.1f Accuracy=%.1f Handling=%.1f",
		*GetNameSafe(Shooter), Resolved.Entity.Index, *GetNameSafe(Preset),
		Stats.Stability, Stats.Control, Stats.Accuracy, Stats.Handling);

	return true;
}

bool UArcRecoilTestLibrary::ArcRecoilPerturbDirection(AActor* Shooter, FVector AimDirection, FVector& OutDirection)
{
	OutDirection = AimDirection;

	if (AimDirection.IsNearlyZero())
	{
		UE_LOGF(LogArcRecoil, Warning,"ArcRecoilPerturbDirection: '%ls' called with a zero aim direction.", *GetNameSafe(Shooter));
		return false;
	}

	ArcRecoil::FResolvedRecoilEntity Resolved;
	const ArcRecoil::EEntityResolveResult Result = ArcRecoil::ResolveRecoilEntity(Shooter, Resolved);
	if (Result != ArcRecoil::EEntityResolveResult::Resolved)
	{
		ArcRecoilTestLibraryPrivate::LogUnresolved(Shooter, Result, TEXT("ArcRecoilPerturbDirection"));
		return false;
	}

	FVector2f AngularOffsetDeg = FVector2f::ZeroVector;
	float SpreadHalfAngleDeg = 0.0f;
	float SpreadExponent = 1.0f;
	if (!ArcRecoil::GetAimPerturbation(*Resolved.EntityManager, Resolved.Entity, AngularOffsetDeg, SpreadHalfAngleDeg, SpreadExponent))
	{
		return false;
	}

	FArcRecoilStateFragment* State = Resolved.EntityManager->GetFragmentDataPtr<FArcRecoilStateFragment>(Resolved.Entity);
	if (State == nullptr)
	{
		return false;
	}

	ArcRecoil::SeedStreamForEntity(State->Stream, Resolved.Entity);

	OutDirection = ArcRecoil::PerturbAimDirection(AimDirection.GetSafeNormal(), AngularOffsetDeg, SpreadHalfAngleDeg, SpreadExponent, State->Stream);

	UE_LOGF(LogArcRecoil, Log,"ArcRecoilPerturbDirection: '%ls' entity %d Offset=(%.4f, %.4f) SpreadHalf=%.4f %ls -> %ls",
		*GetNameSafe(Shooter), Resolved.Entity.Index,
		AngularOffsetDeg.X, AngularOffsetDeg.Y, SpreadHalfAngleDeg,
		*AimDirection.GetSafeNormal().ToString(), *OutDirection.ToString());

	return true;
}
