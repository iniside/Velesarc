// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Materials/MaterialInstanceDynamic.h"

float AArcTimeOfDayActor::CurrentFogDensity() const
{
	const float FogContribution = FMath::Pow(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) / 10.0f, 4.0f) * static_cast<float>(FoggyDensityContribution);
	const float DustContribution = FMath::Pow(static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)), 4.0f) * static_cast<float>(DustDensityContribution);
	return (static_cast<float>(BaseFogDensity) + FMath::Max(FogContribution, DustContribution)) * static_cast<float>(ScaleFogDensity);
}

float AArcTimeOfDayActor::CurrentFogHeightFalloff() const
{
	const float Cloudy = FMath::Lerp(static_cast<float>(BaseHeightFogFalloff), static_cast<float>(CloudyHeightFogFalloff), LocalOvercast01);
	const float Foggy = FMath::Lerp(Cloudy, static_cast<float>(FoggyHeightFogFalloff), static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) / 10.0f);
	return FMath::Lerp(Foggy, static_cast<float>(DustyHeightFogFalloff), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)));
}

float AArcTimeOfDayActor::CurrentFogStartDistance(const float Density) const
{
	const float Alpha = FMath::Pow(MapRangeClamped(Density, 0.0f, static_cast<float>(MinFogStartDistanceDensity), 1.0f, 0.0f), 10.0f);
	return FMath::Lerp(FMath::Min(static_cast<float>(MinFogStartDistance), static_cast<float>(MaxFogStartDistance)), FMath::Max(static_cast<float>(MaxFogStartDistance), static_cast<float>(MinFogStartDistance)), Alpha);
}

float AArcTimeOfDayActor::CurrentSunLightIntensity() const
{
	const UDirectionalLightComponent* SunLight = GetSunLightComponent();
	if (!SunLight || !SunLight->IsVisible())
	{
		return 0.0f;
	}
	if (SkyMode == EArcTimeOfDaySkyMode::Space && bSpaceSkyModeDisablesDirectionalLightsBelowHorizon && CachedSunVector.Z > 0.0f)
	{
		return 0.0f;
	}
	float Intensity = static_cast<float>(SunLightIntensity);
	if (bFadeDownHighSunLightIntensityBelowHorizon)
	{
		const float MinimumScale = FMath::Min(5.0f / FMath::Max(Intensity, UE_SMALL_NUMBER), 1.0f);
		Intensity *= FMath::Lerp(MinimumScale, 1.0f, FMath::Pow(MapRangeClamped(CachedSunVector.Z, 0.0f, 0.11f, 1.0f, 0.0f), 8.0f));
	}
	return MapRangeClamped(CachedSunVector.Z, 0.157f, 0.113f, 0.0f, Intensity) * EclipsePercent * CachedDirectionalDimming * static_cast<float>(DirectionalBalance) * CurrentSceneLightingBrightnessScale;
}

float AArcTimeOfDayActor::CurrentMoonLitPercent() const
{
	if (!bRenderMoon)
	{
		return 0.0f;
	}
	if (!bRenderMoonPhases)
	{
		return 1.0f;
	}
	if (bSimulateRealMoon)
	{
		return MapRangeClamped(CachedSimulatedMoonPhaseAngle.Y, -1.0f, 1.0f, 1.0f, 0.0f);
	}
	return FMath::Pow(FMath::Clamp(FMath::Abs(static_cast<float>(MoonPhase) - 14.765f) / 14.765f, 0.0f, 1.0f), 2.6f);
}

float AArcTimeOfDayActor::CurrentMoonLightIntensity() const
{
	const UDirectionalLightComponent* MoonLight = GetMoonLightComponent();
	if (!bRenderMoon || !MoonLight || !MoonLight->IsVisible())
	{
		return 0.0f;
	}
	if (SkyMode == EArcTimeOfDaySkyMode::Space && bSpaceSkyModeDisablesDirectionalLightsBelowHorizon && CachedMoonVector.Z > 0.0f)
	{
		return 0.0f;
	}
	const float SunFade = MapRangeClamped(CachedSunVector.Z, 0.0f, -0.02f, 1.0f, 0.0f);
	const float MoonFade = MapRangeClamped(CachedMoonVector.Z, 0.0f, -0.195f, 0.0f, 1.0f);
	const float PhaseMultiplier = FMath::Lerp(FMath::Clamp(CachedMoonLitPercent, 0.0f, 1.0f), 1.0f, static_cast<float>(NewMoonLightBrightness));
	return static_cast<float>(MoonLightIntensity) * SunFade * MoonFade * PhaseMultiplier * CachedDirectionalDimming * static_cast<float>(DirectionalBalance) * CurrentSceneLightingBrightnessScale;
}

FLinearColor AArcTimeOfDayActor::CurrentRayleighScatteringColor() const
{
	const FLinearColor Base = ThreeColorTimeBlend(RayleighScatteringColorDay, RayleighScatteringColorDawnDusk, RayleighScatteringColorNight);
	const float CloudDesaturation = LocalOvercast01 * FMath::Lerp(static_cast<float>(DesaturateRayleighScatteringWhenCloudy), 0.7f, MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.03f, 0.2f, 1.0f, 0.0f));
	return Desaturate(Base, FMath::Clamp(CloudDesaturation + (1.0f - static_cast<float>(Saturation)), 0.0f, 1.0f));
}

