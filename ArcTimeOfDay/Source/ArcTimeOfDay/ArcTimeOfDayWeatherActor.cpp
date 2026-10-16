// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayWeatherActor.h"

#include "ArcTimeOfDayActor.h"

#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

namespace ArcTimeOfDayWeatherParticles
{
void SetSharedParameters(UNiagaraComponent* Component, const bool bUseWaterLevel, const float WaterLevel)
{
	Component->SetVariableInt(TEXT("User.Particle Paths Count"), 2000);
	Component->SetVariableFloat(TEXT("User.CPU Spawn Rate"), 2000.0f);
	Component->SetVariableFloat(TEXT("User.GPU Buffer Period"), 1.0f);
	Component->SetEmitterEnable(TEXT("GPU_Collision"), false);
	Component->SetEmitterEnable(TEXT("CPU_Collision"), true);
	Component->SetVariableFloat(TEXT("User.Camera Forward Offset"), 50.0f);
	Component->SetVariableFloat(TEXT("User.Ceiling Check Height"), 8000.0f);
	Component->SetVariableVec3(TEXT("User.World Spawn Offset"), FVector::ZeroVector);
	Component->SetVariableFloat(TEXT("User.Max Spawn Distance"), 2000.0f);
	Component->SetVariableFloat(TEXT("User.Minimum View Distance"), 45.0f);
	Component->SetVariableBool(TEXT("User.Particle Collision Enabled"), true);
	Component->SetVariableFloat(TEXT("User.Spawn Box Height"), 600.0f);
	Component->SetVariableFloat(TEXT("User.Spawn Distance Distribution"), 1.0f);
	Component->SetVariableFloat(TEXT("User.Forward Spawn Bias"), 0.9f);
	Component->SetVariableFloat(TEXT("User.Camera Affects Sprite Angle"), 1.0f);
	Component->SetVariableFloat(TEXT("User.Motion Stretching"), 1.0f);
	Component->SetVariableInt(TEXT("User.Particle Collision Channel"), static_cast<int32>(ECC_Visibility));
	Component->SetVariableFloat(TEXT("User.Alpha Scale Max Distance"), 2500.0f);
	Component->SetVariableFloat(TEXT("User.Alpha Scale Multiplier"), 3.0f);
	Component->SetVariableBool(TEXT("User.Use Water Level"), bUseWaterLevel);
	Component->SetVariableFloat(TEXT("User.Water Kill Height"), WaterLevel);
	Component->SetVariableBool(TEXT("User.Override Camera Transform"), false);
	Component->SetVisibleInRayTracing(false);
}

void SetEmission(UNiagaraComponent* Component, const float Rate)
{
	Component->SetVariableFloat(TEXT("User.Spawn Rate"), Rate);
	if (Rate > 0.0f)
	{
		if (!Component->IsActive())
		{
			Component->Activate(false);
		}
	}
	else if (Component->IsActive())
	{
		Component->Deactivate();
	}
}

void SetMaterial(UNiagaraComponent* Component, const FName ParameterName, const TCHAR* Path)
{
	Component->SetVariableMaterial(ParameterName, LoadObject<UMaterialInterface>(nullptr, Path));
}

UMaterialInstanceDynamic* SetPostProcessMaterial(UPostProcessComponent* Component, UMaterialInterface* Material, UMaterialInstanceDynamic* Existing, AActor* Owner)
{
	if (!Material)
	{
		Component->Settings.WeightedBlendables.Array.Reset();
		Component->BlendWeight = 0.0f;
		return nullptr;
	}
	if (!Existing || Existing->Parent != Material)
	{
		Existing = UMaterialInstanceDynamic::Create(Material, Owner);
		Component->Settings.WeightedBlendables.Array.Reset();
	}
	if (Component->Settings.WeightedBlendables.Array.IsEmpty())
	{
		Component->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Existing));
	}
	return Existing;
}
}

AArcTimeOfDayWeatherActor::AArcTimeOfDayWeatherActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	WeatherRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeatherRoot"));
	SetRootComponent(WeatherRoot);

	WeatherParticlesParent = CreateDefaultSubobject<USceneComponent>(TEXT("WeatherParticles_Parent"));
	WeatherParticlesParent->SetupAttachment(WeatherRoot);
	WeatherParticlesParent->SetRelativeLocation(FVector(0.0f, 0.0f, -120.0f));

	RainParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Editor Rain"));
	RainParticles->SetupAttachment(WeatherParticlesParent);
	RainParticles->SetAutoActivate(false);

	SnowParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Editor Snow"));
	SnowParticles->SetupAttachment(WeatherParticlesParent);
	SnowParticles->SetAutoActivate(false);

	DustParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Editor Dust"));
	DustParticles->SetupAttachment(WeatherParticlesParent);
	DustParticles->SetAutoActivate(false);

	WindDebrisParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Editor Wind Debris"));
	WindDebrisParticles->SetupAttachment(WeatherParticlesParent);
	WindDebrisParticles->SetAutoActivate(false);

	ObscuredLightningParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Editor Obscured Lightning"));
	ObscuredLightningParticles->SetupAttachment(WeatherParticlesParent);
	ObscuredLightningParticles->SetAutoActivate(false);

	LightningFlashOne = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Lightning Flash 1"));
	LightningFlashOne->SetupAttachment(WeatherRoot);
	LightningFlashOne->SetAutoActivate(false);

	LightningFlashTwo = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Lightning Flash 2"));
	LightningFlashTwo->SetupAttachment(WeatherRoot);
	LightningFlashTwo->SetAutoActivate(false);

	LightningLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Lightning Light"));
	LightningLight->SetupAttachment(WeatherRoot);
	LightningLight->SetMobility(EComponentMobility::Movable);
	LightningLight->SetIntensity(0.0f);

	LightningFlashBloom = CreateDefaultSubobject<UPostProcessComponent>(TEXT("LightningFlashBloom"));
	LightningFlashBloom->SetupAttachment(LightningLight);
	LightningFlashBloom->BlendWeight = 0.0f;
	LightningFlashBloom->bUnbound = true;

	SoundMixer = CreateDefaultSubobject<UAudioComponent>(TEXT("Sound_Mixer"));
	SoundMixer->SetupAttachment(WeatherRoot);
	SoundMixer->SetAutoActivate(false);

	SoundDirectional = CreateDefaultSubobject<UAudioComponent>(TEXT("Sound_Directional"));
	SoundDirectional->SetupAttachment(WeatherRoot);
	SoundDirectional->SetAutoActivate(false);

	SoundGlobal = CreateDefaultSubobject<UAudioComponent>(TEXT("Sound_Global"));
	SoundGlobal->SetupAttachment(WeatherRoot);
	SoundGlobal->SetAutoActivate(false);

	WindDirectionalSource = CreateDefaultSubobject<UWindDirectionalSourceComponent>(TEXT("Wind Directional Source"));
	WindDirectionalSource->SetupAttachment(WeatherRoot);
	WindDirectionalSource->SetAutoActivate(false);

	WindFogPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PPWF Post Process"));
	WindFogPostProcess->SetupAttachment(WeatherRoot);
	WindFogPostProcess->bUnbound = true;
	WindFogPostProcess->BlendWeight = 0.0f;

	ScreenDropletsPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Screen Droplets Post Process"));
	ScreenDropletsPostProcess->SetupAttachment(WeatherRoot);
	ScreenDropletsPostProcess->bUnbound = true;
	ScreenDropletsPostProcess->BlendWeight = 0.0f;

	ScreenFrostPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Screen Frost Post Process"));
	ScreenFrostPostProcess->SetupAttachment(WeatherRoot);
	ScreenFrostPostProcess->bUnbound = true;
	ScreenFrostPostProcess->BlendWeight = 0.0f;

	HeatDistortionPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Heat Distortion Post Process"));
	HeatDistortionPostProcess->SetupAttachment(WeatherRoot);
	HeatDistortionPostProcess->bUnbound = true;
	HeatDistortionPostProcess->BlendWeight = 0.0f;

	RainbowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rainbow Mesh"));
	RainbowMesh->SetupAttachment(WeatherRoot);
	RainbowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AArcTimeOfDayWeatherActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyWeather();
}

void AArcTimeOfDayWeatherActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWeatherModifier();
	Super::EndPlay(EndPlayReason);
}

void AArcTimeOfDayWeatherActor::Destroyed()
{
	ReleaseWeatherModifier();
	Super::Destroyed();
}

void AArcTimeOfDayWeatherActor::ReleaseWeatherModifier()
{
	if (IsValid(ModifierSkyActor) && ModifierHandle.IsValid())
	{
		ModifierSkyActor->ReleaseModifierHandle(ModifierHandle);
	}
	ModifierSkyActor = nullptr;
	ModifierHandle = FArcTimeOfDayModifierHandle();
}

#if WITH_EDITOR
bool AArcTimeOfDayWeatherActor::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AArcTimeOfDayWeatherActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyWeather();
}
#endif

void AArcTimeOfDayWeatherActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ScreenDropletsMID && ScreenDropletsPostProcess->BlendWeight > 0.0f)
	{
		ScreenDropletsTime += DeltaSeconds;
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Drops Time"), ScreenDropletsTime);
	}
	if (const UWorld* World = GetWorld())
	{
#if WITH_EDITOR
		if (World->WorldType == EWorldType::Editor)
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			if (GetLightningView(ViewLocation, ViewRotation))
			{
				WeatherParticlesParent->SetWorldLocation(ViewLocation);
			}
		}
		else
#endif
		if (!World->ViewLocationsRenderedLastFrame.IsEmpty())
		{
			WeatherParticlesParent->SetWorldLocation(World->ViewLocationsRenderedLastFrame[0]);
		}
	}

	bool bShowFlashes = true;
#if WITH_EDITOR
	if (const UWorld* World = GetWorld())
	{
		bShowFlashes = World->WorldType != EWorldType::Editor || bShowLightningFlashesInLevelEditor;
	}
#endif
	if (bSpawnLightningFlashes && ThunderLightning > 0.0f && IsValid(SkyActor) && LightningFlashOne->GetAsset() && GetNetMode() != NM_DedicatedServer && bShowFlashes)
	{
		if (LightningFlashFrequency > 0.0f)
		{
			LightningFlashSpawnTimer += DeltaSeconds;
			if (LightningFlashSpawnTimer >= LightningFlashSpawnPeriod)
			{
				LightningRandomStream.FRandRange(0.0f, 360.0f);
				const float Threshold = LightningRandomStream.FRandRange(5.0f, 10.0f);
				if (ThunderLightning > Threshold)
				{
					FVector ViewLocation;
					FRotator ViewRotation;
					if (GetLightningView(ViewLocation, ViewRotation))
					{
						StartLightningFlash(ViewLocation, ViewRotation);
					}
				}
				LightningFlashSpawnTimer = 0.0f;
				LightningFlashSpawnPeriod = (1.0f + LightningFlashTimingRandomization * LightningRandomStream.FRandRange(-0.5f, 0.5f))
					* (60.0f / LightningFlashFrequency);
			}
		}
		UpdateLightningFlash(DeltaSeconds);
	}
	else if (bLightningFlashActive || LightningFlashOne->IsActive() || LightningFlashTwo->IsActive())
	{
		StopLightningFlash();
		LightningFlashOne->Deactivate();
		LightningFlashTwo->Deactivate();
	}
}

bool AArcTimeOfDayWeatherActor::GetLightningView(FVector& OutLocation, FRotator& OutRotation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

#if WITH_EDITOR
	if (World->WorldType == EWorldType::Editor)
	{
		if (GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->GetWorld() == World)
		{
			OutLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
			OutRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
			return true;
		}
		return false;
	}
#endif

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller && Controller->IsLocalController())
		{
			Controller->GetPlayerViewPoint(OutLocation, OutRotation);
			return true;
		}
	}
	return false;
}

