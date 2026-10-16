// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/VolumeTexture.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"

USkyLightComponent* AArcTimeOfDayActor::GetSkyLightComponent() const
{
	if (AdjustForPathTracer && PathTracerSkyLight)
	{
		return PathTracerSkyLight.Get();
	}
	if (bRenderSkyLight && CustomSkyLightActor)
	{
		return CustomSkyLightActor->GetLightComponent();
	}
	return CapturedSceneSkyLight.Get();
}

UExponentialHeightFogComponent* AArcTimeOfDayActor::GetHeightFogComponent() const
{
	return bRenderExponentialHeightFog && CustomHeightFogActor ? CustomHeightFogActor->GetComponent() : HeightFog.Get();
}

bool AArcTimeOfDayActor::IsVolumetricFogActive() const
{
	const UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent();
	const IConsoleVariable* VolumetricFogCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricFog"));
	return bRenderExponentialHeightFog && SkyMode != EArcTimeOfDaySkyMode::Space && FogComponent && FogComponent->IsVisible() && FogComponent->bEnableVolumetricFog && VolumetricFogCVar && VolumetricFogCVar->GetInt() != 0;
}

FLinearColor AArcTimeOfDayActor::ThreeColorTimeBlend(const FLinearColor& Day, const FLinearColor& DawnDusk, const FLinearColor& Night) const
{
	return FMath::Lerp(FMath::Lerp(Day, DawnDusk, MapRangeClamped(SunHeight(), 0.5f, 0.65f, 1.0f, 0.0f)), Night, CachedNightFilter);
}

void AArcTimeOfDayActor::ApplyEnvironmentStaticProperties()
{
	const UWorld* World = GetWorld();
	const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
	USkyLightComponent* SkyLight = GetSkyLightComponent();
	CapturedSceneSkyLight->SetVisibility(!AdjustForPathTracer && !CustomSkyLightActor && bRenderSkyLight);
	if (SkyLight)
	{
		SkyLight->SetCastShadows(SkyLight->Mobility == EComponentMobility::Movable ? bMovableSkyLightCastsShadows : bStaticStationarySkyLightCastsShadows);
		SkyLight->SourceType = SkyLightMode == EArcTimeOfDaySkyLightMode::CaptureBased ? ESkyLightSourceType::SLS_CapturedScene : ESkyLightSourceType::SLS_SpecifiedCubemap;
		SkyLight->bRealTimeCapture = SkyLightMode == EArcTimeOfDaySkyLightMode::CaptureBased && bRealTimeCapture;
		if (bMoveSkyLightCaptureWithCameraLocation)
		{
			const FVector CaptureLocation = bSkyLightMovementUsesZAxis ? ViewLocation - FVector(0.0f, 0.0f, 1000.0f) : FVector(ViewLocation.X, ViewLocation.Y, GetActorLocation().Z - 150.0f);
			SkyLight->SetWorldLocation(CaptureLocation);
		}
	}

	UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent();
	HeightFog->SetVisibility(!CustomHeightFogActor && bRenderExponentialHeightFog && SkyMode != EArcTimeOfDaySkyMode::Space);
	if (FogComponent)
	{
		FogComponent->SetVolumetricFogExtinctionScale(bRenderGlobalVolumetricMaterial && FogComponent->bEnableVolumetricFog ? 0.0f : static_cast<float>(VolumetricFogExtinction));
		FogComponent->SetDirectionalInscatteringStartDistance(10000.0f);
	}
	if (IConsoleVariable* ApplyFogLate = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud.ApplyFogLate")))
	{
		ApplyFogLate->Set(bVolumetricCloudsApplyFogLate ? 1 : 0, ECVF_SetByCode);
	}

	SkyAtmosphere->SetWorldLocation(bKeepPlanetTopAtCameraXYLocation ? FVector(ViewLocation.X, ViewLocation.Y, GetActorLocation().Z) : GetActorLocation() + FVector(0.0f, 0.0f, -70.0f));
	if (bControlSkyAtmosphereSettings)
	{
		SkyAtmosphere->SetAerialPespectiveViewDistanceScale(0.0f);
	}

	if (SkySphereMID && ColorMode == EArcTimeOfDayColorMode::SimplifiedColor)
	{
		SkySphereMID->SetTextureParameterValue(TEXT("SunAtmosphereLUT"), SimplifiedColorSunAtmosphereLUT.Get());
		SkySphereMID->SetTextureParameterValue(TEXT("MoonAtmosphereLUT"), SimplifiedColorMoonAtmosphereLUT.Get());
		SkySphereMID->SetScalarParameterValue(TEXT("Flatten Simplified Color"), static_cast<float>(FlattenSimplifiedColorScattering));
	}

	ApplyGlobalVolumetricFog();
}

void AArcTimeOfDayActor::ApplyGlobalVolumetricFog()
{
	const UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent();
	const bool bApplied = FogComponent && FogComponent->bEnableVolumetricFog && bRenderGlobalVolumetricMaterial;
	GlobalVolumetricFogMesh->SetVisibility(bApplied);
	UpdateCloudMovementParameters();
	if (!bApplied)
	{
		return;
	}
	UMaterialInterface* Parent = bRenderGroundFog ? GlobalVolumetricGroundFogMaterial.Get() : GlobalVolumetricFogMaterial.Get();
	if (!GlobalVolumetricFogMID || AppliedGlobalVolumetricFogMaterial != Parent)
	{
		GlobalVolumetricFogMID = GlobalVolumetricFogMesh->CreateDynamicMaterialInstance(0, Parent);
		AppliedGlobalVolumetricFogMaterial = Parent;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::VolumetricFogAlbedo)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::VolumetricFogGlobalMatExtinction)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::VolumetricFogGroundFogExtinction)].bInitialized = false;
	}
	if (!GlobalVolumetricFogMID)
	{
		return;
	}
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Ground Fog Height"), static_cast<float>(GroundFogHeight));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("3D Noise Max Samples"), static_cast<float>(FogNoiseMaxSamples));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Reduce Sample Distance Scale"), static_cast<float>(FogNoiseOuterDistance));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("3D Noise Intensity"), static_cast<float>(FogNoiseIntensity));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Noise Scale"), static_cast<float>(FogNoiseScale));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Ground Fog Hardness"), static_cast<float>(GroundFogHardness));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Angle Threshold"), static_cast<float>(GroundFogAngleThreshold));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Ground Fog Height Noise"), static_cast<float>(GroundFogHeightNoise));
	GlobalVolumetricFogMID->SetScalarParameterValue(TEXT("Masked Extinction"), static_cast<float>(FogMaskedExtinction));
	const FVector MoveSpeed(0.0f, 0.0f, -0.015f * FogNoiseSpeedScale);
	GlobalVolumetricFogMID->SetVectorParameterValue(TEXT("Move Speed"), FLinearColor(MoveSpeed.X, MoveSpeed.Y, MoveSpeed.Z, 1.0f));
}
