// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcRecoilPreviewSim.h"

#include "ArcRecoilDebt.h"
#include "ArcRecoilModule.h"
#include "ArcRecoilStep.h"
#include "Config/ArcRecoilPreset.h"
#include "Config/ArcRecoilProfileBake.h"
#include "Preview/ArcRecoilPreviewSettings.h"

namespace ArcRecoilPreviewSimPrivate
{
	constexpr int32 MaxSubstepsPerShot = 1024;
	constexpr int32 MaxTailSubsteps = 65536;
	constexpr int32 MaxSamplesPerRun = 1000000;
	constexpr int32 MaxSamplesTotal = 2000000;

	int32 CeilToClampedInt(const double Value, const int32 Min, const int32 Max, const TCHAR* Name)
	{
		const double Requested = FMath::CeilToDouble(Value);
		const double Effective = FMath::Clamp(Requested, static_cast<double>(Min), static_cast<double>(Max));

		if (Effective != Requested)
		{
			UE_LOGF(LogArcRecoil, Warning,"Recoil preview: requested %ls %.0f is outside [%d, %d]; simulating %.0f instead.",
				Name, Requested, Min, Max, Effective);
		}

		return static_cast<int32>(Effective);
	}

	int32 ClampWithWarning(const int32 Requested, const int32 Min, const int32 Max, const TCHAR* Name)
	{
		const int32 Effective = FMath::Clamp(Requested, Min, Max);

		if (Effective != Requested)
		{
			UE_LOGF(LogArcRecoil, Warning,"Recoil preview: requested %ls %d is outside [%d, %d]; simulating %d instead.",
				Name, Requested, Min, Max, Effective);
		}

		return Effective;
	}

	bool IsPositiveFinite(const float Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0f;
	}

	void Substep(FArcRecoilStateFragment& State,
		const FArcRecoilProfileFragment& Profile,
		FVector2f& Debt,
		float& TimeSinceKick,
		const float Dt,
		const bool bCarriesShot)
	{
		TimeSinceKick += Dt;

		if (TimeSinceKick > Profile.RecoveryDelay)
		{
			ArcRecoil::StepDebtRecovery(Debt, Profile.RecoveryRate, Dt);
		}

		if (bCarriesShot)
		{
			const ArcRecoil::FArcRecoilShotImpulse Impulse = ArcRecoil::ApplyShot(State, Profile);

			if (!Impulse.CameraKickDeg.IsNearlyZero())
			{
				Debt += Impulse.CameraKickDeg;
				TimeSinceKick = 0.0f;
			}

			// The camera node consumes this through ConsumePendingCameraKick; the preview consumes it into Debt instead.
			State.PendingCameraKickDeg = FVector2f::ZeroVector;
		}

		ArcRecoil::AdvanceState(State, Profile, Dt);
	}

	float MaxAbsComponent(const FVector2f& Value)
	{
		return FMath::Max(FMath::Abs(Value.X), FMath::Abs(Value.Y));
	}
}

