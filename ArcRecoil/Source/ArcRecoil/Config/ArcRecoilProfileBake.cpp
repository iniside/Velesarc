// Copyright Lukasz Baran. All Rights Reserved.

#include "Config/ArcRecoilProfileBake.h"

#include "ArcRecoilModule.h"
#include "Config/ArcRecoilPreset.h"
#include "Fragments/ArcRecoilFragments.h"

namespace ArcRecoilProfileBakeInternal
{
	float PhaseFactor(const float Phase)
	{
		return -FMath::Cos(UE_PI * (Phase - 50.0f) / 10.0f);
	}

	void BakeLayer(const FArcRecoilLayerKick& Layer, const FArcRecoilStats& Stats, FArcRecoilBakedLayerKick& Out)
	{
		Out.KickPerShotDeg = Layer.KickDeg * (1.0f - Stats.Stability / 100.0f);
		Out.YawBiasSigned = PhaseFactor(Layer.RecoilPhase) * (1.0f - Layer.RecoilPhase / 100.0f);
		Out.YawNoiseFrac = Layer.YawNoise * (1.0f - Stats.Control / 100.0f);
		Out.FirstShotMul = Layer.FirstShotMul;
		Out.WalkRadPerShot = Layer.WalkPeriodShots > 0.0f ? UE_TWO_PI / Layer.WalkPeriodShots : 0.0f;
		Out.WalkAmplitude = Layer.WalkAmplitude;
		Out.WalkPhaseOffsetRad = FMath::DegreesToRadians(Layer.WalkPhaseOffsetDeg);
	}

	constexpr float WalkHoldFraction = 0.5f;

	void BakeWalkUnwind(const UArcRecoilPreset& Preset, FArcRecoilProfileFragment& Out)
	{
		const float LongestPeriodShots = FMath::Max(Preset.CameraKick.WalkPeriodShots, Preset.AimOffsetKick.WalkPeriodShots);
		Out.WalkPeriodShots = LongestPeriodShots;

		if (LongestPeriodShots <= 0.0f || Preset.WalkResetSeconds <= 0.0f)
		{
			Out.WalkHoldSeconds = 0.0f;
			Out.WalkShotsDecay = 0.0f;
			return;
		}

		Out.WalkHoldSeconds = Preset.WalkResetSeconds * WalkHoldFraction;
		Out.WalkShotsDecay = LongestPeriodShots / (Preset.WalkResetSeconds - Out.WalkHoldSeconds);
	}
}