FLinearColor AArcTimeOfDayActor::CurrentSkyAtmosphereAbsorptionColor() const
{
	const float TwilightAlpha = MapRangeClamped(CachedSunVector.Z, -0.025f, 0.025f, 0.0f, 1.0f);
	FLinearColor HSV = FMath::Lerp(SunsetSunriseAbsorptionColor, TwilightAbsorptionColor, TwilightAlpha).LinearRGBToHSV();
	HSV.R = FMath::Fmod(HSV.R + 180.0f, 360.0f);
	FLinearColor Result = HSV.HSVToLinearRGB();
	Result.A = FMath::Lerp(static_cast<float>(SunsetSunriseAbsorptionIntensity), static_cast<float>(TwilightAbsorptionIntensity), TwilightAlpha) * MapRangeClamped(LocalCloudCoverage, 1.3f, 2.5f, 1.0f, 0.4f);
	return Desaturate(Result, 1.0f - static_cast<float>(Saturation));
}

bool AArcTimeOfDayActor::IsDirectionalLightCastingShadows(const bool bForceDisabled, const FVector& LightVector) const
{
	if (bForceDisabled)
	{
		return false;
	}
	if (SkyMode == EArcTimeOfDaySkyMode::Space)
	{
		return !bSpaceSkyModeDisablesDirectionalLightsBelowHorizon || LightVector.Z < SunShadowsCutoffZ;
	}
	const bool bUsingVolumeOrVoxelClouds = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds;
	const bool bCloudPaintCanSubtractCoverage = false;
	const bool bDisableShadows = bAllowDisablingDirectionalShadowsWithCoverage && ((CameraCloudLayerNormalizedHeight == 0.0f && !bCloudPaintCanSubtractCoverage) || !bUsingVolumeOrVoxelClouds);
	if (!bDisableShadows)
	{
		return true;
	}
	const float CloudShadowDensity = bUsingVolumeOrVoxelClouds
		? CloudCoverage03
		: CloudCoverage03 * FMath::Lerp(1.0f, 1.4f, GlobalOvercast01) * static_cast<float>(TwoDCloudsZenithDensityMultiplier) * 1.5f;
	return CloudShadowDensity <= CloudShadowDisableThreshold;
}