void AArcTimeOfDayWeatherActor::StartLightningFlash(const FVector& ViewLocation, const FRotator& ViewRotation)
{
	const FArcTimeOfDayResolvedWeatherValues ResolvedWeather = SkyActor->GetResolvedWeatherValues();
	const float MinDistance = LightningFlashDistanceRange.GetLowerBoundValue();
	const float MaxDistance = LightningFlashDistanceRange.GetUpperBoundValue();
	const float Distance = LightningRandomStream.FRandRange(MinDistance, MaxDistance);
	const float Yaw = ViewRotation.Yaw + LightningRandomStream.FRandRange(-LightningFlashMaxAngleFromForward, LightningFlashMaxAngleFromForward);
	const float Height = SkyActor->GetCloudMainLayerBottomHeight() + LightningHeightOffset;
	CurrentLightningLocation = FVector(ViewLocation.X, ViewLocation.Y, 0.0f) + FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector(Distance, 0.0f, Height));
	CurrentLightningTargetOffset = LightningRandomStream.VRandCone(FVector::DownVector, FMath::DegreesToRadians(55.0f)) * LightningBoltLength;
	LightningFlashLength = LightningRandomStream.FRandRange(LightningFlashDuration.GetLowerBoundValue(), LightningFlashDuration.GetUpperBoundValue());
	LightningFlashTimeElapsed = 0.0f;
	const float ViewDistance = FVector::Distance(CurrentLightningLocation, ViewLocation);
	const float DistanceIntensity = FMath::GetMappedRangeValueClamped(FVector2D(MinDistance, MaxDistance), FVector2D(1.0f, 0.0f), ViewDistance);
	CurrentLightningIntensity = FMath::Clamp(DistanceIntensity, 0.15f, 1.0f);

	bLightningSystemToggle = !bLightningSystemToggle;
	UNiagaraComponent* Strike = bLightningSystemToggle ? LightningFlashTwo : LightningFlashOne;
	Strike->ReinitializeSystem();
	Strike->SetWorldLocation(CurrentLightningLocation);
	Strike->SetVariableFloat(TEXT("User.Lifetime Scale"), LightningFlashLength / 2.0f);
	Strike->SetVariableFloat(TEXT("User.Intensity Scale"), FMath::Lerp(DaytimeLightningFlashIntensity, NighttimeLightningFlashIntensity, SkyActor->GetWeatherNightFilter()));
	Strike->SetVariableBool(TEXT("User.Allow Impact"), false);
	Strike->SetVariableVec3(TEXT("User.Target Offset"), CurrentLightningTargetOffset);
	Strike->SetVariableBool(TEXT("User.Use Rays"), !PLATFORM_ANDROID && !PLATFORM_IOS);
	const float WindScale = FMath::Pow(FMath::Max(ResolvedWeather.WindIntensity, 0.01f) / 10.0f, 1.5f) * 2000.0f;
	const FVector WindForce = FVector::ForwardVector.RotateAngleAxis(ResolvedWeather.CloudDirection, FVector::UpVector) * WindScale;
	Strike->SetVariableFloat(TEXT("User.Wind Intensity"), ResolvedWeather.WindIntensity / 10.0f);
	Strike->SetVariableLinearColor(TEXT("User.Wind Force"), FLinearColor(WindForce.X, WindForce.Y, WindForce.Z, 0.0f));
	Strike->SetVariableFloat(TEXT("User.Raining"), ResolvedWeather.Rain / 10.0f);
	Strike->SetVariableFloat(TEXT("User.Sun Z"), SkyActor->SunWorldRotation.Vector().Z);
	Strike->SetRandomSeedOffset(LightningRandomStream.RandRange(0, 10000));
	Strike->SetVariableBool(TEXT("User.Path Tracer"), SkyActor->AdjustForPathTracer);
	Strike->SetVariableFloat(TEXT("User.Bolt Thickness"), LightningBoltThicknessScale);
	Strike->Activate(true);

	bLightningFlashActive = true;
	if (bLightningFlashLightSource)
	{
		const FVector LightOrigin = CurrentLightningLocation + CurrentLightningTargetOffset * FVector(0.3f, 0.3f, 0.0f);
		LightningLight->SetWorldRotation((ViewLocation - LightOrigin).Rotation());
		LightningLight->SetIntensity(0.0f);
		if (LightningFlashBloomMID && bEnableLightningFlashBloom)
		{
			LightningFlashBloomMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
			LightningFlashBloomMID->SetVectorParameterValue(TEXT("Light Vector"), FLinearColor(LightningLight->GetForwardVector()));
			LightningFlashBloomMID->SetVectorParameterValue(TEXT("Lightning Location"), FLinearColor(CurrentLightningLocation));
		}
	}

	if (bEnableWeatherSoundEffects && CloseThunderVolume > 0.0f && SoundGlobal->Sound)
	{
		if (!SoundGlobal->IsPlaying())
		{
			SoundGlobal->Play();
		}
		SoundGlobal->SetFloatParameter(TEXT("Close Thunder Delay"), ViewDistance / 100000.0f * CloseThunderDelayPerKM);
		SoundGlobal->SetFloatParameter(TEXT("Force Thunder Sound"), ThunderLightning <= 1.0f ? 2.0f : 0.0f);
		SoundGlobal->SetFloatParameter(TEXT("Close Thunder Volume"), CloseThunderVolume * WeatherSoundsMasterVolume);
		SoundGlobal->SetFloatParameter(TEXT("Thunder Angle"), (ViewLocation - CurrentLightningLocation).Rotation().Yaw);
		SoundGlobal->SetTriggerParameter(TEXT("Close Thunder"));
	}
}

void AArcTimeOfDayWeatherActor::UpdateLightningFlash(const float DeltaSeconds)
{
	if (!bLightningFlashActive)
	{
		return;
	}

	LightningFlashTimeElapsed += DeltaSeconds;
	if (bLightningFlashLightSource && LightningFlashLength > 0.0f)
	{
		const float NormalizedTime = FMath::Clamp(LightningFlashTimeElapsed / LightningFlashLength, 0.0f, 1.0f);
		const float Envelope = FMath::Square(1.0f - 2.0f * FMath::Abs(FMath::Pow(NormalizedTime, 0.4f) - 0.5f));
		const float Noise = FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 10.0f);
		const float OverallIntensity = FMath::Pow(Envelope + Envelope * Noise * 0.25f, 3.0f) * CurrentLightningIntensity;
		LightningLight->SetIntensity(LightningFlashLightIntensity * OverallIntensity);
		if (LightningFlashBloomMID && bEnableLightningFlashBloom)
		{
			LightningFlashBloomMID->SetScalarParameterValue(TEXT("Intensity"), LightningFlashLightBloomIntensity / 170.0f * LightningFlashLightIntensity);
		}
	}
	if (LightningFlashTimeElapsed >= LightningFlashLength)
	{
		StopLightningFlash();
	}
}

void AArcTimeOfDayWeatherActor::StopLightningFlash()
{
	bLightningFlashActive = false;
	LightningLight->SetIntensity(0.0f);
	if (LightningFlashBloomMID)
	{
		LightningFlashBloomMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
	}
}

