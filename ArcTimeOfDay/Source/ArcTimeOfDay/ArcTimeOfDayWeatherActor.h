// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "Math/Range.h"
#include "UObject/SoftObjectPtr.h"

#include "ArcTimeOfDayTypes.h"

#include "ArcTimeOfDayWeatherActor.generated.h"

class UAudioComponent;
class AArcTimeOfDayActor;
class UDirectionalLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;
class UPostProcessComponent;
class USceneComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;
class UWindDirectionalSourceComponent;

UCLASS(BlueprintType, Blueprintable)
class ARCTIMEOFDAY_API AArcTimeOfDayWeatherActor final : public AActor
{
	GENERATED_BODY()

public:
	AArcTimeOfDayWeatherActor();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Ultra Dynamic Weather")
	void ApplyWeather();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool GetLightningView(FVector& OutLocation, FRotator& OutRotation) const;
	void StartLightningFlash(const FVector& ViewLocation, const FRotator& ViewRotation);
	void UpdateLightningFlash(float DeltaSeconds);
	void StopLightningFlash();
	void ApplyManualEffects(const FArcTimeOfDayResolvedWeatherValues& ResolvedWeather);
	void ReleaseWeatherModifier();

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ultra Dynamic Weather|Base Parameters")
	TObjectPtr<AArcTimeOfDayActor> SkyActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-10", UIMax="10"))
	float CloudCoverage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-10", UIMax="10"))
	float Rain = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-10", UIMax="10"))
	float Snow = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(DisplayName="Thunder/Lightning", ClampMin="0", ClampMax="10", UIMin="0", UIMax="10"))
	float ThunderLightning = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-10", UIMax="10"))
	float WindIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-10", UIMax="10"))
	float Fog = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-10", UIMax="10"))
	float Dust = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float MaterialWetness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float MaterialSnowCoverage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(ClampMin="0", UIMin="0", UIMax="2"))
	float SnowCoverageLightsUpHeightFog = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float MaterialDustCoverage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(UIMin="-360", UIMax="360"))
	float WindDirection = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Particle Settings")
	bool bUseWaterLevel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Particle Settings")
	float ParticleWaterLevel = -50000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters")
	bool bEnableRainParticles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters")
	bool bEnableSnowParticles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters")
	bool bEnableDustParticles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableWindDebris = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableObscuredLightning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bSpawnLightningFlashes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning", meta=(ClampMin="0"))
	float LightningFlashFrequency = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning", meta=(ClampMin="0"))
	float LightningFlashTimingRandomization = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	FFloatRange LightningFlashDistanceRange = FFloatRange(FFloatRangeBound::Exclusive(155000.0f), FFloatRangeBound::Exclusive(650000.0f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	FFloatRange LightningFlashDuration = FFloatRange(FFloatRangeBound::Exclusive(1.75f), FFloatRangeBound::Exclusive(2.2f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float LightningFlashMaxAngleFromForward = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float LightningHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float LightningBoltLength = 75000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float LightningBoltThicknessScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float DaytimeLightningFlashIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float NighttimeLightningFlashIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	bool bLightningFlashLightSource = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	FLinearColor LightningFlashLightSourceColor = FLinearColor(0.495f, 0.612833f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float LightningFlashLightIntensity = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	bool bLightningFlashesCastShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableLightningFlashBloom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	float LightningFlashLightBloomIntensity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning")
	bool bShowLightningFlashesInLevelEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning|Sound")
	float WeatherSoundsMasterVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning|Sound")
	float CloseThunderVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Lightning|Sound")
	float CloseThunderDelayPerKM = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Sound")
	float RainVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Sound")
	float WindVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Sound")
	float DistantThunderVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Sound")
	float WindWhistlingVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Sound")
	float WeatherSoundUpdatePeriod = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Wind")
	float WindDirectionalSourceSpeedScale = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Wind Fog")
	float PPWFIntensityFromFog = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Wind Fog")
	float PPWFIntensityFromRain = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Wind Fog")
	float PPWFIntensityFromWind = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Wind Fog")
	float PPWFIntensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Screen Frost")
	float ScreenFrostFromSnow = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Screen Frost")
	float ScreenFrostFromMaterialSnow = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Screen Frost")
	float ManualScreenFrost = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Rainbow")
	float RainbowStrengthFromRain = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Rainbow")
	float RainbowStrengthFromFog = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Rainbow")
	float RainbowStrengthInAnyWeather = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Heat Distortion")
	float ManualHeatDistortion = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Heat Distortion")
	float ScaleHeatDistortionIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableWeatherSoundEffects = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableWindDirectionalSource = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnablePostProcessWindFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableScreenDroplets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableScreenFrost = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableRainbow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Ultra Dynamic Weather|Base Parameters", meta=(AdvancedDisplay))
	bool bEnableHeatDistortion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UNiagaraSystem> RainNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Particles/Rain.Rain")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UNiagaraSystem> SnowNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Particles/Snow.Snow")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UNiagaraSystem> DustNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Particles/Dust.Dust")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UNiagaraSystem> WindDebrisNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Particles/Wind_Debris.Wind_Debris")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UNiagaraSystem> ObscuredLightningNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Particles/Obscured_Lightning.Obscured_Lightning")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UNiagaraSystem> LightningFlashNiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Particles/Lightning_Strike.Lightning_Strike")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<USoundBase> DirectionalSoundAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Sound/MetaSounds/UDS_Directional_WeatherSounds.UDS_Directional_WeatherSounds")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<USoundBase> GlobalSoundAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Sound/MetaSounds/UDS_Global_WeatherSounds.UDS_Global_WeatherSounds")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UMaterialInterface> LightningFlashBloomMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Weather/LightningFlashBloom_PP.LightningFlashBloom_PP")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UMaterialInterface> WindFogMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Weather/Post_Process_Wind_Fog.Post_Process_Wind_Fog")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UMaterialInterface> ScreenDropletsMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Weather/Screen_Droplets.Screen_Droplets")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UMaterialInterface> ScreenFrostMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Weather/Screen_Frost.Screen_Frost")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UMaterialInterface> HeatDistortionMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Weather/Heat_Distortion.Heat_Distortion")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UMaterialInterface> RainbowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Materials/Weather/Rainbow_Mat.Rainbow_Mat")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UStaticMesh> RainbowMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Meshes/Rainbow.Rainbow")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UTexture> ScreenFrostNormalTexture = TSoftObjectPtr<UTexture>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Weather/Snow_Normal.Snow_Normal")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ultra Dynamic Weather|Assets")
	TSoftObjectPtr<UTexture> ScreenFrostScatterTexture = TSoftObjectPtr<UTexture>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Weather/Frost_Scatter.Frost_Scatter")));

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<USceneComponent> WeatherRoot;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<USceneComponent> WeatherParticlesParent;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> RainParticles;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> SnowParticles;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> DustParticles;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> WindDebrisParticles;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> ObscuredLightningParticles;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> LightningFlashOne;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UNiagaraComponent> LightningFlashTwo;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UDirectionalLightComponent> LightningLight;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UPostProcessComponent> LightningFlashBloom;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UAudioComponent> SoundMixer;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UAudioComponent> SoundDirectional;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UAudioComponent> SoundGlobal;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UWindDirectionalSourceComponent> WindDirectionalSource;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UPostProcessComponent> WindFogPostProcess;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UPostProcessComponent> ScreenDropletsPostProcess;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UPostProcessComponent> ScreenFrostPostProcess;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UPostProcessComponent> HeatDistortionPostProcess;

	UPROPERTY(BlueprintReadOnly, Category="Ultra Dynamic Weather|Components")
	TObjectPtr<UStaticMeshComponent> RainbowMesh;

private:
	UPROPERTY(Transient)
	TObjectPtr<AArcTimeOfDayActor> ModifierSkyActor;

	FArcTimeOfDayModifierHandle ModifierHandle;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LightningFlashBloomMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WindFogMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenDropletsMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenFrostMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeatDistortionMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RainbowMID;

	FRandomStream LightningRandomStream = FRandomStream(0);
	float LightningFlashSpawnTimer = 0.0f;
	float LightningFlashSpawnPeriod = 0.0f;
	float LightningFlashTimeElapsed = 0.0f;
	float LightningFlashLength = 0.0f;
	float CurrentLightningIntensity = 0.0f;
	FVector CurrentLightningLocation = FVector::ZeroVector;
	FVector CurrentLightningTargetOffset = FVector::ZeroVector;
	bool bLightningSystemToggle = false;
	bool bLightningFlashActive = false;
	float ScreenDropletsTime = 0.0f;
};
