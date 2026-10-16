// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilApi.h"

#include "ArcRecoilModule.h"
#include "ArcRecoilStep.h"
#include "Config/ArcRecoilProfileBake.h"
#include "Fragments/ArcRecoilFragments.h"
#include "Mass/EntityManager.h"

namespace ArcRecoilApiPrivate
{
	int32 DeriveStreamSeed(const FMassEntityHandle Entity)
	{
		return static_cast<int32>(HashCombine(0x41524352u, static_cast<uint32>(Entity.Index)));
	}

	FArcRecoilStateFragment* FindState(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		return EntityManager.IsEntityActive(Entity)
			? EntityManager.GetFragmentDataPtr<FArcRecoilStateFragment>(Entity)
			: nullptr;
	}

	FArcRecoilProfileFragment* FindProfile(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		return EntityManager.IsEntityActive(Entity)
			? EntityManager.GetFragmentDataPtr<FArcRecoilProfileFragment>(Entity)
			: nullptr;
	}
}

void ArcRecoil::SeedStreamForEntity(FRandomStream& Stream, const FMassEntityHandle Entity)
{
	const int32 Seed = ArcRecoilApiPrivate::DeriveStreamSeed(Entity);
	if (Stream.GetInitialSeed() != Seed)
	{
		Stream.Initialize(Seed);
	}
}

void ArcRecoil::NotifyShotFired(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const int32 ShotCount)
{
	if (ShotCount <= 0)
	{
		return;
	}

	FArcRecoilStateFragment* State = ArcRecoilApiPrivate::FindState(EntityManager, Entity);
	FArcRecoilProfileFragment* Profile = ArcRecoilApiPrivate::FindProfile(EntityManager, Entity);
	if (State == nullptr || Profile == nullptr)
	{
		UE_LOGF(LogArcRecoil, Warning,"NotifyShotFired: entity %d:%d has no recoil fragments (State=%d Profile=%d) - shot ignored.",
			Entity.Index, Entity.SerialNumber, State != nullptr ? 1 : 0, Profile != nullptr ? 1 : 0);
		return;
	}

	SeedStreamForEntity(State->Stream, Entity);

	for (int32 ShotIt = 0; ShotIt < ShotCount; ++ShotIt)
	{
		const FArcRecoilShotImpulse Impulse = ApplyShot(*State, *Profile);

		UE_LOGF(LogArcRecoil, Verbose,"Shot: entity %d ShotIndexBefore=%.3f InSuppressionWindow=%ls Camera=(%.4f, %.4f) AimOffset=(%.4f, %.4f) -> Heat=%.3f Offset=(%.4f, %.4f)",
			Entity.Index, Impulse.ShotIndexBefore, Impulse.bSuppressed ? TEXT("yes") : TEXT("no"),
			Impulse.CameraKickDeg.X, Impulse.CameraKickDeg.Y, Impulse.AimOffsetKickDeg.X, Impulse.AimOffsetKickDeg.Y,
			State->Heat, State->AngularOffsetDeg.X, State->AngularOffsetDeg.Y);
	}
}

FVector2f ArcRecoil::ConsumePendingCameraKick(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
{
	FArcRecoilStateFragment* State = ArcRecoilApiPrivate::FindState(EntityManager, Entity);
	if (State == nullptr)
	{
		UE_LOGF(LogArcRecoil, Warning,"ConsumePendingCameraKick: entity %d:%d has no FArcRecoilStateFragment.",
			Entity.Index, Entity.SerialNumber);
		return FVector2f::ZeroVector;
	}

	const FVector2f Pending = State->PendingCameraKickDeg;
	State->PendingCameraKickDeg = FVector2f::ZeroVector;

	return Pending;
}

bool ArcRecoil::GetAimPerturbation(const FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	FVector2f& OutAngularOffsetDeg,
	float& OutSpreadHalfAngleDeg,
	float& OutSpreadExponent)
{
	const FArcRecoilStateFragment* State = ArcRecoilApiPrivate::FindState(EntityManager, Entity);
	const FArcRecoilProfileFragment* Profile = ArcRecoilApiPrivate::FindProfile(EntityManager, Entity);
	if (State == nullptr || Profile == nullptr)
	{
		UE_LOGF(LogArcRecoil, Warning,"GetAimPerturbation: entity %d:%d has no recoil fragments (State=%d Profile=%d).",
			Entity.Index, Entity.SerialNumber, State != nullptr ? 1 : 0, Profile != nullptr ? 1 : 0);

		OutAngularOffsetDeg = FVector2f::ZeroVector;
		OutSpreadHalfAngleDeg = 0.0f;
		OutSpreadExponent = 1.0f;
		return false;
	}

	OutAngularOffsetDeg = State->AngularOffsetDeg;
	OutSpreadHalfAngleDeg = GetSpreadHalfAngleDeg(*State, *Profile);
	OutSpreadExponent = Profile->SpreadExponent;

	return true;
}

bool ArcRecoil::ApplyProfile(FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const UArcRecoilPreset& Preset,
	const FArcRecoilStats& Stats)
{
	FArcRecoilProfileFragment* Profile = ArcRecoilApiPrivate::FindProfile(EntityManager, Entity);
	if (Profile == nullptr)
	{
		UE_LOGF(LogArcRecoil, Warning,"ApplyProfile: entity %d:%d has no FArcRecoilProfileFragment.",
			Entity.Index, Entity.SerialNumber);
		return false;
	}

	BakeProfile(Preset, ClampStats(Stats), *Profile);

	return true;
}