void AArcTimeOfDayActor::ApplyLightingAndSky()
{
	ApplyEnvironmentStaticProperties();
	ApplyCloudShadows();
	UDirectionalLightComponent* SunLightComponent = GetSunLightComponent();
	UDirectionalLightComponent* MoonLightComponent = GetMoonLightComponent();
	UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent();
	const FLinearColor ActiveMoonLightColor = MoonLightComponent ? MoonLightComponent->GetLightColor() : FLinearColor::Black;
	const bool bVolumetricFogEnabled = FogComponent && FogComponent->bEnableVolumetricFog;
	if (!CustomSunLightActor && Sun->GetAttachParent() != SkyRoot)
	{
		Sun->AttachToComponent(SkyRoot, FAttachmentTransformRules::KeepWorldTransform);
	}
	if (!CustomMoonLightActor && Moon->GetAttachParent() != SkyRoot)
	{
		Moon->AttachToComponent(SkyRoot, FAttachmentTransformRules::KeepWorldTransform);
	}

	const FLinearColor SunVectorColor(CachedSunVector.X, CachedSunVector.Y, CachedSunVector.Z, 1.0f);
	const FLinearColor MoonVectorColor(CachedMoonVector.X, CachedMoonVector.Y, CachedMoonVector.Z, 1.0f);
	CacheColorValue(EArcTimeOfDayCachedProperty::SunRootVector, SunVectorColor, 0.00001f);
	CacheColorValue(EArcTimeOfDayCachedProperty::MoonRootVector, MoonVectorColor, 0.00001f);
	SetWeatherScalar(TEXT("Time of Day"), FMath::Fmod(static_cast<float>(TimeOfDay), 2400.0f));
	ApplySkyMaterialGroup(0);

	ApplySkyMaterialGroup(2);
	const float FogDensity = CurrentFogDensity();
	CacheFloatValue(EArcTimeOfDayCachedProperty::FogDensity, FogDensity, 0.000001f);
	const float FogHeightFalloff = CurrentFogHeightFalloff();
	CacheFloatValue(EArcTimeOfDayCachedProperty::FogHeightFalloff, FogHeightFalloff, 0.0001f);
	const float FogStartDistance = CurrentFogStartDistance(FogDensity);
	CacheFloatValue(EArcTimeOfDayCachedProperty::FogStartDistance, FogStartDistance, 50.0f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::FogMaxOpacity, bDisableHeightFogAboveVolumetricCloudLayer ? 1.0f - CameraCloudLayerNormalizedHeight : 1.0f, 0.0001f);

	const float SunTwilightBrightness = FMath::Pow(MapRangeClamped(CachedSunVector.Z, -0.01f, 0.25f, 1.0f, 0.0f), 11.0f);
	const float MoonTwilightBrightness = bRenderMoon ? FMath::Pow(MapRangeClamped(CachedMoonVector.Z, -0.01f, 0.25f, 1.0f, 0.0f), 11.0f) : 0.0f;
	const float SunIntensityRatio = FMath::Min(5.0f / FMath::Max(static_cast<float>(SunLightIntensity), UE_SMALL_NUMBER), 1.0f);
	const float SunIntensityFade = FMath::Pow(MapRangeClamped(CachedSunVector.Z, 0.0f, 0.11f, 1.0f, 0.0f), 8.0f);
	const float AdjustedSunIntensity = static_cast<float>(SunLightIntensity) * FMath::Lerp(SunIntensityRatio, 1.0f, SunIntensityFade);
	const FLinearColor SunFogBase = SunTwilightBrightness > 0.0f ? SunLightColor * (AdjustedSunIntensity / 33.0f * SunTwilightBrightness) : FLinearColor::Black;
	const FLinearColor MoonFogBase = MoonTwilightBrightness > 0.0f ? ActiveMoonLightColor * (static_cast<float>(MoonLightIntensity) / 23.25f * CachedMoonLitPercent) : FLinearColor::Black;
	CachedSunFogBaseColor = SunFogBase;
	CachedMoonFogBaseColor = MoonFogBase;
	const float FogDesaturation = FMath::Clamp((1.0f - static_cast<float>(Saturation)) + FMath::Max(static_cast<float>(FoggyDesaturation) * static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) / 10.0f, static_cast<float>(OvercastDesaturation) * LocalOvercast01), 0.0f, 1.0f);
	const float OvercastBrightness = FMath::Pow(FMath::Lerp(static_cast<float>(OvercastBrightnessDay), static_cast<float>(OvercastBrightnessNight), CachedNightFilter), 1.2f) * MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.07f, 0.2f, 0.7f, 1.0f);
	const float WeatherBrightnessBlend = FMath::Lerp(FMath::Sqrt(LocalOvercast01), FMath::Max3(FMath::Sqrt(LocalOvercast01), static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) / 10.0f, static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0))), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)));
	const float FogBrightnessBase = FMath::Lerp(1.0f, FMath::Lerp(OvercastBrightness, 0.4f, static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0))), WeatherBrightnessBlend);
	const float OvercastLuminanceBoost = FMath::Lerp(1.0f, FMath::Lerp(2.4f, 1.7f, CachedNightFilter) * static_cast<float>(SkyAtmosphereOvercastLuminance) * 1.25f, LocalOvercast01);
	const float LocalFogBrightnessScalar = FogBrightnessBase * OvercastLuminanceBoost;
	const FLinearColor LocalFogBrightnessMultiplier = AllFogColorsMultiplier * (static_cast<float>(FogColorIntensityScale) * LocalFogBrightnessScalar);
	const float OvercastFogAttenuation = FMath::Lerp(1.0f, 0.2f, LocalOvercast01 * MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.0f, 0.15f, 1.0f, 0.0f));
	const FLinearColor OvercastSunTint = FMath::Lerp(FLinearColor::White, FLinearColor(0.979645f, 0.878076f, 1.0f, 1.0f), LocalOvercast01);
	const FLinearColor LitFogColor = FogInscatteringSun * OvercastSunTint * SunFogBase + FogInscatteringMoon * MoonFogBase;
	const FLinearColor DustFogColor = (DustColor - FMath::Lerp(FLinearColor::Black, FLinearColor(0.034208f, 0.055984f, 0.067708f, 1.0f), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)))) * (SunFogBase + MoonFogBase);
	const FLinearColor AmbientFogColor = FMath::Lerp(Desaturate(LitFogColor, FogDesaturation), DustFogColor, FMath::Sqrt(static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)))) * (OvercastFogAttenuation * CachedAbsentDirectionalLightsBrightness) * LocalFogBrightnessMultiplier;
	CachedAmbientFogColor = AmbientFogColor;
	const FLinearColor LightPollutionFogColor = CachedLightPollution * FLinearColor(0.935964f, 0.832036f, 1.0f, 1.0f) * (static_cast<float>(FogColorIntensityScale) * 0.01f);
	const FLinearColor NightSkyGlowFogColor = CachedNightSkyGlow * (FMath::Lerp(1.0f, static_cast<float>(OvercastBrightnessNight), LocalOvercast01) * 0.4f);
	const FLinearColor SkyGlowFogColor = LightPollutionFogColor + NightSkyGlowFogColor;
	CacheColorValue(EArcTimeOfDayCachedProperty::AmbientFogColor, (AmbientFogColor + SkyGlowFogColor) * CachedNightFilteredNightBrightness, 0.0001f);
	ApplySkyMaterialGroup(3);
	const bool bFogColorsFromSkyAtmosphere = FogColorMode == EArcTimeOfDayFogColorMode::SkyAtmosphere || FogColorMode == EArcTimeOfDayFogColorMode::Automatic && ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere;
	const FLinearColor SnowLitFogColor = AmbientFogColor + FLinearColor(0.703125f, 0.821875f, 1.0f, 1.0f) * (AmbientFogColor.GetLuminance() * FMath::Max(FogSnowContribution.GetValue(), 0.0f));
	const FLinearColor FogBaseColor = bFogColorsFromSkyAtmosphere ? FMath::Lerp(FLinearColor::Black, SnowLitFogColor, FMath::Clamp(FMath::Max(FMath::Sqrt(static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0))), 1.0f - CachedDirectionalDimming), 0.0f, 1.0f)) : SnowLitFogColor;
	const FLinearColor FogColor = FogBaseColor * CachedNightFilteredNightBrightness + SkyGlowFogColor;
	FLinearColor BaseDirectionalFog = FLinearColor::Black;
	if (CachedSunVector.Z < 0.05f && FogDirectionalInscatteringColorCurve)
	{
		const float SunZenithAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CachedSunVector.Z, -1.0f, 1.0f)));
		BaseDirectionalFog = FogDirectionalInscatteringColorCurve->GetLinearColorValue(SunZenithAngle) * SunLightColor * FogDirectionalInscattering;
	}
	const FLinearColor DirectionalFogColor = FMath::Lerp(Desaturate(BaseDirectionalFog, FogDesaturation), Desaturate(BaseDirectionalFog, FogDesaturation) * (DustColor * 0.5f), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0))) * LocalFogBrightnessMultiplier;
	CachedDirectionalBaseFogColor = BaseDirectionalFog;
	float DirectionalFogIntensity = 1.0f;
	if (ColorMode != EArcTimeOfDayColorMode::SkyAtmosphere)
	{
		DirectionalFogIntensity = 1.7f / FMath::Clamp(DirectionalIntensityCurve->GetFloatValue(CachedSunVector.Z), 0.05f, 1.0f);
	}
	else if (bFogColorsFromSkyAtmosphere)
	{
		DirectionalFogIntensity = MapRangeClamped(SunHeight(), 0.5f, 0.6f, 0.0f, 0.35f);
	}
	const float ScaledDirectionalBalance = CurrentSceneLightingBrightnessScale * static_cast<float>(DirectionalBalance);
	const FLinearColor DirectionalFog = FMath::IsNearlyZero(ScaledDirectionalBalance) ? FLinearColor::Black : DirectionalFogColor * (DirectionalFogIntensity / ScaledDirectionalBalance);
	CacheColorValue(EArcTimeOfDayCachedProperty::FogInscatteringColor, FogColor, 0.0001f);
	CacheColorValue(EArcTimeOfDayCachedProperty::FogDirectionalInscatteringColor, DirectionalFog, 0.0001f);
	const float SkyAmbientDivisor = 0.8f * static_cast<float>(OverallIntensity) * CachedAbsentDirectionalLightsBrightness;
	FLinearColor SkyAmbientColor = FMath::IsNearlyZero(SkyAmbientDivisor) ? FLinearColor::Black : AmbientFogColor / SkyAmbientDivisor * 0.8f;
	SkyAmbientColor.A = 0.0f;
	CacheColorValue(EArcTimeOfDayCachedProperty::SkyAmbientColor, SkyAmbientColor, 0.0001f);
	const float BaseDirectionalExponent = FMath::Lerp(2.0f, 5.0f, MapRangeClamped(LocalCloudCoverage, 1.15f, 1.7f, 0.0f, 1.0f) * MapRangeClamped(FMath::Max(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)) * 10.0f), 4.0f, 10.0f, 0.0f, 1.0f));
	const float DirectionalExponent = bDisableHeightFogAboveVolumetricCloudLayer ? FMath::Lerp(BaseDirectionalExponent, 64.0f, FMath::Pow(CameraCloudLayerNormalizedHeight, 3.0f)) : BaseDirectionalExponent;
	CacheFloatValue(EArcTimeOfDayCachedProperty::DirectionalInscatteringExponent, DirectionalExponent, 0.05f);
	const float ContrastMidpoint = FMath::Max((SunFogBase + MoonFogBase).GetLuminance() * 0.5f * FMath::Lerp(1.0f, 0.4f, LocalOvercast01), 0.002f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::SkyMaterialContrastMidpoint, ContrastMidpoint, 0.000001f);

	if (ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere)
	{
		if (bControlSkyAtmosphereSettings)
		{
			SkyAtmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtComponentTransform;
			SkyAtmosphere->SetAerialPespectiveViewDistanceScale(0.0f);
			SkyAtmosphere->SetRayleighScatteringScale(0.04f);
			SkyAtmosphere->SetMieScattering(FLinearColor(0.802083f, 0.879982f, 1.0f, 1.0f));
			const float MieAnisotropy = MapRangeClamped(LocalCloudCoverage, 1.3f, 2.3f, 0.75f, 0.05f) * static_cast<float>(MieAnisotropyMultiplier) * EclipsePercent;
			const float MieScale = (FMath::Lerp(0.003996f, 0.02f, FMath::Pow(MapRangeClamped(LocalCloudCoverage, 1.35f, 3.0f, 0.0f, 1.0f), 2.0f)) + static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) * 0.01f) * static_cast<float>(MieScatteringScaleMultiplier);
			CacheFloatValue(EArcTimeOfDayCachedProperty::MieAnisotropy, MieAnisotropy, 0.0001f);
			CacheFloatValue(EArcTimeOfDayCachedProperty::MieScatteringScale, MieScale, 0.00001f);
			const FLinearColor Rayleigh = CurrentRayleighScatteringColor();
			CacheColorValue(EArcTimeOfDayCachedProperty::RayleighScatteringColor, Rayleigh, 0.0001f);
			const FLinearColor Absorption = CurrentSkyAtmosphereAbsorptionColor();
			CacheColorValue(EArcTimeOfDayCachedProperty::SkyAtmoAbsorptionColor, Absorption, 0.0001f);
			const FLinearColor Luminance = FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor::LerpUsingHSV(FLinearColor(0.526558f, 0.547992f, 0.578f, 1.0f), FLinearColor(0.871367f, 0.913099f, 1.0f, 1.0f), MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.07f, 0.2f, 0.0f, 1.0f)), LocalOvercast01);
			const FLinearColor AppliedLuminance = Luminance * static_cast<float>(SkyAtmosphereOvercastLuminance) / CachedDirectionalInscatteringMultiplier;
			CacheColorValue(EArcTimeOfDayCachedProperty::SkyAtmosphereLuminance, AppliedLuminance, 0.0001f);
			const float NoneInfluence = static_cast<float>(SkyAtmosphereFogInfluenceNone) * MapRangeClamped(LocalCloudCoverage, 0.2f, 1.2f, 1.0f, 0.5f);
			const float MoonInfluence = FMath::Lerp(NoneInfluence, static_cast<float>(SkyAtmosphereFogInfluenceMoon), FMath::Clamp((CachedMoonVector.Z + 0.035f) * -2.35f, 0.0f, 1.0f) * CachedMoonLitPercent);
			const float FogContributionNumerator = (FMath::Lerp(MoonInfluence, static_cast<float>(SkyAtmosphereFogInfluenceSun), 1.0f - FMath::Square(MapRangeClamped(CachedSunVector.Z, -0.2f, 0.02f, 0.0f, 1.0f))) + FMath::Max(FogSnowContribution.GetValue(), 0.0f) * FMath::Lerp(2.0f, 0.25f, LocalOvercast01)) * static_cast<float>(FogColorIntensityScale);
			const float FogContribution = FMath::IsNearlyZero(ScaledDirectionalBalance) ? 0.0f : FogContributionNumerator / ScaledDirectionalBalance;
			CacheFloatValue(EArcTimeOfDayCachedProperty::FogSkyAtmosphereContribution, FogContribution, 0.0001f);
		}
	}
	else
	{
		const FLinearColor DayRatio(BaseSkyColorDay.R / 0.050313f, BaseSkyColorDay.G / 0.100844f, BaseSkyColorDay.B / 0.21875f, BaseSkyColorDay.A);
		const FLinearColor DawnDuskRatio(BaseSkyColorDawnDusk.R / 0.094145f, BaseSkyColorDawnDusk.G / 0.167055f, BaseSkyColorDawnDusk.B / 0.260417f, BaseSkyColorDawnDusk.A);
		const FLinearColor NightRatio(BaseSkyColorNight.R / 0.065593f, BaseSkyColorNight.G / 0.102341f, BaseSkyColorNight.B / 0.161458f, BaseSkyColorNight.A);
		CacheColorValue(EArcTimeOfDayCachedProperty::SkyBaseColor, ThreeColorTimeBlend(DayRatio, DawnDuskRatio, NightRatio), 0.0001f);
		const FLinearColor CloudLightDayRatio(CloudLightColorDay.R / 0.708631f, CloudLightColorDay.G / 0.785507f, CloudLightColorDay.B / 0.901042f, CloudLightColorDay.A);
		const FLinearColor CloudLightDawnDuskRatio(CloudLightColorDawnDusk.R, CloudLightColorDawnDusk.G / 0.477468f, CloudLightColorDawnDusk.B / 0.054318f, CloudLightColorDawnDusk.A);
		const FLinearColor CloudLightNightRatio(CloudLightColorNight.R / 1.177604f, CloudLightColorNight.G / 1.321682f, CloudLightColorNight.B / 1.7f, CloudLightColorNight.A);
		const float CloudCoverageMultiplier = MapRangeClamped(CloudCoverage03, 1.7f, 2.3f, 1.0f, 0.5f);
		CacheColorValue(EArcTimeOfDayCachedProperty::CloudLightColor, ThreeColorTimeBlend(CloudLightDayRatio, CloudLightDawnDuskRatio, CloudLightNightRatio) * CloudCoverageMultiplier, 0.0001f);
		const FLinearColor CloudDarkDayRatio(CloudDarkColorDay.R / 0.034505f, CloudDarkColorDay.G / 0.119041f, CloudDarkColorDay.B / 0.276042f, CloudDarkColorDay.A);
		const FLinearColor CloudDarkDawnDuskRatio(CloudDarkColorDawnDusk.R / 0.04305f, CloudDarkColorDawnDusk.G / 0.06991f, CloudDarkColorDawnDusk.B / 0.119792f, CloudDarkColorDawnDusk.A);
		const FLinearColor CloudDarkNightRatio(CloudDarkColorNight.R / 0.070258f, CloudDarkColorNight.G / 0.10947f, CloudDarkColorNight.B / 0.182292f, CloudDarkColorNight.A);
		CacheColorValue(EArcTimeOfDayCachedProperty::CloudDarkColor, ThreeColorTimeBlend(CloudDarkDayRatio, CloudDarkDawnDuskRatio, CloudDarkNightRatio), 0.0001f);
		const FLinearColor MoonScattering = bRenderMoon ? ActiveMoonLightColor * (static_cast<float>(MoonLightIntensity) * ScaledDirectionalBalance) : FLinearColor::Black;
		const FLinearColor SunScattering = CachedSolarEclipseTint * SunLightColor;
		CacheColorValue(EArcTimeOfDayCachedProperty::MoonScatteringColor, MoonScattering, 0.001f);
		CacheColorValue(EArcTimeOfDayCachedProperty::SunScatteringColor, SunScattering, 0.001f);
	}

	const float SunIntensity = CurrentSunLightIntensity();
	if (SunLightComponent)
	{
		const FLinearColor DiskCurve = SunDiskColorCurve ? SunDiskColorCurve->GetLinearColorValue(SunHeight()) : FLinearColor::White;
		const float DiskIntensity = SunLightComponent->IsVisible() && SunIntensity > 0.0f ? FMath::Max(static_cast<float>(SunDiskIntensity), 0.0f) * 0.25f : 0.0f;
		SunLightComponent->SetAtmosphereSunDiskColorScale(DiskCurve * SunDiskTint * DiskIntensity);
	}
	CacheColorValue(EArcTimeOfDayCachedProperty::SunLightColor, CachedSunLightColor, 0.0001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::SunLightIntensity, SunIntensity, 0.00001f);
	const bool bSunLightCastingShadows = IsDirectionalLightCastingShadows(false, CachedSunVector);
	CacheFloatValue(EArcTimeOfDayCachedProperty::SunLightCastsShadows, bSunLightCastingShadows ? 1.0f : 0.0f, 0.000001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::SunDisabledBrightness, 1.0f - LocalOvercast01, 0.005f);
	const float SunSpecular = static_cast<float>(SunSpecularScale) * FMath::Min(FMath::Pow(1.0f - LocalOvercast01, 3.0f), 0.999f) * CachedDirectionalSpecularCancelValue;
	CacheFloatValue(EArcTimeOfDayCachedProperty::SunSpecularScale, SunSpecular, 0.005f);
	const float SunSourceAngle = FMath::RadiansToDegrees(CachedSunScale) * static_cast<float>(SunSourceAngleScale) * FMath::Lerp(1.0f, static_cast<float>(MaxSunSourceAngleScale), FMath::Max((static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) - 6.0f) / 4.0f, LocalOvercast01));
	CacheFloatValue(EArcTimeOfDayCachedProperty::SunLightSourceAngle, SunSourceAngle, 0.005f);
	if (SunLightComponent)
	{
		SunLightComponent->CloudScatteredLuminanceScale = FLinearColor(2.695f, 2.963334f, 3.5f, 1.0f);
	}

	ApplySkyMaterialGroup(6);
	const float MoonIntensity = CurrentMoonLightIntensity();
	MoonLightCurrentIntensityValue = MoonIntensity;
	CacheFloatValue(EArcTimeOfDayCachedProperty::FilteredMoonLightIntensity, MoonIntensity, 0.000001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::MoonCastingShadows, IsDirectionalLightCastingShadows(bSunLightCastingShadows, CachedMoonVector) ? 1.0f : 0.0f, 0.000001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::MoonAtmosphereLightIndex, SunLightComponent && SunLightComponent->IsVisible() && SunIntensity > 0.0f ? 1.0f : 0.0f, 0.1f);
	const float MoonSpecular = static_cast<float>(MoonSpecularScale) * (1.0f - LocalOvercast01) * FMath::Pow(MapRangeClamped(CachedMoonVector.Z, -0.1f, -0.2f, 0.0f, 1.0f), 5.0f) * CachedDirectionalSpecularCancelValue;
	CacheFloatValue(EArcTimeOfDayCachedProperty::MoonLightSpecularScale, MoonSpecular, 0.005f);
	const float MoonSourceAngle = static_cast<float>(MoonScale * MoonSourceAngleScale) * FMath::Lerp(1.0f, static_cast<float>(MaxMoonSourceAngleScale), FMath::Max((static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) - 6.0f) / 4.0f, LocalOvercast01));
	CacheFloatValue(EArcTimeOfDayCachedProperty::MoonLightSourceAngle, MoonSourceAngle, 0.01f);
	if (MoonLightComponent)
	{
		MoonLightComponent->CloudScatteredLuminanceScale = FLinearColor(2.187501f, 2.458332f, 3.0f, 1.0f);
	}

	FLinearColor SkyLightMultiplier = ThreeColorTimeBlend(SkyLightColorMultiplierDay, SkyLightColorMultiplierDawnDusk, SkyLightColorMultiplierNight);
	FLinearColor SkyLightBaseColor = FLinearColor::White;
	float SkyLightBaseIntensity = 1.0f;
	if (SkyLightMode == EArcTimeOfDaySkyLightMode::CubemapDynamicColor)
	{
		SkyLightMultiplier *= SkyLightDynamicColorTintingMultiplier;
		const FLinearColor SkyLightCombinedColor = (SkyGlowFogColor + Desaturate(AmbientFogColor, 0.2f)) * 1000.0f;
		const FVector SkyLightVector(SkyLightCombinedColor.R, SkyLightCombinedColor.G, SkyLightCombinedColor.B);
		SkyLightBaseColor = FLinearColor(SkyLightVector.GetSafeNormal());
		const FLinearColor SkyLightIntensityColor = SkyGlowFogColor * 2.5f + AmbientFogColor;
		SkyLightBaseIntensity = FVector(SkyLightIntensityColor.R, SkyLightIntensityColor.G, SkyLightIntensityColor.B).Length() * MapRangeClamped(FMath::Max3(LocalCloudCoverage, static_cast<float>(Fog.GetClampedValue(0.0, 10.0)) / 4.0f, static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0)) * 2.0f), 1.1f, 2.3f, 2.3f, 2.7f) * CachedNightFilteredNightBrightness;
	}
	const FLinearColor TemperatureTint = FMath::Lerp(
		FMath::Lerp(FLinearColor(1.0f, 0.701961f, 0.466667f, 1.0f), FLinearColor::White, MapRangeClamped(static_cast<float>(SkyLightTemperature), -4000.0f, 0.0f, -1.0f, 1.0f)),
		FLinearColor(0.678431f, 0.776471f, 1.0f, 1.0f),
		MapRangeClamped(static_cast<float>(SkyLightTemperature), 0.0f, 4000.0f, 0.0f, 2.0f));
	FLinearColor SkyLightHSV = (SkyLightBaseColor * SkyLightMultiplier).LinearRGBToHSV();
	SkyLightHSV.B = FMath::Clamp(SkyLightHSV.B, 0.0f, 1.0f);
	SkyLightHSV.A = 1.0f;
	const FLinearColor ClampedSkyLightColor = SkyLightHSV.HSVToLinearRGB();
	const FLinearColor SkyLightColor = ClampedSkyLightColor * TemperatureTint;
	const float SkyLightColorLuminance = SkyLightColor.GetLuminance();
	const float LuminanceBalance = FMath::IsNearlyZero(SkyLightColorLuminance) ? 0.0f : ClampedSkyLightColor.GetLuminance() / SkyLightColorLuminance;
	const float SkyLightFinalIntensity = SkyLightBaseIntensity * static_cast<float>(SkyLightIntensity) * FMath::Max(SkyLightMultiplier.GetLuminance(), 1.0f) * LuminanceBalance;
	CacheFloatValue(EArcTimeOfDayCachedProperty::SkyLightIntensity, SkyLightFinalIntensity, 0.000001f);
	CacheColorValue(EArcTimeOfDayCachedProperty::SkyLightColor, SkyLightColor, 0.0001f);
	FLinearColor LowerHemisphere = SkyLightMode == EArcTimeOfDaySkyLightMode::CaptureBased ? AmbientFogColor * SkyLightLowerHemisphereTintCaptureBased : SkyLightLowerHemisphereTintCubemap;
	if (SkyMode == EArcTimeOfDaySkyMode::Space) LowerHemisphere = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);
	else if (SkyLightMode == EArcTimeOfDaySkyLightMode::CaptureBased) LowerHemisphere.A = 0.0f;
	CacheColorValue(EArcTimeOfDayCachedProperty::SkyLightLowerHemisphereColor, LowerHemisphere, 0.0001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::ExposureBias, ThreeTimeFloats(static_cast<float>(ExposureBiasDay), static_cast<float>(ExposureBiasDawnDusk), static_cast<float>(ExposureBiasNight)) + static_cast<float>(ExposureBiasCloudy) * LocalOvercast01, 0.005f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::SunVolumetricScatteringIntensity, bVolumetricFogEnabled ? static_cast<float>(SunVolumetricScatteringIntensity) : 0.0f, 0.01f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::MoonVolumetricScatteringIntensity, bVolumetricFogEnabled ? static_cast<float>(MoonVolumetricScatteringIntensity) : 0.0f, 0.01f);
	if (bVolumetricFogEnabled)
	{
		CacheColorValue(EArcTimeOfDayCachedProperty::VolumetricFogAlbedo, ThreeColorTimeBlend(VolumetricFogAlbedoDay, VolumetricFogAlbedoDawnDusk, VolumetricFogAlbedoNight), 0.001f);
		const float RainExtinction = FMath::Pow(UDWRainValue.GetClampedValue(0.0f, 10.0f) / 10.0f, 3.0f) * static_cast<float>(ExtraFogExtinctionRainy);
		const float SnowExtinction = FMath::Square(UDWSnowValue.GetClampedValue(0.0f, 10.0f) / 10.0f) * static_cast<float>(ExtraFogExtinctionSnowy);
		const float GlobalExtinction = (static_cast<float>(VolumetricFogExtinction) * FogDensity + static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0) * ExtraFogExtinctionDusty) + RainExtinction + SnowExtinction) / 20.0f;
		CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricFogGlobalMatExtinction, GlobalExtinction, 0.00001f);
		const float WeatherGroundExtinction = FMath::Max(UDWRainValue.GetClampedValue(0.0f, 10.0f) * static_cast<float>(GroundFogExtinctionRainy), UDWSnowValue.GetClampedValue(0.0f, 10.0f) * static_cast<float>(GroundFogExtinctionSnowy)) / 10.0f;
		const float GroundFogExtinction = FMath::Max(FMath::Max3(FMath::Square(MapRangeClamped(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)), 3.0f, 10.0f, 0.0f, 1.0f)) * static_cast<float>(GroundFogExtinctionFoggy), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0) * GroundFogExtinctionDusty), WeatherGroundExtinction), static_cast<float>(GroundFogExtinctionManual));
		CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricFogGroundFogExtinction, bRenderGroundFog ? GroundFogExtinction : 0.0f, 0.000001f);
	}
	SkySphere->SetCastShadow(false);
	SkySphere->bCastDynamicShadow = false;
	SkySphere->bCastStaticShadow = false;
	ApplyExposure();

	if (SunLightComponent) SunLightComponent->MarkRenderStateDirty();
	if (MoonLightComponent) MoonLightComponent->MarkRenderStateDirty();
	if (USkyLightComponent* SkyLight = GetSkyLightComponent()) SkyLight->MarkRenderStateDirty();
	SkyAtmosphere->MarkRenderStateDirty();
	if (FogComponent) FogComponent->MarkRenderStateDirty();
	SkySphere->MarkRenderStateDirty();
}