void AArcTimeOfDayWeatherActor::ApplyManualEffects(const FArcTimeOfDayResolvedWeatherValues& ResolvedWeather)
{
	const bool bHasLocalView = GetNetMode() != NM_DedicatedServer;
	const FVector WindVector = FVector::ForwardVector.RotateAngleAxis(ResolvedWeather.CloudDirection, FVector::UpVector);
	USoundBase* DirectionalSound = bHasLocalView && bEnableWeatherSoundEffects ? DirectionalSoundAsset.LoadSynchronous() : nullptr;
	USoundBase* GlobalSound = bHasLocalView && bEnableWeatherSoundEffects ? GlobalSoundAsset.LoadSynchronous() : nullptr;
	if (SoundDirectional->Sound != DirectionalSound)
	{
		SoundDirectional->SetSound(DirectionalSound);
	}
	if (SoundGlobal->Sound != GlobalSound)
	{
		SoundGlobal->SetSound(GlobalSound);
	}
	if (DirectionalSound)
	{
		SoundDirectional->SetFloatParameter(TEXT("Rain Intensity"), ResolvedWeather.Rain);
		SoundDirectional->SetFloatParameter(TEXT("Wind Intensity"), ResolvedWeather.WindIntensity);
		SoundDirectional->SetFloatParameter(TEXT("Y Wind Lean"), -WindVector.Y * ResolvedWeather.WindIntensity / 25.0f);
		SoundDirectional->SetFloatParameter(TEXT("X Wind Lean"), -WindVector.X * ResolvedWeather.WindIntensity / 25.0f);
		SoundDirectional->SetFloatParameter(TEXT("Rain Volume Multiplier"), RainVolume * WeatherSoundsMasterVolume);
		SoundDirectional->SetFloatParameter(TEXT("Wind Volume Multiplier"), WindVolume * WeatherSoundsMasterVolume);
		SoundDirectional->SetFloatParameter(TEXT("Wind Gust Update Period"), 2.0f);
		if (!SoundDirectional->IsPlaying())
		{
			SoundDirectional->Play();
		}
	}
	else
	{
		SoundDirectional->Stop();
	}
	if (GlobalSound)
	{
		SoundGlobal->SetFloatParameter(TEXT("Thunder Lightning"), ThunderLightning);
		SoundGlobal->SetFloatParameter(TEXT("Distant Thunder Volume"), DistantThunderVolume * WeatherSoundsMasterVolume);
		SoundGlobal->SetFloatParameter(TEXT("Wind Whistling Volume"), WindWhistlingVolume * WeatherSoundsMasterVolume);
		SoundGlobal->SetFloatParameter(TEXT("State Update Period"), WeatherSoundUpdatePeriod);
		if (!SoundGlobal->IsPlaying())
		{
			SoundGlobal->Play();
		}
	}
	else
	{
		SoundGlobal->Stop();
	}
	SoundMixer->SetIntParameter(TEXT("Speaker Angle Front"), 30);
	SoundMixer->SetIntParameter(TEXT("Speaker Angle Side"), 90);
	SoundMixer->SetIntParameter(TEXT("Speaker Angle Back"), 135);

	if (bHasLocalView && bEnableWindDirectionalSource)
	{
		WindDirectionalSource->SetWorldRotation(FRotator(0.0f, ResolvedWeather.CloudDirection, 0.0f));
		WindDirectionalSource->SetStrength(1.0f);
		WindDirectionalSource->SetMinimumGustAmount(0.0f);
		WindDirectionalSource->SetMaximumGustAmount(0.0f);
		WindDirectionalSource->SetSpeed(WindDirectionalSourceSpeedScale * FMath::Pow(ResolvedWeather.WindIntensity / 10.0f, 1.3f));
		if (!WindDirectionalSource->IsActive())
		{
			WindDirectionalSource->SetActive(true);
		}
	}
	else if (WindDirectionalSource->IsActive())
	{
		WindDirectionalSource->SetActive(false);
	}

	UMaterialInterface* WindFogParent = bHasLocalView && bEnablePostProcessWindFog && IsValid(SkyActor) && !SkyActor->IsVolumetricFogActive() ? WindFogMaterial.LoadSynchronous() : nullptr;
	WindFogMID = ArcTimeOfDayWeatherParticles::SetPostProcessMaterial(WindFogPostProcess, WindFogParent, WindFogMID, this);
	const float FogContribution = FMath::GetMappedRangeValueClamped(FVector2D(4.0f, 10.0f), FVector2D(0.0f, 1.0f), ResolvedWeather.Fog) * PPWFIntensityFromFog;
	const float RainContribution = FMath::Square(ResolvedWeather.Rain / 10.0f) * PPWFIntensityFromRain;
	const float WindContribution = FMath::GetMappedRangeValueClamped(FVector2D(3.0f, 10.0f), FVector2D(0.0f, PPWFIntensityFromWind), ResolvedWeather.WindIntensity);
	const float WindFogIntensity = FMath::Max(FogContribution + RainContribution, WindContribution) / 5.0f * PPWFIntensityScale;
	if (WindFogMID)
	{
		if (WindFogPostProcess->Settings.WeightedBlendables.Array.Num() == 1)
		{
			UMaterialInterface* Composite = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UltraDynamicSky/Materials/Weather/Post_Process_Wind_Fog_Composite.Post_Process_Wind_Fog_Composite"));
			if (Composite)
			{
				WindFogPostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Composite));
			}
		}
		WindFogMID->SetScalarParameterValue(TEXT("Max Sample Distance"), 1800.0f);
		WindFogMID->SetScalarParameterValue(TEXT("Step Distance"), 120.0f);
		WindFogMID->SetScalarParameterValue(TEXT("3D Noise Scale"), 2000.0f);
		WindFogMID->SetScalarParameterValue(TEXT("Intensity"), WindFogIntensity);
		WindFogPostProcess->BlendWeight = WindFogIntensity > 0.0f ? 1.0f : 0.0f;
	}

	UMaterialInterface* DropletsParent = bHasLocalView && bEnableScreenDroplets ? ScreenDropletsMaterial.LoadSynchronous() : nullptr;
	ScreenDropletsMID = ArcTimeOfDayWeatherParticles::SetPostProcessMaterial(ScreenDropletsPostProcess, DropletsParent, ScreenDropletsMID, this);
	if (ScreenDropletsMID)
	{
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Inner Strength"), 0.0f);
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Outer Strength"), -2.0f);
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Droplet Tiling"), 1.0f);
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Blur Amount"), 0.002f);
		const float DropsIntensity = bEnableScreenDroplets && ResolvedWeather.Rain > 0.0f ? FMath::Clamp(ResolvedWeather.Rain / 5.0f, 0.0f, 1.0f) : 0.0f;
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Drops Intensity"), DropsIntensity);
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Drips Intensity"), 0.0f);
		ScreenDropletsMID->SetScalarParameterValue(TEXT("Drops Time"), ScreenDropletsTime);
		ScreenDropletsPostProcess->BlendWeight = DropsIntensity > 0.0f ? 1.0f : 0.0f;
	}

	UMaterialInterface* FrostParent = bHasLocalView && bEnableScreenFrost ? ScreenFrostMaterial.LoadSynchronous() : nullptr;
	ScreenFrostMID = ArcTimeOfDayWeatherParticles::SetPostProcessMaterial(ScreenFrostPostProcess, FrostParent, ScreenFrostMID, this);
	if (ScreenFrostMID)
	{
		ScreenFrostMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.52536f, 0.568881f, 0.59375f, 0.175f));
		ScreenFrostMID->SetScalarParameterValue(TEXT("Falloff Shape"), 0.5f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Falloff Power"), 1.35f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Distortion"), 0.2f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Frost Distortion Alpha"), 0.85f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Texture Scale"), 3.0f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Scatter"), 2.0f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Sharpness"), 12.0f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Falloff Center"), 0.0f);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Falloff Edge"), 1.0f);
		ScreenFrostMID->SetTextureParameterValue(TEXT("Normal Tex"), ScreenFrostNormalTexture.LoadSynchronous());
		ScreenFrostMID->SetTextureParameterValue(TEXT("Scatter Tex"), ScreenFrostScatterTexture.LoadSynchronous());
		const float SnowStrength = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, 5.0f), FVector2D(0.0f, 1.0f), ResolvedWeather.Snow) * ScreenFrostFromSnow;
		const float FrostStrength = FMath::Max3(SnowStrength, MaterialSnowCoverage * ScreenFrostFromMaterialSnow, ManualScreenFrost);
		ScreenFrostMID->SetScalarParameterValue(TEXT("Strength"), FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(FrostStrength, 0.0f, 1.0f), 2.0f));
		ScreenFrostPostProcess->BlendWeight = FrostStrength > 0.0f ? 1.0f : 0.0f;
	}

	UMaterialInterface* HeatParent = bHasLocalView && bEnableHeatDistortion ? HeatDistortionMaterial.LoadSynchronous() : nullptr;
	HeatDistortionMID = ArcTimeOfDayWeatherParticles::SetPostProcessMaterial(HeatDistortionPostProcess, HeatParent, HeatDistortionMID, this);
	if (HeatDistortionMID)
	{
		HeatDistortionMID->SetScalarParameterValue(TEXT("Distance"), 7000.0f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Falloff"), 20000.0f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Chromatic Distortion"), 0.2f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Texture Tiling"), 1.7f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Speed"), 0.18f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Zenith Multiplier"), 0.0f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Horizon Multiplier"), 1.0f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Dither"), 1.0f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Scale_Distortion"), 0.08f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Horizon Exponent"), 2.5f);
		HeatDistortionMID->SetScalarParameterValue(TEXT("Effect Mask"), FMath::Max(ManualHeatDistortion, 0.0f) * ScaleHeatDistortionIntensity);
		HeatDistortionPostProcess->BlendWeight = ManualHeatDistortion > 0.0f ? 1.0f : 0.0f;
	}

	UMaterialInterface* RainbowParent = bHasLocalView && bEnableRainbow ? RainbowMaterial.LoadSynchronous() : nullptr;
	if (RainbowParent && (!RainbowMID || RainbowMID->Parent != RainbowParent))
	{
		RainbowMID = UMaterialInstanceDynamic::Create(RainbowParent, this);
		RainbowMesh->SetMaterial(0, RainbowMID);
	}
	if (RainbowMID && RainbowParent)
	{
		RainbowMesh->SetStaticMesh(RainbowMeshAsset.LoadSynchronous());
		RainbowMID->SetScalarParameterValue(TEXT("Mask Above Clouds"), 1.0f);
		RainbowMID->SetScalarParameterValue(TEXT("Mask Below Water"), bUseWaterLevel ? 1.0f : 0.0f);
		RainbowMID->SetScalarParameterValue(TEXT("2D Clouds"), 0.0f);
		const float RainStrength = FMath::GetMappedRangeValueClamped(FVector2D(0.04f, 4.0f), FVector2D(0.0f, 1.0f), ResolvedWeather.Rain) * RainbowStrengthFromRain;
		const float FogStrength = FMath::GetMappedRangeValueClamped(FVector2D(3.0f, 7.0f), FVector2D(0.0f, 1.0f), ResolvedWeather.Fog) * RainbowStrengthFromFog;
		const float RainbowStrength = FMath::Max(RainbowStrengthInAnyWeather, RainStrength + FogStrength);
		RainbowMID->SetScalarParameterValue(TEXT("Intensity"), RainbowStrength * (IsValid(SkyActor) ? SkyActor->SunLightIntensity : 0.0f) / 20.0f);
	}
	else if (RainbowMID)
	{
		RainbowMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
	}
}

