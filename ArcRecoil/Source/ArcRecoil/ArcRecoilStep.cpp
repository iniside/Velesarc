// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilStep.h"

#include "ArcRecoilImpulse.h"
#include "Fragments/ArcRecoilFragments.h"

namespace ArcRecoilStepPrivate
{
	struct FSpringStep
	{
		float PosFromPos = 1.0f;
		float PosFromVel = 0.0f;
		float VelFromPos = 0.0f;
		float VelFromVel = 1.0f;
	};

	FSpringStep BuildSpringStep(const float Stiffness, const float DampingCoeff, const float DeltaTime)
	{
		FSpringStep Step;
		if (DeltaTime <= 0.0f)
		{
			return Step;
		}

		const float Alpha = 0.5f * DampingCoeff;
		const float Discriminant = Alpha * Alpha - Stiffness;
		const float Tolerance = UE_KINDA_SMALL_NUMBER * FMath::Max(Stiffness, 1.0f);

		float CosLike = 0.0f;
		float SinLike = 0.0f;

		if (Discriminant < -Tolerance)
		{
			const float Omega = FMath::Sqrt(-Discriminant);
			const float Decay = FMath::Exp(-Alpha * DeltaTime);
			CosLike = Decay * FMath::Cos(Omega * DeltaTime);
			SinLike = Decay * FMath::Sin(Omega * DeltaTime) / Omega;
		}
		else if (Discriminant > Tolerance)
		{
			// Per-root exponentials rather than cosh/sinh: both roots are <= 0 for baked non-negative coefficients, so neither term can overflow.
			const float Beta = FMath::Sqrt(Discriminant);
			const float SlowMode = FMath::Exp((Beta - Alpha) * DeltaTime);
			const float FastMode = FMath::Exp((-Beta - Alpha) * DeltaTime);
			CosLike = 0.5f * (SlowMode + FastMode);
			SinLike = 0.5f * (SlowMode - FastMode) / Beta;
		}
		else
		{
			const float Decay = FMath::Exp(-Alpha * DeltaTime);
			CosLike = Decay;
			SinLike = Decay * DeltaTime;
		}

		Step.PosFromPos = CosLike + Alpha * SinLike;
		Step.PosFromVel = SinLike;
		Step.VelFromPos = -Stiffness * SinLike;
		Step.VelFromVel = CosLike - Alpha * SinLike;

		return Step;
	}

	bool ClampToOffsetEllipse(FVector2f& Offset, FVector2f& Velocity, const FVector2f& MaxOffsetDeg)
	{
		if (!(MaxOffsetDeg.X > UE_KINDA_SMALL_NUMBER) || !(MaxOffsetDeg.Y > UE_KINDA_SMALL_NUMBER))
		{
			return false;
		}

		const float NormalizedX = Offset.X / MaxOffsetDeg.X;
		const float NormalizedY = Offset.Y / MaxOffsetDeg.Y;
		const float RadiusSquared = NormalizedX * NormalizedX + NormalizedY * NormalizedY;
		if (RadiusSquared <= 1.0f)
		{
			return false;
		}

		Offset /= FMath::Sqrt(RadiusSquared);

		// Ellipse normal, not the radial direction — they differ wherever MaxX != MaxY.
		const FVector2f Normal = FVector2f(
			Offset.X / (MaxOffsetDeg.X * MaxOffsetDeg.X),
			Offset.Y / (MaxOffsetDeg.Y * MaxOffsetDeg.Y)).GetSafeNormal();

		const float OutwardSpeed = FVector2f::DotProduct(Velocity, Normal);
		if (OutwardSpeed > 0.0f)
		{
			Velocity -= Normal * OutwardSpeed;
		}

		return true;
	}
}

namespace ArcRecoil
{
	FArcRecoilStats ClampStats(const FArcRecoilStats& Stats)
	{
		FArcRecoilStats Clamped;
		Clamped.Stability = FMath::Clamp(Stats.Stability, 0.0f, 100.0f);
		Clamped.Control = FMath::Clamp(Stats.Control, 0.0f, 100.0f);
		Clamped.Accuracy = FMath::Clamp(Stats.Accuracy, 0.0f, 100.0f);
		Clamped.Handling = FMath::Clamp(Stats.Handling, 0.0f, 100.0f);

		return Clamped;
	}