namespace ArcRecoil::Preview
{
	FArcRecoilPreviewResult Simulate(const UArcRecoilPreset& Preset,
		const UArcRecoilPreviewSettings& Settings,
		const FArcRecoilPreviewQuality& Quality)
	{
		FArcRecoilPreviewResult Result;
		ArcRecoil::BakeProfile(Preset, ArcRecoil::ClampStats(Settings.Stats), Result.BakedProfile);

		if (Settings.MaxShots <= 0 || Settings.SeedCount <= 0
			|| !ArcRecoilPreviewSimPrivate::IsPositiveFinite(Settings.RoundsPerMinute)
			|| !ArcRecoilPreviewSimPrivate::IsPositiveFinite(Settings.SimulationHz))
		{
			UE_LOGF(LogArcRecoil, Warning,"Recoil preview: nothing to simulate with MaxShots=%d SeedCount=%d RoundsPerMinute=%f SimulationHz=%f.",
				Settings.MaxShots, Settings.SeedCount, Settings.RoundsPerMinute, Settings.SimulationHz);
			return Result;
		}

		const float SecondsPerShot = 60.0f / Settings.RoundsPerMinute;
		const int32 Steps = ArcRecoilPreviewSimPrivate::CeilToClampedInt(
			static_cast<double>(SecondsPerShot) * Settings.SimulationHz, 1, ArcRecoilPreviewSimPrivate::MaxSubstepsPerShot,
			TEXT("substeps per shot"));
		const float Dt = SecondsPerShot / static_cast<float>(Steps);
		if (!ArcRecoilPreviewSimPrivate::IsPositiveFinite(Dt))
		{
			UE_LOGF(LogArcRecoil, Warning,"Recoil preview: RoundsPerMinute=%f over %d substeps yields a substep of %f seconds.",
				Settings.RoundsPerMinute, Steps, Dt);
			return Result;
		}

		const float TailSeconds = FMath::IsFinite(Settings.TailSeconds) ? FMath::Max(Settings.TailSeconds, 0.0f) : 0.0f;
		const int32 TailSteps = ArcRecoilPreviewSimPrivate::CeilToClampedInt(
			static_cast<double>(TailSeconds) / Dt, 0, ArcRecoilPreviewSimPrivate::MaxTailSubsteps,
			TEXT("tail substeps"));

		const int32 ShotCount = ArcRecoilPreviewSimPrivate::ClampWithWarning(Settings.MaxShots, 1,
			FMath::Max((ArcRecoilPreviewSimPrivate::MaxSamplesPerRun - TailSteps) / Steps, 1), TEXT("shots"));
		const int32 SampleCount = ShotCount * Steps + TailSteps;
		const int32 RequestedRunCount = Quality.MaxSeedCount > 0
			? FMath::Min(Settings.SeedCount, Quality.MaxSeedCount)
			: Settings.SeedCount;
		const int32 SeedRunBudget = FMath::Max(ArcRecoilPreviewSimPrivate::MaxSamplesTotal / SampleCount, 1);
		const int32 RunCount = ArcRecoilPreviewSimPrivate::ClampWithWarning(RequestedRunCount, 1,
			SeedRunBudget, TEXT("seed runs"));
		const int32 FullRunCount = FMath::Clamp(Settings.SeedCount, 1, SeedRunBudget);
		const int32 MaxSpreadSamples = FMath::Max(Quality.MaxSpreadSamples, 0);

		const FArcRecoilProfileFragment& Profile = Result.BakedProfile;

		Result.SampleCount = SampleCount;
		Result.StepsPerShot = Steps;
		Result.ShotCount = ShotCount;
		Result.RunCount = RunCount;
		Result.FullRunCount = FullRunCount;
		Result.MaxSpreadSamples = MaxSpreadSamples;
		Result.bSeedCapBound = RunCount < FullRunCount;
		Result.bSpreadCapBound = MaxSpreadSamples > 0 && MaxSpreadSamples < Settings.SpreadSamplesPerShot;
		Result.bDraft = Result.bSeedCapBound || Result.bSpreadCapBound;
		Result.SecondsPerSample = Dt;
		Result.Runs.SetNum(RunCount);
		Result.MeanCameraDeg.SetNumZeroed(SampleCount);
		Result.MeanAimOffsetDeg.SetNumZeroed(SampleCount);
		Result.SpreadHalfAngleDeg.Reserve(SampleCount);
		Result.ShotSampleIndices.Reserve(ShotCount);

		for (int32 RunIndex = 0; RunIndex < RunCount; ++RunIndex)
		{
			FArcRecoilPreviewRun& Run = Result.Runs[RunIndex];
			Run.CameraDeg.Reserve(SampleCount);
			Run.AimOffsetDeg.Reserve(SampleCount);

			FArcRecoilStateFragment State;
			State.Stream.Initialize(Settings.BaseSeed + RunIndex);

			// Under zero player compensation the camera node's view angle is its debt, so this local mirrors that state.
			FVector2f Debt = FVector2f::ZeroVector;
			float TimeSinceKick = 0.0f;

			for (int32 ShotIndex = 0; ShotIndex < ShotCount; ++ShotIndex)
			{
				if (RunIndex == 0)
				{
					Result.ShotSampleIndices.Add(Run.CameraDeg.Num());
				}

				for (int32 StepIndex = 0; StepIndex < Steps; ++StepIndex)
				{
					ArcRecoilPreviewSimPrivate::Substep(State, Profile, Debt, TimeSinceKick, Dt, StepIndex == 0);

					Run.CameraDeg.Add(Debt);
					Run.AimOffsetDeg.Add(State.AngularOffsetDeg);

					if (RunIndex == 0)
					{
						Result.SpreadHalfAngleDeg.Add(ArcRecoil::GetSpreadHalfAngleDeg(State, Profile));
					}
				}
			}

			for (int32 TailIndex = 0; TailIndex < TailSteps; ++TailIndex)
			{
				ArcRecoilPreviewSimPrivate::Substep(State, Profile, Debt, TimeSinceKick, Dt, false);

				Run.CameraDeg.Add(Debt);
				Run.AimOffsetDeg.Add(State.AngularOffsetDeg);

				if (RunIndex == 0)
				{
					Result.SpreadHalfAngleDeg.Add(ArcRecoil::GetSpreadHalfAngleDeg(State, Profile));
				}
			}

			for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				Result.MeanCameraDeg[SampleIndex] += Run.CameraDeg[SampleIndex];
				Result.MeanAimOffsetDeg[SampleIndex] += Run.AimOffsetDeg[SampleIndex];
			}
		}

		const float InvRunCount = 1.0f / static_cast<float>(RunCount);
		float CameraMaxAbsDeg = 0.0f;
		float ImpactMaxAbsDeg = 0.0f;

		for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
		{
			Result.MeanCameraDeg[SampleIndex] *= InvRunCount;
			Result.MeanAimOffsetDeg[SampleIndex] *= InvRunCount;
		}

		for (const FArcRecoilPreviewRun& Run : Result.Runs)
		{
			for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				CameraMaxAbsDeg = FMath::Max(
					CameraMaxAbsDeg, ArcRecoilPreviewSimPrivate::MaxAbsComponent(Run.CameraDeg[SampleIndex]));
				ImpactMaxAbsDeg = FMath::Max(ImpactMaxAbsDeg,
					ArcRecoilPreviewSimPrivate::MaxAbsComponent(Run.AimOffsetDeg[SampleIndex])
						+ Result.SpreadHalfAngleDeg[SampleIndex]);
			}
		}

		Result.CameraMaxAbsDeg = FMath::Max(CameraMaxAbsDeg, UE_KINDA_SMALL_NUMBER);
		Result.ImpactMaxAbsDeg = FMath::Max(ImpactMaxAbsDeg, UE_KINDA_SMALL_NUMBER);

		return Result;
	}
}
