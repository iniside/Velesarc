// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "ArcTimeOfDayModule.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"

namespace ArcTimeOfDayClock
{
	constexpr float DayUnits = 2400.0f;
}

double AArcTimeOfDayActor::GetUDSTimeOfDay() const
{
	return HoursToUDSTime(GetTimeOfDay(), GetDayLength());
}

void AArcTimeOfDayActor::SetUDSTimeOfDay(const double NewTimeOfDay)
{
	const double WrappedTime = FMath::Fmod(NewTimeOfDay, static_cast<double>(ArcTimeOfDayClock::DayUnits));
	TimeOfDay = WrappedTime < 0.0 ? WrappedTime + ArcTimeOfDayClock::DayUnits : WrappedTime;
	const float Hours = UDSTimeToHours(TimeOfDay, GetDayLength());
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		SetTimeOfDay(Hours);
	}
#if WITH_EDITOR
	else
	{
		SetTimeOfDayPreview(Hours);
		return;
	}
#endif
	RefreshFromRawDaySequenceTime();
}

float AArcTimeOfDayActor::UDSTimeToHours(const double InTimeOfDay, const float InDayLength)
{
	return static_cast<float>(FMath::Clamp(InTimeOfDay, 0.0, static_cast<double>(ArcTimeOfDayClock::DayUnits)) / ArcTimeOfDayClock::DayUnits * FMath::Max(InDayLength, UE_SMALL_NUMBER));
}

double AArcTimeOfDayActor::HoursToUDSTime(const float InHours, const float InDayLength)
{
	const float EffectiveDayLength = FMath::Max(InDayLength, UE_SMALL_NUMBER);
	const float Remainder = FMath::Fmod(InHours, EffectiveDayLength);
	const float WrappedHours = Remainder < 0.0f ? Remainder + EffectiveDayLength : Remainder;
	return static_cast<double>(WrappedHours / EffectiveDayLength * ArcTimeOfDayClock::DayUnits);
}

void AArcTimeOfDayActor::RefreshFromRawDaySequenceTime()
{
	SynchronizeTimeFromRawDaySequence();
	if (bAutomaticallyApplyCalculatedValues)
	{
		EvaluateEditorSky(false);
	}
}

FArcTimeOfDayModifierHandle AArcTimeOfDayActor::AcquireModifierHandle()
{
	for (int32 Index = 0; Index < ArcTimeOfDay::ModifierCapacity; ++Index)
	{
		if (!ModifierSlotsInUse[Index])
		{
			ModifierSlotsInUse[Index] = true;
			FArcTimeOfDayModifierHandle Handle;
			Handle.Index = Index;
			return Handle;
		}
	}
	UE_LOGF(LogArcTimeOfDay, Error, "ArcTimeOfDay modifier capacity exhausted for %ls", *GetPathName());
	return FArcTimeOfDayModifierHandle();
}

void AArcTimeOfDayActor::ReleaseModifierHandle(const FArcTimeOfDayModifierHandle Handle)
{
	if (!Handle.IsValid() || !ModifierSlotsInUse[Handle.Index])
	{
		return;
	}
	CloudCoverage.ClearModifier(Handle);
	Fog.ClearModifier(Handle);
	DustAmount.ClearModifier(Handle);
	CloudDirection.ClearModifier(Handle);
	UDWWindValue.ClearModifier(Handle);
	UDWRainValue.ClearModifier(Handle);
	UDWSnowValue.ClearModifier(Handle);
	FogSnowContribution.ClearModifier(Handle);
	ModifierSlotsInUse[Handle.Index] = false;
	RefreshSky();
}