void ArcRecoil::BakeProfile(const UArcRecoilPreset& Preset, const FArcRecoilStats& Stats, FArcRecoilProfileFragment& Out)
{
	ArcRecoilProfileBakeInternal::BakeLayer(Preset.CameraKick, Stats, Out.CameraKick);
	ArcRecoilProfileBakeInternal::BakeLayer(Preset.AimOffsetKick, Stats, Out.AimOffsetKick);

	Out.SuppressionShots = Preset.SuppressionShots;
	Out.ImpulseToVelGain = Preset.ImpulseToVelGain;

	Out.HeatPerShot = Preset.HeatPerShotBase * (2.0f - Stats.Accuracy / 100.0f);
	Out.CoolRate = Preset.CoolRateBase * (0.5f + Stats.Handling / 100.0f);
	Out.CooldownDelay = Preset.CooldownDelayBase * (1.5f - Stats.Handling / 200.0f);
	Out.ShotIndexDecay = Preset.ShotIndexDecayBase * (0.5f + Stats.Handling / 100.0f);
	ArcRecoilProfileBakeInternal::BakeWalkUnwind(Preset, Out);

	Out.BaseSpreadFullDeg = (100.0f - Stats.Accuracy) / 12.0f;
	Out.MaxSpreadFullDeg = Out.BaseSpreadFullDeg + Preset.SpreadRangeFullDeg * (1.0f - Stats.Accuracy / 100.0f);
	Out.SpreadCurveExp = Preset.SpreadCurveExp;
	Out.SpreadExponent = Preset.SpreadExponent;

	Out.Stiffness = Preset.StiffnessBase * (0.5f + Stats.Control / 100.0f);
	Out.DampingCoeff = Preset.DampingRatio * 2.0f * FMath::Sqrt(Out.Stiffness);
	Out.MaxOffsetDeg = Preset.MaxOffsetBaseDeg * (1.0f - 0.5f * Stats.Stability / 100.0f);

	Out.RecoveryDelay = Preset.RecoveryDelayBase * (1.5f - Stats.Handling / 200.0f);
	Out.RecoveryRate = Preset.RecoveryRateBase * (0.5f + Stats.Handling / 100.0f);

	UE_LOGF(LogArcRecoil, Verbose,"BakeProfile '%ls' Stability=%.1f Control=%.1f Accuracy=%.1f Handling=%.1f -> CameraKick[KickPerShotDeg=(%.4f,%.4f) YawBiasSigned=%.4f YawNoiseFrac=%.4f FirstShotMul=%.3f WalkRadPerShot=%.4f WalkAmplitude=(%.3f,%.3f) WalkPhaseOffsetRad=%.4f] AimOffsetKick[KickPerShotDeg=(%.4f,%.4f) YawBiasSigned=%.4f YawNoiseFrac=%.4f FirstShotMul=%.3f WalkRadPerShot=%.4f WalkAmplitude=(%.3f,%.3f) WalkPhaseOffsetRad=%.4f] SuppressionShots=%d ImpulseToVelGain=%.3f HeatPerShot=%.4f CoolRate=%.3f CooldownDelay=%.3f ShotIndexDecay=%.3f WalkHoldSeconds=%.3f WalkShotsDecay=%.3f WalkPeriodShots=%.3f BaseSpreadFullDeg=%.4f MaxSpreadFullDeg=%.4f SpreadCurveExp=%.3f SpreadExponent=%.3f Stiffness=%.3f DampingCoeff=%.3f MaxOffsetDeg=(%.3f,%.3f) RecoveryDelay=%.3f RecoveryRate=%.3f",
		*Preset.GetName(), Stats.Stability, Stats.Control, Stats.Accuracy, Stats.Handling,
		Out.CameraKick.KickPerShotDeg.X, Out.CameraKick.KickPerShotDeg.Y,
		Out.CameraKick.YawBiasSigned, Out.CameraKick.YawNoiseFrac, Out.CameraKick.FirstShotMul,
		Out.CameraKick.WalkRadPerShot, Out.CameraKick.WalkAmplitude.X, Out.CameraKick.WalkAmplitude.Y, Out.CameraKick.WalkPhaseOffsetRad,
		Out.AimOffsetKick.KickPerShotDeg.X, Out.AimOffsetKick.KickPerShotDeg.Y,
		Out.AimOffsetKick.YawBiasSigned, Out.AimOffsetKick.YawNoiseFrac, Out.AimOffsetKick.FirstShotMul,
		Out.AimOffsetKick.WalkRadPerShot, Out.AimOffsetKick.WalkAmplitude.X, Out.AimOffsetKick.WalkAmplitude.Y, Out.AimOffsetKick.WalkPhaseOffsetRad,
		Out.SuppressionShots, Out.ImpulseToVelGain,
		Out.HeatPerShot, Out.CoolRate, Out.CooldownDelay, Out.ShotIndexDecay, Out.WalkHoldSeconds, Out.WalkShotsDecay, Out.WalkPeriodShots,
		Out.BaseSpreadFullDeg, Out.MaxSpreadFullDeg, Out.SpreadCurveExp, Out.SpreadExponent,
		Out.Stiffness, Out.DampingCoeff, Out.MaxOffsetDeg.X, Out.MaxOffsetDeg.Y,
		Out.RecoveryDelay, Out.RecoveryRate);

	if (Out.Stiffness <= 0.0f || Out.CoolRate <= 0.0f || Out.MaxOffsetDeg.X <= 0.0f || Out.MaxOffsetDeg.Y <= 0.0f)
	{
		UE_LOGF(LogArcRecoil, Warning,"BakeProfile '%ls' produced non-positive coefficients: Stiffness=%.3f CoolRate=%.3f MaxOffsetDeg=(%.3f,%.3f)",
			*Preset.GetName(), Out.Stiffness, Out.CoolRate, Out.MaxOffsetDeg.X, Out.MaxOffsetDeg.Y);
	}
}