	FArcRecoilShotImpulse ApplyShot(FArcRecoilStateFragment& State, const FArcRecoilProfileFragment& Profile)
	{
		const float ShotIndexBefore = State.ShotIndex;
		const float WalkShotsBefore = State.WalkShots;

		FArcRecoilShotImpulse Impulse;
		Impulse.ShotIndexBefore = ShotIndexBefore;
		Impulse.bSuppressed = ShotIndexBefore < static_cast<float>(Profile.SuppressionShots);
		Impulse.CameraKickDeg = ComputeShotImpulse(Profile.CameraKick, Profile.SuppressionShots, ShotIndexBefore, WalkShotsBefore, State.Stream);
		Impulse.AimOffsetKickDeg = ComputeShotImpulse(Profile.AimOffsetKick, Profile.SuppressionShots, ShotIndexBefore, WalkShotsBefore, State.Stream);

		State.AngularOffsetDeg += Impulse.AimOffsetKickDeg;
		State.OffsetVelDegSec += Impulse.AimOffsetKickDeg * Profile.ImpulseToVelGain;
		State.Heat = FMath::Min(State.Heat + Profile.HeatPerShot, 1.0f);
		State.ShotIndex += 1.0f;
		State.WalkShots = Profile.WalkPeriodShots > 0.0f
			? FMath::Fmod(State.WalkShots + 1.0f, Profile.WalkPeriodShots)
			: State.WalkShots + 1.0f;
		State.PendingCameraKickDeg += Impulse.CameraKickDeg;
		State.TimeSinceShot = 0.0f;

		return Impulse;
	}

	FArcRecoilStepReport AdvanceState(FArcRecoilStateFragment& State, const FArcRecoilProfileFragment& Profile, const float DeltaTime)
	{
		FArcRecoilStepReport Report;

		State.TimeSinceShot += DeltaTime;

		if (State.TimeSinceShot > Profile.CooldownDelay)
		{
			State.Heat = FMath::Max(State.Heat - Profile.CoolRate * DeltaTime, 0.0f);
			State.ShotIndex = FMath::Max(State.ShotIndex - Profile.ShotIndexDecay * DeltaTime, 0.0f);
		}

		if (State.TimeSinceShot > Profile.WalkHoldSeconds)
		{
			State.WalkShots = FMath::Max(State.WalkShots - Profile.WalkShotsDecay * DeltaTime, 0.0f);
		}

		const ArcRecoilStepPrivate::FSpringStep SpringStep =
			ArcRecoilStepPrivate::BuildSpringStep(Profile.Stiffness, Profile.DampingCoeff, DeltaTime);

		const FVector2f Offset = State.AngularOffsetDeg;
		const FVector2f Velocity = State.OffsetVelDegSec;
		State.AngularOffsetDeg = Offset * SpringStep.PosFromPos + Velocity * SpringStep.PosFromVel;
		State.OffsetVelDegSec = Offset * SpringStep.VelFromPos + Velocity * SpringStep.VelFromVel;

		const bool bFinite =
			FMath::IsFinite(State.AngularOffsetDeg.X) && FMath::IsFinite(State.AngularOffsetDeg.Y) &&
			FMath::IsFinite(State.OffsetVelDegSec.X) && FMath::IsFinite(State.OffsetVelDegSec.Y);

		if (!bFinite)
		{
			Report.bDiverged = true;
			Report.PreZeroOffsetDeg = State.AngularOffsetDeg;
			Report.PreZeroVelDegSec = State.OffsetVelDegSec;

			State.AngularOffsetDeg = FVector2f::ZeroVector;
			State.OffsetVelDegSec = FVector2f::ZeroVector;
		}
		else
		{
			Report.bClamped = ArcRecoilStepPrivate::ClampToOffsetEllipse(
				State.AngularOffsetDeg, State.OffsetVelDegSec, Profile.MaxOffsetDeg);
		}

		return Report;
	}

	float GetSpreadHalfAngleDeg(const FArcRecoilStateFragment& State, const FArcRecoilProfileFragment& Profile)
	{
		const float SpreadFullDeg = Profile.BaseSpreadFullDeg
			+ (Profile.MaxSpreadFullDeg - Profile.BaseSpreadFullDeg) * FMath::Pow(State.Heat, Profile.SpreadCurveExp);

		return 0.5f * SpreadFullDeg;
	}
}