void AArcTimeOfDayActor::ApplyWeatherValues(const FArcTimeOfDayModifierHandle Handle, const double InCloudCoverage, const double InFog, const double InDustAmount, const double InCloudDirection, const double InWind, const double InRain, const double InSnow, const double InFogSnowContribution)
{
	if (!Handle.IsValid() || !ModifierSlotsInUse[Handle.Index])
	{
		return;
	}
	CloudCoverage.SetModifier(Handle, InCloudCoverage);
	Fog.SetModifier(Handle, InFog);
	DustAmount.SetModifier(Handle, InDustAmount);
	CloudDirection.SetModifier(Handle, InCloudDirection);
	UDWWindValue.SetModifier(Handle, static_cast<float>(InWind));
	UDWRainValue.SetModifier(Handle, static_cast<float>(InRain));
	UDWSnowValue.SetModifier(Handle, static_cast<float>(InSnow));
	FogSnowContribution.SetModifier(Handle, static_cast<float>(InFogSnowContribution));
	RefreshSky();
}

FArcTimeOfDayResolvedWeatherValues AArcTimeOfDayActor::GetResolvedWeatherValues() const
{
	FArcTimeOfDayResolvedWeatherValues Values;
	Values.CloudCoverage = static_cast<float>(CloudCoverage.GetClampedValue(0.0, 10.0));
	Values.Fog = static_cast<float>(Fog.GetClampedValue(0.0, 10.0));
	Values.DustAmount = static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0));
	Values.CloudDirection = static_cast<float>(CloudDirection.GetWrappedValue(360.0));
	Values.WindIntensity = UDWWindValue.GetClampedValue(0.0f, 10.0f);
	Values.Rain = UDWRainValue.GetClampedValue(0.0f, 10.0f);
	Values.Snow = UDWSnowValue.GetClampedValue(0.0f, 10.0f);
	Values.FogSnowContribution = FMath::Max(FogSnowContribution.GetValue(), 0.0f);
	return Values;
}

void AArcTimeOfDayActor::SynchronizeTimeFromRawDaySequence()
{
	float CurrentHours = GetTimeOfDay();
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		CurrentHours = GetTimeOfDayPreview();
	}
#endif
	TimeOfDay = HoursToUDSTime(CurrentHours, GetDayLength());
}

void AArcTimeOfDayActor::EvaluateEditorSky(const bool bHardReset)
{
	if (bHardReset)
	{
		HardResetCache();
	}
	EnsureVisualResources();
	USkyLightComponent* SkyLightTarget = GetSkyLightComponent();
	UExponentialHeightFogComponent* HeightFogTarget = GetHeightFogComponent();
	if (SkyLightTarget != CachedSkyLightTarget.Get() || HeightFogTarget != CachedHeightFogTarget.Get())
	{
		CachedSkyLightTarget = SkyLightTarget;
		CachedHeightFogTarget = HeightFogTarget;
		HardResetCache();
	}
	BuildVolumetricCloudLayers();
	RefreshStaticCelestialProperties();
	UpdateCommonDerivatives();
	MonitorForChanges();
	for (bool& bGroupChanged : ChangedCacheGroups)
	{
		bGroupChanged = false;
	}
	EvaluateCachedProperties();
	ApplyWriterGroup(EArcTimeOfDayWriterGroup::Immediate);
	if (!bFillingStartingCache)
	{
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Max);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::High1);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::High2);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::High3);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::High4);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low1);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low2);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low3);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low4);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low5);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low6);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low7);
		ApplyWriterGroup(EArcTimeOfDayWriterGroup::Low8);
	}
	bFillingStartingCache = false;
}