void AArcTimeOfDayWeatherActor::ApplyWeather()
{
#if WITH_EDITOR
	if (const UWorld* World = GetWorld(); World && World->WorldType == EWorldType::Editor)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		if (GetLightningView(ViewLocation, ViewRotation))
		{
			WeatherParticlesParent->SetWorldLocation(ViewLocation);
		}
	}
#endif

	if (ModifierSkyActor != SkyActor)
	{
		ReleaseWeatherModifier();
	}
	if (IsValid(SkyActor) && !ModifierHandle.IsValid())
	{
		ModifierHandle = SkyActor->AcquireModifierHandle();
		if (ModifierHandle.IsValid())
		{
			ModifierSkyActor = SkyActor;
		}
	}
	if (IsValid(ModifierSkyActor) && ModifierHandle.IsValid())
	{
		ModifierSkyActor->ApplyWeatherValues(ModifierHandle, CloudCoverage, Fog, Dust / 10.0f, WindDirection, WindIntensity, Rain, Snow, MaterialSnowCoverage * SnowCoverageLightsUpHeightFog);
	}

	FArcTimeOfDayResolvedWeatherValues ResolvedWeather;
	if (IsValid(SkyActor))
	{
		ResolvedWeather = SkyActor->GetResolvedWeatherValues();
	}
	else
	{
		const float DirectionRemainder = FMath::Fmod(WindDirection, 360.0f);
		ResolvedWeather.CloudCoverage = FMath::Clamp(CloudCoverage, 0.0f, 10.0f);
		ResolvedWeather.Fog = FMath::Clamp(Fog, 0.0f, 10.0f);
		ResolvedWeather.DustAmount = FMath::Clamp(Dust / 10.0f, 0.0f, 1.0f);
		ResolvedWeather.CloudDirection = DirectionRemainder < 0.0f ? DirectionRemainder + 360.0f : DirectionRemainder;
		ResolvedWeather.WindIntensity = FMath::Clamp(WindIntensity, 0.0f, 10.0f);
		ResolvedWeather.Rain = FMath::Clamp(Rain, 0.0f, 10.0f);
		ResolvedWeather.Snow = FMath::Clamp(Snow, 0.0f, 10.0f);
		ResolvedWeather.FogSnowContribution = FMath::Max(MaterialSnowCoverage * SnowCoverageLightsUpHeightFog, 0.0f);
	}
	const float UDWDustAmount = ResolvedWeather.DustAmount * 10.0f;

	if (UWorld* World = GetWorld())
	{
		UMaterialParameterCollection* WeatherCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters.UltraDynamicWeather_Parameters"));
		UMaterialParameterCollection* ParticleCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/UltraDynamicSky/Particles/UDW_WeatherParticle_Parameters.UDW_WeatherParticle_Parameters"));
		if (WeatherCollection)
		{
			if (UMaterialParameterCollectionInstance* Parameters = World->GetParameterCollectionInstance(WeatherCollection))
			{
				const float WindScale = FMath::Pow(FMath::Max(ResolvedWeather.WindIntensity, 0.01f) / 10.0f, 1.5f) * 2000.0f;
				const FVector WindForce = FVector::ForwardVector.RotateAngleAxis(ResolvedWeather.CloudDirection, FVector::UpVector) * WindScale;
				Parameters->SetScalarParameterValue(TEXT("Raining"), ResolvedWeather.Rain / 10.0f);
				Parameters->SetScalarParameterValue(TEXT("Snowing"), ResolvedWeather.Snow / 10.0f);
				Parameters->SetScalarParameterValue(TEXT("Sand/Dust"), ResolvedWeather.DustAmount);
				Parameters->SetScalarParameterValue(TEXT("Cloud Coverage"), ResolvedWeather.CloudCoverage);
				Parameters->SetScalarParameterValue(TEXT("Fog"), ResolvedWeather.Fog);
				Parameters->SetScalarParameterValue(TEXT("Wind Intensity"), ResolvedWeather.WindIntensity / 10.0f);
				Parameters->SetScalarParameterValue(TEXT("Last Frame Wind Intensity"), ResolvedWeather.WindIntensity / 10.0f);
				Parameters->SetVectorParameterValue(TEXT("Wind Force"), FLinearColor(WindForce.X, WindForce.Y, WindForce.Z, 0.0f));
				Parameters->SetScalarParameterValue(TEXT("Wind Angle"), ResolvedWeather.CloudDirection);
				Parameters->SetScalarParameterValue(TEXT("Dust or Snow"), FMath::Clamp(MaterialDustCoverage * 6.0f, 0.0f, 1.0f));
				Parameters->SetScalarParameterValue(TEXT("Wet"), MaterialWetness);
				Parameters->SetScalarParameterValue(TEXT("Snowy"), MaterialSnowCoverage);
				Parameters->SetScalarParameterValue(TEXT("Dusty"), MaterialDustCoverage * 0.5f);
				Parameters->SetScalarParameterValue(TEXT("DLWE_Base Wetness"), FMath::Lerp(0.5f, 0.75f, FMath::Clamp(ResolvedWeather.Rain / 4.0f, 0.0f, 1.0f)));
				Parameters->SetScalarParameterValue(TEXT("Water Roughness"), 0.04f);
				Parameters->SetScalarParameterValue(TEXT("Tiling Ripples Scale"), 250.0f);
				Parameters->SetScalarParameterValue(TEXT("Tiling Ripples Framerate"), 30.0f);
				Parameters->SetVectorParameterValue(TEXT("Dust Color"), FLinearColor(0.260417f, 0.235053f, 0.191244f, 3.6f));
				Parameters->SetVectorParameterValue(TEXT("Snow Color"), FLinearColor(0.9f, 0.9f, 0.9f, 8.0f));
				Parameters->SetScalarParameterValue(TEXT("Snow/Dust Wind Shift"), 0.3f / 2000.0f);
				Parameters->SetVectorParameterValue(TEXT("Lightning Color"), FLinearColor(0.311279f, 0.43923f, 0.703125f, 35.0f));
				Parameters->SetScalarParameterValue(TEXT("Water Level"), ParticleWaterLevel);
				Parameters->SetScalarParameterValue(TEXT("Water Level Falloff"), 100.0f);
				Parameters->SetScalarParameterValue(TEXT("Weather Uses Water Level"), bUseWaterLevel ? 1.0f : 0.0f);
				Parameters->SetScalarParameterValue(TEXT("Weather Particle Collision Channel"), static_cast<float>(ECC_Visibility));
			}
		}
		if (ParticleCollection)
		{
			if (UMaterialParameterCollectionInstance* Parameters = World->GetParameterCollectionInstance(ParticleCollection))
			{
				const IConsoleVariable* AntiAliasingMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
				const int32 AntiAliasingIndex = AntiAliasingMethod ? FMath::Clamp(AntiAliasingMethod->GetInt(), 0, 7) : 0;
				const float DecalAlphas[8] = { 0.65f, 0.7f, 1.1f, 0.75f, 1.1f, 0.75f, 1.0f, 1.0f };
				Parameters->SetScalarParameterValue(TEXT("Rain Sprite Alpha"), 0.25f);
				Parameters->SetVectorParameterValue(TEXT("Rain Color"), FLinearColor::White);
				Parameters->SetScalarParameterValue(TEXT("Scale Decal Alpha"), DecalAlphas[AntiAliasingIndex]);
				Parameters->SetScalarParameterValue(TEXT("Rain Refraction Intensity"), 1.75f);
				Parameters->SetScalarParameterValue(TEXT("Rain Ambient Light Intensity"), 0.0f);
				Parameters->SetScalarParameterValue(TEXT("Splash Sprite Alpha"), 1.0f);
				Parameters->SetScalarParameterValue(TEXT("Snow Sprite Alpha"), 1.0f);
				Parameters->SetVectorParameterValue(TEXT("Snow Color"), FLinearColor::White);
				Parameters->SetScalarParameterValue(TEXT("Snow Ambient Light Intensity"), 0.0f);
				Parameters->SetScalarParameterValue(TEXT("Weather Particle DOF Scale"), 1.0f);
			}
		}
	}

	UNiagaraSystem* RainSystem = RainNiagaraSystem.LoadSynchronous();
	UNiagaraSystem* SnowSystem = SnowNiagaraSystem.LoadSynchronous();
	UNiagaraSystem* DustSystem = DustNiagaraSystem.LoadSynchronous();
	UNiagaraSystem* DebrisSystem = WindDebrisNiagaraSystem.LoadSynchronous();
	UNiagaraSystem* ObscuredSystem = ObscuredLightningNiagaraSystem.LoadSynchronous();
	UNiagaraSystem* FlashSystem = LightningFlashNiagaraSystem.LoadSynchronous();
	if (RainParticles->GetAsset() != RainSystem) RainParticles->SetAsset(RainSystem);
	if (SnowParticles->GetAsset() != SnowSystem) SnowParticles->SetAsset(SnowSystem);
	if (DustParticles->GetAsset() != DustSystem) DustParticles->SetAsset(DustSystem);
	if (WindDebrisParticles->GetAsset() != DebrisSystem) WindDebrisParticles->SetAsset(DebrisSystem);
	if (ObscuredLightningParticles->GetAsset() != ObscuredSystem) ObscuredLightningParticles->SetAsset(ObscuredSystem);
	if (LightningFlashOne->GetAsset() != FlashSystem) LightningFlashOne->SetAsset(FlashSystem);
	if (LightningFlashTwo->GetAsset() != FlashSystem) LightningFlashTwo->SetAsset(FlashSystem);

	ArcTimeOfDayWeatherParticles::SetSharedParameters(RainParticles, bUseWaterLevel, ParticleWaterLevel);
	ArcTimeOfDayWeatherParticles::SetSharedParameters(SnowParticles, bUseWaterLevel, ParticleWaterLevel);
	ArcTimeOfDayWeatherParticles::SetSharedParameters(DustParticles, bUseWaterLevel, ParticleWaterLevel);
	ArcTimeOfDayWeatherParticles::SetSharedParameters(WindDebrisParticles, bUseWaterLevel, ParticleWaterLevel);
	ArcTimeOfDayWeatherParticles::SetSharedParameters(ObscuredLightningParticles, bUseWaterLevel, ParticleWaterLevel);

	ArcTimeOfDayWeatherParticles::SetMaterial(RainParticles, TEXT("User.Rain Sprite Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Rain_ParticleMat.Rain_ParticleMat"));
	const IConsoleVariable* DBuffer = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
	const bool bUseTranslucentSplashes = DBuffer && DBuffer->GetInt() == 0;
	ArcTimeOfDayWeatherParticles::SetMaterial(RainParticles, TEXT("User.Splash Material"), bUseTranslucentSplashes
		? TEXT("/Game/UltraDynamicSky/Materials/Weather/Splash_ParticleMat_Translucent.Splash_ParticleMat_Translucent")
		: TEXT("/Game/UltraDynamicSky/Materials/Weather/Splash_ParticleMat.Splash_ParticleMat"));
	ArcTimeOfDayWeatherParticles::SetMaterial(RainParticles, TEXT("User.Splash Droplet Material"), bUseTranslucentSplashes
		? TEXT("/Game/UltraDynamicSky/Materials/Weather/Splash_Droplet_Particle_Translucent.Splash_Droplet_Particle_Translucent")
		: TEXT("/Game/UltraDynamicSky/Materials/Weather/Splash_Droplet_Particle.Splash_Droplet_Particle"));
	ArcTimeOfDayWeatherParticles::SetMaterial(RainParticles, TEXT("User.Spot Decal Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Rain_Spot_Decal.Rain_Spot_Decal"));
	ArcTimeOfDayWeatherParticles::SetMaterial(RainParticles, TEXT("User.Ripple Decal Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Rain_Ripple_Decal.Rain_Ripple_Decal"));
	RainParticles->SetVariableFloat(TEXT("User.Sprite Scale"), 1.0f);
	RainParticles->SetVariableFloat(TEXT("User.SplashParticleSize"), 1.0f);
	RainParticles->SetVariableFloat(TEXT("User.Splash Droplets Scale"), 1.0f);
	RainParticles->SetVariableFloat(TEXT("User.Raindrop Decal Scale"), 1.0f);
	RainParticles->SetVariableFloat(TEXT("User.Splash Percentage"), 1.0f);
	RainParticles->SetVariableFloat(TEXT("User.Rain Sparkle"), 4.0f);
	RainParticles->SetEmitterEnable(TEXT("GPU_Drop_Decal"), true);
	RainParticles->SetEmitterEnable(TEXT("GPU_Rain_Ripples"), true);
	RainParticles->SetVariableFloat(TEXT("User.Splash Duplicate Range"), 22.0f);
	RainParticles->SetVariableBool(TEXT("User.Use Splash Droplets"), true);
	RainParticles->SetVariableFloat(TEXT("User.Custom Time Dilation"), 1.0f);
	RainParticles->SetVariableFloat(TEXT("User.Splash Min Z"), 0.02f);
	RainParticles->SetVariableFloat(TEXT("User.Wind Velocity Scale"), 1800.0f);
	RainParticles->SetVariableFloat(TEXT("User.Velocity Randomization Scale"), 1.0f);

	ArcTimeOfDayWeatherParticles::SetMaterial(SnowParticles, TEXT("User.Snow Sprite Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Snow_ParticleMat.Snow_ParticleMat"));
	ArcTimeOfDayWeatherParticles::SetMaterial(SnowParticles, TEXT("User.Stuck Snow Decal Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Stuck_Snow_Decal_Mat.Stuck_Snow_Decal_Mat"));
	SnowParticles->SetVariableFloat(TEXT("User.Custom Time Dilation"), 1.0f);
	SnowParticles->SetVariableFloat(TEXT("User.Particles Stick Time"), 1.5f);
	SnowParticles->SetVariableBool(TEXT("User.Stuck Snow Enabled"), true);
	SnowParticles->SetVariableFloat(TEXT("User.Sprite Scale"), 1.0f);
	SnowParticles->SetVariableFloat(TEXT("User.Wind Velocity Scale"), 1500.0f);
	SnowParticles->SetVariableFloat(TEXT("User.Velocity Randomization Scale"), 1.0f);

	ArcTimeOfDayWeatherParticles::SetMaterial(DustParticles, TEXT("User.Dust Sprite Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Dust_ParticleMat.Dust_ParticleMat"));
	DustParticles->SetVariableFloat(TEXT("User.Max Spawn Distance"), 1600.0f);
	DustParticles->SetVariableFloat(TEXT("User.Sprite Scale"), 1.0f);
	DustParticles->SetVariableVec3(TEXT("User.Sprite Color Multiplier"), FVector::OneVector);
	DustParticles->SetVariableFloat(TEXT("User.Custom Time Dilation"), 1.0f);
	DustParticles->SetVariableFloat(TEXT("User.Wind Velocity Scale"), 1400.0f);
	DustParticles->SetVariableFloat(TEXT("User.Velocity Randomization Scale"), 1.0f);

	WindDebrisParticles->SetVariableTexture(TEXT("User.Debris Texture"), LoadObject<UTexture>(nullptr, TEXT("/Game/UltraDynamicSky/Textures/Weather/Debris_Twigs_and_Leaves.Debris_Twigs_and_Leaves")));
	ArcTimeOfDayWeatherParticles::SetMaterial(WindDebrisParticles, TEXT("User.Wind Debris Sprite Material"), TEXT("/Game/UltraDynamicSky/Materials/Weather/Wind_Debris.Wind_Debris"));
	WindDebrisParticles->SetVariableFloat(TEXT("User.Sprite Scale"), 1.0f);
	WindDebrisParticles->SetVariableFloat(TEXT("User.Sprite Alpha"), 0.5f);
	WindDebrisParticles->SetVariableFloat(TEXT("User.Velocity Randomization"), 0.2f);
	WindDebrisParticles->SetVariableFloat(TEXT("User.Wind Velocity Scale"), 2200.0f);

	ObscuredLightningParticles->SetVariableFloat(TEXT("User.Spawn Height"), IsValid(SkyActor) ? SkyActor->GetCloudMainLayerBottomHeight() : 0.0f);
	ObscuredLightningParticles->SetVariableFloat(TEXT("User.Spawn Radius"), 1100000.0f);
	ObscuredLightningParticles->SetVariableFloat(TEXT("User.Card Scale"), 2.0f);
	ObscuredLightningParticles->SetVariableFloat(TEXT("User.Nighttime Intensity"), 0.6f);
	ObscuredLightningParticles->SetVariableFloat(TEXT("User.Daytime Intensity"), 5.0f);
	ObscuredLightningParticles->SetVariableFloat(TEXT("User.Hide with Local Cloud Coverage"), 0.0f);

	const bool bHasLocalView = GetNetMode() != NM_DedicatedServer;
	const float RainRate = bHasLocalView && bEnableRainParticles ? FMath::Pow(ResolvedWeather.Rain / 10.0f, 1.7f) * 20000.0f : 0.0f;
	const float SnowRate = bHasLocalView && bEnableSnowParticles ? FMath::Pow(ResolvedWeather.Snow / 10.0f, 1.5f) * 20000.0f : 0.0f;
	const float DustScale = bHasLocalView && bEnableDustParticles ? FMath::Clamp((UDWDustAmount - 3.0f) / 7.0f, 0.0f, 1.0f) : 0.0f;
	const float DebrisRate = bHasLocalView && bEnableWindDebris ? FMath::Square(FMath::Clamp((ResolvedWeather.WindIntensity - 6.0f) / 4.0f, 0.0f, 1.0f)) * 5000.0f : 0.0f;
	const float ObscuredRate = bHasLocalView && bEnableObscuredLightning ? FMath::Clamp((ThunderLightning - 2.0f) / 6.0f, 0.0f, 1.0f) * 2.5f : 0.0f;
	DustParticles->SetVariableFloat(TEXT("User.Sprite Alpha"), DustScale * 0.6f);
	ArcTimeOfDayWeatherParticles::SetEmission(RainParticles, RainSystem ? RainRate : 0.0f);
	ArcTimeOfDayWeatherParticles::SetEmission(SnowParticles, SnowSystem ? SnowRate : 0.0f);
	ArcTimeOfDayWeatherParticles::SetEmission(DustParticles, DustSystem ? DustScale * 1000.0f : 0.0f);
	ArcTimeOfDayWeatherParticles::SetEmission(WindDebrisParticles, DebrisSystem ? DebrisRate : 0.0f);
	ArcTimeOfDayWeatherParticles::SetEmission(ObscuredLightningParticles, ObscuredSystem ? ObscuredRate : 0.0f);
	LightningLight->SetCastShadows(bLightningFlashesCastShadows);
	LightningLight->SetLightColor(LightningFlashLightSourceColor, true);
	LightningLight->SetEnableLightShaftBloom(false);
	UMaterialInterface* BloomMaterial = bEnableLightningFlashBloom ? LightningFlashBloomMaterial.LoadSynchronous() : nullptr;
	if (BloomMaterial && (!LightningFlashBloomMID || LightningFlashBloomMID->Parent != BloomMaterial))
	{
		LightningFlashBloomMID = UMaterialInstanceDynamic::Create(BloomMaterial, this);
		LightningFlashBloom->Settings.WeightedBlendables.Array.Reset();
		LightningFlashBloom->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, LightningFlashBloomMID));
		LightningFlashBloomMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
	}
	if (LightningFlashBloomMID)
	{
		LightningFlashBloomMID->SetVectorParameterValue(TEXT("Color"), LightningFlashLightSourceColor);
	}
	LightningFlashBloom->BlendWeight = BloomMaterial ? 1.0f : 0.0f;
	if (!bLightningFlashLightSource)
	{
		LightningLight->SetIntensity(0.0f);
		if (LightningFlashBloomMID)
		{
			LightningFlashBloomMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
		}
	}

	ApplyManualEffects(ResolvedWeather);

	bool bShowFlashes = true;
#if WITH_EDITOR
	if (const UWorld* World = GetWorld())
	{
		bShowFlashes = World->WorldType != EWorldType::Editor || bShowLightningFlashesInLevelEditor;
	}
#endif
	const bool bFlashEnabled = bHasLocalView && bSpawnLightningFlashes && ThunderLightning > 0.0f && IsValid(SkyActor) && FlashSystem && bShowFlashes;
	SetActorTickEnabled((RainSystem && RainRate > 0.0f) || (SnowSystem && SnowRate > 0.0f) || (DustSystem && DustScale > 0.0f) || (DebrisSystem && DebrisRate > 0.0f) || (ObscuredSystem && ObscuredRate > 0.0f) || bFlashEnabled || (ScreenDropletsMID && ScreenDropletsPostProcess->BlendWeight > 0.0f));

	if (!bFlashEnabled)
	{
		StopLightningFlash();
		LightningFlashOne->Deactivate();
		LightningFlashTwo->Deactivate();
	}
	if (!bEnableWeatherSoundEffects)
	{
		SoundMixer->Stop();
		SoundDirectional->Stop();
		SoundGlobal->Stop();
	}
}
