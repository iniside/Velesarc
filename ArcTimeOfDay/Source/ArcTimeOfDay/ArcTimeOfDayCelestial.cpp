// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "NiagaraComponent.h"

UDirectionalLightComponent* AArcTimeOfDayActor::GetSunLightComponent() const
{
	return CustomSunLightActor ? Cast<UDirectionalLightComponent>(CustomSunLightActor->GetLightComponent()) : Sun.Get();
}

UDirectionalLightComponent* AArcTimeOfDayActor::GetMoonLightComponent() const
{
	return CustomMoonLightActor ? Cast<UDirectionalLightComponent>(CustomMoonLightActor->GetLightComponent()) : Moon.Get();
}

void AArcTimeOfDayActor::RefreshStaticCelestialProperties()
{
	UDirectionalLightComponent* SunLight = GetSunLightComponent();
	UDirectionalLightComponent* MoonLight = GetMoonLightComponent();
	if (CustomSunLightActor)
	{
		Sun->SetIntensity(0.0f);
	}
	if (CustomMoonLightActor)
	{
		Moon->SetIntensity(0.0f);
	}
	if (SunLight != CachedSunLightTarget.Get())
	{
		CachedSunLightTarget = SunLight;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunLightIntensity)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunLightCastsShadows)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunSpecularScale)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunDisabledBrightness)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunLightColor)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunLightSourceAngle)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SunVolumetricScatteringIntensity)].bInitialized = false;
	}
	if (MoonLight != CachedMoonLightTarget.Get())
	{
		CachedMoonLightTarget = MoonLight;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::FilteredMoonLightIntensity)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::MoonCastingShadows)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::MoonLightSpecularScale)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::MoonLightSourceAngle)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::MoonVolumetricScatteringIntensity)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::MoonAtmosphereLightIndex)].bInitialized = false;
	}
	if (SunLight)
	{
		SunLight->SetAtmosphereSunLight(true);
		SunLight->SetAtmosphereSunLightIndex(0);
	}
	if (MoonLight)
	{
		MoonLight->SetAtmosphereSunLight(true);
		MoonLight->SetAtmosphereSunLightIndex(1);
	}
	if (RealStarsSprites)
	{
		RealStarsSprites->SetAsset(RealStarsSpritesSystem);
		RealStarsSprites->SetVariableTexture(TEXT("Starmap"), RealStarsSpritesStarmap);
		bRealStarsSpritesActive = bSimulateRealStars && bRealStarsFadeToSpritesWhenZoomed;
		RealStarsSprites->SetVisibility(bRealStarsSpritesActive);
		if (RealStarsSprites->IsVisible())
		{
			RealStarsSprites->Activate();
		}
		else
		{
			RealStarsSprites->Deactivate();
		}
	}
}

