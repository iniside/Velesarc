// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "ArcTimeOfDayModule.h"

#include "DaySequenceSubsystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "NiagaraComponent.h"
#include "Misc/App.h"

namespace ArcTimeOfDayActor
{
	constexpr float UDSDayUnits = 2400.0f;
	constexpr float DefaultDayLength = 24.0f;
}

AArcTimeOfDayActor::AArcTimeOfDayActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	SetDayLength(ArcTimeOfDayActor::DefaultDayLength);
	SetInitialTimeOfDay(UDSTimeToHours(TimeOfDay, ArcTimeOfDayActor::DefaultDayLength));
#if WITH_EDITOR
	SetTimeOfDayPreview(UDSTimeToHours(TimeOfDay, ArcTimeOfDayActor::DefaultDayLength));
#endif

	SkyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SkyRoot"));
	SetRootComponent(SkyRoot);
	GeneralPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("General Post Process"));
	GeneralPostProcess->SetupAttachment(SkyRoot);
	GeneralPostProcess->bUnbound = true;
	SunParent = CreateDefaultSubobject<USceneComponent>(TEXT("Sun Parent"));
	SunParent->SetupAttachment(SkyRoot);
	MoonParent = CreateDefaultSubobject<USceneComponent>(TEXT("Moon Parent"));
	MoonParent->SetupAttachment(SkyRoot);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SkyRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->SetIntensity(5.0f);
	Sun->SetVisibility(true);
	Sun->SetTransmission(false);
	Sun->SetIndirectLightingIntensity(1.0f);
	Sun->bEnableLightShaftBloom = false;
	Sun->BloomMaxBrightness = 0.35f;
	Sun->BloomThreshold = 0.75f;
	Sun->BloomTint = FColor::White;
	Sun->bUseTemperature = true;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(SkyRoot);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetIntensity(0.0f);
	Moon->SetVisibility(true);
	Moon->SetLightColor(FLinearColor(0.445801f, 0.557475f, 0.864583f, 1.0f), true);
	Moon->SetTransmission(false);
	Moon->SetIndirectLightingIntensity(1.0f);
	Moon->bEnableLightShaftBloom = false;
	Moon->BloomMaxBrightness = 0.15f;
	Moon->BloomThreshold = 0.15f;
	Moon->BloomTint = FColor::White;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(SkyRoot);
	SkyAtmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtComponentTransform;
	SkyAtmosphere->SetRayleighScatteringScale(0.04f);
	SkyAtmosphere->SetRayleighScattering(FLinearColor(0.168627f, 0.407843f, 1.0f, 1.0f));
	SkyAtmosphere->SetMieScatteringScale(0.013996f);
	SkyAtmosphere->SetMieScattering(FLinearColor(0.802083f, 0.879982f, 1.0f, 1.0f));
	SkyAtmosphere->SetMieAnisotropy(0.75f);
	SkyAtmosphere->SetOtherAbsorptionScale(0.002f);
	SkyAtmosphere->SetOtherAbsorption(FLinearColor(0.897238f, 1.0f, 0.095307f, 0.002f));
	SkyAtmosphere->SetAerialPespectiveViewDistanceScale(0.0f);
	SkyAtmosphere->SetHeightFogContribution(2.0f);
	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(SkyRoot);
	HeightFog->SetMobility(EComponentMobility::Movable);
	HeightFog->SetFogInscatteringColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	HeightFog->SetDirectionalInscatteringExponent(5.0f);
	HeightFog->SetDirectionalInscatteringStartDistance(10000.0f);
	HeightFog->SetVolumetricFog(false);
	HeightFog->SetVolumetricFogDistance(8000.0f);
	HeightFog->SetVolumetricFogExtinctionScale(2.0f);
	VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
	VolumetricCloud->SetupAttachment(SkyRoot);
	VolumetricCloud->SetMobility(EComponentMobility::Movable);
	VolumetricCloud->SetbUsePerSampleAtmosphericLightTransmittance(true);
	VolumetricCloud->SetSkyLightCloudBottomOcclusion(0.5f);
	VolumetricAurora = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricAurora"));
	VolumetricAurora->SetupAttachment(SkyRoot);
	VolumetricAurora->SetMobility(EComponentMobility::Movable);
	VolumetricAurora->SetSkyLightCloudBottomOcclusion(0.0f);
	VolumetricAurora->SetViewSampleCountScale(1.0f);
	VolumetricAurora->SetLayerHeight(1.20000005f);
	VolumetricAurora->SetLayerBottomAltitude(0.5f);
	VolumetricAurora->SetTracingStartMaxDistance(30.0f);
	VolumetricAurora->SetHiddenInGame(true);
	CapturedSceneSkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("Captured Scene Sky Light"));
	CapturedSceneSkyLight->SetupAttachment(SkyRoot);
	CapturedSceneSkyLight->SetMobility(EComponentMobility::Movable);
	CapturedSceneSkyLight->bRealTimeCapture = true;
	CapturedSceneSkyLight->SourceCubemapAngle = 0.0f;
	GlobalVolumetricFogMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Global Volumetric Fog Mesh"));
	GlobalVolumetricFogMesh->SetupAttachment(SkyRoot);
	GlobalVolumetricFogMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlobalVolumetricFogMesh->SetCastShadow(false);
	GlobalVolumetricFogMesh->SetVisibility(false);

	SkySphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sky_Sphere"));
	SkySphere->SetupAttachment(SkyRoot);
	SkySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkySphere->SetCastShadow(false);
	SkySphere->bCastDynamicShadow = false;
	SkySphere->bCastStaticShadow = false;

	UDSLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UDS Label"));
	UDSLabel->SetupAttachment(SkyRoot);
	Compass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Compass"));
	Compass->SetupAttachment(SkyRoot);
	WestLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("W"));
	WestLabel->SetupAttachment(Compass);
	ClockDisk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClockDisk"));
	ClockDisk->SetupAttachment(Compass);
	NorthLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("N"));
	NorthLabel->SetupAttachment(Compass);
	SouthLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("S"));
	SouthLabel->SetupAttachment(Compass);
	EastLabel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("E"));
	EastLabel->SetupAttachment(Compass);
	MoonHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Moon Handle"));
	MoonHandle->SetupAttachment(MoonParent);
	SunHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sun Handle"));
	SunHandle->SetupAttachment(SunParent);
	RealStarsSprites = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Real Stars Sprites"));
	RealStarsSprites->SetupAttachment(SkyRoot);
	RealStarsSprites->SetAutoActivate(false);
	VolumetricCloudLightRays = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Volumetric Cloud Light Rays"));
	VolumetricCloudLightRays->SetupAttachment(SkyRoot);
	VolumetricCloudLightRays->SetAutoActivate(false);
	InsideCloudFogMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Inside Cloud Fog Mesh"));
	InsideCloudFogMesh->SetupAttachment(SkyRoot);
	InsideCloudFogMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InsideCloudFogMesh->SetCastShadow(false);
	InsideCloudFogMesh->SetVisibility(false);

	LoadFoundationResources();
}

