// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "DaySequenceActor.h"
#include "ArcTimeOfDayTypes.h"
#include "Containers/StaticArray.h"
#include "Engine/Scene.h"

#include "ArcTimeOfDayActor.generated.h"

class UCurveFloat;
class UCurveLinearColor;
class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UNiagaraComponent;
class UNiagaraSystem;
class UPostProcessComponent;
class UInstancedStaticMeshComponent;
class UPrimaryDataAsset;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;
class UTexture2D;
class UTextureRenderTarget2D;
class UVolumeTexture;
class UVolumetricCloudComponent;

UCLASS(BlueprintType, Blueprintable)
class ARCTIMEOFDAY_API AArcTimeOfDayActor final : public ADaySequenceActor
{
	GENERATED_BODY()

public:
	AArcTimeOfDayActor(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category="Ultra Dynamic Sky|Time")
	double GetUDSTimeOfDay() const;

	UFUNCTION(BlueprintCallable, Category="Ultra Dynamic Sky|Time")
	void SetUDSTimeOfDay(double NewTimeOfDay);

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Ultra Dynamic Sky")
	void RefreshSky();

	FArcTimeOfDayModifierHandle AcquireModifierHandle();

	void ReleaseModifierHandle(FArcTimeOfDayModifierHandle Handle);

	void ApplyWeatherValues(FArcTimeOfDayModifierHandle Handle, double InCloudCoverage, double InFog, double InDustAmount, double InCloudDirection, double InWind, double InRain, double InSnow, double InFogSnowContribution);
	FArcTimeOfDayResolvedWeatherValues GetResolvedWeatherValues() const;

	float GetCloudMainLayerBottomHeight() const { return CloudMainLayerBottomHeight; }
	float GetWeatherNightFilter() const { return CachedNightFilter; }
	bool IsVolumetricFogActive() const;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SequencePlayerUpdated(float CurrentTime, float PreviousTime) override;
#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginDestroy() override;
#endif

private:
	static float UDSTimeToHours(double InTimeOfDay, float InDayLength);
	static double HoursToUDSTime(float InHours, float InDayLength);
	static float MapRangeClamped(float Value, float InMin, float InMax, float OutMin, float OutMax);
	static FLinearColor Desaturate(const FLinearColor& Color, float Fraction);
	static FVector RotateAroundAxis(const FVector& Vector, const FVector& Axis, float Degrees);