void AArcTimeOfDayActor::CacheSunAndMoonOrientation()
{
	FVector ExtendMultiplier = FVector::OneVector;
	if (ExtendDawnAndDusk > 0.0)
	{
		const float Minimum = MapRangeClamped(static_cast<float>(ExtendDawnAndDusk), 0.0f, 5.0f, 1.0f, 0.03f);
		const float DayFraction = FMath::Frac(static_cast<float>(TimeOfDay) / 1200.0f);
		const float Alpha = FMath::Clamp(FMath::Square(FMath::Abs(DayFraction - 0.5f) * 2.0f), 0.0f, 1.0f);
		ExtendMultiplier.Z = FMath::Lerp(Minimum, 1.0f, Alpha);
	}

	const FVector SunBase = FRotator(static_cast<float>(SunPitch), 0.0f, 0.0f).RotateVector(FVector::UpVector);
	const FVector SunAxis = FRotator(static_cast<float>(SunPitch), 0.0f, 0.0f).RotateVector(FVector::ForwardVector);
	FVector ManualSunVector = RotateAroundAxis(SunBase, SunAxis, TimeCycleDegrees);
	ManualSunVector = RotateAroundAxis(ManualSunVector, FVector::UpVector, static_cast<float>(SunYaw) + GetActorRotation().Yaw);
	ManualSunVector.Z -= static_cast<float>(SunVerticalOffset);
	ManualSunVector = (ManualSunVector * ExtendMultiplier).GetSafeNormal();
	if (bManuallyPositionSunTarget)
	{
		ManualSunVector = GetActorRotation().RotateVector(-SunTarget).GetSafeNormal();
	}

	const FVector MoonBase = FRotator(static_cast<float>(MoonPitch), 0.0f, 0.0f).RotateVector(FVector::UpVector);
	const FVector MoonAxis = FRotator(static_cast<float>(MoonPitch), 0.0f, 0.0f).RotateVector(FVector::ForwardVector);
	const float MoonCycle = FMath::Frac(TimeCycleDegrees / 360.0f + static_cast<float>(MoonOrbitOffset)) * 360.0f + 180.0f;
	FVector ManualMoonVector = RotateAroundAxis(MoonBase, MoonAxis, MoonCycle);
	ManualMoonVector = RotateAroundAxis(ManualMoonVector, FVector::UpVector, static_cast<float>(MoonYaw) + GetActorRotation().Yaw);
	ManualMoonVector.Z -= static_cast<float>(MoonVerticalOffset);
	ManualMoonVector.Normalize();
	if (bManuallyPositionMoonTarget)
	{
		ManualMoonVector = GetActorRotation().RotateVector(-MoonTarget).GetSafeNormal();
	}

	FArcTimeOfDayAstronomyResult Astronomy;
	if (bSimulateRealSun || bSimulateRealMoon || bSimulateRealStars)
	{
		UpdateAstronomicalSimulation(Astronomy);
	}
	CachedSunVector = bSimulateRealSun && !bManuallyPositionSunTarget ? Astronomy.SunDirection : ManualSunVector;
	CachedMoonVector = bSimulateRealMoon && !bManuallyPositionMoonTarget ? Astronomy.MoonDirection : ManualMoonVector;
	CachedMoonAlignment = bSimulateRealMoon && !bManuallyPositionMoonTarget ? Astronomy.MoonAlignment : CurrentMoonAlignment();
	CachedSimulatedMoonPhaseAngle = Astronomy.MoonPhaseAngle;
	CachedStarsOrientation = bSimulateRealStars ? Astronomy.StarsOrientation : FLinearColor::Black;
	CachedStarsOrbit = bSimulateRealStars ? Astronomy.StarsOrbit : TimeCycleDegrees / 360.0f;
	if (RealStarsSprites && bRealStarsSpritesActive)
	{
		const FVector Orientation(CachedStarsOrientation.R, CachedStarsOrientation.G, CachedStarsOrientation.B);
		const FQuat OrbitRotation = FRotator(0.0f, 0.0f, CachedStarsOrbit * -360.0f).Quaternion();
		const FQuat OrientationRotation(Orientation.GetSafeNormal(), FMath::DegreesToRadians(CachedStarsOrientation.A * -360.0f));
		RealStarsSprites->SetWorldRotation((OrientationRotation * OrbitRotation).Rotator());
	}
	const FVector RealZVector = RotateAroundAxis(RotateAroundAxis(FVector::ForwardVector, FVector(0.0f, -1.0f, 0.0f), static_cast<float>(Latitude)), FVector::UpVector, static_cast<float>(NorthYaw) + GetActorRotation().Yaw);
	const FVector ManualSunZVector = RotateAroundAxis(RotateAroundAxis(FVector::ForwardVector, FVector(0.0f, -1.0f, 0.0f), static_cast<float>(SunPitch)), FVector::UpVector, static_cast<float>(SunYaw) + GetActorRotation().Yaw);
	const FVector ManualMoonZVector = RotateAroundAxis(RotateAroundAxis(FVector::ForwardVector, FVector(0.0f, -1.0f, 0.0f), static_cast<float>(MoonPitch)), FVector::UpVector, static_cast<float>(MoonYaw) + GetActorRotation().Yaw);
	CachedSunZVector = bSimulateRealSun ? RealZVector : ManualSunZVector;
	CachedMoonZVector = bSimulateRealSun ? RealZVector : ManualMoonZVector;
	CachedSunScale = CurrentSunRadius();
	CachedMoonScale = CurrentMoonScale();
	CachedDirectionalDimming = CurrentDirectionalDimming();
	EclipsePercent = CurrentSolarEclipse();
	CurrentEclipsePercent = EclipsePercent;
}

