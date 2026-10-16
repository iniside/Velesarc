// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"

namespace ArcTimeOfDayCache
{
	constexpr int32 CacheGroupCount = 10;
}

void AArcTimeOfDayActor::HardResetCache()
{
	for (FArcTimeOfDayCachedValue& Value : CachedPropertyValues)
	{
		Value = FArcTimeOfDayCachedValue();
	}
	for (bool& bGroupChanged : ChangedCacheGroups)
	{
		bGroupChanged = false;
	}
	bFillingStartingCache = true;
}

void AArcTimeOfDayActor::MonitorForChanges()
{
	static constexpr float InstantChangeThreshold = 100.0f;
	const float CurrentOffset = FMath::Abs(static_cast<float>(TimeOfDay - LastFrameTimeOfDay));
	LastFrameTimeOfDay = TimeOfDay;
	if (CurrentOffset > InstantChangeThreshold && CurrentOffset < 2390.0f)
	{
		HardResetCache();
	}
}

void AArcTimeOfDayActor::CacheFloatValue(const EArcTimeOfDayCachedProperty Property, const float Value, const float Tolerance)
{
	FArcTimeOfDayCachedValue& CachedValue = CachedPropertyValues[static_cast<int32>(Property)];
	const bool bChanged = !CachedValue.bInitialized || bFillingStartingCache || !FMath::IsNearlyEqual(CachedValue.FloatValue, Value, Tolerance);
	CachedValue.FloatValue = Value;
	CachedValue.bInitialized = true;
	CachedValue.bReached = true;
	if (bChanged)
	{
		CachedValue.bPending = true;
		ChangedCacheGroups[GetCacheGroup(Property)] = true;
	}
}

void AArcTimeOfDayActor::CacheColorValue(const EArcTimeOfDayCachedProperty Property, const FLinearColor& Value, const float Tolerance)
{
	FArcTimeOfDayCachedValue& CachedValue = CachedPropertyValues[static_cast<int32>(Property)];
	const bool bChanged = !CachedValue.bInitialized || bFillingStartingCache || !CachedValue.ColorValue.Equals(Value, Tolerance);
	CachedValue.ColorValue = Value;
	CachedValue.bInitialized = true;
	CachedValue.bReached = true;
	if (bChanged)
	{
		CachedValue.bPending = true;
		ChangedCacheGroups[GetCacheGroup(Property)] = true;
	}
}