	void EnsureVisualResources();
	void LoadFoundationResources();
	void RefreshFromRawDaySequenceTime();
	void SynchronizeTimeFromRawDaySequence();
	void EvaluateEditorSky(bool bHardReset);
	void HardResetCache();
	void MonitorForChanges();
	void EvaluateCachedProperties();
	void ApplyWriterGroup(EArcTimeOfDayWriterGroup WriterGroup);
	void ApplyCachedProperty(EArcTimeOfDayCachedProperty Property);
	void RetargetSkyLightCachedProperties();
	static EArcTimeOfDayWriterGroup GetWriterGroup(EArcTimeOfDayCachedProperty Property);
	static int32 GetCacheGroup(EArcTimeOfDayCachedProperty Property);
	void UpdateCommonDerivatives();
	void SetTimeCycleDegrees();
	void CacheSunAndMoonOrientation();
	void RefreshStaticCelestialProperties();
	void UpdateAstronomicalSimulation(FArcTimeOfDayAstronomyResult& Result);
	bool ReadCalendarData(FArcTimeOfDayCalendarData& OutCalendar) const;
	void ForceValidDate(const FArcTimeOfDayCalendarData& CalendarData);
	bool IsLeapYear(int32 InYear, const FArcTimeOfDayCalendarData& CalendarData) const;
	bool IsDaylightSavingsTime(const FArcTimeOfDayCalendarData& CalendarData) const;
	void OffsetDate(int32& InOutMonth, int32& InOutDay, int32& InOutYear, int32 Offset, const FArcTimeOfDayCalendarData& CalendarData) const;
	static double CalculateJulianDay(double InDay, int32 InMonth, int32 InYear);
	static double WrapDegrees(double Degrees);
	void ApplyLightingAndSky();
	void ApplyVolumetricClouds();
	void AdvanceCloudMovement(float DeltaSeconds);
	void UpdateCloudMovementParameters();
	void ApplyAlternateVisualModes();
	void ApplyPostProcessStages();
	void RebuildSpaceLayer();
	void RebuildVoxelClouds();
	USceneComponent* CreateSpaceRootComponent(const FString& Name, EArcTimeOfDaySpaceParent Parent, const FVector& Location, const FRotator& Rotation);
	UStaticMeshComponent* CreateSpaceMeshComponent(const FString& Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	void BuildVolumetricCloudLayers();
	void ApplyInsideCloudFog();
	void ApplyCloudWisps();
	void ApplyCloudShadows();
	void ApplyVolumetricCloudLightRays();
	void ApplySkyMaterialGroup(int32 CacheGroup);
	void ApplyExposure();
	void SetCloudScalar(FName Name, float Value);
	void SetCloudVector(FName Name, const FLinearColor& Value);
	void SetWeatherScalar(FName Name, float Value);
	void SetWeatherVector(FName Name, const FLinearColor& Value);
	void CacheFloatValue(EArcTimeOfDayCachedProperty Property, float Value, float Tolerance);
	void CacheColorValue(EArcTimeOfDayCachedProperty Property, const FLinearColor& Value, float Tolerance);
	float ThreeTimeFloats(float Day, float DawnDusk, float Night) const;
	float SunHeight() const;
	float NightFilter() const;
	float CurrentFogDensity() const;
	float CurrentFogHeightFalloff() const;
	float CurrentFogStartDistance(float Density) const;
	float CurrentSunLightIntensity() const;
	float CurrentMoonLightIntensity() const;
	float CurrentMoonLitPercent() const;
	float CurrentSunRadius() const;
	float CurrentMoonScale() const;
	float CurrentDirectionalDimming() const;
	bool IsDirectionalLightCastingShadows(bool bForceDisabled, const FVector& LightVector) const;
	float CurrentSolarEclipse();
	FVector CurrentMoonPhaseAngle() const;
	FVector CurrentMoonAlignment() const;
	UDirectionalLightComponent* GetSunLightComponent() const;
	UDirectionalLightComponent* GetMoonLightComponent() const;
	USkyLightComponent* GetSkyLightComponent() const;
	UExponentialHeightFogComponent* GetHeightFogComponent() const;
	FLinearColor CurrentSkyAtmosphereAbsorptionColor() const;
	FLinearColor CurrentRayleighScatteringColor() const;
	FLinearColor CurrentVolumetricCloudAlbedo() const;
	FLinearColor ThreeColorTimeBlend(const FLinearColor& Day, const FLinearColor& DawnDusk, const FLinearColor& Night) const;
	void ApplyEnvironmentStaticProperties();
	void ApplyGlobalVolumetricFog();

#if WITH_EDITOR
	void HandleTimeOfDayPreviewChanged(float PreviewHours);
#endif

public:
	UPROPERTY(EditAnywhere, Getter=GetUDSTimeOfDay, Setter=SetUDSTimeOfDay, BlueprintGetter=GetUDSTimeOfDay, BlueprintSetter=SetUDSTimeOfDay, SaveGame, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Time of Day", UIMin="0", UIMax="2400", ToolTip="The time, from 0 to 2400. Can be interpreted as hundredths of an hour. You can set or access this value using DateTime and TimeCode structures, using the Utility functions."))
	double TimeOfDay = 960.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Automatically Apply Calculated Values"))
	bool bAutomaticallyApplyCalculatedValues = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Cloud Coverage", UIMin="0", UIMax="10", ToolTip="The amount of coverage for the cloud layer, from a clear sky to overcast. If Ultra Dynamic Weather is in the level, this variable is controlled from UDW with the weather state."))
	FArcModifiableDouble CloudCoverage = FArcModifiableDouble(3.8);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(UIMin="0", UIMax="10", ClampMin="0", ClampMax="10", MultiLine="true", ToolTip="The fogginess of the scene, from minimal fog to heavy fog by default. Note, this does not map directly to fog density. The way that this and other values contribute to fog density is determined with the settings in the Fog Density category. If Ultra Dynamic Weather is in the level, this variable is controlled from UDW with the weather state."))
	FArcModifiableDouble Fog = FArcModifiableDouble(1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Overall Intensity", UIMin="0", UIMax="2", ClampMin="0", ToolTip="The overall brightness of the sky colors"))
	double OverallIntensity = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(UIMin="0", UIMax="1", ClampMin="0", MultiLine="true", ToolTip="A simple contrast boost for the sky material and clouds"))
	double Contrast = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(UIMin="0", UIMax="1.5", ClampMin="0", ToolTip="A simple saturation adjustment for the sky and lighting"))
	double Saturation = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Sky Mode", ToolTip="Determines the type of clouds/aurora rendered", MultiLine="true"))
	EArcTimeOfDaySkyMode SkyMode = EArcTimeOfDaySkyMode::VolumetricClouds;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Color Mode", ToolTip="Determines the source of sky/cloud colors", MultiLine="true"))
	EArcTimeOfDayColorMode ColorMode = EArcTimeOfDayColorMode::SkyAtmosphere;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(ClampMin="50", ClampMax="2350", ToolTip="The Time of Day value which corresponds to the sun rising above the horizon. This setting is not used if Simulate Real Sun is true.", MultiLine="true"))
	double DawnTime = 600.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(ClampMin="50", ClampMax="2350", ToolTip="The Time of Day value which corresponds to the sun falling below the horizon. This setting is not used if Simulate Real Sun is true.", MultiLine="true"))
	double DuskTime = 1800.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Lighting Brightness (Day)", UIMin="0.3", UIMax="10", ClampMin=".3", ToolTip="Allows adjusting the scene lighting brightness without scaling the intensity of the sky colors.\r\nUnderstand any value other than 1 is not realistic, but useful for art direction.\r\nThis value is used during the day."))
	double LightingBrightnessDay = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Lighting Brightness (Dawn/Dusk)", UIMin="0.3", UIMax="10", ClampMin=".3", ToolTip="Allows adjusting the scene lighting brightness without scaling the intensity of the sky colors.\r\nUnderstand any value other than 1 is not realistic, but useful for art direction.\r\nThis value is used at dawn/dusk (applied at full effect when the sun is exactly on the horizon)"))
	double LightingBrightnessDawnDusk = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Lighting Brightness (Night)", UIMin="0.3", UIMax="10", ClampMin=".3", ToolTip="Allows adjusting the scene lighting brightness without scaling the intensity of the sky colors.\r\nUnderstand any value other than 1 is not realistic, but useful for art direction.\r\nThis value is used at night."))
	double LightingBrightnessNight = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Night Brightness", UIMin="0", UIMax="2", ToolTip="Can be used to scale the brightness of the sky and lighting, at night."))
	double NightBrightness = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Directional Lights Absent Brightness", UIMin="0", UIMax="10", ClampMin="0", MultiLine="true", ToolTip="Boosts intensity of sky color and ambient light, when both the sun and moon are down (or when the moon is up but moon phase is limiting the moonlight intensity). This is to help address low visibility during very dark twilight hours lacking directional light."))
	double DirectionalLightsAbsentBrightness = 2.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Extra Night Brightness When Cloudy", UIMin="0", UIMax="5", MultiLine="true", ToolTip="Added to Night Brightness when cloud coverage gets overcast."))
	double ExtraNightBrightnessWhenCloudy = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟻  ·  Cloud Movement", meta=(DisplayName="Cloud Speed", ToolTip="Scales the speed the clouds move, if using Volumetric Clouds or 2D Dynamic Clouds.", UIMin="0", UIMax="5"))
	double CloudSpeed = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟻  ·  Cloud Movement", meta=(DisplayName="Cloud Direction", UIMin="0", UIMax="360", ToolTip="The direction the clouds move. NOTE: Control of this variable will be taken by Ultra Dynamic Weather if you have one in your level. UDW controls cloud direction using its Wind Direction.", ClampMin="0", ClampMax="360", ForceUnits="Degrees"))
	FArcModifiableDouble CloudDirection = FArcModifiableDouble(180.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟻  ·  Cloud Movement", meta=(DisplayName="Cloud Phase", ToolTip="A time offset for the clouds. Useful to fine tune a specific formation when working with a Cloud Speed of 0, or to keyframe cloud movement directly."))
	double CloudPhase = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟻  ·  Cloud Movement", meta=(DisplayName="Formation Change Speed", MultiLine="true", ToolTip="Scales the speed (relative to how fast the clouds are generally moving) that the cloud formations will change shape.", UIMin="0", UIMax="1.5", ClampMin="0"))
	double FormationChangeSpeed = 0.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Light Intensity", UIMin="0", ClampMin="0", ForceUnits="Lux", ToolTip="The intensity of the sun's direction light source, in Lux."))
	double SunLightIntensity = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Manually Position Sun Target", MultiLine="true", ToolTip="If this option is checked, you can select the Sun Target widget visible when UDS is selected, and move it around to manually control the location of the sun. If this is unchecked, the sun's location will be determined by time of day and the options below for yaw and pitch, or the Simulation options if using Simulate Real Sun."))
	bool bManuallyPositionSunTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Yaw", UIMin="0", UIMax="360", ForceUnits="Degrees", ToolTip="The yaw of the Sun's path. This determines the direction the sun rises/sets. Ignored if using Simulate Real Sun."))
	double SunYaw = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Pitch", UIMin="-85", UIMax="85", ForceUnits="Degrees", ToolTip="Pitches the sun's path across the sky. A value of 0 would have the sun go to the top of the sky at midday. Increasing values angle it so it peaks lower. Ignored if using Simulate Real Sun."))
	double SunPitch = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Vertical Offset", MultiLine="true", UIMin="-1", UIMax="1", ToolTip="Forces the sun higher or lower in the sky.  Ignored if using Simulate Real Sun."))
	double SunVerticalOffset = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Extend Dawn And Dusk", MultiLine="true", UIMin="0", UIMax="4", ToolTip="This will artificially lengthen dawn and dusk by flattening the sun's trajectory around the horizon.  Ignored if using Simulate Real Sun."))
	double ExtendDawnAndDusk = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Light Color", ToolTip="The maximum, unfiltered color of the sun directional light."))
	FLinearColor SunLightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Target", MultiLine="true", MakeEditWidget="true", ToolTip="The vector used for the sun direction when Manually Position Sun Target is enabled. Note, the intended way of editing this vector is by moving the blue diamond shaped widget on UDS, labelled Sun Target."))
	FVector SunTarget = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Scale", UIMin="0", UIMax="5", ForceUnits="Degrees", ToolTip="The angular diameter of the sun disk in the sky."))
	double SunScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Softness", MultiLine="true", UIMin=".5", UIMax="8", ClampMin=".5", ClampMax="8", ToolTip="Controls the softness/sharpness of the sun disk."))
	double SunSoftness = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Disk Tint"))
	FLinearColor SunDiskTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Disk Intensity", UIMin="0", UIMax="20", ToolTip="The intensity of the sun disk"))
	double SunDiskIntensity = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Source Angle Scale", MultiLine="true", UIMin="0", UIMax="10", ToolTip="Multiplied over the Source Angle and Source Soft Angle calculated for the sun."))
	double SunSourceAngleScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Solar Eclipse", MultiLine="true", ToolTip="If enabled, the sun's light and sky color will be affected when the sun goes behind the moon, or behind one of the planets in the Space Layer."))
	bool bSolarEclipse = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Solar Eclipse Tint Color", MultiLine="true", ToolTip="The tint color applied to the sun's light and effect on sky color, during an eclipse, if the Solar Eclipse option is enabled."))
	FLinearColor SolarEclipseTintColor = FLinearColor(0.994792f, 0.754438f, 0.784207f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Solar Eclipse Intensity Multiplier", MultiLine="true", UIMin="0", UIMax="1", ClampMin="0", ToolTip="Multiplied over the sun light intensity when the entire sun disk is blocked."))
	double SolarEclipseIntensityMultiplier = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Specular Scale", MultiLine="true", UIMin="0", UIMax="5", ClampMin="0", ToolTip="The max specular scale of the sun directional light. Is dimmed down as the sky becomes overcast."))
	double SunSpecularScale = 1.0;

	UPROPERTY(EditInstanceOnly, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Custom Sun Light Actor", ToolTip="If a directional light actor is selected for this, it will be used in place of the built in directional light component for the sun."))
	TObjectPtr<ADirectionalLight> CustomSunLightActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Max Sun Source Angle Scale", MultiLine="true", UIMin="1", UIMax="15", ToolTip="The source angle of the sun will be scaled up with cloud coverage and fog, to a max of this multiplier."))
	double MaxSunSourceAngleScale = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Source Angle Softness", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Scales source soft angle, relative to the calculated source angle."))
	double SunSourceAngleSoftness = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Scale Sun Radius As It Nears Horizon", MultiLine="true", UIMin="0.1", UIMax="15", ToolTip="Can be used to make the sun disk larger when its closer to the horizon, as a stylistic effect."))
	double ScaleSunRadiusNearHorizon = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Fade Down High Sun Light Intensity Below Horizon", MultiLine="true", ToolTip="If sun light intensity is higher than default (if using physical values, in the thousands, for example) the light intensity will be dimmed down as the sun goes below the horizon, to ease the transition to twilight lighting and moonlight."))
	bool bFadeDownHighSunLightIntensityBelowHorizon = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Casts Shadows", MultiLine="true", ToolTip="Controls if \"cast shadows\" will ever be enabled on the sun directional light."))
	bool bSunCastsShadows = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun - Lighting Channel 0", MultiLine="true", ToolTip="Controls which lighting channels on the directional light are enabled, when the directional light is above the horizon."))
	bool bSunLightingChannel0 = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun - Lighting Channel 1", MultiLine="true", ToolTip="Controls which lighting channels on the directional light are enabled, when the directional light is above the horizon."))
	bool bSunLightingChannel1 = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun - Lighting Channel 2", MultiLine="true", ToolTip="Controls which lighting channels on the directional light are enabled, when the directional light is above the horizon."))
	bool bSunLightingChannel2 = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Sun Disk Color Curve", ToolTip="Controls the color of the sun disk (scaled by the sun light color), with the height of the sun. The input for this curve is a 0 to 1 range, with 0 being the sun pointing straight up, and 1 being the sun pointing straght down."))
	TObjectPtr<UCurveLinearColor> SunDiskColorCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Light Intensity", UIMin="0", UIMax=".5", ClampMin="0", ForceUnits="Lux", ToolTip="The maximum intensity of the moon directional light, in Lux."))
	double MoonLightIntensity = 0.15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Manually Position Moon Target", MultiLine="true", ToolTip="If this option is checked, you can select the Moon Target widget visible when UDS is selected, and move it around to manually control the location of the moon. If this is unchecked, the moon's location will be determined by time of day and the options below for yaw and pitch, or the Simulation options if using Simulate Real Moon."))
	bool bManuallyPositionMoonTarget = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Render Moon", MultiLine="true", ToolTip="Setting this to false will completely disable the moon from being drawn in the sky or affecting lighting."))
	bool bRenderMoon = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Yaw", UIMin="0", UIMax="360", ForceUnits="Degrees", ToolTip="The yaw of the moon's path. Determines the direction the moon rises/sets. Ignored if using Simulate Real Moon."))
	double MoonYaw = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Pitch", UIMin="-90", UIMax="90", ForceUnits="Degrees", ToolTip="The pitch of the moon's path. 0 would mean the moon goes to the top of the sky, Increasing values pitch the angle so the moon peaks lower. Ignored if using Simulate Real Moon."))
	double MoonPitch = 35.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Vertical Offset", MultiLine="true", UIMin="-.5", UIMax=".5", ToolTip="Forces the moon higher or lower in the sky.  Ignored if using Simulate Real Moon."))
	double MoonVerticalOffset = 0.04;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Orbit Offset", UIMin="0", UIMax="1", ToolTip="Offsets how the moon's cycle aligns with the sun. By default its path is opposing, rising right when the sun sets. You can use this setting to change that relationship."))
	double MoonOrbitOffset = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Phase", UIMin="0", UIMax="29.53", ClampMin="0", ClampMax="29.53", ForceUnits="Days", ToolTip="The current lunar phase. Controlled by the simulation if using Simulate Real Moon"))
	double MoonPhase = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Target", MultiLine="true", MakeEditWidget="true", ToolTip="The vector used for the moon direction when Manually Position Moon Target is enabled. Note, the intended way of editing this vector is by moving the blue diamond shaped widget on UDS, labelled Moon Target."))
	FVector MoonTarget = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Scale", UIMin=".1", UIMax="12", ClampMin=".1", ForceUnits="Degrees", ToolTip="The angular diameter of the moon in the sky"))
	double MoonScale = 0.95;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Casts Shadows", MultiLine="true", ToolTip="Controls if \"cast shadows\" will ever be enabled on the moon directional light."))
	bool bMoonCastsShadows = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Texture", MultiLine="true", ToolTip="The RGB color and alpha channel of the moon. You can use an alpha which is not circular, but the shape needs to fit inside the circular footprint."))
	TObjectPtr<UTexture2D> MoonTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Material Color", ToolTip="The color of the moon texture in the sky material."))
	FLinearColor MoonMaterialColor = FLinearColor(0.486328f, 0.574971f, 0.864583f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Texture Intensity (Night)", UIMin="0", UIMax="1", ToolTip="Intensity of the Moon texture during the night"))
	double MoonTextureIntensityNight = 0.3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Texture Intensity (Day)", UIMin="0", UIMax="1", ToolTip="Intensity of the Moon texture during the day."))
	double MoonTextureIntensityDay = 0.6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Glow Intensity", UIMin="0", UIMax=".2", ClampMin="0", ToolTip="The intensity of the glow around the moon"))
	double MoonGlowIntensity = 0.05;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Glow Scale", UIMin="0", UIMax="2", ClampMin="0", ToolTip="Scale the size of the glow around the moon"))
	double MoonGlowScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Render Moon Phases", ToolTip="If true, uses the Moon Phase Normal texture to render the shading of the moon using the moon phase value. If disabled, the moon texture will show at its full brightness without the phase shading."))
	bool bRenderMoonPhases = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Phase Contrast", UIMin="1", UIMax="4", ToolTip="Sharpens/softens the line separating the dark side and light side of the moon"))
	double MoonPhaseContrast = 1.6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Dark Side Brightness", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Use this to brighten the dark side of the moon. 0 is pure black. 1 is as bright as the light side."))
	double DarkSideBrightness = 0.005;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Phase Normal", MultiLine="true", ToolTip="The texture containing the XYZ channels of the moon normal used to render moon phases, if enabled."))
	TObjectPtr<UTexture2D> MoonNormalTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="New Moon Light Brightness", MultiLine="true", UIMin="0", UIMax="1", ToolTip="The brightness of a new moon as a 0 to 1 percentage of full moon brightness. This determines how moon light strength will be affected by phase."))
	double NewMoonLightBrightness = 0.015;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Source Angle Scale", MultiLine="true", UIMin="0", UIMax="10", ToolTip="Multiplied over the Source Angle and Source Soft Angle calculated for the moon."))
	double MoonSourceAngleScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Texture Rotation", UIMin="0", UIMax="360", ToolTip="Manual rotation control of the moon texture. This is ignored if using Simulate Real Moon."))
	double MoonTextureRotation = 0.0;

	UPROPERTY(EditInstanceOnly, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Custom Moon Light Actor", ToolTip="If a directional light actor is selected for this, it will be used in place of the built in directional light component for the moon."))
	TObjectPtr<ADirectionalLight> CustomMoonLightActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Scale Moon Radius As It Nears Horizon", MultiLine="true", UIMin="0.1", UIMax="10", ToolTip="Can be used to make the moon larger when its closer to the horizon, as a stylistic effect."))
	double ScaleMoonRadiusNearHorizon = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Specular Scale", MultiLine="true", UIMin="0", UIMax="1", ToolTip="The maximum specular scale of the moon directional light. Further scales down with cloud coverage/fog."))
	double MoonSpecularScale = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Max Moon Source Angle Scale", MultiLine="true", UIMin="1", UIMax="30", ToolTip="The source angle of the moon will be scaled up with cloud coverage and fog, to a max of this multiplier."))
	double MaxMoonSourceAngleScale = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon Source Angle Softness", MultiLine="true", UIMin="0", UIMax="2", ToolTip="Scales source soft angle, relative to the calculated source angle."))
	double MoonSourceAngleSoftness = 0.3;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon - Lighting Channel 0", MultiLine="true", ToolTip="Controls which lighting channels on the directional light are enabled, when the directional light is above the horizon."))
	bool bMoonLightingChannel0 = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon - Lighting Channel 1", MultiLine="true", ToolTip="Controls which lighting channels on the directional light are enabled, when the directional light is above the horizon."))
	bool bMoonLightingChannel1 = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟸  ·  Moon", meta=(DisplayName="Moon - Lighting Channel 2", MultiLine="true", ToolTip="Controls which lighting channels on the directional light are enabled, when the directional light is above the horizon."))
	bool bMoonLightingChannel2 = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Directional Balance", MultiLine="true", UIMin=".3", UIMax="2", ClampMin=".01", ToolTip="Can be used to scale the contribution of directional light on the scene without affecting the brightness of the sky itself."))
	double DirectionalBalance = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Dim Directional Lights With Cloud Coverage", MultiLine="true", ToolTip="If this is enabled, directional light intensity will reduce all the way to 0 as cloud coverage gets overcast. The sky color will try to compensate by injecting the current fog color into the clouds/sky. This can be useful to correct for materials and effects which cannot sample the light function used for cloud shadowing."))
	bool bDimDirectionalLightsWithCloudCoverage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Dimming Range", ToolTip="If Dim Directional Lights With Cloud Coverage is enabled, this defines the start and end points of Cloud Coverage which the dimming is applied across"))
	FFloatRange DimmingRange = FFloatRange(FFloatRangeBound::Exclusive(1.4f), FFloatRangeBound::Exclusive(2.0f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Dimming Range Exponent", MultiLine="true", ToolTip="If Dim Directional Lights With Cloud Coverage is enabled, this is the exponent of the 0 to 1 range defined by the lower and upper cloud coverage bounds."))
	double DimmingRangeExponent = 2.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Space Sky Mode Disables Directional Lights Below Horizon", ToolTip="The Space sky mode by default assumes no ground/horizon beneath the viewer, so leaves the directional light active at upward angles.\r\nYou can enable this to have it disable the directional lights below the horizon, similar to the other sky modes. This could be useful for a scene on a planet/moon with no atmosphere for example."))
	bool bSpaceSkyModeDisablesDirectionalLightsBelowHorizon = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Sun World Rotation", ToolTip="The current world rotation of the sunlight"))
	FRotator SunWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙴𝟺  ·  Directional Light", meta=(DisplayName="Moon World Rotation", ToolTip="The current world rotation of the moon light"))
	FRotator MoonWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙴𝟷  ·  Sun", meta=(DisplayName="Eclipse Percent", MultiLine="true", ToolTip="1 when an eclipse is not happening, 0 when sun is fully covered"))
	double CurrentEclipsePercent = 1.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="System - Moon", meta=(DisplayName="Moon Light Current Intensity Value", MultiLine="true", BlueprintPrivate="true"))
	double MoonLightCurrentIntensityValue = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Simulate Real Sun", MultiLine="true", ToolTip="Enable a sun direction calculated using the date + coordinates below."))
	bool bSimulateRealSun = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Simulate Real Moon", MultiLine="true", ToolTip="Enable a moon direction and phase calculated using the date + coordinates below."))
	bool bSimulateRealMoon = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Simulate Real Stars", MultiLine="true", ToolTip="Enable a real 360 degree starmap for the stars, angled correctly using the date + coordinates below. Note that the default texture used for this real starmap is (necessarily) extremely high resolution, at 8192 x 8192."))
	bool bSimulateRealStars = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Latitude", MultiLine="true", UIMin="-90", UIMax="90", ForceUnits="Degrees", ToolTip="The latitude of the simulated location. Positive values are to the North, negative values are to the South."))
	double Latitude = 51.178856;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Longitude", MultiLine="true", UIMin="-180", UIMax="180", ForceUnits="Degrees", ToolTip="The longitude of the simulated location. Positive values are to the East. Negative Values are to the West."))
	double Longitude = -1.826185;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Time Zone", MultiLine="true", UIMin="-12", UIMax="12", ForceUnits="Hours", ToolTip="The UTC offset for the simulated location. Determines how the current Time of Day is converted to UTC time to control the simulation."))
	double TimeZone = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Month", MultiLine="true", UIMin="1", UIMax="12", ClampMin="1", ToolTip="The current month of the simulation."))
	int32 Month = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Day", MultiLine="true", UIMin="1", UIMax="31", ClampMin="1", ToolTip="The current day of the month, for the simulation."))
	int32 CalendarDay = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Year", MultiLine="true", ToolTip="The current year for the simulation."))
	int32 Year = 2021;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="North Yaw", MultiLine="true", UIMin="0", UIMax="360", ForceUnits="Degrees", ToolTip="Rotate around the angle that represents North. X+ is used by default."))
	double NorthYaw = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Apply Daylight Savings Time", MultiLine="true", ToolTip="Interpret the Time of Day with Daylight Savings Time. This will also be applied to all Time of Day relevant calculations, if any Simulate option is enabled, to keep the system in sync."))
	bool bApplyDaylightSavingsTime = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Calendar", MultiLine="true", ToolTip="The calendar system used in interpreting and controlling the date values. The gregorian calendar is the included calendar, but you can make your own calendar asset of the same type, to customize the number of months, length of months, days of the week, etc."))
	TObjectPtr<UPrimaryDataAsset> Calendar;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Simulation Controls Sun Scale", ToolTip="If true and Simulate Real Sun is enabled, the sim will control Sun Scale (the angular diameter of the sun) with the simulation."))
	bool bSimulationControlsSunScale = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Simulation Controls Moon Scale", ToolTip="If true and Simulate Real Moon is enabled, the sim will control Moon Scale (the angular diameter of the moon) with the simulation."))
	bool bSimulationControlsMoonScale = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST Start Month", MultiLine="true", UIMin="1", UIMax="12", ClampMin="1", ToolTip="The month daylight savings time starts, if enabled"))
	int32 DSTStartMonth = 3;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST Start Day Of The Week", UIMin="0", UIMax="6", ToolTip="The day of the week DST starts on. Which of this day in the month is used is set by DST Start Day Count.\r\nIn the default Gregorian calendar, 0 = Monday."))
	int32 DSTStartDayOfWeek = 6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST Start Day Count", UIMin="0", UIMax="3", ClampMin="0", ToolTip="Defines how many counts of the start day of the week before the DST start day. For example, if DST starts on the second sunday of the month, this would be 2.\r\nA value of 0 means the last of this day of the week that month. So if DST starts on the last sunday of the month, this would be 0."))
	int32 DSTStartDayCount = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST Start Hour", MultiLine="true", UIMin="0", UIMax="24", ClampMin="0", ClampMax="24", ToolTip="The hour daylight savings time starts"))
	int32 DSTStartHour = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST End Month", MultiLine="true", UIMin="1", UIMax="12", ClampMin="1", ToolTip="The month daylight savings time ends, if enabled"))
	int32 DSTEndMonth = 10;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST End Day Of The Week", UIMin="0", UIMax="6", ToolTip="The day of the week DST ends on. Which of this day in the month is used is set by DST End Day Count.\r\nIn the default Gregorian calendar, 0 = Monday."))
	int32 DSTEndDayOfWeek = 6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST End Day Count", UIMin="0", UIMax="3", ClampMin="0", ToolTip="Defines how many counts of the end day of the week before the DST end day. For example, if DST ends on the first sunday of the month, this would be 1.\r\nA value of 0 means the last of this day of the week that month. So if ends starts on the last sunday of the month, this would be 0."))
	int32 DSTEndDayCount = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="DST End Hour", MultiLine="true", UIMin="0", UIMax="24", ClampMin="0", ClampMax="24", ToolTip="The hour daylight savings time ends"))
	int32 DSTEndHour = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙳𝟷  ·  Simulation", meta=(DisplayName="Moon Simulation Terms", UIMin="1", UIMax="60", ClampMin="1", ClampMax="60", ToolTip="In simulating the moon position, there is flexibility in how many terms to factor in. Each term increases accuracy, but with diminishing effect. For most typical projects, only the first, most significant terms are necessary. But accuracy can be increased with up to 60 terms, the trade off being the additional calculation cost."))
	int32 MoonSimulationTerms = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Stars Intensity", UIMin="0", UIMax="8", ClampMin="0", ToolTip="Scales the max brightness of the stars"))
	double StarsIntensity = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Stars Color", ToolTip="Tints the stars"))
	FLinearColor StarsColor = FLinearColor(0.484375f, 0.639062f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Stars Daytime Intensity", MultiLine="true", UIMin="0", UIMax="5", ToolTip="The amount that stars intensity will be multiplied by during the day."))
	double StarsDaytimeIntensity = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Stars Tiling", UIMin="1", UIMax="10", ToolTip="For tiling stars, scales how many times they tile across the sky"))
	double StarsTiling = 2.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Tiling Stars Move With Time Of Day", UIMin="0", UIMax="10", ClampMin="0", ToolTip="How many times tiling stars will pan across their UVs in a full day."))
	int32 TilingStarsMoveWithTimeOfDay = 5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Tiling Stars Constant Speed", UIMin="0", UIMax="1", ToolTip="Adds a constant movement speed to the tiling stars, independent from Time of Day."))
	double TilingStarsConstantSpeed = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Twinkle Amount", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Adjusts the intensity of a twinkling/flickering/shimmering effect of the stars. Disbled on Low global material quality."))
	double TwinkleAmount = 0.3;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Twinkle Speed", MultiLine="true", UIMin="0", UIMax="2", ToolTip="Adjusts the speed of the above twinkling effect"))
	double TwinkleSpeed = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Tiling Stars Texture", MultiLine="true", ToolTip="The texture to use for the stars when Simulate Real Stars is disabled"))
	TObjectPtr<UTexture2D> StarsTexture;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Real Stars Texture", MultiLine="true", ToolTip="The texture to use for the 360 starmap, used when Simulate Real Stars is enabled."))
	TObjectPtr<UTexture2D> RealStarsTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Tiling Stars Phase", ToolTip="Offsets the time of the tiling stars movement."))
	double TilingStarsPhase = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Real Stars Fade To Sprites when Zoomed", ToolTip="When using Simulate Real Stars, if this is enabled, the stars will fade from a texture sample over to individual sprites (for the brightest 500k stars in the sky) rendered by a niagara system, when the camera zooms in enough to make that necessary. This allows long/telescopic lenses to show the stars without revealing the limits of the real stars texture resolution.\r\nRequires the project to support DBuffer Decals."))
	bool bRealStarsFadeToSpritesWhenZoomed = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Real Stars Sprites Active", BlueprintPrivate="true"))
	bool bRealStarsSpritesActive = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Real Stars Sprites System", ToolTip="The niagara system which renders the individual star sprites when \"Real Stars Fade to Sprites When Zoomed\" is enabled."))
	TObjectPtr<UNiagaraSystem> RealStarsSpritesSystem;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟷  ·  Stars", meta=(DisplayName="Real Stars Sprites Starmap", ToolTip="A lookup table containing all the star locations (Right Ascension(Degrees) in R channel, Declination(Degrees) + 90 in G channel) which are used by the real stars sprites system.\r\nNeeds to be a resolution of 500x1000. The default texture contains the 500,000 brightest stars in the sky."))
	TObjectPtr<UTexture2D> RealStarsSpritesStarmap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟹  ·  Sky Glow", meta=(DisplayName="Night Sky Glow", UIMin="0", UIMax="2", ToolTip="Adds a starlight/ambient glow to the sky color, irrespective of directional light."))
	double NightSkyGlowIntensity = 0.6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟹  ·  Sky Glow", meta=(DisplayName="Night Sky Glow Color", ToolTip="Controls the color of the Night Sky Glow."))
	FLinearColor NightSkyGlowColor = FLinearColor(0.093045f, 0.141694f, 0.255208f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟹  ·  Sky Glow", meta=(DisplayName="Light Pollution Intensity", MultiLine="true", UIMin="0", UIMax=".65", ClampMin="0", ToolTip="Simulates the contribution of city light which adds to sky glow and lights up volumetric clouds. Mainly visible when at night when there is no moon."))
	double LightPollutionIntensity = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟹  ·  Sky Glow", meta=(DisplayName="Light Pollution Color", MultiLine="true", ToolTip="Controls the color of the light contributed by Light Pollution Intensity."))
	FLinearColor LightPollutionColor = FLinearColor(0.630208f, 0.481016f, 0.377575f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟹  ·  Sky Glow", meta=(DisplayName="Sky Glow Reflection Intensity", UIMin="0", UIMax="5", ToolTip="Scales the intensity of the sky glow in the reflection capture."))
	double SkyGlowReflectionIntensity = 2.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Mode", MultiLine="true", ToolTip="Select the type of skylight source being used. Hover over each option for a detailed description of each. If using a custom actor for your sky light, note that you'll need to change source type to match. \"SLS Captured Scene\" for capture based. \"SLS Specified Cubemap\" for the other two."))
	EArcTimeOfDaySkyLightMode SkyLightMode = EArcTimeOfDaySkyLightMode::CaptureBased;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Intensity", MultiLine="true", UIMin="0", UIMax="5", ToolTip="Scales the intensity of the sky light."))
	double SkyLightIntensity = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Temperature", MultiLine="true", UIMin="-4000", UIMax="4000", ToolTip="A simple adjustment of the sky light's color temperature, applied regardless of the sky light mode."))
	double SkyLightTemperature = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Color Multiplier (Day)", MultiLine="true", ToolTip="Multiplied over the sky light color during the day"))
	FLinearColor SkyLightColorMultiplierDay = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Color Multiplier (Dawn/ Dusk)", MultiLine="true", ToolTip="Multiplied over the sky light color during the dawn/dusk"))
	FLinearColor SkyLightColorMultiplierDawnDusk = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Color Multiplier (Night)", MultiLine="true", ToolTip="Multiplied over the sky light color during the night"))
	FLinearColor SkyLightColorMultiplierNight = FLinearColor::White;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Real Time Capture", MultiLine="true", ToolTip="Real Time Capture allows the Capture Based Sky Light to update with the changing sky in real time, without the old process of doing a full recapture. Real Time Capture is limited to capturing sky materials, fog, and volumetric clouds."))
	bool bRealTimeCapture = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Render Sky Light", ToolTip="Toggle off to completely disable the sky light component"))
	bool bRenderSkyLight = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Move Sky Light Capture With Camera Location", ToolTip="If the sky light is capture based, movable, and using real time capture, this option will enable the sky light's position being set to the camera location as it moves. This allows the global reflection environment to be accurate to where you are in the level, with respect to things like the cloud formations in the sky."))
	bool bMoveSkyLightCaptureWithCameraLocation = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Movement Uses Z Axis", ToolTip="If \"Move Sky Light Capture with Camera Location\" is enabled, this toggles if the Z axis of the camera position is also used for the update."))
	bool bSkyLightMovementUsesZAxis = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Lower Hemisphere Tint (Capture Based)", MultiLine="true", ToolTip="Tints the color of the lower hemisphere of the sky light when the Sky Light Mode is Capture Based."))
	FLinearColor SkyLightLowerHemisphereTintCaptureBased = FLinearColor(0.260417f, 0.229905f, 0.202094f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Lower Hemisphere Tint (Cubemap)", MultiLine="true", ToolTip="Tints the color of the lower hemisphere of the sky light when the Sky Light Mode is Custom Cubemap or Cubemap with Dynamic Color Tinting."))
	FLinearColor SkyLightLowerHemisphereTintCubemap = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);
	UPROPERTY(EditInstanceOnly, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Custom Sky Light Actor", ToolTip="If a sky light actor is selected for this, it will be used in place of the built in sky light component."))
	TObjectPtr<ASkyLight> CustomSkyLightActor;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Static/ Stationary Sky Light Casts Shadows", MultiLine="true", ToolTip="Sky light will cast static or stationary shadows, in lightmass, if its Mobility is set to Static or Stationary."))
	bool bStaticStationarySkyLightCastsShadows = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Movable Sky Light Casts Shadows (Enable DFAO)", MultiLine="true", ToolTip="This toggles Cast Shadows on the sky light if the Mobility is set to Movable. In practice, this just toggles the effect of Distance Field Ambient Occlusion, if enabled by the project configuration."))
	bool bMovableSkyLightCastsShadows = false;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟹  ·  Sky Light", meta=(DisplayName="Sky Light Dynamic Color Tinting Multiplier", ToolTip="Multiplied by the sky light color, specifically when using the \"Cubemap with Dynamic Color Tinting\" option for Sky Light Mode."))
	FLinearColor SkyLightDynamicColorTintingMultiplier = FLinearColor::White;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Apply Exposure Settings", MultiLine="true", ToolTip="If enabled, controls the exposure settings on a post process component, to apply basic auto exposure settings with the values below"))
	bool bApplyExposureSettings = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Metering Mode", ToolTip="The exposure mode applied by UDS' exposure settings, if Apply Exposure Settings is true."))
	TEnumAsByte<EAutoExposureMethod> ExposureMeteringMode = EAutoExposureMethod::AEM_Histogram;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Compensation Curve", MultiLine="true", ToolTip="Adjusts exposure compensation with the average scene EV100, if Apply Exposure Settings is true"))
	TObjectPtr<UCurveFloat> ExposureCompensationCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Bias Day", MultiLine="true", UIMin="-10", UIMax="20", ToolTip="The base exposure bias during the day", ForceUnits="EV"))
	double ExposureBiasDay = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Bias Dawn/ Dusk", MultiLine="true", UIMin="-10", UIMax="20", ToolTip="The base exposure bias during the dawn/dusk", ForceUnits="EV"))
	double ExposureBiasDawnDusk = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Bias Night", MultiLine="true", UIMin="-10", UIMax="20", ToolTip="The base exposure bias during the night", ForceUnits="EV"))
	double ExposureBiasNight = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Bias Cloudy", MultiLine="true", UIMin="-10", UIMax="20", ToolTip="Added to exposure bias with high cloud coverage", ForceUnits="EV"))
	double ExposureBiasCloudy = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Bias Foggy", MultiLine="true", UIMin="-10", UIMax="20", ToolTip="Added to exposure bias with high fog", ForceUnits="EV"))
	double ExposureBiasFoggy = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Bias Dusty", MultiLine="true", UIMin="-10", UIMax="20", ToolTip="Added to exposure bias with high dust", ForceUnits="EV"))
	double ExposureBiasDusty = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Brightness Range", ToolTip="Controls the minimum and maximum level of scene brightness that the auto exposure will adjust to."))
	FFloatRange ExposureBrightnessRange = FFloatRange(FFloatRangeBound::Exclusive(-10.0f), FFloatRangeBound::Exclusive(20.0f));
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟼  ·  Exposure", meta=(DisplayName="Exposure Compensation Curve For Physical Light Intensity", MultiLine="true", ToolTip="This will be used for the exposure compensation curve if the Sun Light Intensity is set to a very high value, presumably for physically accurate lighting intensity. The default for this curve adjusts exposure more appropriately for that context, avoiding overbrightening."))
	TObjectPtr<UCurveFloat> ExposureCompensationCurvePhysical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Color Intensity Scale", MultiLine="true", UIMin="0", UIMax="3", ToolTip="Scales the brightness of the height fog color overall"))
	double FogColorIntensityScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Overcast Brightness Night", MultiLine="true", UIMin=".001", UIMax="1", ClampMin=".001", ToolTip="Modulates the fog colors when the cloud coverage is overcast during the night."))
	double OvercastBrightnessNight = 0.2;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Render Exponential Height Fog", ToolTip="Toggle off to completely disable the exponential height fog"))
	bool bRenderExponentialHeightFog = true;
	UPROPERTY(EditInstanceOnly, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Custom Height Fog Actor", ToolTip="If an exponential height fog actor is selected for this, it will be used in place of the built in exponential height fog component."))
	TObjectPtr<AExponentialHeightFog> CustomHeightFogActor;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Color Mode", MultiLine="true", ToolTip="Determines the source of fog colors. Read the tooltip for each mode for more information."))
	EArcTimeOfDayFogColorMode FogColorMode = EArcTimeOfDayFogColorMode::Automatic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Overcast Brightness Day", MultiLine="true", UIMin=".001", UIMax="1", ToolTip="Modulates the brightness of the fog colors when the cloud coverage is overcast during the day.", ClampMin=".001"))
	double OvercastBrightnessDay = 0.2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Sky Atmosphere Fog Influence Sun", UIMin="0", UIMax="4", ToolTip="Scales the sky atmosphere influence on fog, when the sun is up."))
	double SkyAtmosphereFogInfluenceSun = 2.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Sky Atmosphere Fog Influence Moon", UIMin="0", UIMax="4", ToolTip="Scales the sky atmosphere influence on fog, when the moon is up at night."))
	double SkyAtmosphereFogInfluenceMoon = 1.65;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Sky Atmosphere Fog Influence None", UIMin="0", UIMax="4", ToolTip="Scales sky atmosphere influence on fog, when neither the sun or moon is up."))
	double SkyAtmosphereFogInfluenceNone = 0.8;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Inscattering Color Curve", MultiLine="true", ToolTip="Defines how the contribution of sun or moon light modulates the fog inscattering color. No effect on sky atmosphere fog colors."))
	TObjectPtr<UCurveLinearColor> FogInscatteringColorCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Inscattering Sun", MultiLine="true", ToolTip="The base color used for the sun to light up the fog inscattering color. No effect on sky atmosphere fog colors."))
	FLinearColor FogInscatteringSun = FLinearColor(0.08362f, 0.156441f, 0.302083f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Inscattering Moon", MultiLine="true", ToolTip="The base color used for the moon to light up the fog inscattering color. No effect on sky atmosphere fog colors."))
	FLinearColor FogInscatteringMoon = FLinearColor(0.076091f, 0.108017f, 0.171875f, 1.0f);
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Directional Inscattering Color Curve", MultiLine="true", ToolTip="Defines how the contribution of sun or moon light modulates the fog directional inscattering color. No effect on sky atmosphere fog colors."))
	TObjectPtr<UCurveLinearColor> FogDirectionalInscatteringColorCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Fog Directional Inscattering", MultiLine="true", ToolTip="The base color used for the directional inscattering, affected by the above curve. No effect on sky atmosphere fog colors."))
	FLinearColor FogDirectionalInscattering = FLinearColor(0.146484f, 0.147386f, 0.15625f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Foggy Desaturation", MultiLine="true", UIMin="0", UIMax="1", ToolTip="The amount fog colors are desaturated as fogginess increases to max. No effect on sky atmosphere fog colors."))
	double FoggyDesaturation = 0.4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Overcast Desaturation", MultiLine="true", UIMin="0 ", UIMax="1", ToolTip="The amount fog colors are desaturated as cloud coverage increases to overcast. No effect on sky atmosphere fog colors."))
	double OvercastDesaturation = 0.3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="All Fog Colors Multiplier", MultiLine="true", ToolTip="Multiplied over the fog inscattering and directional inscattering color. No effect on sky atmosphere fog colors."))
	FLinearColor AllFogColorsMultiplier = FLinearColor::White;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Volumetric Clouds Apply Fog Late", ToolTip="This applies a renderer cvar (r.VolumetricCloud.ApplyFogLate) which fixes some issues at the renderer level with volumetric clouds rendering behind fog.\r\nOne drawback to this is that it removes fog rendering on top of clouds in the real time sky light capture. This can cause incongruous sky reflections if you have clouds behind heavy fog."))
	bool bVolumetricCloudsApplyFogLate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Base Fog Density", MultiLine="true", ToolTip="When calculating fog density, this is the value which is started with, and then added to using the contribution values below.", UIMin="0", UIMax=".1"))
	double BaseFogDensity = 0.0055;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Foggy Density Contribution", MultiLine="true", ToolTip="Adds to fog density when Fog is high", UIMin="0", UIMax="1"))
	double FoggyDensityContribution = 0.1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Cloudy Density Contribution", MultiLine="true", ToolTip="Adds to fog density when Cloud Coverage is high.", UIMin="0", UIMax=".1"))
	double CloudyDensityContribution = 0.002;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Dust Density Contribution", MultiLine="true", ToolTip="Adds to fog density with Dust Amount", UIMin="0", UIMax="1"))
	double DustDensityContribution = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Scale Fog Density", MultiLine="true", ToolTip="An overall multiplier for fog density", UIMin="0", ClampMin="0", UIMax="5"))
	double ScaleFogDensity = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Min Fog Start Distance", MultiLine="true", UIMin="0", UIMax="100000", ToolTip="The min value for fog start distance, the distance from the camera height fog (not volumetric fog) will start rendering."))
	double MinFogStartDistance = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Max Fog Start Distance", MultiLine="true", UIMin="0", UIMax="100000", ToolTip="The max value for fog start distance, the distance from the camera height fog (not volumetric fog) will start rendering."))
	double MaxFogStartDistance = 15000.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Min Fog Start Distance Density", MultiLine="true", ToolTip="Fog start distance will go from Max Fog Start Distance to Min Fog Start Distance, as fog density approaches this value.", UIMin=".02", UIMax="1", ClampMin=".0001"))
	double MinFogStartDistanceDensity = 0.08;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Fog Density Daytime Mutliplier", MultiLine="true", ToolTip="A multiplier for overall fog density in the day.", UIMin="0", UIMax="2"))
	double FogDensityDaytimeMultiplier = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Fog Density Nighttime Multiplier", MultiLine="true", ToolTip="A multiplier for overall fog density at night.", UIMin="0", UIMax="2"))
	double FogDensityNighttimeMultiplier = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Base Height Fog Falloff", MultiLine="true", ToolTip="The height fog falloff when neither cloudy or foggy.\r\nHeight fog falloff determines how fog density will decrease/increase with height relative to the fog component (within the UDS actor).", UIMin="0 ", UIMax=".5"))
	double BaseHeightFogFalloff = 0.065;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Cloudy Height Fog Falloff", MultiLine="true", ToolTip="The height fog falloff, when cloudy, but not foggy.\r\nHeight fog falloff determines how fog density will decrease/increase with height relative to the fog component (within the UDS actor).", UIMin="0", UIMax=".5"))
	double CloudyHeightFogFalloff = 0.05;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Foggy Height Fog Falloff", MultiLine="true", ToolTip="The height fog falloff when foggy.\r\nHeight fog falloff determines how fog density will decrease/increase with height relative to the fog component (within the UDS actor).", UIMin="0", UIMax=".5"))
	double FoggyHeightFogFalloff = 0.015;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Dusty Height Fog Falloff", MultiLine="true", ToolTip="The height fog falloff when Dust Amount is high.\r\nHeight fog falloff determines how fog density will decrease/increase with height relative to the fog component (within the UDS actor).", UIMin="0", UIMax=".5"))
	double DustyHeightFogFalloff = 0.03;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟸  ·  Fog Density", meta=(DisplayName="Disable Height Fog Above Volumetric Cloud Layer", MultiLine="true", ToolTip="Controls Fog Max Opacity based on the camera's position relative to the volumetric cloud layer. Above the cloud layer it will be completely disabled."))
	bool bDisableHeightFogAboveVolumetricCloudLayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟺  ·  Dust", meta=(DisplayName="Dust Amount", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Affects fog color using the below Dust Color and adds to fog density using \"Dust Density Contribution\" in Fog Density. If Ultra Dynamic Weather is in the level, this variable is controlled from UDW with the weather state.", ClampMax="1", ClampMin="0"))
	FArcModifiableDouble DustAmount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟺  ·  Dust", meta=(DisplayName="Dust Color", MultiLine="true", ToolTip="The color applied to fog colors using Dust Amount"))
	FLinearColor DustColor = FLinearColor(0.260417f, 0.21676f, 0.184462f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Volumetric Fog Extinction", MultiLine="true", UIMin="0", UIMax="15", ToolTip="The extinction/thickness of the volumetric fog, as scaled by fog density."))
	double VolumetricFogExtinction = 2.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Sun Volumetric Scattering Intensity", UIMin="0", UIMax="50", ToolTip="The amount of light the sun contributes to the volumetric fog."))
	double SunVolumetricScatteringIntensity = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Moon Volumetric Scattering Intensity", UIMin="0", UIMax="50", ToolTip="The amount of light the moon contributes to the volumetric fog."))
	double MoonVolumetricScatteringIntensity = 1.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Volumetric Fog Albedo (Day)", ToolTip="The albedo of the volumetric fog, which tints the color of light it recieves from light sources. This value is used during the day."))
	FLinearColor VolumetricFogAlbedoDay = FLinearColor::White;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Volumetric Fog Albedo (Dawn/ Dusk)", ToolTip="The albedo of the volumetric fog, which tints the color of light it recieves from light sources. This value is used around dawn and dusk."))
	FLinearColor VolumetricFogAlbedoDawnDusk = FLinearColor::White;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Volumetric Fog Albedo (Night)", ToolTip="The albedo of the volumetric fog, which tints the color of light it recieves from light sources. This value is used at night."))
	FLinearColor VolumetricFogAlbedoNight = FLinearColor::White;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Render Global Volumetric Material", MultiLine="true", ToolTip="Enables a volumetric material, rendered everywhere, to apply effects to volumetric fog like 3D noise, ground fog, and water fog, as well as masking with Weather Mask actors.\r\nCannot apply if Project Mode is VR and Instanced Stereo is enabled in project settings."))
	bool bRenderGlobalVolumetricMaterial = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Noise Scale", MultiLine="true", UIMin="100", UIMax="6000", ToolTip="The scale of the 3D noise used for the global volumetric fog material"))
	double FogNoiseScale = 2000.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Noise Intensity", MultiLine="true", UIMin="0", UIMax="5", ToolTip="The amount that the 3D noise should affect the extinction of the volumetric fog."))
	double FogNoiseIntensity = 2.5;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Noise Max Samples", MultiLine="true", UIMax="8", UIMin="0", ToolTip="The number of 3D noise samples used for volumetric fog right next to the camera. Determines the max level of detail of the 3D noise."))
	double FogNoiseMaxSamples = 4.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Noise Speed Scale", MultiLine="true", UIMin="0", UIMax="2", ToolTip="Scales how fast the fog noise will move"))
	double FogNoiseSpeedScale = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Noise Offset", ToolTip="Added to the fog noise mapping coordinates. Can be used to keyframe the movement of fog noise directly, independent of cloud movement."))
	FVector FogNoiseOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Noise Outer Distance", ToolTip="The distance which the 3D noise samples for the Global Volumetric Material reduce to 0", ForceUnits="Centimeters", UIMin="1", UIMax="10000"))
	double FogNoiseOuterDistance = 4000.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Fog Masked Extinction", ToolTip="Used for the extinction of the global volumetric fog mat in space masked by the weather mask. This is so if you're using the weather mask to mask the sky/weather fog extinction inside an interior, you can have a constant level of extinction which remains in the interior.", UIMin="0", UIMax=".05"))
	double FogMaskedExtinction = 0.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Extra Fog Extinction (Dusty)", UIMin="0", UIMax="1", ToolTip="Adds extra fog extinction into the global volumetric material, with the Dust Amount."))
	double ExtraFogExtinctionDusty = 0.5;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Extra Fog Extinction (Rainy)", UIMin="0", UIMax="1"))
	double ExtraFogExtinctionRainy = 0.4;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Extra Fog Extinction (Snowy)", UIMin="0", UIMax="1"))
	double ExtraFogExtinctionSnowy = 0.4;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Render Ground Fog (Fog Above Distance Fields)", MultiLine="true", ToolTip="If enabled, the Global Volumetric Material will sample the global distance field to have fog form in the air immediately above them."))
	bool bRenderGroundFog = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Extinction (Foggy)", MultiLine="true", UIMin="0", UIMax="1", ToolTip="The amount of extinction added to the ground fog layer as Fog is increased."))
	double GroundFogExtinctionFoggy = 0.2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Extinction (Dusty)", MultiLine="true", UIMin="0", UIMax="1", ToolTip="The amount of extinction added to the ground fog layer as Dust Amount is increased."))
	double GroundFogExtinctionDusty = 0.3;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Extinction (Rainy)", UIMin="0", UIMax="1"))
	double GroundFogExtinctionRainy = 0.1;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Extinction (Snowy)", UIMin="0", UIMax="1"))
	double GroundFogExtinctionSnowy = 0.1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Extinction (Manual)", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Added to the ground fog extinction, regardless of weather/fog", ClampMin="0"))
	double GroundFogExtinctionManual = 0.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Height", MultiLine="true", UIMin="10", UIMax="200", ToolTip="The height from the distance fields that the ground fog will be visible.", ForceUnits="Centimeters"))
	double GroundFogHeight = 70.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Height Noise", MultiLine="true", UIMin="0", UIMax="4", ToolTip="Scales how much the 3D noise is used to modulate the ground fog height value."))
	double GroundFogHeightNoise = 2.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Angle Threshold", MultiLine="true", UIMin="0", UIMax="2", ToolTip="Increasing will allow ground fog to form against distance fields at steeper angles."))
	double GroundFogAngleThreshold = 0.2;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙵𝟹  ·  Volumetric Fog", meta=(DisplayName="Ground Fog Hardness", MultiLine="true", UIMin="1", UIMax="7", ToolTip="Hardens the border of the ground fog, creating sharper edged \"clouds\" of fog."))
	double GroundFogHardness = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Rayleigh Scattering Color (Day)", MultiLine="true", ToolTip="The base rayleigh scattering color for the sky atmosphere component, during the day."))
	FLinearColor RayleighScatteringColorDay = FLinearColor(0.168627f, 0.407843f, 1.0f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Rayleigh Scattering Color (Dawn/ Dusk)", MultiLine="true", ToolTip="The base rayleigh scattering color for the sky atmosphere component, during the dawn/dusk hours."))
	FLinearColor RayleighScatteringColorDawnDusk = FLinearColor(0.17f, 0.409234f, 1.0f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Rayleigh Scattering Color (Night)", MultiLine="true", ToolTip="The base rayleigh scattering color for the sky atmosphere component, during the night."))
	FLinearColor RayleighScatteringColorNight = FLinearColor(0.241211f, 0.347328f, 0.609375f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Desaturate Rayleigh Scattering When Cloudy", MultiLine="true", ToolTip="Reduces saturation on the rayleigh scattering color when cloudy"))
	double DesaturateRayleighScatteringWhenCloudy = 0.4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Sunset/ Sunrise Color (Absorption)", MultiLine="true", ToolTip="This is used to set the Absorption variable on the Sky Atmosphere component, which mainly has the effect of tinting the sky color when the sun or moon is low in the sky. Can be used in conjuction with the intensity value below to achieve dramatically colored sunsets. Also note that this color isn't delivered directly to the sky atmosphere, but has its hue inverted first. This is because the actual color for absorption will be the opposite of the one visible, because the absorbed light is what the value represents. So this inverting is done for convenience."))
	FLinearColor SunsetSunriseAbsorptionColor = FLinearColor(0.198069f, 0.095307f, 1.0f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Sunset/ Sunrise Color Intensity (Absorption Scale)", MultiLine="true", UIMin="0", UIMax=".018", ToolTip="Controls the Absorption Intensity of the Sky Atmosphere component. Primarily affects how much of the Sunset/Sunrise Color (Absorption) is applied when the sun or moon is low in the sky."))
	double SunsetSunriseAbsorptionIntensity = 0.002;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Twilight Color (Absorption)", MultiLine="true", ToolTip="Replaces the \"Sunset/Sunrise Color\" when the sun goes below the horizon. Allows specific control of the sky color during twilight."))
	FLinearColor TwilightAbsorptionColor = FLinearColor(0.030354f, 0.181178f, 1.0f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Twilight Color Intensity (Absorption Scale)", MultiLine="true", UIMin="0", UIMax=".018", ToolTip="Replaces the \"Sunset/Sunrise Color Intensity\" when the sun is below the horizon. Allows specific control of sky color during twilight."))
	double TwilightAbsorptionIntensity = 0.004;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Mie Scattering Scale Multiplier", ToolTip="Multiplied over the calculated value for Mie Scattering Scale", UIMin="0", UIMax="3"))
	double MieScatteringScaleMultiplier = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Sky Atmosphere Overcast Luminance", MultiLine="true", ToolTip="Scales sky atmosphere luminance during high cloud coverage", UIMin=".2", UIMax="10", ClampMin="0"))
	double SkyAtmosphereOvercastLuminance = 1.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Keep Planet Top At Camera XY Location", MultiLine="true", ToolTip="Keeps the sky atmosphere's planet \"top\" at the X and Y location of the camera, to help with making extremely large flat worlds."))
	bool bKeepPlanetTopAtCameraXYLocation = true;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Control Sky Atmosphere Settings", MultiLine="true", ToolTip="Can be used to toggle off UDS' control of the settings on its sky atmosphere component."))
	bool bControlSkyAtmosphereSettings = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Mie Anisotropy Multiplier", ToolTip="Multiplied over the calculated value for Mie anisotropy", UIMin="0", UIMax="1", ClampMin="0"))
	double MieAnisotropyMultiplier = 1.0;
	UPROPERTY(EditDefaultsOnly, Interp, BlueprintReadWrite, Category="𝙶𝟷  ·  Sky Atmosphere", meta=(DisplayName="Sky Atmosphere Density Curve", BlueprintPrivate="true"))
	TObjectPtr<UCurveFloat> SkyAtmosphereDensityCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Base Sky Color (Day)", MultiLine="true", ToolTip="If using the color mode Simplified Color, this is the base sky color, before scattering is applied, during the day."))
	FLinearColor BaseSkyColorDay = FLinearColor(0.050313f, 0.100844f, 0.21875f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Base Sky Color (Dawn/ Dusk)", MultiLine="true", ToolTip="If using the color mode Simplified Color, this is the base sky color, before scattering is applied, at dawn/dusk."))
	FLinearColor BaseSkyColorDawnDusk = FLinearColor(0.094145f, 0.167055f, 0.260417f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Base Sky Color (Night)", MultiLine="true", ToolTip="If using the color mode Simplified Color, this is the base sky color, before scattering is applied,  at night."))
	FLinearColor BaseSkyColorNight = FLinearColor(0.065593f, 0.102341f, 0.161458f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Cloud Light Color (Day)", ToolTip="If using the color mode Simplified Color, this is the color used for 2D and static clouds light color, during the day."))
	FLinearColor CloudLightColorDay = FLinearColor(0.708631f, 0.785507f, 0.901042f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Cloud Light Color (Dawn/ Dusk)", ToolTip="If using the color mode Simplified Color, this is the color used for 2D and static clouds light color, at dawn/dusk."))
	FLinearColor CloudLightColorDawnDusk = FLinearColor(1.0f, 0.477468f, 0.054318f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Cloud Light Color (Night)", ToolTip="If using the color mode Simplified Color, this is the color used for 2D and static clouds light color, at night."))
	FLinearColor CloudLightColorNight = FLinearColor(1.177604f, 1.321682f, 1.7f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Cloud Dark Color (Day)", ToolTip="If using the color mode Simplified Color, this is the color used for 2D and static clouds dark color, during the day."))
	FLinearColor CloudDarkColorDay = FLinearColor(0.034505f, 0.119041f, 0.276042f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Cloud Dark Color (Dawn/ Dusk)", ToolTip="If using the color mode Simplified Color, this is the color used for 2D and static clouds dark color, at dawn/dusk."))
	FLinearColor CloudDarkColorDawnDusk = FLinearColor(0.04305f, 0.06991f, 0.119792f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Cloud Dark Color (Night)", ToolTip="If using the color mode Simplified Color, this is the color used for 2D and static clouds dark color, at night."))
	FLinearColor CloudDarkColorNight = FLinearColor(0.070258f, 0.10947f, 0.182292f, 1.0f);
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Flatten Simplified Color Scattering", ToolTip="Flattens the atmospheric scattering across the sky with Simplified Color, resulting in more flat sky colors for stylized looks.", UIMin="0", UIMax="1"))
	double FlattenSimplifiedColorScattering = 0.0;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Directional Intensity Curve", ToolTip="If using the color mode Simplified Color, this curve defines how the directional lights are dimmed as they near the horizon. The input for this curve is the Z channel of the light's forward vector."))
	TObjectPtr<UCurveFloat> DirectionalIntensityCurve;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Directional Light Scattering Curve", ToolTip="If using the color mode Simplified Color, this curve defines how the light color is affected as the sun or moon nears the horizon. The input for this curve is a 0 to 1 range, with 0 being the sun pointing straight up, and 1 being the sun pointing straght down."))
	TObjectPtr<UCurveLinearColor> DirectionalLightScatteringCurve;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Simplified Color Sun Atmosphere LUT", ToolTip="The color LUT texture for determining atmospheric scattering for the sun light, for Simplified Color."))
	TObjectPtr<UVolumeTexture> SimplifiedColorSunAtmosphereLUT;
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙶𝟸  ·  Simplified Color", meta=(DisplayName="Simplified Color Moon Atmosphere LUT", ToolTip="The color LUT texture for determining atmospheric scattering for the moon light, for Simplified Color."))
	TObjectPtr<UVolumeTexture> SimplifiedColorMoonAtmosphereLUT;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Volumetric Cloud Rendering Mode", MultiLine="true", ToolTip="This setting controls engine cvars (r.VolumetricRenderTarget.Mode and r.VolumetricRenderTarget) which determine how volumetric clouds are rendered. It has performance implications and each of these modes also has very different amounts and types of visual artifacting. Be sure to read the tooltips for each and choose a mode which is appropriate for your project."))
	EArcTimeOfDayVolumetricRenderingMode VolumetricCloudRenderingMode = EArcTimeOfDayVolumetricRenderingMode::FidelityAndPerformance;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Bottom Altitude", MultiLine="true", ToolTip="The height relative to UDS, in kilometers, where the bottom of the volumetric cloud layer sits.", UIMin="-2", UIMax="2", ForceUnits="Kilometers"))
	double BottomAltitude = 0.6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Volumetric Clouds Scale", MultiLine="true", UIMin=".05", UIMax="5", ToolTip="Scales the size of all volumetric cloud elements together. Increasing this will basically scale the entire volumetric cloud field up."))
	double VolumetricCloudsScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Layer Height Scale", MultiLine="true", ToolTip="Scales up the height of the volumetric cloud layer, stretching it vertically.", UIMin=".1", UIMax="2"))
	double LayerHeightScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Cloud Formation Texture Scale", MultiLine="true", ToolTip="The scale of the cloud formation textures on the X and Y axes, determining the major cloud shapes.", UIMin=".2", UIMax="3", ClampMin="0"))
	double CloudFormationTextureScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Cloud Formation Z Shift", UIMin="0", UIMax="0.4", ToolTip="The cloud formation can mutate with Z height within the cloud layer. This scales the amount of that effect.\nUsing large amounts of this effect can make cloud shadows less accurate"))
	double CloudFormationZShift = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Additional Cloud Layers", ToolTip="Each element in this array is an additional cloud layer. These layers are rendered relative to the main layer (either above or below it), and are essentially adjusted copies of the main layer.\nNote that adding additional cloud layers increases the GPU cost of cloud rendering."))
	TArray<FArcTimeOfDayAdditionalCloudLayer> AdditionalCloudLayers;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="3D Noise Scale", MultiLine="true", ToolTip="The scale of the 3D tiling noise texture used for erosion of the cloud formations.", UIMin=".3", UIMax="4", ClampMin="0"))
	double ThreeDNoiseScale = 0.9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="3D Erosion Intensity", MultiLine="true", ToolTip="The amount of the 3D erosion to apply to the cloud formations. Scaled by the B channel of the cloud profile LUT.", UIMin="0", UIMax="2"))
	double ThreeDErosionIntensity = 1.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="High Frequency Noise Amount", MultiLine="true", ToolTip="Amount of the high frequency level of 3D texture detail.", UIMin="-.5", UIMax="0.5"))
	double HighFrequencyNoiseAmount = 0.24;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="High Frequency Noise Distance Scale", ToolTip="Scales the distance over which high frequency noise samples will reduce to 0. This is also scaled with field of view, to allow distant clouds to get appropriate noise levels when zoomed in on.\nThis setting does affect the cost of cloud rendering, with larger values costing more, due to adding additional noise samples to more distant clouds.", UIMin="10000", UIMax="500000", ClampMin="1"))
	double HighFrequencyNoiseDistanceScale = 150000.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Max High Frequency Noise Levels", UIMin="0", UIMax="3", ToolTip="The number of high frequency noise levels will scale with the sample distance from the camera and the field of view. This value clamps how high that number of successive noise samples can go. Will have an effect on cost of cloud rendering, mainly when close up to clouds.", ClampMax="5"))
	int32 MaxHighFrequencyNoiseLevels = 2;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Distort High Frequency Noise", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Distorts the high frequency noise using the low frequency noise"))
	double DistortHighFrequencyNoise = 0.21;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Extinction Scale", MultiLine="true", UIMin=".1", UIMax="25", ToolTip="Multiplied over the extinction value coming from the G channel of the Cloud Profile LUT texture. Extinction is what makes the clouds look harder/sharper when higher, or hazier/wispier when low."))
	double ExtinctionScale = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Base Floor Variation", MultiLine="true", UIMin="0", UIMax="1.5", ToolTip="Uses the cloud formation texture to vary the height level of the bottom of the clouds. This is the value used when there is only partial cloud coverage."))
	double BaseFloorVariation = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Overcast Floor Variation", MultiLine="true", UIMin="0", UIMax="1.5", ToolTip="Uses the cloud formation texture to vary the height level of the bottom of the clouds. This is the value used when there is overcast cloud coverage."))
	double OvercastFloorVariation = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Floor Luminance Scale", MultiLine="true", ToolTip="Scales how much the floor variation will affect the luminance of the clouds.", UIMin="0", UIMax="2"))
	double FloorLuminanceScale = 1.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="View Sample Scale (Day)", MultiLine="true", ToolTip="Scales the amount of samples used to render the clouds. Lower is worse looking but more performant. Higher is better looking but more expensive. This value is used in the day.", UIMin="0", UIMax="10"))
	double ViewSampleScaleDay = 2.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="View Sample Scale (Night)", MultiLine="true", ToolTip="Scales the amount of samples used to render the clouds. Lower is worse looking but more performant. Higher is better looking but more expensive. This value is used at night.", UIMin="0", UIMax="10"))
	double ViewSampleScaleNight = 1.8;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Shadow Sample Scale", MultiLine="true", ToolTip="Scales the amount of samples used for shadows within the clouds. Lower is worse looking but more performant. Higher is better looking but more expensive.", UIMin=".1", UIMax="2"))
	double ShadowSampleScale = 0.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Shadow Tracing Distance", MultiLine="true", ToolTip="The max distance the clouds will cast shadows onto each other through the volumetric layer. In kilometers.", UIMin="0", UIMax="3"))
	double ShadowTracingDistance = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Tracing Max Distance (Outside Cloud Layer)", MultiLine="true", ToolTip="The max distance rays wll trace through the cloud layer. Used when the camera is not within the altitude range of the cloud layer.", UIMin="2", UIMax="200", ForceUnits="Kilometers", ClampMin=".1"))
	double TracingMaxDistanceOutsideCloudLayer = 50.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Tracing Max Distance (Inside Cloud Layer)", UIMin="2", UIMax="50", ToolTip="The max distance rays wll trace through the cloud layer. Used when the camera is within the altitude range of the cloud layer.", ClampMin=".1", ForceUnits="Kilometers"))
	double TracingMaxDistanceInsideCloudLayer = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Volumetric Cloud Color", MultiLine="true", ToolTip="The albedo of the volumetric clouds. Affects the color recieved by the directional lights."))
	FLinearColor VolumetricCloudColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Cloud Profile LUT", MultiLine="true", ToolTip="A lookup table which defines the profile of the cloud shapes with cloud coverage. Use the \"Cloud Profile Authoring Tool\" to make your own profile LUT textures. That utility is found in Blueprints/Tools."))
	TObjectPtr<UTexture> CloudProfileLUT;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Volumetric Cloud Formation Volume Texture", ToolTip="The texture which defines the main cloud formations for the volumetric clouds. The X and Y axes are the spatial XY space of the cloud field. The Z axis contains a looping animation of the changing cloud shapes, to be sampled based on time/cloud movement settings."))
	TObjectPtr<UVolumeTexture> VolumetricCloudFormationVolumeTexture;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Minimum Erosion", MultiLine="true", ToolTip="Clamp the minimum amount of 3D erosion erosion intensity as it varies with height..", UIMin="0", UIMax="1"))
	double MinimumErosion = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Cloud Formation Mip Level", ToolTip="The mip level sampled for the cloud formation texture. (Higher values use a lower resolution/low detail sample)", UIMin="0", UIMax="4"))
	double CloudFormationMipLevel = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Reflection Sample Count Scale", MultiLine="true", ToolTip="Scales the amount of samples used to render the clouds in reflection/sky light captures.", UIMin="0", UIMax="2"))
	double ReflectionSampleCountScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Reflection Shadow Sample Count Scale", MultiLine="true", ToolTip="Scales the shadow samples used in reflection/sky light captures.", UIMin="0", UIMax="2"))
	double ReflectionShadowSampleCountScale = 0.3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Multiscattering Light Intensity", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Scales the intensity of the multiscattering light pass."))
	double MultiscatteringLightIntensity = 0.85;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Multiscattering Light Occlusion", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Scales the amount of multiscattered light the clouds absorb as rays pass through them."))
	double MultiscatteringLightOcclusion = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="3D Noise Vertical Movement", MultiLine="true", ToolTip="Controls how much the 3D Noise moves vertically with cloud movement", UIMin="-1", UIMax="1"))
	double ThreeDNoiseVerticalMovement = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Macro Variation", MultiLine="true", ToolTip="A very simple large scale variation which varies cloud density in big patches across the sky. Increase to have heavier variation, decrease to reduce variation.", UIMin="-1", UIMax="1"))
	double MacroVariation = 0.16;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Macro Scale", MultiLine="true", ToolTip="Scales the pattern used for macro variation, relative to cloud formation texture scale.", UIMin=".5", UIMax="5"))
	double MacroScale = 1.3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Multiscattering Phase 1", MultiLine="true", ToolTip="The amount the first phase function is scattered.", UIMin="-1", UIMax="1"))
	double MultiscatteringPhase1 = 0.85;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Multiscattering Phase 2", MultiLine="true", ToolTip="The amount the second phase function is scattered.", UIMin="-1", UIMax="1"))
	double MultiscatteringPhase2 = 0.4;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Multiscattering Eccentricity", MultiLine="true", ToolTip="Determines how directional multiscattering remains after successive octaves of scattering.", UIMin="0", UIMax="1"))
	double MultiscatteringEccentricity = 0.4;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Tracing Max Start Distance", MultiLine="true", ToolTip="The max distance that rays will start being cast though the volumetric cloud volume. If you need to render clouds that are further away, try raising this.", ForceUnits="Kilometers"))
	double TracingMaxStartDistance = 100.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds|Inside Cloud Fog", meta=(DisplayName="Enable Fog Inside Clouds", MultiLine="true", ToolTip="Enable an effect which adds a fog effect inside clouds. Achieved using volumetric fog if enabled, and a post process effect if not."))
	bool bEnableFogInsideClouds = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds|Inside Cloud Fog", meta=(DisplayName="Inside Cloud Fog Strength", MultiLine="true", UIMin="0", UIMax="3", ToolTip="Scales the strength of the fog applied inside clouds, if the above option for it is enabled."))
	double InsideCloudFogStrength = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds|Inside Cloud Fog", meta=(DisplayName="Inside Cloud Fog Color", MultiLine="true", ToolTip="If inside cloud fog is enabled, this acts as a color multiplier for it."))
	FLinearColor InsideCloudFogColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="2D Overcast Turbulence", MultiLine="true", UIMax="3", UIMin="0", ToolTip="Adds an additional layer of 2D texture detail to overcast skies, for extra detail without making the volumetric rendering itself more complex"))
	double TwoDOvercastTurbulence = 0.8;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Close Fade Distance", MultiLine="true", ToolTip="Volumetric clouds will fade as they get closer to the camera than this distance.", UIMin="1", UIMax="200000", ForceUnits="Centimeters"))
	double CloseFadeDistance = 100000.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Close Fade Offset", MultiLine="true", ToolTip="Offsets the start of the close fade further out from the camera", UIMin="0", UIMax="200000", ForceUnits="Centimeters"))
	double CloseFadeOffset = 5000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Ambient Light from Sky Atmosphere", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Scales the sky atmosphere's direct contribution to the volumetric clouds ambient light. UDS' default settings use alternate logic for more flexible ambient light colors, but you can enable this sky atmo contribution with this option if you want."))
	double AmbientLightFromSkyAtmosphere = 0.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Change to Simplified Material when Overcast", MultiLine="true", ToolTip="Removes the 3D texture samples from the volumetric cloud shader when cloud coverage reaches the threshold value below. This is a very significant optimization with almost no reduction in visual quality, because when fully overcast the 3D noise of the clouds is mostly lost anyway."))
	bool bChangeToSimplifiedMaterialWhenOvercast = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Simplified Material Cloud Coverage Threshold", MultiLine="true", UIMin="1.8", UIMax="3", ClampMin="0", ToolTip="The cloud coverage level where the volumetric cloud material will change to the simplified version, if the above option is enabled."))
	double SimplifiedMaterialCloudCoverageThreshold = 2.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Simplified Material Fade Length", MultiLine="true", ClampMin="0.01", UIMin=".01", ToolTip="The amount of the cloud coverage range to fade over to to the simplified shader. This fade just exists to ease the transition. The performance benefits of the change will only kick in when the threshold value above is reached.", UIMax="1.5"))
	double SimplifiedMaterialFadeLength = 0.2;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="View Ray Sample Max Count", UIMin="1", UIMax="5000", ClampMin="1", ToolTip="Clamps the number of samples volumetric rendering can reach."))
	int32 ViewRaySampleMaxCount = 2000;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Distance to Sample Max Count", ForceUnits="Kilometers", UIMin="1", UIMax="50", ToolTip="The distance at which the volumetric cloud rendering will use its max sample count."))
	double DistanceToSampleMaxCount = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Scale View Samples when Overcast", MultiLine="true", ToolTip="When completely overcast, the clouds view sample scale will be multiplied by this amount. Can reduce cost of cloud rendering when high sample count is unneeded.", UIMin="0.1", UIMax="2"))
	double ScaleViewSamplesWhenOvercast = 0.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Scale View Samples when Camera is In Cloud Layer", MultiLine="true", ToolTip="View Sample Scale will be multiplied by this value when the camera is within the volumetric cloud layer. Can be used to add more samples as needed to correct for additional artifacting which can happen inside the layer.", UIMin="0.1", UIMax="5"))
	double ScaleViewSamplesWhenCameraIsInCloudLayer = 1.75;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Low Material Quality Sample Count Scale", MultiLine="true", ToolTip="Scales the sample quality of the volumetric cloud rendering when Global Material Quality is set to Low.", UIMin="0.5", UIMax="1"))
	double LowMaterialQualitySampleCountScale = 0.8;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Medium Material Quality Sample Count Scale", MultiLine="true", ToolTip="Scales the sample quality of the volumetric cloud rendering when Global Material Quality is set to Medium.", UIMin=".5", UIMax="1"))
	double MediumMaterialQualitySampleCountScale = 0.9;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Epic Material Quality Sample Count Scale", MultiLine="true", ToolTip="Scales the sample quality of the volumetric cloud rendering when Global Material Quality Level is set to Epic.", UIMin="0.5", UIMax="4"))
	double EpicMaterialQualitySampleCountScale = 1.15;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="2D Overcast Turbulence Texture", ToolTip="The texture used for the 2D Overcast Turbulence effect"))
	TObjectPtr<UTexture2D> TwoDOvercastTurbulenceTexture;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Epic Quality Volume Texture", MultiLine="true", ToolTip="The 3D volume texture used to erode the clouds on Epic material quality level."))
	TObjectPtr<UVolumeTexture> EpicQualityVolumeTexture;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="High Quality Volume Texture", MultiLine="true", ToolTip="The 3D volume texture used to erode the clouds on Medium and High material volume level."))
	TObjectPtr<UVolumeTexture> HighQualityVolumeTexture;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Low Quality Volume Texture", MultiLine="true", ToolTip="The 3D volume texture used to erode the clouds on Low material quality level."))
	TObjectPtr<UVolumeTexture> LowQualityVolumeTexture;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Disable Sky/Fog Cloudiness Above Cloud Layer", MultiLine="true", ToolTip="If enabled, when the camera goes above the cloud layer, the effects of cloud coverage on things like fog, lighting, sky color, etc, will fade out."))
	bool bDisableSkyFogCloudinessAboveCloudLayer = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟷  ·  Volumetric Clouds", meta=(DisplayName="Custom Volumetric Cloud Material", ToolTip="If this has a valid object, the material selected will be used instead of the included UDS cloud mats. You could potentially author your own new material driven by the same parameters UDS uses. But I think the most likely use for this will be for a modified duplicate of the UDS cloud material. That material is \"Volumetric Clouds\" in the Materials folder."))
	TObjectPtr<UMaterialInterface> CustomVolumetricCloudMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Opacity (Clear)", UIMin="0", UIMax="1", ClampMin="0", ToolTip="The opacity of the wispy static layer of clouds behind the dynamic ones. This value is used when there is no cloud coverage."))
	double CloudWispsOpacityClear = 0.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Opacity (Cloudy)", UIMin="0", UIMax="1", ClampMin="0", ToolTip="The opacity of the wispy static layer of clouds behind the dynamic ones. This value is used when there is medium cloud coverage."))
	double CloudWispsOpacityCloudy = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Texture", MultiLine="true", ToolTip="The texture to use for the background cloud wisps"))
	TObjectPtr<UTexture2D> CloudWispsTextureSetting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Color Intensity", MultiLine="true", ToolTip="Scales the brightness of the color of the cloud wisps", UIMin="0", UIMax="4"))
	double CloudWispsColorIntensity = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Tint (Day)", MultiLine="true", ToolTip="The tint of the cloud wisps layer, during the day"))
	FLinearColor CloudWispsTintDay = FLinearColor(0.476966f, 0.473985f, 0.526042f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Tint (Dawn/Dusk)", MultiLine="true", ToolTip="The tint of the cloud wisps layer, at sunset/sunrise"))
	FLinearColor CloudWispsTintDawnDusk = FLinearColor(1.0f, 0.587119f, 0.395833f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Tint (Night)", MultiLine="true", ToolTip="The tint of the cloud wisps layer, at night"))
	FLinearColor CloudWispsTintNight = FLinearColor(0.367187f, 0.428386f, 0.489583f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Movement Effect", ToolTip="Toggle an effect which warps the cloud wisps texture over time with cloud movement."))
	bool bCloudWispsMovementEffect = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Movement Period", UIMin="2", UIMax="20", ToolTip="Defines how frequently the cloud wisps movement effect will loop"))
	double CloudWispsMovementPeriod = 10.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Cloud Wisps Movement Intensity", UIMax=".5", UIMin=".05", ToolTip="Scales the amount that the cloud wisps movement effect will warp the texture."))
	double CloudWispsMovementIntensity = 0.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Increase Wisps Brightness Around Sun", MultiLine="true", ToolTip="Pushes the intensity of the cloud wisps higher around the sun.", UIMin="0", UIMax="10"))
	double IncreaseWispsBrightnessAroundSun = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙲𝟼  ·  Cloud Wisps", meta=(DisplayName="Increase Wisps Brightness Around Moon", MultiLine="true", ToolTip="Pushes the intensity of the cloud wisps higher around the moon.", UIMin="0", UIMax="10"))
	double IncreaseWispsBrightnessAroundMoon = 3.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Use Cloud Shadows", ToolTip="Applies a light function to the sun and moon directional lights which approximates cloud shadows. For volumetric clouds, these shadows are accurate to the actual 3D cloud layer shapes. For the other modes, the general coverage level is approximated. Note that cloud shadows can be enabled for the sky modes without clouds."))
	bool bUseCloudShadows = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Cloud Shadows Intensity When Sunny", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Darkness of the cloud shadows when Cloud Coverage is low."))
	double CloudShadowsIntensityWhenSunny = 0.7;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Cloud Shadows Intensity When Overcast", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Darkness of the cloud shadows when Cloud Coverage is high"))
	double CloudShadowsIntensityWhenOvercast = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Cloud Shadows Softness", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="How soft the edges of the cloud shadows are"))
	double CloudShadowsSoftness = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Fog Shadows", MultiLine="true", ToolTip="Controls how much heavy fog will attenuate directional light from the sun and moon.", UIMin="0", UIMax="1"))
	double FogShadows = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Dust Shadows", MultiLine="true", ToolTip="Controls how much Dust Amount will attenuate directional light from the sun and moon.", UIMin="0", UIMax="1"))
	double DustShadows = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="2D Cloud Shadows Speed", ToolTip="Scales the speed of cloud shadows, for modes other than Volumetric Clouds", UIMin="0.001", UIMax="5"))
	double TwoDCloudShadowsSpeed = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="2D Cloud Shadows Scale", ToolTip="The world space scaling of the cloud shadows, centered on the location of the sun light. This value is not used when Volumetric Clouds are selected. The real scale of the clouds is used then.", UIMin="5000", ClampMin="1", UIMax="850000"))
	double TwoDCloudShadowsScale = 450000.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="2D Cloud Shadows Light Angle Offset Scale", ToolTip="Scales the amount which 2D cloud shadows will offset with the angle of the sun/moon light. This offset is relative to the Fallback Cloud Layer Altitude.\nIf this is set to 0, the 2D cloud shadows will not offset with the angle of the light, and will appear to be projected straight down from the Z axis.", UIMin="0", UIMax="1"))
	double TwoDCloudShadowsLightAngleOffsetScale = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Correct Specular Scale for Low Angle Cloud Shadows", ToolTip="If enabled, specular scale on the Sun and Moon will turn down when the light is low to the horizon and there is significant cloud coverage. This can help avoid obvious mismatches between cloud shadows and the directional specular of the light, especially on large distant reflective surfaces like an ocean.\nIf using a realtime captured sky light, the sun disk will be enabled in the reflection capture when specular scale is reduced, to stand in for sun reflections as well as it can."))
	bool bCorrectSpecularScaleForLowAngleCloudShadows = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Disable Directional Light Shadows when Fully Shadowed by Clouds", MultiLine="true", ToolTip="If the directional light is completely occluded everywhere by the cloud shadows (to the point that the directional light is having no visible effect on the lighting) the shadows on the light will be disabled, as an optimization."))
	bool bDisableDirectionalLightShadowsWhenFullyShadowedByClouds = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Custom Light Function Material", ToolTip="Supply a custom light function material, to replace the default ones used for cloud shadows (and water caustics)"))
	TObjectPtr<UMaterialInterface> CustomLightFunctionMaterial;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Force Light Functions On", MultiLine="true", ToolTip="Bypasses the scalability setting and forces light functions to be enabled. This is because by default the Low scalability profile disables light functions, which breaks cloud shadows."))
	bool bForceLightFunctionsOn = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Light Function Atlas Scale", ForceUnits="Kilometers", ToolTip="The light function scale, which for the UDS light function materials will only affect the scale of the light function atlas if in use.", UIMin=".01"))
	double LightFunctionAtlasScale = 8.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Apply Light Function Atlas Resolution", ToolTip="If enabled, the cvar \"r.LightFunctionAtlas.SlotResolution\" will be set using the value below."))
	bool bApplyLightFunctionAtlasResolution = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙴𝟻  ·  Cloud Shadows", meta=(DisplayName="Light Function Atlas Resolution", ToolTip="If enabled, this controls the cvar \"r.LightFunctionAtlas.SlotResolution\" to set the resolution of the light function atlas."))
	int32 LightFunctionAtlasResolution = 512;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Enable Volumetric Cloud Light Rays", MultiLine="true", ToolTip="Render an effect of light rays coming down through gaps in volumetric cloud coverage. Visible when the local cloud coverage is high."))
	bool bEnableVolumetricCloudLightRays = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Ray Intensity", MultiLine="true", UIMin=".01", UIMax="5", ToolTip="The stength / brightness of the light rays effect."))
	double LightRayIntensity = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Ray Tint Color", MultiLine="true", ToolTip="Multiplied over the color of the light rays effect"))
	FLinearColor LightRayTintColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Rays Point Spacing", MultiLine="true", UIMin="10000", UIMax="80000", ToolTip="Determines the amount of ray particles rendered in the grid. A lower number will be denser, and heavier on performance.", ForceUnits="Centimeters"))
	double LightRaysPointSpacing = 50000.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Ray Length", MultiLine="true", UIMin="0.1", UIMax="3", ToolTip="How far down below the cloud layer the light rays will reach"))
	double LightRayLength = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Individual Clouds Light Rays", MultiLine="true", UIMin="0", UIMax="1", ToolTip="Controls how much light rays are cast down through gaps in individual cloud formations. This is in addition to rays cast through gaps in overall cloud coverage. Increases performance cost."))
	double IndividualCloudsLightRays = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Scale Light Ray Width", MultiLine="true", UIMin=".1", UIMax="3", ToolTip="Scales the width of the individual ray particles"))
	double ScaleLightRayWidth = 1.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Rays Max Distance (Km)", MultiLine="true", ToolTip="The max distance from the camera, in kilometers, in which the light rays effect will be present.", UIMin="1", UIMax="30", ForceUnits="Kilometers"))
	double LightRaysMaxDistanceKm = 12.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Rays Depth Fade Distance", MultiLine="true", UIMin="1000", UIMax="250000", ToolTip="Controls the distance of the scene depth fade in the light ray sprite material.", ForceUnits="Centimeters"))
	double LightRaysDepthFadeDistance = 100000.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Light Rays 3D Noise Scale", MultiLine="true", UIMin="100000", UIMax="1000000", ToolTip="Scale the 3D noise texture used to add variation to the light rays", ForceUnits="Centimeters"))
	double LightRaysThreeDNoiseScale = 700000.0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙺𝟷  ·  Volumetric Cloud Light Rays", meta=(DisplayName="Using Volumetric Light Rays"))
	bool bUsingVolumetricLightRays = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙰𝟷  ·  Basic Controls", meta=(DisplayName="Project Mode", MultiLine="true", ToolTip="Determines how the system will behave at runtime. Game / Real-time mode allows the system to optimize and spread out some of its runtime calculations, to reduce load on the game thread. Cinematic / Offline mode bypasses these optimizations so that nearly everything on the sky does a complete update every frame at runtime. Cinematic mode also automatically adjusts some quality settings at runtime for maximum quality while rendering. See the Cinematic/Offline Rendering category for settings to adjust the specifics of those quality adjustments."))
	EArcTimeOfDayProjectMode ProjectMode = EArcTimeOfDayProjectMode::GameRealtime;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="Soften 2D Dynamic Cloud Layer 1", ToolTip="Increasing this erodes and softens the edge of the clouds.", UIMin="0", UIMax="10", ClampMin="0"))
	double SoftenTwoDDynamicCloudLayer1 = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="Soften 2D Dynamic Cloud Layer 2", ToolTip="Increasing this erodes and softens the edge of the clouds.", UIMin="0", UIMax="10", ClampMin="0"))
	double SoftenTwoDDynamicCloudLayer2 = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="Sharpen 2D Dynamic Cloud Outer Edge", ToolTip="Increasing this gives the 2D dynamic clouds an exxagerated hard edge. Can give the sky a somewhat stylized comic-book appearance.", UIMin="0", UIMax="30", ClampMin="0"))
	double SharpenTwoDDynamicCloudOuterEdge = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Use Auroras", ToolTip="Use this option to enable an aurora effect at night. Note, this is just for the 2D (non-volumetric) aurora effect. This setting won\\'t be used if using the Sky Mode \"Volumetric Aurora\".", ClampMin="0", UIMin="0"))
	bool UseAuroras = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Intensity", ToolTip="The intensity of the aurora effect (if enabled)", ClampMin="0", UIMax=".3", UIMin="0"))
	double AuroraIntensity = 0.119999997;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Speed", ToolTip="The speed the aurora effect moves and changes across the sky", UIMin="0", UIMax="1"))
	double AuroraSpeed = 0.15;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Cloud Tiling", ToolTip="The overall tiling (scale) of the 2D dynamic cloud textures", UIMax="5", UIMin="0.3"))
	double TwoDDynamicCloudTiling = 3.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="Soften 2D Dynamic Cloud Horizon", ToolTip="Softens the edges of 2D dynamic clouds around the horizon", UIMin="0", UIMax="10", ClampMin="0"))
	double SoftenTwoDDynamicCloudHorizon = 2;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds - Use Single Layer", ToolTip="If enabled, removes one of the 2D dynamic cloud layers. This uses a lower complexity material for a small performance gain."))
	bool TwoDDynamicCloudsUseSingleLayer = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Cloud Height", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="The visual height of the 2D dynamic clouds. Note, this is just an adjustment of the perspective distortion, and has nothing to do with height in real 3D space."))
	double TwoDDynamicCloudHeight = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Overcast Texture", MultiLine="true", ToolTip="Amount of the swirling texture overlayed on the 2D dynamic clouds when cloud density is high.", UIMin="0", UIMax="5"))
	double TwoDDynamicCloudsOvercastTexture = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Phase", ToolTip="Time Offset for Aurora Movement"))
	double AuroraPhase = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Cloud Formation Texture", ToolTip="The texture sampled for the 2D dynamic cloud formations"))
	TSoftObjectPtr<UTexture2D> TwoDDynamicCloudFormationTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Clouds/clouds_diverse.clouds_diverse")));

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Sun Shader Intensity", ToolTip="The intensity of the sun on the 2D dynamic clouds", UIMin="0", UIMax=".65"))
	double TwoDDynamicCloudsSunShaderIntensity = 0.35;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Directional Lighting Intensity", ToolTip="The amount of light the 2D dynamic clouds recieve from the sun and moon", UIMin="0", UIMax="5", ClampMin="0"))
	double TwoDDynamicCloudsDirectionalLightingIntensity = 3;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Sun Highlight Radius Scale", ToolTip="The radius of the sharp backlighting around the 2D dynamic clouds", UIMin="0", UIMax="2"))
	double TwoDDynamicCloudsSunHighlightRadiusScale = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Cloud Opacity", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="The opacity of the 2D dynamic clouds"))
	double TwoDDynamicCloudOpacity = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Horizon Density Multiplier", MultiLine="true", UIMax="2", UIMin="0", ToolTip="A multiplier for 2D dynamic cloud density close to the horizon."))
	double TwoDCloudsHorizonDensityMultiplier = 1.1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Zenith Density Multiplier", MultiLine="true", UIMin="0", UIMax="2", ToolTip="A multiplier for 2D dynamic cloud density at the top of the sky. Also affects the cloud shadows"))
	double TwoDCloudsZenithDensityMultiplier = 0.9;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Latitude Gradient Position", MultiLine="true", ToolTip="The height of the gradient from \"horizon\" to \"zenith\" values, being blended above.", UIMax="1", UIMin="0"))
	double TwoDCloudsLatitudeGradientPosition = 0.25;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Latitude Gradient Width", MultiLine="true", ClampMin=".01", ToolTip="The width of the gradient from \"horizon\" to \"zenith\" values, being blended above.", UIMin=".5", UIMax="250"))
	double TwoDCloudsLatitudeGradientWidth = 50;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Around Sun Density Multiplier", MultiLine="true", UIMax="2", UIMin="0", ToolTip="A multiplier for 2D dynamic cloud density in the immediate area around the sun."))
	double TwoDCloudsAroundSunDensityMultiplier = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Around Sun Density Exponent", MultiLine="true", UIMax="100", UIMin="1", ToolTip="The exponent for the radial gradient defining the area around the sun. Higher will constrict the radius more."))
	double TwoDCloudsAroundSunDensityExponent = 25;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Around Moon Density Multiplier", MultiLine="true", UIMax="2", UIMin="0", ToolTip="A multiplier for 2D dynamic cloud density in the immediate area around the moon."))
	double TwoDCloudsAroundMoonDensityMultiplier = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Around Moon Density Exponent", MultiLine="true", UIMax="100", UIMin="1", ToolTip="The exponent for the radial gradient defining the area around the moon. Higher will constrict the radius more."))
	double TwoDCloudsAroundMoonDensityExponent = 45;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Clouds Shading Offset", MultiLine="true", UIMin="0", UIMax="2", ToolTip="Scales the amount that the texture sample is offset for the 2D dynamic clouds to create the \"shading\" effect"))
	double TwoDCloudsShadingOffset = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Shine Intensity Curve", ToolTip="Scales the shine intensity with the height of the sun. The input for this curve is a 0 to 1 range, with 0 being the sun pointing straight up, and 1 being the sun pointing straght down."))
	TObjectPtr<UCurveFloat> TwoDDynamicCloudsShineIntensityCurve;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Sun Highlight Intensity Curve", ToolTip="Scales the sun highlight intensity with the height of the sun. The input for this curve is a 0 to 1 range, with 0 being the sun pointing straight up, and 1 being the sun pointing straght down."))
	TObjectPtr<UCurveFloat> TwoDDynamicCloudsSunHighlightIntensityCurve;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟸  ·  2D Dynamic Clouds", meta=(DisplayName="2D Dynamic Clouds Sun Highlight Radius Curve", ToolTip="Scales the radius of the sun highlights with the height of the sun. The input for this curve is a 0 to 1 range, with 0 being the sun pointing straight up, and 1 being the sun pointing straght down."))
	TObjectPtr<UCurveFloat> TwoDDynamicCloudsSunHighlightRadiusCurve;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙼𝟷  ·  Cinematics / Offline Rendering", meta=(DisplayName="Adjust For Path Tracer", MultiLine="true", ToolTip="Enable if using the path tracer for rendering. This makes several adjustments to the sky light and rendering settings to correct as best as possible for the current limitations of the path tracer."))
	bool AdjustForPathTracer = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙵𝟷  ·  Fog Color", meta=(DisplayName="Render Height Fog In Path Tracer Using Post Process", ToolTip="If the setting Adjust for Path Tracer is true, and this is enabled, the sky will render a post process material designed to approximate the exponential height fog, to fill in for the fog rendering support the path tracer itself does not have."))
	bool RenderHeightFogInPathTracerUsingPostProcess = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Moves With Time Of Day", ToolTip="If true, the aurora will move with the progress of Time of Day."))
	bool AuroraMovesWithTimeOfDay = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Time Of Day Movement Multiplier", UIMin=".01", UIMax="5", ToolTip="If \"Aurora Moves with Time of Day\" is true, scales how much time contributes movement"))
	double AuroraTimeOfDayMovementMultiplier = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Shape Change Speed", MultiLine="true", ToolTip="Scales the speed the aurora lines will change speed, relative to how they move directionally.", UIMin="0", UIMax="10"))
	double AuroraShapeChangeSpeed = 2;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Texture Scale", MultiLine="true", UIMin=".1", UIMax="3", ToolTip="Scales the texture used to render the aurora effect (either 2D or volumetric)"))
	double AuroraTextureScale = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Daytime Aurora Intensity", UIMin="0", UIMax="5", ToolTip="Scales the intensity of the aurora in the daytime"))
	double DaytimeAuroraIntensity = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Color 1", MultiLine="true", ToolTip="One of three colors which tint the aurora effect"))
	FLinearColor AuroraColor1 = FLinearColor(0.619791985f, 0.0911350027f, 0.341744006f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Color 2", MultiLine="true", ToolTip="One of three colors which tint the aurora effect"))
	FLinearColor AuroraColor2 = FLinearColor(0.189217001f, 1.0f, 0.0501959994f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Color 3", MultiLine="true", ToolTip="One of three colors which tint the aurora effect"))
	FLinearColor AuroraColor3 = FLinearColor(0.0700310022f, 0.347007006f, 0.65625f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Sky Light/ Reflection Brightness", ToolTip="Scales the aurora brightness when rendered in the sky light capture for ambient light and global reflections", UIMin="0", UIMax="20"))
	double AuroraSkyLightReflectionBrightness = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Aurora Texture", MultiLine="true", ToolTip="The texture used for the auroras, both the volumetric and non-volumetric."))
	TSoftObjectPtr<UTexture2D> AuroraTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Clouds/Aurora_Clouds.Aurora_Clouds")));

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="2D Aurora Texture Mip", ToolTip="Mip of the aurora texture sampled for the 2D aurora", UIMin="0", UIMax="4"))
	double TwoDAuroraTextureMip = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="2D Aurora Max Samples", MultiLine="true", UIMin="4", UIMax="30", ToolTip="The max number of samples used to render the 2D (non-volumetric) auroras. This is the number of samples used on a part of the screen where bright/tall auroras are visible. The samples scale downward from there."))
	double TwoDAuroraMaxSamples = 10;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="2D Aurora Sample Step Size", MultiLine="true", UIMin="0.005", UIMax=".12", ToolTip="The amount each sample of the 2D aurora is scaled per sample"))
	double TwoDAuroraSampleStepSize = 0.05;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="2D Aurora Line Exponent", MultiLine="true", UIMin="3", UIMax="35", ToolTip="Exponent for the lines composing the 2D (non-volumetric) auroras. Controls the sharpness/thickness of the lines."))
	double TwoDAuroraLineExponent = 20;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="2D Aurora Mask Exponent", MultiLine="true", UIMin="1", UIMax="5", ToolTip="Exponent for the large scale variation in the intensity/height of the 2D (non-volumetric) auroras"))
	double TwoDAuroraMaskExponent = 3.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="2D Aurora Dither", ToolTip="Scales the temporal dithering which helps smooth 2D aurora samples when using temporal AA.", UIMin="0", UIMax="1"))
	double TwoDAuroraDither = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Volumetric Aurora Texture Mip", UIMin="0", UIMax="3", ClampMin="0", ToolTip="The mip level sampled from the Aurora Texture for volumetric auroras"))
	double VolumetricAuroraTextureMip = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟺  ·  Aurora", meta=(DisplayName="Volumetric Aurora Rendering Mode", ToolTip="This setting controls engine cvars which determine how volumetric aurora are rendered. It has performance implications and each of these modes also has very different amounts and types of visual artifacting. Be sure to read the tooltips for each and choose a mode which is appropriate for your project."))
	EArcTimeOfDayVolumetricRenderingMode VolumetricAuroraRenderingMode = EArcTimeOfDayVolumetricRenderingMode::MeshIntersectionsFastMovementPerformance;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟷  ·  Screen Space Light Shafts", meta=(DisplayName="Sun Light Shaft Bloom - Scale", MultiLine="true", ToolTip="Scales the overall strength of the light shaft bloom.", UIMin="0", UIMax="3", ClampMin="0"))
	double SunLightShaftBloomScale = 0.3;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟷  ·  Screen Space Light Shafts", meta=(DisplayName="Moon Light Shaft Bloom - Scale", MultiLine="true", UIMin="0", UIMax="10", ToolTip="Scales the overall strength of the light shaft bloom."))
	double MoonLightShaftBloomScale = 0.4;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Enable Sun Lens Flare", MultiLine="true", ToolTip="Enable an optical lens flare effect on the sun."))
	bool EnableSunLensFlare = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Lens Flare Type", MultiLine="true", ToolTip="Select from the included lens flare presets."))
	EArcTimeOfDayLensFlareType LensFlareType = EArcTimeOfDayLensFlareType::ZoomChromatic;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Lens Flare Strength", MultiLine="true", UIMin="0.01", UIMax="1", ClampMin="0", ToolTip="Controls the intensity (brightness) of the lens flare."))
	double LensFlareStrength = 0.300000012;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Lens Flare Tint", MultiLine="true", ToolTip="A tint color for the lens flare effect"))
	FLinearColor LensFlareTint = FLinearColor(1.0f, 0.710466981f, 0.440497994f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Lens Flare Brightness Threshold", MultiLine="true", ToolTip="The level of brightness of the color graded pixels in the sun\\'s direction which will allow the lens flare to be visible. It might be necessary to lower this if you\\'re using a color grading LUT which darkens the brightest pixels in the image.", UIMin="0", UIMax=".99"))
	double LensFlareBrightnessThreshold = 0.95;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Custom Lens Flare Parent Instance", MultiLine="true", ToolTip="You can use this option to make your own lens flare. Just create an instance of the Lens Flare material in the Materials folder (or duplicate one of the presets as a starting point. Those are found in Materials/Material Instances/Lens Flares). If you select a material in this slot it overrides the selected Lens Flare Type."))
	TObjectPtr<UMaterialInterface> CustomLensFlareParentInstance;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Scale Flare Elements", MultiLine="true", UIMin=".5", UIMax="2", ClampMin=".001", ToolTip="Scales all 5 \"elements\", the cards that make up the main pieces of the flare."))
	double ScaleFlareElements = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙸𝟸  ·  Lens Flare", meta=(DisplayName="Disable Image Based Lens Flares When Enabled", MultiLine="true", ToolTip="If the Sun lens flare is enabled, this option also disables the engine\\'s stock image based flares so the effects don\\'t overlap."))
	bool DisableImageBasedLensFlaresWhenEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙸𝟹  ·  Post Processing", meta=(DisplayName="Post Process Components", MultiLine="true", ToolTip="Each entry in this array will be a post process component.\r\nIf you enable any of the \"Disable\" toggles, its blend weight will be actively updated at runtime with time and weather, based on the settings for each entry. This can be used to apply post processing settings that are specific to when its daytime, or specific to when it\\'s overcast, for example."))
	TArray<FArcTimeOfDayPostProcessStage> PostProcessComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙸𝟹  ·  Post Processing", meta=(DisplayName="User Post Process Components", MultiLine="true"))
	TArray<TObjectPtr<UPostProcessComponent>> UserPostProcessComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Planets/ Moons", MultiLine="true", ToolTip="Each entry in this array will be a planet/moon rendered in the sky layer (behind the clouds/aurora, on top of stars, moon, sun). Note, this additional rendering into the sky layer makes use of the Dbuffer, so it will only work if Dbuffer Decals are enabled and supported."))
	TArray<FArcTimeOfDaySpacePlanet> PlanetsMoons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Layer Brightness (Night)", MultiLine="true", ToolTip="The intensity of the space layer, at night", UIMin="0", UIMax="5", ClampMin="0"))
	double SpaceLayerBrightnessNight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Layer Brightness (Day)", MultiLine="true", ToolTip="The intensity of the space layer, during the day", UIMin="0", UIMax="15", ClampMin="0"))
	double SpaceLayerBrightnessDay = 5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Glow Brightness", MultiLine="true", UIMin="0", UIMax=".2", ClampMin="0", ToolTip="Scale the brightness of the glow effect around each planet/moon. This determines the max brightness of these glow elements, with the individual scaling set with each planet\\'s \"Glow\" setting."))
	double SpaceGlowBrightness = 0.08;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Render Nebula", ToolTip="Renders a layer of noise based nebula, as part of the space layer"))
	bool RenderNebula = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Intensity", UIMin=".1", UIMax="2", ToolTip="Scales the opacity of the nebula layer"))
	double NebulaIntensity = 0.7;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Color 1", ToolTip="The first of the 3 colors blended with noise for the nebula layer"))
	FLinearColor NebulaColor1 = FLinearColor(0.0194489993f, 1.0f, 0.950241029f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Color 2", ToolTip="The second of 3 colors blended with noise for the nebula layer"))
	FLinearColor NebulaColor2 = FLinearColor(1.0f, 0.0828289986f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Color 3", ToolTip="The third of 3 colors blended with noise for the nebula layer"))
	FLinearColor NebulaColor3 = FLinearColor(0.0f, 0.355670005f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Core Highlight", UIMin="0", UIMax="1", ToolTip="Adds white light to the brightest features in the nebula layer"))
	double NebulaCoreHighlight = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Noise Scale", UIMin="1", UIMax="10", ToolTip="Scales the noise used for the nebula layer"))
	double NebulaNoiseScale = 5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Exponent", UIMin="2", UIMax="7", ToolTip="The exponent which powers the distribution of the nebula\\'s formations in the noise"))
	double NebulaExponent = 4.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Large Distortion", UIMin=".001", UIMax=".2", ToolTip="Scales the amount the nebula noise is distorted by a large noise sample"))
	double NebulaLargeDistortion = 0.11;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Small Distortion", UIMin=".001", UIMax=".06", ToolTip="Scales the amount the nebula noise is distorted by a small noise sample"))
	double NebulaSmallDistortion = 0.03;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Noise Offset", ToolTip="Offsets the coordinates used to map the noise for the nebula."))
	double NebulaNoiseOffset = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Nebula Sort Index", UIMin="0", ClampMin="0", ToolTip="Determines how the nebula will be sorted on top/behind the planets/moons in the space layer"))
	int32 NebulaSortIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Custom Space Layer Decals", ToolTip="Can be used to render your own custom decal materials into the space layer, positioned and scaled similarly to the planets in the Planets/Moons array.\r\nEach entry needs a valid material selected for it to be rendered. This material should be a Deferred Decal material which outputs to Base Color."))
	TArray<FArcTimeOfDaySpaceLayerDecal> CustomSpaceLayerDecals;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Moon Renders Behind Space Layer", ToolTip="Controls if the moon renders in front or behind the objects in the space layer."))
	bool MoonRendersBehindSpaceLayer = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Force Enable Space Layer", ToolTip="Enable to have the sky material sample the dbuffer for the space layer, even if no planets/moons/nebula are enabled."))
	bool ForceEnableSpaceLayer = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Planet Distance", BlueprintPrivate="true"))
	double SpacePlanetDistance = 8e+09;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Scene Components", MultiLine="true"))
	TArray<TObjectPtr<USceneComponent>> SpaceSceneComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Planet Components", MultiLine="true"))
	TArray<TObjectPtr<UStaticMeshComponent>> SpacePlanetComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Roots", MultiLine="true", ToolTip="The scene components which each planet in the space layer is parented to."))
	TArray<TObjectPtr<USceneComponent>> SpaceRoots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Clouds Texture", MultiLine="true", ToolTip="The texture used for clouds when sky mode is set to static clouds.\r\nTo author your own, use the Static Clouds Authoring Tool utility."))
	TSoftObjectPtr<UTexture2D> StaticCloudsTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/StaticClouds/StaticClouds_A.StaticClouds_A")));

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Clouds Rotation", MultiLine="true", UIMin="0", UIMax="360", ToolTip="Rotates the static clouds texture around the Z axis."))
	double StaticCloudsRotation = 0;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Clouds Rotation Speed", MultiLine="true", UIMin="-1", UIMax="1", ToolTip="If this value is not 0, the static clouds texture will slowly rotate, with the speed scaled by this value."))
	double StaticCloudsRotationSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Clouds Color Intensity", MultiLine="true", UIMin="0", UIMax="2", ToolTip="Scales the intensity of the static clouds colors"))
	double StaticCloudsColorIntensity = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙼𝟷  ·  Cinematics / Offline Rendering", meta=(DisplayName="Cinematic Clouds View Sample Scale", UIMin="5", UIMax="60", ToolTip="View sample scale used for volumetric clouds at runtime when project mode is Cinematic/Offline."))
	double CinematicCloudsViewSampleScale = 25;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙼𝟷  ·  Cinematics / Offline Rendering", meta=(DisplayName="Cinematic Clouds Shadow Sample Scale", UIMin="0.2", UIMax="2", ToolTip="Shadow sample scale used for volumetric clouds at runtime when project mode is Cinematic/Offline."))
	double CinematicCloudsShadowSampleScale = 0.6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙼𝟷  ·  Cinematics / Offline Rendering", meta=(DisplayName="Cinematic Clouds Tracing Max Distance", ToolTip="Tracing Max Distance used for volumetric clouds at runtime when project mode is set to Cinematic/Offline", UIMin="1", UIMax="50", ForceUnits="Kilometers"))
	double CinematicCloudsTracingMaxDistance = 20;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙼𝟷  ·  Cinematics / Offline Rendering", meta=(DisplayName="Cinematic Clouds View Ray Sample Max Count", ToolTip="Applied when Project Mode is Cinematic/Offline. Extends the max sample count for volumetric cloud rendering at runtime to this value. The engine\\'s normal default for this value in realtime contexts is 768.", UIMin="768", UIMax="20000"))
	int32 CinematicCloudsViewRaySampleMaxCount = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙷𝟸  ·  Space Layer", meta=(DisplayName="Space Nebula Parent Material"))
	TSoftObjectPtr<UMaterialInterface> SpaceNebulaParentMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Space/Nebula_Layer.Nebula_Layer")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Overcast Texture", ToolTip="Used to modulate sky color as the cloud coverage becomes overcast, when using the Static Clouds sky mode."))
	TSoftObjectPtr<UTexture2D> StaticOvercastTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Sky/Static_Overcast.Static_Overcast")));

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Overcast Strength", UIMin="0", UIMax="2", ToolTip="Scales the intensity of the static overcast texture"))
	double StaticOvercastStrength = 1;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟹  ·  Static Clouds", meta=(DisplayName="Static Clouds Fade In Clear Skies", ToolTip="Controls if static clouds opacity will fade down when cloud coverage goes below a partly cloudy level down to 0."))
	bool StaticCloudsFadeInClearSkies = true;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙼𝟷  ·  Cinematics / Offline Rendering", meta=(DisplayName="Path Tracer Sky Resolution", ToolTip="The path tracer renders the sky using a realtime captured sky light cubemap. This sets the resolution of that cubemap. Understand increasing this above 2048 can risk crashing the GPU depending on the hardware and the context."))
	EArcTimeOfDayPathTracerSkyResolution PathTracerSkyResolution = EArcTimeOfDayPathTracerSkyResolution::Resolution2048;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Grid Count", UIMin="1", UIMax="100", ClampMin="1", ToolTip="The number of voxel 10x10 grids which compose the total voxel field on the X and Y axes.\r\nFor example, set to 50, the total number of voxels would be 50x50x10x10, so 250,000."))
	int32 VoxelCloudsGridCount = 40;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Voxel Size", UIMin="100", UIMax="20000", ClampMin="1", ToolTip="The size of individual voxels", ForceUnits="Centimeters"))
	double VoxelCloudsVoxelSize = 7000;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Shading Intensity", UIMin="0", UIMax="1", ToolTip="The amount of the shading effect applied to voxel clouds."))
	double VoxelCloudsShadingIntensity = 0.6;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Shading Softness", UIMin="0", UIMax="1", ToolTip="Interpolates from a hard nearest neighbor shading effect over to a smooth shading effect."))
	double VoxelCloudsShadingSoftness = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds AO Intensity", UIMin="0", UIMax="1", ToolTip="Scales the amount of a simple \"AO\" effect, which darkens corners between bordering voxels."))
	double VoxelCloudsAOIntensity = 0.5;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Formation Changes", UIMin="0", UIMax="1", ToolTip="Enable to allow the cloud formations of voxel clouds to change over time with cloud movement. Disabled by default because with voxel clouds this results in a lot of voxels popping in and out in a visually distracting way."))
	bool VoxelCloudsFormationChanges = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Uses Cloud Wisps", UIMin="0", UIMax="1", ToolTip="Enable to allow Cloud Wisps to be visible when using the Voxel Clouds sky mode. Disabled by default due to the obvious stylistic mismatch between the two features."))
	bool VoxelCloudsUsesCloudWisps = false;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Horizon Deform", ToolTip="Shifts distant voxels down, to help the voxel field visually meet the horizon without requiring extremely distant voxels to be rendered."))
	double VoxelCloudsHorizonDeform = 110000;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, AdvancedDisplay, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Horizon Distance", ToolTip="The distance from the camera at which Horizon Deform is fully applied."))
	double VoxelCloudsHorizonDistance = 2500000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Static Mesh"))
	TSoftObjectPtr<UStaticMesh> VoxelCloudsStaticMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Meshes/Voxel10x10.Voxel10x10")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Material"))
	TSoftObjectPtr<UMaterialInterface> VoxelCloudsMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Voxel_Clouds.Voxel_Clouds")));

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="𝙲𝟺  ·  Voxel Clouds", meta=(DisplayName="Voxel Clouds Center Position", BlueprintPrivate="true"))
	FVector VoxelCloudsCenterPosition = FVector(0, 0, -50000000);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USceneComponent> SkyRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UPostProcessComponent> GeneralPostProcess;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USceneComponent> SunParent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USceneComponent> MoonParent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UExponentialHeightFogComponent> HeightFog;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UVolumetricCloudComponent> VolumetricAurora;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USkyLightComponent> CapturedSceneSkyLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> SkySphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> UDSLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> Compass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> WestLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> ClockDisk;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> NorthLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> SouthLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> EastLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> MoonHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> SunHandle;