void AArcTimeOfDayActor::ApplySkyMaterialGroup(const int32 CacheGroup)
{
	if (!SkySphereMID)
	{
		return;
	}

	if (CacheGroup == 0)
	{
		const FVector MoonXAxis = CachedMoonAlignment;
		const FLinearColor MoonXAxisColor(MoonXAxis.X, MoonXAxis.Y, MoonXAxis.Z, 1.0f);
		CacheColorValue(EArcTimeOfDayCachedProperty::MoonXAxisColor, MoonXAxisColor, 0.0001f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::StarsOrbit, CachedStarsOrbit, 0.000001f);
		CacheColorValue(EArcTimeOfDayCachedProperty::StarsOrientation, CachedStarsOrientation, 0.0001f);
	}
	else if (CacheGroup == 2)
	{
		SkySphereMID->SetScalarParameterValue(TEXT("Extra_Contrast"), static_cast<float>(Contrast));
		SkySphereMID->SetScalarParameterValue(TEXT("Night Sky Glow Reflection Intensity"), static_cast<float>(SkyGlowReflectionIntensity));
		SkySphereMID->SetScalarParameterValue(TEXT("Sun Softness"), static_cast<float>(SunSoftness));
		SkySphereMID->SetScalarParameterValue(TEXT("Cancel Dbuffer"), 1.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Moon Blocks Space Layer"), 1.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Space Brightness"), 0.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Stars_Tiling"), static_cast<float>(StarsTiling));
		SkySphereMID->SetScalarParameterValue(TEXT("Stars Speed"), static_cast<float>(TilingStarsConstantSpeed));
		SkySphereMID->SetScalarParameterValue(TEXT("Time of Day Stars Movement"), static_cast<float>(TilingStarsMoveWithTimeOfDay));
		SkySphereMID->SetScalarParameterValue(TEXT("Real_Star_Movement"), bSimulateRealStars ? 1.0f : 0.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Sun Angle"), 0.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Twinkle Floor"), 1.0f - static_cast<float>(TwinkleAmount));
		SkySphereMID->SetScalarParameterValue(TEXT("Twinkle Strength"), static_cast<float>(TwinkleAmount) * 5.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Twinkle Speed"), static_cast<float>(TwinkleSpeed) * 0.025f);
		SkySphereMID->SetScalarParameterValue(TEXT("Disable Real Stars Sprites"), bSimulateRealStars && bRealStarsFadeToSpritesWhenZoomed ? 0.0f : 1.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Cloud Density"), CloudCoverage03 * 1.4f);
		const float SkyIntensity = static_cast<float>(OverallIntensity) * FMath::Lerp(0.375f, 0.45f, CachedNightFilter);
		CacheFloatValue(EArcTimeOfDayCachedProperty::SkyOverallIntensity, SkyIntensity, 0.000001f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::SkyLitIntensity, 0.8f, 0.000001f);
		const float SkySaturation = 1.0f - static_cast<float>(Saturation);
		CacheFloatValue(EArcTimeOfDayCachedProperty::SkySaturation, SkySaturation, 0.01f);
		const float AppliedStarsIntensity = FMath::Lerp(static_cast<float>(StarsDaytimeIntensity), static_cast<float>(StarsIntensity) * (1.0f - LocalOvercast01), CachedNightFilter);
		CacheColorValue(EArcTimeOfDayCachedProperty::StarsColor, StarsColor * AppliedStarsIntensity, 0.005f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::StarsPhase, static_cast<float>(TilingStarsPhase), 0.0003f);
		SkySphereMID->SetTextureParameterValue(TEXT("Stars_Texture"), StarsTexture);
		SkySphereMID->SetTextureParameterValue(TEXT("Real Stars Texture"), RealStarsTexture);
	}
	else if (CacheGroup == 3)
	{
		const FLinearColor NightSkyGlowValue = CachedNightSkyGlow + CachedLightPollution * FLinearColor(0.77725f, 0.703f, 1.0f, 1.0f) * 0.0055f;
		CacheColorValue(EArcTimeOfDayCachedProperty::NightSkyGlowColor, NightSkyGlowValue, 0.0001f);
	}
	else if (CacheGroup == 6)
	{
		SkySphereMID->SetScalarParameterValue(TEXT("Enable Moon Gradient"), 1.0f);
		SkySphereMID->SetTextureParameterValue(TEXT("Moon_Texture"), MoonTexture);
		SkySphereMID->SetTextureParameterValue(TEXT("Moon_Normal"), MoonNormalTexture);
		SkySphereMID->SetScalarParameterValue(TEXT("Moon Glow Scale"), FMath::Min(static_cast<float>(MoonScale), 4.0f) / 4.7f * static_cast<float>(MoonGlowScale));
		SkySphereMID->SetScalarParameterValue(TEXT("Moon_Earthlight"), bRenderMoonPhases ? static_cast<float>(DarkSideBrightness) : 1.0f);
		const float MoonLit = CachedMoonLitPercent;
		const FVector MoonPhaseVector = CurrentMoonPhaseAngle();
		const FLinearColor MoonPhaseColor(MoonPhaseVector.X, MoonPhaseVector.Y, MoonPhaseVector.Z, 1.0f);
		const float MoonMaterialIntensity = bRenderMoon ? FMath::Lerp(static_cast<float>(MoonTextureIntensityDay), static_cast<float>(MoonTextureIntensityNight), CachedNightFilter) : 0.0f;
		CacheFloatValue(EArcTimeOfDayCachedProperty::MoonScale, CachedMoonScale, 0.000001f);
		CacheColorValue(EArcTimeOfDayCachedProperty::MoonPhaseAngle, MoonPhaseColor, 0.0001f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::MoonMaterialIntensity, MoonMaterialIntensity, 0.00001f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::MoonLitPercent, MoonLit, 0.001f);
		CacheColorValue(EArcTimeOfDayCachedProperty::MoonMaterialColor, MoonMaterialColor, 0.0001f);
	}
}

void AArcTimeOfDayActor::ApplyExposure()
{
	if (!GeneralPostProcess)
	{
		return;
	}
	if (!bApplyExposureSettings)
	{
		return;
	}
	GeneralPostProcess->Settings.bOverride_AutoExposureMethod = true;
	GeneralPostProcess->Settings.AutoExposureMethod = ExposureMeteringMode;
	GeneralPostProcess->Settings.bOverride_AutoExposureBias = true;
	GeneralPostProcess->Settings.AutoExposureBias = CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::ExposureBias)].FloatValue;
	GeneralPostProcess->Settings.bOverride_AutoExposureBiasCurve = true;
	GeneralPostProcess->Settings.AutoExposureBiasCurve = SunLightIntensity > 10000.0 ? ExposureCompensationCurvePhysical : ExposureCompensationCurve;
	GeneralPostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
	GeneralPostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
	GeneralPostProcess->Settings.AutoExposureMinBrightness = ExposureBrightnessRange.GetLowerBoundValue();
	GeneralPostProcess->Settings.AutoExposureMaxBrightness = ExposureBrightnessRange.GetUpperBoundValue();
}