float AArcTimeOfDayActor::CurrentSunRadius() const
{
	const float BaseRadius = FMath::DegreesToRadians(static_cast<float>(SunScale) * (1.0f + FMath::Max(static_cast<float>(SunSoftness) - 0.75f, 0.0f) * 0.1f));
	if (ScaleSunRadiusNearHorizon == 1.0)
	{
		return BaseRadius;
	}
	return BaseRadius * MapRangeClamped(CachedSunVector.Z, 0.0f, -0.5f, static_cast<float>(ScaleSunRadiusNearHorizon), 1.0f);
}

float AArcTimeOfDayActor::CurrentMoonScale() const
{
	if (!bRenderMoon)
	{
		return 0.0f;
	}
	const float BaseScale = FMath::DegreesToRadians(static_cast<float>(MoonScale)) * 2.0f;
	if (ScaleMoonRadiusNearHorizon == 1.0)
	{
		return BaseScale;
	}
	return BaseScale * MapRangeClamped(CachedMoonVector.Z, 0.0f, -0.6f, static_cast<float>(ScaleMoonRadiusNearHorizon), 1.0f);
}

float AArcTimeOfDayActor::CurrentDirectionalDimming() const
{
	if (!bDimDirectionalLightsWithCloudCoverage)
	{
		return 1.0f;
	}
	const float Lower = DimmingRange.GetLowerBoundValue();
	const float Upper = DimmingRange.GetUpperBoundValue();
	return FMath::Pow(MapRangeClamped(LocalCloudCoverage, Lower, Upper, 1.0f, 0.0f), static_cast<float>(DimmingRangeExponent));
}

float AArcTimeOfDayActor::CurrentSolarEclipse()
{
	if (!bSolarEclipse || !bRenderMoon || FVector::DotProduct(CachedSunVector, CachedMoonVector) <= 0.9f)
	{
		CachedSolarEclipseTint = FLinearColor::White;
		return 1.0f;
	}
	const float SunAngularDiameter = FMath::RadiansToDegrees(CachedSunScale) * 1.05f;
	const float MoonAngularDiameter = FMath::RadiansToDegrees(CachedMoonScale) * 0.5f;
	const float Separation = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CachedSunVector, CachedMoonVector), -1.0f, 1.0f)));
	const float MinimumSeparation = FMath::Abs(MoonAngularDiameter - SunAngularDiameter);
	const float CoveredFraction = MoonAngularDiameter > SunAngularDiameter ? 0.0f : 1.0f - MoonAngularDiameter / SunAngularDiameter;
	const float SunFractionShowing = MapRangeClamped(Separation, MinimumSeparation, MoonAngularDiameter + SunAngularDiameter, CoveredFraction, 1.0f);
	CachedSolarEclipseTint = FMath::Lerp(SolarEclipseTintColor, FLinearColor::White, SunFractionShowing);
	return FMath::Lerp(static_cast<float>(SolarEclipseIntensityMultiplier), 1.0f, SunFractionShowing);
}

FVector AArcTimeOfDayActor::CurrentMoonAlignment() const
{
	const FVector BaseAlignment = FVector::CrossProduct(CachedMoonVector, FRotator(static_cast<float>(MoonPitch), static_cast<float>(MoonYaw), 0.0f).RotateVector(FVector::ForwardVector)).GetSafeNormal();
	return RotateAroundAxis(BaseAlignment, CachedMoonVector, static_cast<float>(MoonTextureRotation));
}

FVector AArcTimeOfDayActor::CurrentMoonPhaseAngle() const
{
	if (bSimulateRealMoon)
	{
		return CachedSimulatedMoonPhaseAngle * static_cast<float>(MoonPhaseContrast);
	}
	return FVector(0.0f, -1.0f, 0.0f).RotateAngleAxis(MapRangeClamped(static_cast<float>(MoonPhase), 0.0f, 29.530001f, 0.0f, 360.0f), FVector::UpVector) * static_cast<float>(MoonPhaseContrast);
}