void AArcTimeOfDayActor::EvaluateCachedProperties()
{
	SetTimeCycleDegrees();
	CacheSunAndMoonOrientation();
	CachedSunLightColor = SunLightColor * CachedSolarEclipseTint;
	CachedMoonLitPercent = CurrentMoonLitPercent();
	CachedNightFilter = NightFilter();
	const float DirectionalCloudScale = MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.1f, 0.7f, 0.25f, 0.35f) * 0.5f;
	CachedDirectionalInscatteringMultiplier = FMath::Lerp(1.0f, DirectionalCloudScale, FMath::Pow(LocalOvercast01, 2.0f)) * MapRangeClamped(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)), 5.0f, 10.0f, 1.0f, DirectionalCloudScale * 0.5f);
	CachedNightFilteredNightBrightness = FMath::Lerp(1.0f, static_cast<float>(NightBrightness + ExtraNightBrightnessWhenCloudy * LocalOvercast01), CachedNightFilter);
	const float CombinedNightBrightness = SkyMode == EArcTimeOfDaySkyMode::Space ? 1.0f : static_cast<float>(NightBrightness + ExtraNightBrightnessWhenCloudy * LocalOvercast01);
	const float AppliedNightFilter = SkyMode == EArcTimeOfDaySkyMode::Space ? 1.0f : CachedNightFilter;
	if (ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere && SkyAtmosphereDensityCurve && SkyAtmosphere)
	{
		const UWorld* World = GetWorld();
		const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
		CameraSkyAtmosphereNormalizedDensity = SkyAtmosphereDensityCurve->GetFloatValue((ViewLocation.Z - SkyAtmosphere->GetComponentLocation().Z) / SkyAtmosphere->AtmosphereHeight);
	}
	else
	{
		CameraSkyAtmosphereNormalizedDensity = 1.0f;
	}
	const bool bApplyFlatCloudiness = SkyMode == EArcTimeOfDaySkyMode::StaticClouds
		|| SkyMode == EArcTimeOfDaySkyMode::NoClouds
		|| SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora
		|| SkyMode == EArcTimeOfDaySkyMode::VoxelClouds
		|| SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && ColorMode == EArcTimeOfDayColorMode::SimplifiedColor;
	const bool bFlatCloudiness = bApplyFlatCloudiness || SkyMode == EArcTimeOfDaySkyMode::DynamicClouds2D;
	const float CloudinessScale = bFlatCloudiness ? 1.0f : 1.0f - LocalOvercast01;
	CachedNightSkyGlow = NightSkyGlowColor * (static_cast<float>(NightSkyGlowIntensity) * AppliedNightFilter * 0.02f * CameraSkyAtmosphereNormalizedDensity * CloudinessScale * CombinedNightBrightness);
	CachedLightPollution = LightPollutionColor * (CachedNightFilter * static_cast<float>(LightPollutionIntensity));
	const float SunTwilightBrightness = FMath::Pow(MapRangeClamped(CachedSunVector.Z, -0.01f, 0.25f, 1.0f, 0.0f), 11.0f);
	const float MoonEffectiveIllumination = FMath::Clamp((CachedMoonVector.Z + 0.035f) * -2.35f, 0.0f, 1.0f) * CachedMoonLitPercent;
	CachedAbsentDirectionalLightsBrightness = 1.0f + (1.0f - FMath::Max(SunTwilightBrightness, MoonEffectiveIllumination)) * static_cast<float>(DirectionalLightsAbsentBrightness);
	CurrentSceneLightingBrightnessScale = ThreeTimeFloats(static_cast<float>(LightingBrightnessDay), static_cast<float>(LightingBrightnessDawnDusk), static_cast<float>(LightingBrightnessNight));
	ApplyLightingAndSky();
	ApplyVolumetricClouds();
	ApplyAlternateVisualModes();
}