void AArcTimeOfDayActor::ApplyCachedProperty(const EArcTimeOfDayCachedProperty Property)
{
	const FArcTimeOfDayCachedValue& Value = CachedPropertyValues[static_cast<int32>(Property)];
	switch (Property)
	{
	case EArcTimeOfDayCachedProperty::CloudWispsOpacity:
		SkySphereMID->SetScalarParameterValue(TEXT("Wispy Cloud Alpha"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonScale:
		SkySphereMID->SetScalarParameterValue(TEXT("Moon Scale"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonPhaseAngle:
		SkySphereMID->SetVectorParameterValue(TEXT("Moon_Phase_Angle"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonMaterialIntensity:
		SkySphereMID->SetScalarParameterValue(TEXT("Moon Intensity"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonLitPercent:
		SkySphereMID->SetScalarParameterValue(TEXT("Moon_Lit_Percent"), Value.FloatValue);
		SkySphereMID->SetScalarParameterValue(TEXT("Moon Glow Intensity"), Value.FloatValue * static_cast<float>(MoonGlowIntensity));
		break;
	case EArcTimeOfDayCachedProperty::StarsOrbit:
		SkySphereMID->SetScalarParameterValue(TEXT("Stars Orbit"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::StarsOrientation:
		SkySphereMID->SetVectorParameterValue(TEXT("Stars Orientation"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::SunRootVector:
		SunWorldRotation = FRotationMatrix::MakeFromXZ(CachedSunVector, CachedSunZVector).Rotator();
		SunParent->SetWorldRotation(SunWorldRotation);
		if (CustomSunLightActor)
		{
			CustomSunLightActor->SetActorRotation(SunWorldRotation);
		}
		else
		{
			Sun->SetWorldRotation(SunWorldRotation);
		}
		SetWeatherVector(TEXT("Sun Vector"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonRootVector:
		MoonWorldRotation = FRotationMatrix::MakeFromXZ(CachedMoonVector, CachedMoonZVector).Rotator();
		MoonParent->SetWorldRotation(MoonWorldRotation);
		if (CustomMoonLightActor)
		{
			CustomMoonLightActor->SetActorRotation(MoonWorldRotation);
		}
		else
		{
			Moon->SetWorldRotation(MoonWorldRotation);
		}
		SetWeatherVector(TEXT("Moon Vector"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::CloudShadowsLightVector:
		SetCloudVector(TEXT("Cloud Shadows Light Vector"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::CloudShadowsCancel:
		SetCloudScalar(TEXT("Cloud Shadows Cancel"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonXAxisColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Moon X Axis"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::ShadingOffsetVector:
		SkySphereMID->SetVectorParameterValue(TEXT("Shading Offset Vector"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::NightSkyGlowColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Night Sky Glow Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyOverallIntensity:
		SkySphereMID->SetScalarParameterValue(TEXT("Overall Intensity"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyLitIntensity:
		SkySphereMID->SetScalarParameterValue(TEXT("Lit_Intensity"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SkySaturation:
		SkySphereMID->SetScalarParameterValue(TEXT("Saturation"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::ExposureBias:
		if (bApplyExposureSettings && GeneralPostProcess) GeneralPostProcess->Settings.AutoExposureBias = Value.FloatValue;
		break;
	case EArcTimeOfDayCachedProperty::SkyBaseColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Sky Base Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::CloudLightColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Cloud Light Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::CloudDarkColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Cloud Dark Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonScatteringColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Moon Scattering Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::StarsColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Stars Color"), Value.ColorValue);
		if (bRealStarsSpritesActive && RealStarsSprites) RealStarsSprites->SetVisibility(!FVector(Value.ColorValue.R, Value.ColorValue.G, Value.ColorValue.B).IsZero(), false);
		break;
	case EArcTimeOfDayCachedProperty::AuroraIntensity:
		if (SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora)
		{
			if (VolumetricAuroraMID) VolumetricAuroraMID->SetScalarParameterValue(TEXT("Aurora Intensity"), Value.FloatValue);
			VolumetricAurora->SetHiddenInGame(Value.FloatValue < 0.0001f);
#if WITH_EDITOR
			VolumetricAurora->SetIsTemporarilyHiddenInEditor(Value.FloatValue < 0.0001f);
#endif
		}
		else
		{
			SkySphereMID->SetScalarParameterValue(TEXT("Aurora Intensity"), Value.FloatValue);
			SkySphereMID->SetScalarParameterValue(TEXT("Aurora Max Samples"), Value.FloatValue > 0.0f ? static_cast<float>(TwoDAuroraMaxSamples) : 0.1f);
		}
		break;
	case EArcTimeOfDayCachedProperty::ShineIntensity:
		SkySphereMID->SetScalarParameterValue(TEXT("Shine Intensity"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunHighlightRadius:
		SkySphereMID->SetScalarParameterValue(TEXT("Sun Highlight Radius"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunHighlightIntensity:
		SkySphereMID->SetScalarParameterValue(TEXT("Sun_Highlight_Intensity"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunLightIntensity:
		if (UDirectionalLightComponent* Light = GetSunLightComponent()) Light->SetIntensity(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunLightCastsShadows:
		if (UDirectionalLightComponent* Light = GetSunLightComponent())
		{
			const bool bActive = Value.FloatValue > 0.0f;
			Light->SetCastShadows(bActive && bSunCastsShadows);
			Light->SetLightingChannels(bActive && bSunLightingChannel0, bActive && bSunLightingChannel1, bActive && bSunLightingChannel2);
			if (Light->bEnableLightShaftBloom) Light->SetBloomScale(static_cast<float>(SunLightShaftBloomScale) * Value.FloatValue);
		}
		break;
	case EArcTimeOfDayCachedProperty::SunSpecularScale:
		if (UDirectionalLightComponent* Light = GetSunLightComponent()) Light->SetSpecularScale(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunDisabledBrightness:
		if (UDirectionalLightComponent* Light = GetSunLightComponent()) Light->SetLightFunctionDisabledBrightness(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunLightColor:
		if (UDirectionalLightComponent* Light = GetSunLightComponent()) Light->SetLightColor(Value.ColorValue, true);
		break;
	case EArcTimeOfDayCachedProperty::SunLightSourceAngle:
		if (UDirectionalLightComponent* Light = GetSunLightComponent())
		{
			Light->SetLightSourceAngle(Value.FloatValue);
			Light->SetLightSourceSoftAngle(Value.FloatValue * static_cast<float>(SunSourceAngleSoftness));
		}
		break;
	case EArcTimeOfDayCachedProperty::FogInscatteringColor:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetFogInscatteringColor(Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::FogDirectionalInscatteringColor:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetDirectionalInscatteringColor(Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyAmbientColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Ambient Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::FogDensity:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetFogDensity(Value.FloatValue);
		SetCloudScalar(TEXT("Fog Density"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::FogSkyAtmosphereContribution:
		if (bControlSkyAtmosphereSettings) SkyAtmosphere->SetHeightFogContribution(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::FogHeightFalloff:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetFogHeightFalloff(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::FogStartDistance:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetStartDistance(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::AmbientFogColor:
		SetWeatherVector(TEXT("Ambient Fog Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::FilteredMoonLightIntensity:
		if (UDirectionalLightComponent* Light = GetMoonLightComponent()) Light->SetIntensity(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonCastingShadows:
		if (UDirectionalLightComponent* Light = GetMoonLightComponent())
		{
			const bool bActive = Value.FloatValue > 0.5f;
			Light->SetCastShadows(bActive && bMoonCastsShadows);
			Light->SetLightingChannels(bActive && bMoonLightingChannel0, bActive && bMoonLightingChannel1, bActive && bMoonLightingChannel2);
			if (Light->bEnableLightShaftBloom) Light->SetBloomScale(static_cast<float>(MoonLightShaftBloomScale) * Value.FloatValue);
		}
		break;
	case EArcTimeOfDayCachedProperty::MoonLightSpecularScale:
		if (UDirectionalLightComponent* Light = GetMoonLightComponent()) Light->SetSpecularScale(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonLightSourceAngle:
		if (UDirectionalLightComponent* Light = GetMoonLightComponent())
		{
			Light->SetLightSourceAngle(Value.FloatValue);
			Light->SetLightSourceSoftAngle(Value.FloatValue * static_cast<float>(MoonSourceAngleSoftness));
		}
		break;
	case EArcTimeOfDayCachedProperty::SkyLightColor:
		if (USkyLightComponent* SkyLight = GetSkyLightComponent()) SkyLight->SetLightColor(Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyLightIntensity:
		if (USkyLightComponent* SkyLight = GetSkyLightComponent()) SkyLight->SetIntensity(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyLightLowerHemisphereColor:
		if (USkyLightComponent* SkyLight = GetSkyLightComponent()) SkyLight->SetLowerHemisphereColor(Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyAtmosphereLuminance:
		if (bControlSkyAtmosphereSettings) SkyAtmosphere->SetSkyLuminanceFactor(Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::MieAnisotropy:
		if (bControlSkyAtmosphereSettings) SkyAtmosphere->SetMieAnisotropy(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MieScatteringScale:
		if (bControlSkyAtmosphereSettings) SkyAtmosphere->SetMieScatteringScale(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::RayleighScatteringColor:
		if (bControlSkyAtmosphereSettings) SkyAtmosphere->SetRayleighScattering(Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::OvercastTurbulenceStrength:
		SetCloudScalar(TEXT("2D Overcast Turbulence"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudAlbedo:
		SetCloudVector(TEXT("Albedo"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudMaxTraceDistance:
		VolumetricCloud->SetTracingMaxDistance(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MultiscatteringOcclusion:
		SetCloudScalar(TEXT("MultiScattering Occlusion"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MacroVariation:
		SetCloudScalar(TEXT("Macro Variation"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudLerpToSimplified:
		SetCloudScalar(TEXT("Lerp to Simplified"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::StaticCloudsLightingMask:
		SkySphereMID->SetVectorParameterValue(TEXT("Static Clouds Lighting Mask"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationClear:
		SetCloudScalar(TEXT("Floor Variation Clear"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationColor:
		SetCloudScalar(TEXT("Floor Variation Color"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyMaterialContrastMidpoint:
		SkySphereMID->SetScalarParameterValue(TEXT("Contrast Midpoint"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::CloudShadowTraceDistance:
		VolumetricCloud->SetShadowTracingDistance(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudViewSampleScale:
		VolumetricCloud->SetViewSampleCountScale(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::DirectionalInscatteringExponent:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetDirectionalInscatteringExponent(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::CloudWispsGradientVector:
		SkySphereMID->SetVectorParameterValue(TEXT("Cloud Wisps Gradient"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::SpaceLayerBrightness:
		SkySphereMID->SetScalarParameterValue(TEXT("Space Brightness"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudMultiscatteringPhase1:
		SetCloudScalar(TEXT("PhaseG"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunScatteringColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Sun Scattering Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::VolCloudInnerEmitLimit:
		SetCloudScalar(TEXT("Inner Emit Limit"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SkyAtmoAbsorptionColor:
		if (bControlSkyAtmosphereSettings)
		{
			SkyAtmosphere->SetOtherAbsorption(Value.ColorValue);
			SkyAtmosphere->SetOtherAbsorptionScale(Value.ColorValue.A);
		}
		break;
	case EArcTimeOfDayCachedProperty::CloudWispsColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Cloud Wisps Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::MultiscatteringIntensity:
		SetCloudScalar(TEXT("MultiScattering Contribution"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationCloudy:
		SetCloudScalar(TEXT("Floor Variation Cloudy"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudLightRaysColor:
		if (VolumetricCloudLightRays)
		{
			VolumetricCloudLightRays->SetVisibility(bUsingVolumetricLightRays && Value.ColorValue.A > 0.0f, false);
			VolumetricCloudLightRays->SetVariableFloat(TEXT("Ray Length"), MaxVolumetricCloudLightRayLength * Value.ColorValue.A);
			VolumetricCloudLightRays->SetVariableLinearColor(TEXT("Light Color"), Value.ColorValue);
		}
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudSkyAtmoContribution:
		SetCloudScalar(TEXT("Ambient Occlusion"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::StaticCloudsTintColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Static Clouds Color Tint"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::StaticCloudsShadowColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Static Clouds Shadow Tint"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricFogAlbedo:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetVolumetricFogAlbedo(Value.ColorValue.ToFColor(false));
		if (GlobalVolumetricFogMID) GlobalVolumetricFogMID->SetVectorParameterValue(TEXT("Albedo"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::HighFrequencyNoiseAmount:
		SetCloudScalar(TEXT("High Frequency Noise"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::SunVolumetricScatteringIntensity:
		if (UDirectionalLightComponent* Light = GetSunLightComponent()) Light->SetVolumetricScatteringIntensity(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonVolumetricScatteringIntensity:
		if (UDirectionalLightComponent* Light = GetMoonLightComponent()) Light->SetVolumetricScatteringIntensity(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricFogGlobalMatExtinction:
		if (GlobalVolumetricFogMID) GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Global Extinction"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricFogGroundFogExtinction:
		if (GlobalVolumetricFogMID) GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Ground Fog Extinction"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::FogMaxOpacity:
		if (UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent()) FogComponent->SetFogMaxOpacity(Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::PhaseBlend:
		SetCloudScalar(TEXT("Phase Blend"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::VolumetricCloudMaterialSwitch:
		VolumetricCloud->SetMaterial(Value.FloatValue > 0.5f ? VolumetricCloudMIDSimple : VolumetricCloudMIDComplex);
		break;
	case EArcTimeOfDayCachedProperty::TwoDCloudTint:
		SkySphereMID->SetVectorParameterValue(TEXT("2D Cloud Tint"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::StaticCloudsDynamicRotation:
		SkySphereMID->SetScalarParameterValue(TEXT("Static Clouds Rotation"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::AuroraPhase:
		if (SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora)
		{
			if (VolumetricAuroraMID) VolumetricAuroraMID->SetScalarParameterValue(TEXT("Aurora Phase"), Value.FloatValue);
		}
		else
		{
			SkySphereMID->SetScalarParameterValue(TEXT("Aurora Phase"), Value.FloatValue);
		}
		break;
	case EArcTimeOfDayCachedProperty::StarsPhase:
		SkySphereMID->SetScalarParameterValue(TEXT("Stars Phase"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonMaterialColor:
		SkySphereMID->SetVectorParameterValue(TEXT("Moon Color"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::PaintedCloudCoverageOpacityLayer1:
		SetCloudScalar(TEXT("Cloud Coverage Target Opacity"), Value.FloatValue);
		break;
	case EArcTimeOfDayCachedProperty::EnableLensFlarePostProcess:
		if (LensFlarePostProcess) LensFlarePostProcess->bEnabled = Value.FloatValue > 0.5f;
		break;
	case EArcTimeOfDayCachedProperty::CloudWispsMovementVector:
		SkySphereMID->SetVectorParameterValue(TEXT("Cloud Wisps Movement Vector"), Value.ColorValue);
		break;
	case EArcTimeOfDayCachedProperty::MoonAtmosphereLightIndex:
		if (UDirectionalLightComponent* Light = GetMoonLightComponent()) Light->SetAtmosphereSunLightIndex(FMath::CeilToInt(Value.FloatValue));
		break;
	default:
		break;
	}
}

void AArcTimeOfDayActor::RetargetSkyLightCachedProperties()
{
	CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SkyLightColor)].bReached = true;
	CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SkyLightIntensity)].bReached = true;
	CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::SkyLightLowerHemisphereColor)].bReached = true;
}

int32 AArcTimeOfDayActor::GetCacheGroup(const EArcTimeOfDayCachedProperty Property)
{
	if (Property == EArcTimeOfDayCachedProperty::StarsOrbit || Property == EArcTimeOfDayCachedProperty::StarsOrientation || Property == EArcTimeOfDayCachedProperty::SunRootVector || Property == EArcTimeOfDayCachedProperty::MoonRootVector || Property == EArcTimeOfDayCachedProperty::MoonXAxisColor) return 0;
	if (Property == EArcTimeOfDayCachedProperty::CloudShadowsLightVector || Property == EArcTimeOfDayCachedProperty::CloudShadowsCancel) return 1;
	if (Property == EArcTimeOfDayCachedProperty::CloudWispsOpacity || Property == EArcTimeOfDayCachedProperty::SkyOverallIntensity || Property == EArcTimeOfDayCachedProperty::SkyLitIntensity || Property == EArcTimeOfDayCachedProperty::SkySaturation || Property == EArcTimeOfDayCachedProperty::StarsColor || Property == EArcTimeOfDayCachedProperty::AuroraIntensity || Property == EArcTimeOfDayCachedProperty::FogDensity || Property == EArcTimeOfDayCachedProperty::FogHeightFalloff || Property == EArcTimeOfDayCachedProperty::FogStartDistance || Property == EArcTimeOfDayCachedProperty::CloudWispsGradientVector || Property == EArcTimeOfDayCachedProperty::SpaceLayerBrightness || Property == EArcTimeOfDayCachedProperty::FogMaxOpacity || Property == EArcTimeOfDayCachedProperty::AuroraPhase || Property == EArcTimeOfDayCachedProperty::StarsPhase || Property == EArcTimeOfDayCachedProperty::CloudWispsMovementVector) return 2;
	if (Property == EArcTimeOfDayCachedProperty::NightSkyGlowColor || Property == EArcTimeOfDayCachedProperty::FogInscatteringColor || Property == EArcTimeOfDayCachedProperty::FogDirectionalInscatteringColor || Property == EArcTimeOfDayCachedProperty::SkyAmbientColor || Property == EArcTimeOfDayCachedProperty::AmbientFogColor || Property == EArcTimeOfDayCachedProperty::SkyMaterialContrastMidpoint || Property == EArcTimeOfDayCachedProperty::DirectionalInscatteringExponent || Property == EArcTimeOfDayCachedProperty::CloudWispsColor || Property == EArcTimeOfDayCachedProperty::VolumetricFogAlbedo || Property == EArcTimeOfDayCachedProperty::VolumetricFogGlobalMatExtinction || Property == EArcTimeOfDayCachedProperty::VolumetricFogGroundFogExtinction) return 3;
	if (Property == EArcTimeOfDayCachedProperty::SkyBaseColor || Property == EArcTimeOfDayCachedProperty::CloudLightColor || Property == EArcTimeOfDayCachedProperty::CloudDarkColor || Property == EArcTimeOfDayCachedProperty::MoonScatteringColor || Property == EArcTimeOfDayCachedProperty::FogSkyAtmosphereContribution || Property == EArcTimeOfDayCachedProperty::SkyAtmosphereLuminance || Property == EArcTimeOfDayCachedProperty::MieAnisotropy || Property == EArcTimeOfDayCachedProperty::MieScatteringScale || Property == EArcTimeOfDayCachedProperty::RayleighScatteringColor || Property == EArcTimeOfDayCachedProperty::SunScatteringColor || Property == EArcTimeOfDayCachedProperty::SkyAtmoAbsorptionColor) return 4;
	if (Property == EArcTimeOfDayCachedProperty::SunLightIntensity || Property == EArcTimeOfDayCachedProperty::SunLightCastsShadows || Property == EArcTimeOfDayCachedProperty::SunSpecularScale || Property == EArcTimeOfDayCachedProperty::SunDisabledBrightness || Property == EArcTimeOfDayCachedProperty::SunLightColor || Property == EArcTimeOfDayCachedProperty::SunLightSourceAngle || Property == EArcTimeOfDayCachedProperty::SunVolumetricScatteringIntensity) return 5;
	if (Property == EArcTimeOfDayCachedProperty::MoonScale || Property == EArcTimeOfDayCachedProperty::MoonPhaseAngle || Property == EArcTimeOfDayCachedProperty::MoonMaterialIntensity || Property == EArcTimeOfDayCachedProperty::MoonLitPercent || Property == EArcTimeOfDayCachedProperty::FilteredMoonLightIntensity || Property == EArcTimeOfDayCachedProperty::MoonCastingShadows || Property == EArcTimeOfDayCachedProperty::MoonLightSpecularScale || Property == EArcTimeOfDayCachedProperty::MoonLightSourceAngle || Property == EArcTimeOfDayCachedProperty::MoonVolumetricScatteringIntensity || Property == EArcTimeOfDayCachedProperty::MoonMaterialColor || Property == EArcTimeOfDayCachedProperty::MoonAtmosphereLightIndex) return 6;
	if (Property == EArcTimeOfDayCachedProperty::ExposureBias || Property == EArcTimeOfDayCachedProperty::SkyLightColor || Property == EArcTimeOfDayCachedProperty::SkyLightIntensity || Property == EArcTimeOfDayCachedProperty::SkyLightLowerHemisphereColor || Property == EArcTimeOfDayCachedProperty::EnableLensFlarePostProcess) return 7;
	if (Property == EArcTimeOfDayCachedProperty::ShadingOffsetVector || Property == EArcTimeOfDayCachedProperty::ShineIntensity || Property == EArcTimeOfDayCachedProperty::SunHighlightRadius || Property == EArcTimeOfDayCachedProperty::SunHighlightIntensity || Property == EArcTimeOfDayCachedProperty::VolumetricCloudAlbedo || Property == EArcTimeOfDayCachedProperty::VolumetricCloudMaxTraceDistance || Property == EArcTimeOfDayCachedProperty::MultiscatteringOcclusion || Property == EArcTimeOfDayCachedProperty::MacroVariation || Property == EArcTimeOfDayCachedProperty::VolumetricCloudLerpToSimplified || Property == EArcTimeOfDayCachedProperty::StaticCloudsLightingMask || Property == EArcTimeOfDayCachedProperty::StaticCloudsTintColor || Property == EArcTimeOfDayCachedProperty::StaticCloudsShadowColor || Property == EArcTimeOfDayCachedProperty::HighFrequencyNoiseAmount || Property == EArcTimeOfDayCachedProperty::PhaseBlend || Property == EArcTimeOfDayCachedProperty::VolumetricCloudMaterialSwitch || Property == EArcTimeOfDayCachedProperty::TwoDCloudTint || Property == EArcTimeOfDayCachedProperty::StaticCloudsDynamicRotation) return 8;
	return FMath::Clamp(9, 0, ArcTimeOfDayCache::CacheGroupCount - 1);
}

EArcTimeOfDayWriterGroup AArcTimeOfDayActor::GetWriterGroup(const EArcTimeOfDayCachedProperty Property)
{
	if (Property == EArcTimeOfDayCachedProperty::SunRootVector || Property == EArcTimeOfDayCachedProperty::MoonRootVector || Property == EArcTimeOfDayCachedProperty::CloudShadowsLightVector || Property == EArcTimeOfDayCachedProperty::SkyOverallIntensity || Property == EArcTimeOfDayCachedProperty::SunLightIntensity || Property == EArcTimeOfDayCachedProperty::FogSkyAtmosphereContribution || Property == EArcTimeOfDayCachedProperty::FilteredMoonLightIntensity || Property == EArcTimeOfDayCachedProperty::SkyLightIntensity || Property == EArcTimeOfDayCachedProperty::SkyAtmosphereLuminance || Property == EArcTimeOfDayCachedProperty::AuroraPhase) return EArcTimeOfDayWriterGroup::Max;
	if (Property == EArcTimeOfDayCachedProperty::StarsOrbit || Property == EArcTimeOfDayCachedProperty::MoonXAxisColor) return EArcTimeOfDayWriterGroup::High1;
	if (Property == EArcTimeOfDayCachedProperty::SkyLitIntensity || Property == EArcTimeOfDayCachedProperty::VolumetricCloudLightRaysColor || Property == EArcTimeOfDayCachedProperty::StarsPhase) return EArcTimeOfDayWriterGroup::High2;
	if (Property == EArcTimeOfDayCachedProperty::SunLightColor || Property == EArcTimeOfDayCachedProperty::FogInscatteringColor || Property == EArcTimeOfDayCachedProperty::FogDirectionalInscatteringColor || Property == EArcTimeOfDayCachedProperty::SkyAmbientColor || Property == EArcTimeOfDayCachedProperty::FogDensity || Property == EArcTimeOfDayCachedProperty::FogStartDistance || Property == EArcTimeOfDayCachedProperty::AmbientFogColor || Property == EArcTimeOfDayCachedProperty::MoonLightSpecularScale || Property == EArcTimeOfDayCachedProperty::CloudWispsColor || Property == EArcTimeOfDayCachedProperty::MoonVolumetricScatteringIntensity || Property == EArcTimeOfDayCachedProperty::FogMaxOpacity) return EArcTimeOfDayWriterGroup::High3;
	if (Property == EArcTimeOfDayCachedProperty::CloudLightColor || Property == EArcTimeOfDayCachedProperty::SkyLightColor || Property == EArcTimeOfDayCachedProperty::SkyLightLowerHemisphereColor || Property == EArcTimeOfDayCachedProperty::MacroVariation || Property == EArcTimeOfDayCachedProperty::VolumetricCloudLerpToSimplified || Property == EArcTimeOfDayCachedProperty::SunScatteringColor || Property == EArcTimeOfDayCachedProperty::HighFrequencyNoiseAmount) return EArcTimeOfDayWriterGroup::High4;
	if (Property == EArcTimeOfDayCachedProperty::StarsOrientation || Property == EArcTimeOfDayCachedProperty::CloudShadowsCancel || Property == EArcTimeOfDayCachedProperty::SunHighlightIntensity || Property == EArcTimeOfDayCachedProperty::VolumetricCloudAlbedo || Property == EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationClear || Property == EArcTimeOfDayCachedProperty::VolumetricCloudSkyAtmoContribution || Property == EArcTimeOfDayCachedProperty::StaticCloudsDynamicRotation) return EArcTimeOfDayWriterGroup::Low1;
	if (Property == EArcTimeOfDayCachedProperty::MoonScale || Property == EArcTimeOfDayCachedProperty::MoonPhaseAngle || Property == EArcTimeOfDayCachedProperty::MoonMaterialIntensity || Property == EArcTimeOfDayCachedProperty::MoonLitPercent || Property == EArcTimeOfDayCachedProperty::ShineIntensity || Property == EArcTimeOfDayCachedProperty::VolumetricCloudMaxTraceDistance || Property == EArcTimeOfDayCachedProperty::StaticCloudsLightingMask || Property == EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationCloudy || Property == EArcTimeOfDayCachedProperty::MoonMaterialColor || Property == EArcTimeOfDayCachedProperty::PaintedCloudCoverageOpacityLayer1) return EArcTimeOfDayWriterGroup::Low2;
	if (Property == EArcTimeOfDayCachedProperty::ExposureBias || Property == EArcTimeOfDayCachedProperty::SunHighlightRadius || Property == EArcTimeOfDayCachedProperty::MultiscatteringOcclusion || Property == EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationColor || Property == EArcTimeOfDayCachedProperty::SpaceLayerBrightness || Property == EArcTimeOfDayCachedProperty::StaticCloudsTintColor || Property == EArcTimeOfDayCachedProperty::EnableLensFlarePostProcess) return EArcTimeOfDayWriterGroup::Low3;
	if (Property == EArcTimeOfDayCachedProperty::CloudWispsOpacity || Property == EArcTimeOfDayCachedProperty::ShadingOffsetVector || Property == EArcTimeOfDayCachedProperty::SkySaturation || Property == EArcTimeOfDayCachedProperty::StarsColor || Property == EArcTimeOfDayCachedProperty::AuroraIntensity || Property == EArcTimeOfDayCachedProperty::OvercastTurbulenceStrength || Property == EArcTimeOfDayCachedProperty::SkyMaterialContrastMidpoint || Property == EArcTimeOfDayCachedProperty::CloudWispsGradientVector || Property == EArcTimeOfDayCachedProperty::StaticCloudsShadowColor || Property == EArcTimeOfDayCachedProperty::PhaseBlend || Property == EArcTimeOfDayCachedProperty::CloudWispsMovementVector) return EArcTimeOfDayWriterGroup::Low4;
	if (Property == EArcTimeOfDayCachedProperty::NightSkyGlowColor || Property == EArcTimeOfDayCachedProperty::FogHeightFalloff || Property == EArcTimeOfDayCachedProperty::VolumetricCloudViewSampleScale || Property == EArcTimeOfDayCachedProperty::DirectionalInscatteringExponent || Property == EArcTimeOfDayCachedProperty::VolumetricFogAlbedo || Property == EArcTimeOfDayCachedProperty::VolumetricFogGlobalMatExtinction || Property == EArcTimeOfDayCachedProperty::VolumetricFogGroundFogExtinction || Property == EArcTimeOfDayCachedProperty::VolumetricCloudMaterialSwitch || Property == EArcTimeOfDayCachedProperty::TwoDCloudTint) return EArcTimeOfDayWriterGroup::Low5;
	if (Property == EArcTimeOfDayCachedProperty::SunLightCastsShadows || Property == EArcTimeOfDayCachedProperty::SunSpecularScale || Property == EArcTimeOfDayCachedProperty::SunDisabledBrightness || Property == EArcTimeOfDayCachedProperty::SunLightSourceAngle || Property == EArcTimeOfDayCachedProperty::VolumetricCloudMultiscatteringPhase1 || Property == EArcTimeOfDayCachedProperty::SunVolumetricScatteringIntensity) return EArcTimeOfDayWriterGroup::Low6;
	if (Property == EArcTimeOfDayCachedProperty::MoonCastingShadows || Property == EArcTimeOfDayCachedProperty::MoonLightSourceAngle || Property == EArcTimeOfDayCachedProperty::VolCloudInnerEmitLimit || Property == EArcTimeOfDayCachedProperty::MoonAtmosphereLightIndex) return EArcTimeOfDayWriterGroup::Low7;
	if (Property == EArcTimeOfDayCachedProperty::SkyBaseColor || Property == EArcTimeOfDayCachedProperty::CloudDarkColor || Property == EArcTimeOfDayCachedProperty::MoonScatteringColor || Property == EArcTimeOfDayCachedProperty::MieAnisotropy || Property == EArcTimeOfDayCachedProperty::MieScatteringScale || Property == EArcTimeOfDayCachedProperty::RayleighScatteringColor || Property == EArcTimeOfDayCachedProperty::SkyAtmoAbsorptionColor) return EArcTimeOfDayWriterGroup::Low8;
	if (Property == EArcTimeOfDayCachedProperty::CloudShadowTraceDistance) return EArcTimeOfDayWriterGroup::Low4;
	return EArcTimeOfDayWriterGroup::Low8;
}