void AArcTimeOfDayActor::RefreshSky()
{
	bAlternateVisualResourcesDirty = true;
	SynchronizeTimeFromRawDaySequence();
	EvaluateEditorSky(true);
}

void AArcTimeOfDayActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
#if WITH_EDITOR
	OnTimeOfDayPreviewChangedEvent.RemoveAll(this);
	OnTimeOfDayPreviewChangedEvent.AddUObject(this, &AArcTimeOfDayActor::HandleTimeOfDayPreviewChanged);
#endif
	RefreshFromRawDaySequenceTime();
}

void AArcTimeOfDayActor::BeginPlay()
{
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		if (UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			DaySequenceSubsystem->SetDaySequenceActor(this);
		}
	}
	RefreshFromRawDaySequenceTime();
}

void AArcTimeOfDayActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	OnTimeOfDayPreviewChangedEvent.RemoveAll(this);
	OnTimeOfDayPreviewChangedEvent.AddUObject(this, &AArcTimeOfDayActor::HandleTimeOfDayPreviewChanged);
#endif
	if (const UWorld* World = GetWorld())
	{
		if (UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			if (DaySequenceSubsystem->GetDaySequenceActor(false) != this)
			{
				DaySequenceSubsystem->SetDaySequenceActor(this);
			}
		}
	}
}

void AArcTimeOfDayActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceCloudMovement(FApp::GetDeltaTime());
	UpdateCloudMovementParameters();
}

void AArcTimeOfDayActor::SequencePlayerUpdated(const float CurrentTime, const float PreviousTime)
{
	const bool bLogTiming = IsPlaying() && UE_LOG_ACTIVE(LogArcTimeOfDay, Verbose);
	const double UpdateStartSeconds = bLogTiming ? FPlatformTime::Seconds() : 0.0;
	Super::SequencePlayerUpdated(CurrentTime, PreviousTime);
	RefreshFromRawDaySequenceTime();
	if (bLogTiming)
	{
		const double UpdateMilliseconds = (FPlatformTime::Seconds() - UpdateStartSeconds) * 1000.0;
		UE_LOGF(LogArcTimeOfDay, Verbose, "%ls dynamic time-of-day update: %.3f ms", *GetName(), UpdateMilliseconds);
	}
}

#if WITH_EDITOR
bool AArcTimeOfDayActor::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AArcTimeOfDayActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bAlternateVisualResourcesDirty = true;
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AArcTimeOfDayActor, TimeOfDay))
	{
		SetUDSTimeOfDay(TimeOfDay);
		return;
	}
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AArcTimeOfDayActor, bAutomaticallyApplyCalculatedValues))
	{
		SynchronizeTimeFromRawDaySequence();
		if (bAutomaticallyApplyCalculatedValues)
		{
			EvaluateEditorSky(true);
		}
		return;
	}
	RefreshFromRawDaySequenceTime();
}

void AArcTimeOfDayActor::BeginDestroy()
{
	OnTimeOfDayPreviewChangedEvent.RemoveAll(this);
	Super::BeginDestroy();
}

void AArcTimeOfDayActor::HandleTimeOfDayPreviewChanged(const float PreviewHours)
{
	TimeOfDay = HoursToUDSTime(PreviewHours, GetDayLength());
	RefreshFromRawDaySequenceTime();
}
#endif