void AArcTimeOfDayActor::ApplyWriterGroup(const EArcTimeOfDayWriterGroup WriterGroup)
{
	if (WriterGroup == EArcTimeOfDayWriterGroup::Immediate)
	{
		for (int32 CacheGroup = 0; CacheGroup < ChangedCacheGroups.Num(); ++CacheGroup)
		{
			for (int32 Index = 0; Index < static_cast<int32>(EArcTimeOfDayCachedProperty::Count); ++Index)
			{
				const EArcTimeOfDayCachedProperty Property = static_cast<EArcTimeOfDayCachedProperty>(Index);
				FArcTimeOfDayCachedValue& CachedValue = CachedPropertyValues[Index];
				if (!CachedValue.bReached || GetCacheGroup(Property) != CacheGroup)
				{
					continue;
				}
				ApplyCachedProperty(Property);
				CachedValue.bReached = false;
				if (bFillingStartingCache)
				{
					CachedValue.bPending = false;
				}
			}
		}
		return;
	}

	for (int32 CacheGroup = 0; CacheGroup < ChangedCacheGroups.Num(); ++CacheGroup)
	{
		if (!ChangedCacheGroups[CacheGroup])
		{
			continue;
		}
		for (int32 Index = 0; Index < static_cast<int32>(EArcTimeOfDayCachedProperty::Count); ++Index)
		{
			const EArcTimeOfDayCachedProperty Property = static_cast<EArcTimeOfDayCachedProperty>(Index);
			FArcTimeOfDayCachedValue& CachedValue = CachedPropertyValues[Index];
			if (!CachedValue.bPending || GetCacheGroup(Property) != CacheGroup)
			{
				continue;
			}
			if (GetWriterGroup(Property) == WriterGroup)
			{
				ApplyCachedProperty(Property);
				CachedValue.bPending = false;
			}
		}
	}
}

void AArcTimeOfDayActor::UpdateCommonDerivatives()
{
	CloudCoverage03 = static_cast<float>(CloudCoverage.GetClampedValue(0.0, 10.0) / (10.0 / 3.0));
	GlobalOvercast01 = MapRangeClamped(CloudCoverage03, 1.2f, 2.1f, 0.0f, 1.0f);
	const bool bUseCloudLayerHeight = bDisableSkyFogCloudinessAboveCloudLayer && (SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds);
	if (!bUseCloudLayerHeight)
	{
		CameraCloudLayerNormalizedHeight = 0.0f;
		CameraCloudLayerNormalizedHeightFullRange = 0.0f;
	}
	LocalCloudCoverage = CloudCoverage03 * MapRangeClamped(CameraCloudLayerNormalizedHeight, 0.2f, 1.0f, 1.0f, 0.0f);
	LocalOvercast01 = MapRangeClamped(LocalCloudCoverage, 1.2f, 2.1f, 0.0f, 1.0f);
}

void AArcTimeOfDayActor::SetTimeCycleDegrees()
{
	const float TimeInRange = FMath::Fmod(static_cast<float>(TimeOfDay) + ArcTimeOfDayClock::DayUnits, ArcTimeOfDayClock::DayUnits);
	if (TimeInRange > DawnTime && TimeInRange < DuskTime)
	{
		TimeCycleDegrees = MapRangeClamped(TimeInRange, static_cast<float>(DawnTime), static_cast<float>(DuskTime), 90.0f, 270.0f);
		return;
	}
	const float NightTime = TimeInRange <= DawnTime ? TimeInRange + ArcTimeOfDayClock::DayUnits : TimeInRange;
	TimeCycleDegrees = FMath::Fmod(MapRangeClamped(NightTime, static_cast<float>(DuskTime), static_cast<float>(DawnTime + ArcTimeOfDayClock::DayUnits), 270.0f, 450.0f), 360.0f);
}

float AArcTimeOfDayActor::SunHeight() const
{
	return CachedSunVector.Z * -0.5f + 0.5f;
}

float AArcTimeOfDayActor::NightFilter() const
{
	return FMath::Pow(1.0f - MapRangeClamped(CachedSunVector.Z, 0.03604f, -0.06319f, 0.0f, EclipsePercent), 2.0f);
}

float AArcTimeOfDayActor::ThreeTimeFloats(const float Day, const float DawnDusk, const float Night) const
{
	const float DayToDawn = MapRangeClamped(SunHeight(), 0.505f, 0.635f, 1.0f, 0.0f);
	const float DawnToNight = MapRangeClamped(SunHeight(), 0.5f, 0.466f, 0.0f, 1.0f);
	return FMath::Lerp(FMath::Lerp(Day, DawnDusk, DayToDawn), Night, DawnToNight);
}