private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SkyMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> VolumetricCloudMaterialComplex;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> VolumetricCloudMaterialSimple;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> VolumetricCloudMaterialComplexTwoLayer;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> VolumetricCloudMaterialSimpleTwoLayer;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> DefaultVolumetricCloudShadowMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> InsideCloudFogMaterialOneLayer;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> InsideCloudFogMaterialTwoLayer;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CloudLayerMapBrushMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> VolumetricCloudParameters;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> WeatherParameters;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SkySphereMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VolumetricCloudMIDComplex;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VolumetricCloudMIDSimple;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedVolumetricCloudMaterialComplex;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedVolumetricCloudMaterialSimple;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedCloudShadowsMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedInsideCloudFogMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudShadowsMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> InsideCloudsFogMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudLayerMapBrushMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlobalVolumetricFogMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedGlobalVolumetricFogMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> GlobalVolumetricFogMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlobalVolumetricFogMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlobalVolumetricGroundFogMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> InsideCloudFogMesh;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> VolumetricCloudLightRays;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> CloudLayerMapRenderTarget;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> VolumetricCloudLightRaysSystem;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> RealStarsSprites;
	UPROPERTY(Transient) TObjectPtr<UCurveFloat> ExposureCurve;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> VoxelCloudsMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VoxelCloudsMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedVoxelCloudsMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VolumetricAuroraMID;
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> LensFlarePostProcess;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LensFlareMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> AppliedLensFlareMaterial;
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> PathTracerFogPostProcess;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PathTracerFogMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SpaceNebulaMID;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SpaceNebulaComponent;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> SpaceGlowMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> SpaceDistantPlanetComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> SpaceDecalComponents;
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> PathTracerSkyLight;

	float TimeCycleDegrees = 0.0f;
	float CloudCoverage03 = 0.0f;
	float GlobalOvercast01 = 0.0f;
	float LocalCloudCoverage = 0.0f;
	float LocalOvercast01 = 0.0f;
	float CachedNightFilter = 0.0f;
	float CachedDirectionalInscatteringMultiplier = 1.0f;
	float CachedNightFilteredNightBrightness = 1.0f;
	float CameraSkyAtmosphereNormalizedDensity = 1.0f;
	FLinearColor CachedNightSkyGlow = FLinearColor::Black;
	FLinearColor CachedLightPollution = FLinearColor::Black;
	FLinearColor CachedAmbientFogColor = FLinearColor::Black;
	FLinearColor CachedDirectionalBaseFogColor = FLinearColor::Black;
	FLinearColor CachedSunFogBaseColor = FLinearColor::Black;
	FLinearColor CachedMoonFogBaseColor = FLinearColor::Black;
	FLinearColor CachedSolarEclipseTint = FLinearColor::White;
	FLinearColor CachedSunLightColor = FLinearColor::White;
	float CachedMoonLitPercent = 1.0f;
	float CachedAbsentDirectionalLightsBrightness = 1.0f;
	float CurrentSceneLightingBrightnessScale = 1.0f;
	FVector CachedSunVector = FVector::UpVector;
	FVector CachedMoonVector = FVector::DownVector;
	FVector CachedSunZVector = FVector::ForwardVector;
	FVector CachedMoonZVector = FVector::ForwardVector;
	FVector CachedMoonAlignment = FVector::ForwardVector;
	FVector CachedSimulatedMoonPhaseAngle = FVector::ZeroVector;
	FLinearColor CachedStarsOrientation = FLinearColor::Black;
	float CachedStarsOrbit = 0.0f;
	float CachedSunScale = FMath::DegreesToRadians(1.0f);
	float CachedMoonScale = FMath::DegreesToRadians(0.95f) * 2.0f;
	float CachedDirectionalDimming = 1.0f;
	float EclipsePercent = 1.0f;
	FVector2D ExtendedCloudHeightRange = FVector2D(0.0, 1.0);
	float CloudMainLayerBottomHeight = 0.0f;
	float CloudMainLayerTopHeight = 0.0f;
	float CloudFullLayerBottomHeight = 0.0f;
	float CloudFullLayerTopHeight = 0.0f;
	FVector CloudsPosition = FVector::ZeroVector;
	float CloudsBTime = 0.0f;
	FVector WeatherFogPosition = FVector::ZeroVector;
	FArcModifiableFloat UDWWindValue = FArcModifiableFloat(2.0f);
	FArcModifiableFloat UDWRainValue;
	FArcModifiableFloat UDWSnowValue;
	FArcModifiableFloat FogSnowContribution;
	TStaticArray<bool, ArcTimeOfDay::ModifierCapacity> ModifierSlotsInUse{};
	float CameraCloudLayerNormalizedHeight = 0.0f;
	float CameraCloudLayerNormalizedHeightFullRange = 0.0f;
	TArray<float> CloudLayersAltitudeRangeCache;
	FVector CloudPhaseVectorMultiplier = FVector::ZeroVector;
	float MaxVolumetricCloudLightRayLength = 0.0f;
	float CachedDirectionalSpecularCancelValue = 1.0f;
	float CloudShadowDisableThreshold = 3.0f;
	bool bAllowDisablingDirectionalShadowsWithCoverage = false;
	float SunShadowsCutoffZ = 0.002f;
	float StaticCloudsDynamicRotation = 0.0f;
	double LastStaticCloudsUpdateTime = 0.0;
	bool bSpaceLayerActive = false;
	bool bAlternateVisualResourcesDirty = true;
	bool bAppliedRenderNebula = false;
	int32 AppliedVoxelCloudsGridCount = INDEX_NONE;
	bool bDaylightSavingsActive = false;
	double LastFrameTimeOfDay = -100.0;
	TWeakObjectPtr<UDirectionalLightComponent> CachedSunLightTarget;
	TWeakObjectPtr<UDirectionalLightComponent> CachedMoonLightTarget;
	TWeakObjectPtr<USkyLightComponent> CachedSkyLightTarget;
	TWeakObjectPtr<UExponentialHeightFogComponent> CachedHeightFogTarget;
	TStaticArray<FArcTimeOfDayCachedValue, static_cast<int32>(EArcTimeOfDayCachedProperty::Count)> CachedPropertyValues;
	TStaticArray<bool, 10> ChangedCacheGroups;
	bool bFillingStartingCache = true;
};
