// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/RendererSettings.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace ArcTimeOfDayAlternateModes
{
	template <typename TObjectType>
	TObjectType* Load(const TCHAR* Path)
	{
		return LoadObject<TObjectType>(nullptr, Path);
	}

	void SetConsoleInt(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name)) Variable->Set(Value, ECVF_SetByGameSetting);
	}

	UPostProcessComponent* MakePostProcess(AActor* Owner, USceneComponent* Parent, const FName Name, UMaterialInterface* Material, TObjectPtr<UMaterialInstanceDynamic>& OutMID)
	{
		UPostProcessComponent* Component = NewObject<UPostProcessComponent>(Owner, Name, RF_Transient);
		Component->SetupAttachment(Parent);
		Component->RegisterComponent();
		Component->bUnbound = true;
		if (Material)
		{
			OutMID = UMaterialInstanceDynamic::Create(Material, Owner);
			Component->Settings.AddBlendable(OutMID, 1.0f);
		}
		return Component;
	}
}

void AArcTimeOfDayActor::ApplyAlternateVisualModes()
{
	if (!SkySphereMID) return;
	if (bAlternateVisualResourcesDirty)
	{
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::AuroraIntensity)].bInitialized = false;
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::AuroraPhase)].bInitialized = false;
	}

	if (SkyMode == EArcTimeOfDaySkyMode::DynamicClouds2D)
	{
		if (CloudShadowsMID) CloudShadowsMID->SetScalarParameterValue(TEXT("Soften_Cloud_Layer_1"), static_cast<float>(SoftenTwoDDynamicCloudLayer1 + 1.0));
		SkySphereMID->SetScalarParameterValue(TEXT("2D Clouds Shading Offset"), static_cast<float>(TwoDCloudsShadingOffset) * -0.028f);
		SkySphereMID->SetScalarParameterValue(TEXT("Soften_Cloud_Layer_2"), static_cast<float>(SoftenTwoDDynamicCloudLayer2 + 1.0));
		SkySphereMID->SetScalarParameterValue(TEXT("Soften Horizon"), static_cast<float>(SoftenTwoDDynamicCloudHorizon));
		SkySphereMID->SetScalarParameterValue(TEXT("Cloud Opacity"), static_cast<float>(TwoDDynamicCloudOpacity));
		SkySphereMID->SetScalarParameterValue(TEXT("Cloud Tiling"), static_cast<float>(TwoDDynamicCloudTiling));
		SkySphereMID->SetScalarParameterValue(TEXT("Cloud Height"), static_cast<float>(TwoDDynamicCloudHeight));
		SkySphereMID->SetScalarParameterValue(TEXT("OvercastSwirl"), static_cast<float>(TwoDDynamicCloudsOvercastTexture / 13.0));
		SkySphereMID->SetTextureParameterValue(TEXT("clouds_basetex"), TwoDDynamicCloudFormationTexture.LoadSynchronous());
		SkySphereMID->SetScalarParameterValue(TEXT("Cloud Sharpness"), static_cast<float>(SharpenTwoDDynamicCloudOuterEdge + 1.0));
		SkySphereMID->SetScalarParameterValue(TEXT("One cloud layer"), TwoDDynamicCloudsUseSingleLayer ? 0.0f : 1.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Horizon_Density_Multiplier"), static_cast<float>(TwoDCloudsHorizonDensityMultiplier));
		SkySphereMID->SetScalarParameterValue(TEXT("Zenith_Density_Multiplier"), static_cast<float>(TwoDCloudsZenithDensityMultiplier));
		SkySphereMID->SetScalarParameterValue(TEXT("Latitude_Gradient_Position"), static_cast<float>(TwoDCloudsLatitudeGradientPosition));
		SkySphereMID->SetScalarParameterValue(TEXT("Latitude_Gradient_Width"), static_cast<float>(100.0 / TwoDCloudsLatitudeGradientWidth));
		SkySphereMID->SetScalarParameterValue(TEXT("Around_Sun_Density_Exponent"), static_cast<float>(TwoDCloudsAroundSunDensityExponent));
		SkySphereMID->SetScalarParameterValue(TEXT("Around_Sun_Density_Multiplier"), static_cast<float>(TwoDCloudsAroundSunDensityMultiplier));
		SkySphereMID->SetScalarParameterValue(TEXT("Around_Moon_Density_Exponent"), static_cast<float>(TwoDCloudsAroundMoonDensityExponent));
		SkySphereMID->SetScalarParameterValue(TEXT("Around_Moon_Density_Multiplier"), static_cast<float>(TwoDCloudsAroundMoonDensityMultiplier));

		const float SunSelector = (CachedSunVector.Z - 0.03f) * 15.0f;
		const FVector ShadingVector = SunSelector > 0.5f ? CachedMoonVector : CachedSunVector;
		const float ShadingAlpha = MapRangeClamped(FMath::Abs(FMath::Clamp(SunSelector, 0.0f, 1.0f) - 0.5f), 0.0f, 0.5f, 1.0f, 0.0f);
		CacheColorValue(EArcTimeOfDayCachedProperty::ShadingOffsetVector, FLinearColor(ShadingVector.X, ShadingVector.Y, ShadingVector.Z, ShadingAlpha), 0.05f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::ShineIntensity, TwoDDynamicCloudsShineIntensityCurve ? TwoDDynamicCloudsShineIntensityCurve->GetFloatValue(SunHeight()) : 0.0f, 0.001f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::SunHighlightRadius, (TwoDDynamicCloudsSunHighlightRadiusCurve ? TwoDDynamicCloudsSunHighlightRadiusCurve->GetFloatValue(SunHeight()) : 0.0f) * static_cast<float>(TwoDDynamicCloudsSunHighlightRadiusScale), 0.01f);
		CacheFloatValue(EArcTimeOfDayCachedProperty::SunHighlightIntensity, (TwoDDynamicCloudsSunHighlightIntensityCurve ? TwoDDynamicCloudsSunHighlightIntensityCurve->GetFloatValue(SunHeight()) : 0.0f) * EclipsePercent, 0.5f);
		const FLinearColor OvercastTint = ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere ? FLinearColor(0.428691f, 0.558341f, 0.715694f, 1.0f) : FLinearColor::White;
		CacheColorValue(EArcTimeOfDayCachedProperty::TwoDCloudTint, FMath::Lerp(FLinearColor::White, OvercastTint, GlobalOvercast01), 0.001f);
	}

	if (SkyMode == EArcTimeOfDaySkyMode::StaticClouds)
	{
		SkySphereMID->SetTextureParameterValue(TEXT("Static Clouds"), StaticCloudsTexture.LoadSynchronous());
		SkySphereMID->SetTextureParameterValue(TEXT("Flat Overcast Texture"), StaticOvercastTexture.LoadSynchronous());
		SkySphereMID->SetScalarParameterValue(TEXT("Flat Overcast Strength"), static_cast<float>(StaticOvercastStrength));
		const UWorld* World = GetWorld();
		const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
		if (!FMath::IsNearlyZero(StaticCloudsRotationSpeed))
		{
			StaticCloudsDynamicRotation += static_cast<float>(FMath::Min(CurrentTime - LastStaticCloudsUpdateTime, 0.5) * StaticCloudsRotationSpeed);
			LastStaticCloudsUpdateTime = CurrentTime;
		}
		else
		{
			StaticCloudsDynamicRotation = 0.0f;
		}
		CacheFloatValue(EArcTimeOfDayCachedProperty::StaticCloudsDynamicRotation, FMath::Fmod(StaticCloudsDynamicRotation + static_cast<float>(StaticCloudsRotation), 360.0f) / 360.0f, 0.001f);

		const float SunOffset = CachedSunVector.Z - 0.12f;
		const float LightBlendAlpha = MapRangeClamped(FMath::Abs(SunOffset), 0.0f, 0.12f, 1.0f, 0.0f);
		const FVector SecondaryVector = bRenderMoon ? CachedMoonVector : FVector::UpVector;
		const FVector LocalVector = FRotator(0.0f, static_cast<float>(StaticCloudsRotation) + StaticCloudsDynamicRotation, 0.0f).UnrotateVector(SunOffset < 0.0f ? CachedSunVector : SecondaryVector);
		const float Angle = FMath::Fmod(FRotationMatrix::MakeFromX(LocalVector).Rotator().Yaw + 360.0f, 360.0f) / 120.0f;
		const FVector ColorMask(1.0f - FMath::Clamp(FMath::Min(FMath::Abs(Angle - 3.0f), FMath::Abs(Angle)), 0.0f, 1.0f), 1.0f - FMath::Clamp(FMath::Abs(Angle - 1.0f), 0.0f, 1.0f), 1.0f - FMath::Clamp(FMath::Abs(Angle - 2.0f), 0.0f, 1.0f));
		const float Blend = FMath::Max(MapRangeClamped(LocalVector.Z, -0.75f, -0.95f, 0.0f, 1.0f), LightBlendAlpha);
		const FVector LightingMask = FMath::Lerp(ColorMask, FVector(1.0 / 3.0), Blend) * MapRangeClamped(FMath::Abs(CachedSunVector.Z - 0.03f), 0.0f, 0.03f, 0.4f, 1.0f);
		CacheColorValue(EArcTimeOfDayCachedProperty::StaticCloudsLightingMask, FLinearColor(LightingMask.X, LightingMask.Y, LightingMask.Z, 1.0f), 0.001f);

		const FLinearColor BaseTint = ThreeColorTimeBlend(FLinearColor(0.701009f, 0.702989f, 0.760417f), FLinearColor(0.600911f, 0.664388f, 0.8125f), FLinearColor(0.396755f, 0.40724f, 0.428691f)) * static_cast<float>(StaticCloudsColorIntensity);
		const FLinearColor ShadowTint = BaseTint * ThreeColorTimeBlend(FLinearColor(1.55f, 1.494028f, 1.340104f), FLinearColor(1.35f, 1.081833f, 1.068399f), FLinearColor::White) * 0.8f;
		FLinearColor LightTint = BaseTint;
		LightTint.A = StaticCloudsFadeInClearSkies ? FMath::Clamp(LocalCloudCoverage, 0.0f, 1.0f) : 1.0f;
		FLinearColor AppliedShadowTint = ShadowTint;
		AppliedShadowTint.A = MapRangeClamped(CachedSunVector.Z, -0.04f, 0.05f, 0.0f, 1.0f);
		CacheColorValue(EArcTimeOfDayCachedProperty::StaticCloudsTintColor, LightTint, 0.0001f);
		CacheColorValue(EArcTimeOfDayCachedProperty::StaticCloudsShadowColor, AppliedShadowTint, 0.0001f);
	}

	const bool bUsingAurora = SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora || UseAuroras;
	if (bUsingAurora)
	{
		UMaterialInstanceDynamic* AuroraTarget = SkySphereMID;
		if (SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora)
		{
			if (!VolumetricAuroraMID)
			{
				if (UMaterialInterface* Parent = ArcTimeOfDayAlternateModes::Load<UMaterialInterface>(TEXT("/Game/UltraDynamicSky/Materials/Volumetric_Aurora.Volumetric_Aurora")))
				{
					VolumetricAuroraMID = UMaterialInstanceDynamic::Create(Parent, VolumetricAurora);
					CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::AuroraIntensity)].bInitialized = false;
					CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::AuroraPhase)].bInitialized = false;
				}
			}
			AuroraTarget = VolumetricAuroraMID;
			VolumetricAurora->SetMaterial(VolumetricAuroraMID);
			const float FadeDistance = VolumetricAurora->TracingStartMaxDistance * 100000.0f;
			VolumetricAurora->SetTracingMaxDistance(10.0f);
			VolumetricAurora->SetReflectionViewSampleCountScale(0.4166f);
			VolumetricAurora->SetShadowViewSampleCountScale(0.25f);
			VolumetricAurora->SetShadowReflectionViewSampleCountScale(0.075f);
			VolumetricAurora->SetShadowTracingDistance(0.1f);
			ArcTimeOfDayAlternateModes::SetConsoleInt(TEXT("r.VolumetricCloud.DistanceToSampleMaxCount"), 3);
			ArcTimeOfDayAlternateModes::SetConsoleInt(TEXT("r.VolumetricRenderTarget.Mode"), VolumetricAuroraRenderingMode == EArcTimeOfDayVolumetricRenderingMode::FidelityAndPerformance ? 0 : 2);
			if (AuroraTarget)
			{
				AuroraTarget->SetScalarParameterValue(TEXT("Texture Mip"), static_cast<float>(VolumetricAuroraTextureMip));
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Scale"), static_cast<float>(AuroraTextureScale) * 4000000.0f);
				AuroraTarget->SetScalarParameterValue(TEXT("Fade Distance"), FadeDistance);
			}
		}
		if (AuroraTarget)
		{
			AuroraTarget->SetTextureParameterValue(TEXT("Aurora Tex"), AuroraTexture.LoadSynchronous());
			AuroraTarget->SetVectorParameterValue(TEXT("Aurora_Color_1"), AuroraColor1);
			AuroraTarget->SetVectorParameterValue(TEXT("Aurora_Color_2"), AuroraColor2);
			AuroraTarget->SetVectorParameterValue(TEXT("Aurora_Color_3"), AuroraColor3);
			AuroraTarget->SetScalarParameterValue(TEXT("Aurora Speed"), static_cast<float>(AuroraSpeed));
			AuroraTarget->SetScalarParameterValue(TEXT("Aurora Shape Change Speed"), static_cast<float>(AuroraShapeChangeSpeed));
			AuroraTarget->SetScalarParameterValue(TEXT("Aurora Reflection Brightness"), static_cast<float>(AuroraSkyLightReflectionBrightness));
			if (AuroraTarget == SkySphereMID)
			{
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Line Power"), static_cast<float>(TwoDAuroraLineExponent));
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Mask Power"), static_cast<float>(TwoDAuroraMaskExponent));
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Step Size"), static_cast<float>(TwoDAuroraSampleStepSize));
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Scale"), static_cast<float>(1.0 / AuroraTextureScale));
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Mip"), static_cast<float>(TwoDAuroraTextureMip));
				AuroraTarget->SetScalarParameterValue(TEXT("Aurora Dither"), static_cast<float>(TwoDAuroraDither));
			}
		}
		const float CombinedNightBrightness = static_cast<float>(NightBrightness + ExtraNightBrightnessWhenCloudy * LocalOvercast01);
		CacheFloatValue(EArcTimeOfDayCachedProperty::AuroraIntensity, (1.0f - LocalOvercast01) * FMath::Lerp(static_cast<float>(DaytimeAuroraIntensity), static_cast<float>(AuroraIntensity) * CombinedNightBrightness, CachedNightFilter), 0.0003f);
		const float AuroraPhaseValue = AuroraMovesWithTimeOfDay ? static_cast<float>(TimeOfDay / 25.0 * AuroraTimeOfDayMovementMultiplier + AuroraPhase) : static_cast<float>(AuroraPhase);
		CacheFloatValue(EArcTimeOfDayCachedProperty::AuroraPhase, AuroraPhaseValue, 0.0003f);
	}
	else
	{
		CacheFloatValue(EArcTimeOfDayCachedProperty::AuroraIntensity, 0.0f, 0.0003f);
	}

	if (GeneralPostProcess)
	{
		GeneralPostProcess->Settings.bOverride_LensFlareIntensity = EnableSunLensFlare && DisableImageBasedLensFlaresWhenEnabled;
		if (GeneralPostProcess->Settings.bOverride_LensFlareIntensity) GeneralPostProcess->Settings.LensFlareIntensity = 0.0f;
	}
	if (EnableSunLensFlare)
	{
		UMaterialInterface* Parent = CustomLensFlareParentInstance;
		if (!Parent)
		{
			const TCHAR* Path = LensFlareType == EArcTimeOfDayLensFlareType::Anamorphic ? TEXT("/Game/UltraDynamicSky/Materials/Material_Instances/Lens_Flares/Anamorphic.Anamorphic") : LensFlareType == EArcTimeOfDayLensFlareType::Prime35mm ? TEXT("/Game/UltraDynamicSky/Materials/Material_Instances/Lens_Flares/35mm_Prime.35mm_Prime") : TEXT("/Game/UltraDynamicSky/Materials/Material_Instances/Lens_Flares/Zoom_Chromatic.Zoom_Chromatic");
			Parent = ArcTimeOfDayAlternateModes::Load<UMaterialInterface>(Path);
		}
		if (LensFlarePostProcess && AppliedLensFlareMaterial != Parent)
		{
			LensFlarePostProcess->DestroyComponent();
			LensFlarePostProcess = nullptr;
			LensFlareMID = nullptr;
		}
		if (!LensFlarePostProcess)
		{
			LensFlarePostProcess = ArcTimeOfDayAlternateModes::MakePostProcess(this, SkyRoot, TEXT("Lens Flare Post Process"), Parent, LensFlareMID);
			AppliedLensFlareMaterial = Parent;
			CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::EnableLensFlarePostProcess)].bInitialized = false;
		}
		if (LensFlareMID)
		{
			LensFlareMID->SetVectorParameterValue(TEXT("Color"), LensFlareTint * static_cast<float>(LensFlareStrength));
			LensFlareMID->SetScalarParameterValue(TEXT("Scale_Elements"), static_cast<float>(1.0 / ScaleFlareElements));
			LensFlareMID->SetScalarParameterValue(TEXT("Brightness Threshold"), static_cast<float>(LensFlareBrightnessThreshold));
		}
		const UWorld* World = GetWorld();
		const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
		const bool bAllowFlare = SkyMode == EArcTimeOfDaySkyMode::Space || ProjectMode == EArcTimeOfDayProjectMode::CinematicOffline || CachedSunVector.Z < 0.0f && (LocalCloudCoverage < 2.0f || (SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds) && ViewLocation.Z > FMath::Lerp(CloudMainLayerBottomHeight, CloudMainLayerTopHeight, 0.25f));
		CacheFloatValue(EArcTimeOfDayCachedProperty::EnableLensFlarePostProcess, bAllowFlare ? 1.0f : 0.0f, 0.5f);
	}
	else
	{
		CacheFloatValue(EArcTimeOfDayCachedProperty::EnableLensFlarePostProcess, 0.0f, 0.5f);
	}

	if (AdjustForPathTracer)
	{
		USkyLightComponent* PersistentSkyLight = bRenderSkyLight && CustomSkyLightActor
			? CustomSkyLightActor->GetLightComponent()
			: CapturedSceneSkyLight.Get();
		const bool bCreatedPathTracerSkyLight = !PathTracerSkyLight;
		if (!PathTracerSkyLight)
		{
			PathTracerSkyLight = NewObject<USkyLightComponent>(this, TEXT("Path Tracer Sky Light"), RF_Transient);
			PathTracerSkyLight->SetupAttachment(SkyRoot);
			PathTracerSkyLight->RegisterComponent();
		}
		PathTracerSkyLight->SetMobility(PersistentSkyLight->Mobility);
		PathTracerSkyLight->SetCastShadows(PersistentSkyLight->Mobility == EComponentMobility::Movable ? bMovableSkyLightCastsShadows : bStaticStationarySkyLightCastsShadows);
		PathTracerSkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
		PathTracerSkyLight->Cubemap = nullptr;
		PathTracerSkyLight->bRealTimeCapture = true;
		PathTracerSkyLight->CubemapResolution = PathTracerSkyResolution == EArcTimeOfDayPathTracerSkyResolution::Resolution4096 ? 4096 : 2048;
		if (bMoveSkyLightCaptureWithCameraLocation)
		{
			const UWorld* World = GetWorld();
			const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
			const FVector CaptureLocation = bSkyLightMovementUsesZAxis ? ViewLocation - FVector(0.0f, 0.0f, 1000.0f) : FVector(ViewLocation.X, ViewLocation.Y, GetActorLocation().Z - 150.0f);
			PathTracerSkyLight->SetWorldLocation(CaptureLocation);
		}
		if (bCreatedPathTracerSkyLight) RetargetSkyLightCachedProperties();
		PathTracerSkyLight->SetVisibility(true);
		CapturedSceneSkyLight->SetVisibility(false);
		if (GeneralPostProcess)
		{
			GeneralPostProcess->Settings.bOverride_PathTracingEnableReferenceAtmosphere = true;
			GeneralPostProcess->Settings.PathTracingEnableReferenceAtmosphere = false;
		}
		SkySphereMID->SetScalarParameterValue(TEXT("Night Sky Glow Reflection Intensity"), 1.0f);
		SkySphereMID->SetScalarParameterValue(TEXT("Stars Speed"), 0.0f);
		UMaterialInterface* PathTracerFogParent = ArcTimeOfDayAlternateModes::Load<UMaterialInterface>(TEXT("/Game/UltraDynamicSky/Materials/PathTracer_ExpHeightFog.PathTracer_ExpHeightFog"));
		UExponentialHeightFogComponent* FogComponent = GetHeightFogComponent();
		const bool bEnablePathTracerFog = RenderHeightFogInPathTracerUsingPostProcess && bRenderExponentialHeightFog && SkyMode != EArcTimeOfDaySkyMode::Space && PathTracerFogParent && FogComponent;
		if (bEnablePathTracerFog)
		{
			if (!PathTracerFogPostProcess)
			{
				PathTracerFogPostProcess = ArcTimeOfDayAlternateModes::MakePostProcess(this, SkyRoot, TEXT("Path Tracer Fog Post Process"), PathTracerFogParent, PathTracerFogMID);
			}
			const bool bHasPathTracerFogMaterial = PathTracerFogPostProcess && PathTracerFogMID;
			if (PathTracerFogPostProcess) PathTracerFogPostProcess->bEnabled = bHasPathTracerFogMaterial;
			if (bHasPathTracerFogMaterial)
			{
				PathTracerFogMID->SetScalarParameterValue(TEXT("Fog Density"), FogComponent->FogDensity);
				PathTracerFogMID->SetScalarParameterValue(TEXT("Height Falloff"), FogComponent->FogHeightFalloff);
				PathTracerFogMID->SetScalarParameterValue(TEXT("Start Distance"), FogComponent->StartDistance);
				PathTracerFogMID->SetScalarParameterValue(TEXT("Directional Inscattering Exponent"), FogComponent->DirectionalInscatteringExponent);
				PathTracerFogMID->SetVectorParameterValue(TEXT("Inscattering"), FogComponent->FogInscatteringLuminance);
				PathTracerFogMID->SetVectorParameterValue(TEXT("Directional Inscattering"), FogComponent->DirectionalInscatteringLuminance);
				const FVector SunDirection = SunWorldRotation.Vector() * -1.0f;
				PathTracerFogMID->SetVectorParameterValue(TEXT("Sun Vector"), FLinearColor(SunDirection.X, SunDirection.Y, SunDirection.Z, 1.0f));
				PathTracerFogMID->SetScalarParameterValue(TEXT("Fog Height"), FogComponent->GetComponentLocation().Z);
				PathTracerFogMID->SetScalarParameterValue(TEXT("Dir Scattering Start Distance"), FogComponent->DirectionalInscatteringStartDistance);
				PathTracerFogMID->SetScalarParameterValue(TEXT("Fog Max Opacity"), FogComponent->bEnableVolumetricFog ? MapRangeClamped(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)), 5.0f, 10.0f, 1.0f, 0.0f) : 1.0f);
			}
		}
		else if (PathTracerFogPostProcess)
		{
			PathTracerFogPostProcess->bEnabled = false;
		}
	}
	else
	{
		if (PathTracerSkyLight)
		{
			PathTracerSkyLight->DestroyComponent();
			PathTracerSkyLight = nullptr;
			CapturedSceneSkyLight->SetVisibility(!CustomSkyLightActor && bRenderSkyLight);
			RetargetSkyLightCachedProperties();
		}
		if (PathTracerFogPostProcess) PathTracerFogPostProcess->bEnabled = false;
		if (GeneralPostProcess) GeneralPostProcess->Settings.bOverride_PathTracingEnableReferenceAtmosphere = false;
	}

	ApplyPostProcessStages();
	const bool bVoxelCloudModeChanged = (SkyMode == EArcTimeOfDaySkyMode::VoxelClouds) != (VoxelCloudsMesh != nullptr);
	const bool bVoxelCloudGridChanged = SkyMode == EArcTimeOfDaySkyMode::VoxelClouds && AppliedVoxelCloudsGridCount != VoxelCloudsGridCount;
	if (bAlternateVisualResourcesDirty || bVoxelCloudModeChanged || bVoxelCloudGridChanged)
	{
		RebuildVoxelClouds();
	}
	if (VoxelCloudsMID)
	{
		VoxelCloudsMID->SetTextureParameterValue(TEXT("Formation Texture"), VolumetricCloudFormationVolumeTexture);
		VoxelCloudsMID->SetScalarParameterValue(TEXT("Grid Size"), static_cast<float>(VoxelCloudsVoxelSize * VolumetricCloudsScale));
		VoxelCloudsMID->SetScalarParameterValue(TEXT("Shading Softness"), static_cast<float>(VoxelCloudsShadingSoftness));
		VoxelCloudsMID->SetScalarParameterValue(TEXT("AO Intensity"), static_cast<float>(VoxelCloudsAOIntensity));
		VoxelCloudsMID->SetScalarParameterValue(TEXT("Horizon Distance"), static_cast<float>(VoxelCloudsHorizonDistance));
		VoxelCloudsMID->SetScalarParameterValue(TEXT("Horizon Deform"), static_cast<float>(VoxelCloudsHorizonDeform));
		VoxelCloudsMID->SetScalarParameterValue(TEXT("Shading Intensity"), static_cast<float>(VoxelCloudsShadingIntensity));
	}
	if (bAlternateVisualResourcesDirty || bAppliedRenderNebula != RenderNebula)
	{
		RebuildSpaceLayer();
	}
	bAlternateVisualResourcesDirty = false;
	if (SkyMode == EArcTimeOfDaySkyMode::VoxelClouds && VoxelCloudsMesh)
	{
		const UWorld* World = GetWorld();
		const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
		const float VoxelScale = static_cast<float>(VoxelCloudsVoxelSize * VolumetricCloudsScale);
		const float Grid = VoxelScale * 10.0f;
		const FVector NewLocation(FMath::GridSnap(ViewLocation.X, Grid), FMath::GridSnap(ViewLocation.Y, Grid), CloudMainLayerBottomHeight);
		VoxelCloudsMesh->SetWorldScale3D(FVector(VoxelScale / 100.0f));
		if (!NewLocation.Equals(VoxelCloudsCenterPosition))
		{
			VoxelCloudsCenterPosition = NewLocation;
			VoxelCloudsMesh->SetWorldLocation(NewLocation);
		}
	}
	const bool bDBufferSupported = GetDefault<URendererSettings>()->bDBuffer;
	bSpaceLayerActive = bDBufferSupported && (!PlanetsMoons.IsEmpty() || RenderNebula || !CustomSpaceLayerDecals.IsEmpty() || ForceEnableSpaceLayer);
	SkySphereMID->SetScalarParameterValue(TEXT("Cancel Dbuffer"), bSpaceLayerActive ? 0.0f : 1.0f);
	SkySphereMID->SetScalarParameterValue(TEXT("Moon Blocks Space Layer"), MoonRendersBehindSpaceLayer ? 0.0f : 1.0f);
	for (UStaticMeshComponent* Component : SpaceDistantPlanetComponents)
	{
		if (Component) Component->SetRelativeLocation(FVector(SpacePlanetDistance, 0.0, 0.0));
	}
	for (UStaticMeshComponent* Component : SpaceDecalComponents)
	{
		if (Component) Component->SetRelativeLocation(FVector(SpacePlanetDistance, 0.0, 0.0));
	}
	if (SpaceNebulaComponent) SpaceNebulaComponent->SetTranslucentSortPriority(NebulaSortIndex * 4 + 2);
	for (int32 Index = 0; Index < SpaceGlowMIDs.Num() && Index < PlanetsMoons.Num(); ++Index)
	{
		if (SpaceGlowMIDs[Index]) SpaceGlowMIDs[Index]->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(PlanetsMoons[Index].Glow * SpaceGlowBrightness));
	}
	if (SpaceNebulaMID)
	{
		SpaceNebulaMID->SetVectorParameterValue(TEXT("Color 1"), NebulaColor1);
		SpaceNebulaMID->SetVectorParameterValue(TEXT("Color 2"), NebulaColor2);
		SpaceNebulaMID->SetVectorParameterValue(TEXT("Color 3"), NebulaColor3);
		SpaceNebulaMID->SetScalarParameterValue(TEXT("Large Distortion Factor"), static_cast<float>(1.0 / NebulaLargeDistortion));
		SpaceNebulaMID->SetScalarParameterValue(TEXT("Small Distortion Factor"), static_cast<float>(1.0 / NebulaSmallDistortion));
		SpaceNebulaMID->SetScalarParameterValue(TEXT("3D Noise Scale"), static_cast<float>(NebulaNoiseScale));
		SpaceNebulaMID->SetScalarParameterValue(TEXT("Main Structure Exponent"), static_cast<float>(NebulaExponent));
		SpaceNebulaMID->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>(NebulaIntensity));
		SpaceNebulaMID->SetScalarParameterValue(TEXT("Noise Offset"), static_cast<float>(NebulaNoiseOffset));
		SpaceNebulaMID->SetScalarParameterValue(TEXT("Core Highlight"), static_cast<float>(NebulaCoreHighlight));
	}
	if (bSpaceLayerActive)
	{
		const bool bFlatCloudiness = SkyMode == EArcTimeOfDaySkyMode::StaticClouds || SkyMode == EArcTimeOfDaySkyMode::NoClouds || SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds || SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && ColorMode == EArcTimeOfDayColorMode::SimplifiedColor;
		const float MoonsCloudMask = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && (VolumetricCloudRenderingMode == EArcTimeOfDayVolumetricRenderingMode::MeshIntersectionsFastMovementPerformance || VolumetricCloudRenderingMode == EArcTimeOfDayVolumetricRenderingMode::MeshIntersectionsFastMovementFidelity) ? MapRangeClamped(LocalCloudCoverage, 1.6f, 2.2f, 1.0f, 0.0f) : bFlatCloudiness ? 1.0f - LocalOvercast01 : 1.0f;
		float SpaceBrightness = static_cast<float>(SpaceLayerBrightnessNight);
		if (SkyMode != EArcTimeOfDaySkyMode::Space)
		{
			const float Twilight = FMath::Pow(MapRangeClamped(CachedSunVector.Z, -0.01f, 0.25f, 1.0f, 0.0f), 11.0f);
			const float NightValue = static_cast<float>(SpaceLayerBrightnessNight) * (ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere ? static_cast<float>(NightBrightness + ExtraNightBrightnessWhenCloudy * LocalOvercast01) : 1.0f);
			const float DayValue = static_cast<float>(SpaceLayerBrightnessDay * SunLightIntensity / 5.0);
			SpaceBrightness = FMath::Lerp(DayValue, NightValue, FMath::Lerp(1.0f, 1.0f - FMath::Pow(Twilight, 3.0f), EclipsePercent)) * MoonsCloudMask;
		}
		CacheFloatValue(EArcTimeOfDayCachedProperty::SpaceLayerBrightness, FMath::Max(SpaceBrightness / 20.0f, 0.0f), 0.01f);
	}
}

void AArcTimeOfDayActor::ApplyPostProcessStages()
{
	if (UserPostProcessComponents.Num() != PostProcessComponents.Num())
	{
		for (UPostProcessComponent* Component : UserPostProcessComponents) if (Component) Component->DestroyComponent();
		UserPostProcessComponents.Reset();
		for (int32 Index = 0; Index < PostProcessComponents.Num(); ++Index)
		{
			UPostProcessComponent* Component = NewObject<UPostProcessComponent>(this, *FString::Printf(TEXT("UDS Post Process Stage %d"), Index), RF_Transient);
			Component->SetupAttachment(SkyRoot);
			Component->RegisterComponent();
			Component->bUnbound = true;
			UserPostProcessComponents.Add(Component);
		}
	}
	for (int32 Index = 0; Index < PostProcessComponents.Num(); ++Index)
	{
		FArcTimeOfDayPostProcessStage& Stage = PostProcessComponents[Index];
		Stage.bStatic = !(Stage.bDisableDuringDay
			|| Stage.bDisableAtDawnDusk
			|| Stage.bDisableAtNight
			|| Stage.bDisableIfOvercast
			|| Stage.bDisableIfNotOvercast
			|| Stage.bDisableIfFoggy
			|| Stage.bDisableIfNotFoggy
			|| Stage.bDisableIfDusty
			|| Stage.bDisableIfNotDusty
			|| Stage.bDisableIfInsideInterior
			|| Stage.bDisableIfOutsideInterior
			|| Stage.bDisableIfRaining
			|| Stage.bDisableIfNotRaining
			|| Stage.bDisableIfSnowing
			|| Stage.bDisableIfNotSnowing);
		UPostProcessComponent* Component = UserPostProcessComponents[Index];
		Component->Settings = Stage.PostProcessSettings;
		Component->Priority = static_cast<float>(Stage.Priority);
		float Weight = 1.0f;
		if (!Stage.bStatic)
		{
			const float TimeMultiplier = ThreeTimeFloats(Stage.bDisableDuringDay ? 0.0f : 1.0f, Stage.bDisableAtDawnDusk ? 0.0f : 1.0f, Stage.bDisableAtNight ? 0.0f : 1.0f);
			const float CloudyMultiplier = (Stage.bDisableIfOvercast ? 1.0f - LocalOvercast01 : 1.0f) * (Stage.bDisableIfNotOvercast ? LocalOvercast01 : 1.0f);
			const float Fog01 = MapRangeClamped(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)), 2.0f, 10.0f, 0.0f, 1.0f);
			const float FogGate = (Stage.bDisableIfFoggy ? 1.0f - Fog01 : 1.0f) * (Stage.bDisableIfNotFoggy ? Fog01 : 1.0f);
			const float Dust01 = static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0));
			const float DustGate = (Stage.bDisableIfDusty ? 1.0f - Dust01 : 1.0f) * (Stage.bDisableIfNotDusty ? Dust01 : 1.0f);
			const float InteriorGate = 1.0f;
			const float RainGate = FMath::Lerp(Stage.bDisableIfNotRaining ? 0.0f : 1.0f, Stage.bDisableIfRaining ? 0.0f : 1.0f, FMath::Clamp(UDWRainValue.GetClampedValue(0.0f, 10.0f) * 3.5f, 0.0f, 1.0f));
			const float SnowGate = FMath::Lerp(Stage.bDisableIfNotSnowing ? 0.0f : 1.0f, Stage.bDisableIfSnowing ? 0.0f : 1.0f, FMath::Clamp(UDWSnowValue.GetClampedValue(0.0f, 10.0f) * 3.5f, 0.0f, 1.0f));
			const float WeatherGate = RainGate * SnowGate;
			Weight = TimeMultiplier * CloudyMultiplier * FogGate * DustGate * InteriorGate * WeatherGate;
		}
		Component->BlendWeight = Weight;
		Component->bEnabled = Weight > 0.001f;
	}
}

void AArcTimeOfDayActor::RebuildVoxelClouds()
{
	if (SkyMode != EArcTimeOfDaySkyMode::VoxelClouds)
	{
		if (VoxelCloudsMesh) VoxelCloudsMesh->DestroyComponent();
		VoxelCloudsMesh = nullptr;
		VoxelCloudsMID = nullptr;
		AppliedVoxelCloudsMaterial = nullptr;
		AppliedVoxelCloudsGridCount = INDEX_NONE;
		return;
	}
	if (!VoxelCloudsMesh)
	{
		VoxelCloudsMesh = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Voxel Clouds Mesh"), RF_Transient);
		VoxelCloudsMesh->SetupAttachment(SkyRoot);
		VoxelCloudsMesh->RegisterComponent();
		VoxelCloudsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UStaticMesh* Mesh = VoxelCloudsStaticMesh.LoadSynchronous();
	if (VoxelCloudsMesh->GetStaticMesh() != Mesh)
	{
		VoxelCloudsMesh->SetStaticMesh(Mesh);
	}
	UMaterialInterface* Material = VoxelCloudsMaterial.LoadSynchronous();
	if (!VoxelCloudsMID || AppliedVoxelCloudsMaterial != Material)
	{
		VoxelCloudsMID = UMaterialInstanceDynamic::Create(Material, this);
		AppliedVoxelCloudsMaterial = Material;
		VoxelCloudsMesh->SetMaterial(0, VoxelCloudsMID);
	}
	if (AppliedVoxelCloudsGridCount != VoxelCloudsGridCount)
	{
		VoxelCloudsMesh->ClearInstances();
		for (int32 GridX = 1; GridX <= VoxelCloudsGridCount; ++GridX)
		{
			for (int32 GridY = 1; GridY <= VoxelCloudsGridCount; ++GridY)
			{
				const FVector Location(GridX * 1000.0f - VoxelCloudsGridCount * 500.0f, GridY * 1000.0f - VoxelCloudsGridCount * 500.0f, 0.0f);
				VoxelCloudsMesh->AddInstance(FTransform(FRotator::ZeroRotator, Location));
			}
		}
		AppliedVoxelCloudsGridCount = VoxelCloudsGridCount;
	}
	VoxelCloudsCenterPosition = FVector(0.0, 0.0, -50000000.0);
	const float VoxelScale = static_cast<float>(VoxelCloudsVoxelSize * VolumetricCloudsScale);
	const float GridStep = VoxelScale * 10.0f;
	VoxelCloudsMesh->SetWorldScale3D(FVector(VoxelScale / 100.0f));
	const UWorld* World = GetWorld();
	const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
	const FVector NewLocation(FMath::GridSnap(ViewLocation.X, GridStep), FMath::GridSnap(ViewLocation.Y, GridStep), CloudMainLayerBottomHeight);
	VoxelCloudsCenterPosition = NewLocation;
	VoxelCloudsMesh->SetWorldLocation(NewLocation);
}

USceneComponent* AArcTimeOfDayActor::CreateSpaceRootComponent(const FString& Name, const EArcTimeOfDaySpaceParent Parent, const FVector& Location, const FRotator& Rotation)
{
	USceneComponent* Root = NewObject<USceneComponent>(this, *Name, RF_Transient);
	Root->SetupAttachment(Parent == EArcTimeOfDaySpaceParent::Sun ? SunParent : Parent == EArcTimeOfDaySpaceParent::Moon ? MoonParent : SkyRoot);
	Root->SetRelativeLocation(Location);
	Root->SetRelativeRotation(Rotation);
	Root->RegisterComponent();
	SpaceSceneComponents.Add(Root);
	SpaceRoots.Add(Root);
	return Root;
}

UStaticMeshComponent* AArcTimeOfDayActor::CreateSpaceMeshComponent(const FString& Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, *Name, RF_Transient);
	Component->SetupAttachment(Parent);
	Component->SetStaticMesh(Mesh);
	Component->SetRelativeLocation(Location);
	Component->SetRelativeRotation(Rotation);
	Component->SetRelativeScale3D(Scale);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->RegisterComponent();
	SpaceSceneComponents.Add(Component);
	return Component;
}

void AArcTimeOfDayActor::RebuildSpaceLayer()
{
	for (USceneComponent* Component : SpaceSceneComponents) if (Component) Component->DestroyComponent();
	SpaceSceneComponents.Reset();
	SpacePlanetComponents.Reset();
	SpaceGlowMIDs.Reset();
	SpaceDistantPlanetComponents.Reset();
	SpaceDecalComponents.Reset();
	SpaceRoots.Reset();
	SpaceNebulaMID = nullptr;
	SpaceNebulaComponent = nullptr;

	const bool bDBufferSupported = GetDefault<URendererSettings>()->bDBuffer;
	bSpaceLayerActive = bDBufferSupported && (!PlanetsMoons.IsEmpty() || RenderNebula || !CustomSpaceLayerDecals.IsEmpty() || ForceEnableSpaceLayer);
	bAppliedRenderNebula = RenderNebula;
	SkySphereMID->SetScalarParameterValue(TEXT("Cancel Dbuffer"), bSpaceLayerActive ? 0.0f : 1.0f);
	SkySphereMID->SetScalarParameterValue(TEXT("Moon Blocks Space Layer"), MoonRendersBehindSpaceLayer ? 0.0f : 1.0f);
	if (!bSpaceLayerActive)
	{
		SkySphereMID->SetScalarParameterValue(TEXT("Space Brightness"), 0.0f);
		return;
	}

	UStaticMesh* PlanetMesh = ArcTimeOfDayAlternateModes::Load<UStaticMesh>(TEXT("/Game/UltraDynamicSky/Meshes/Space_Planet.Space_Planet"));
	UStaticMesh* GlowMesh = ArcTimeOfDayAlternateModes::Load<UStaticMesh>(TEXT("/Game/UltraDynamicSky/Meshes/Space_Glow_Circle.Space_Glow_Circle"));
	UStaticMesh* RingFrontMesh = ArcTimeOfDayAlternateModes::Load<UStaticMesh>(TEXT("/Game/UltraDynamicSky/Meshes/Space_Ring_FrontHalf.Space_Ring_FrontHalf"));
	UStaticMesh* RingBackMesh = ArcTimeOfDayAlternateModes::Load<UStaticMesh>(TEXT("/Game/UltraDynamicSky/Meshes/Space_Ring_BackHalf.Space_Ring_BackHalf"));
	SpaceGlowMIDs.SetNum(PlanetsMoons.Num());
	for (int32 Index = 0; Index < PlanetsMoons.Num(); ++Index)
	{
		const FArcTimeOfDaySpacePlanet& Planet = PlanetsMoons[Index];
		const FVector RootLocation = Planet.bRingAroundLocalPlanet ? FVector(0.0, 0.0, SkyAtmosphere->BottomRadius * -100000.0f) : FVector::ZeroVector;
		USceneComponent* Root = CreateSpaceRootComponent(FString::Printf(TEXT("Space Planet Root %d"), Index), Planet.Parent, RootLocation, Planet.RelativeRotation - FRotator(0.0, 180.0, 0.0));
		UStaticMeshComponent* Sphere = CreateSpaceMeshComponent(FString::Printf(TEXT("Space Planet %d"), Index), Root, PlanetMesh, Planet.bRingAroundLocalPlanet ? FVector::ZeroVector : FVector(SpacePlanetDistance, 0.0, 0.0), FRotator::ZeroRotator, FVector(1.0, Planet.Scale, Planet.Scale));
		Sphere->SetVisibility(!Planet.bRingAroundLocalPlanet);
		Sphere->SetTranslucentSortPriority((Index + 1) * 4);
		SpacePlanetComponents.Add(Sphere);
		if (!Planet.bRingAroundLocalPlanet) SpaceDistantPlanetComponents.Add(Sphere);
		const TCHAR* ParentPath = !Planet.NormalMap.IsNull() ? Planet.bRenderAtmosphere || !Planet.EmissiveTexture.IsNull() ? TEXT("/Game/UltraDynamicSky/Materials/Space/Instances/Space_Planet_NA.Space_Planet_NA") : TEXT("/Game/UltraDynamicSky/Materials/Space/Instances/Space_Planet_N.Space_Planet_N") : Planet.bRenderAtmosphere || !Planet.EmissiveTexture.IsNull() ? TEXT("/Game/UltraDynamicSky/Materials/Space/Instances/Space_Planet_A.Space_Planet_A") : TEXT("/Game/UltraDynamicSky/Materials/Space/Instances/Space_Planet_Default.Space_Planet_Default");
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ArcTimeOfDayAlternateModes::Load<UMaterialInterface>(ParentPath), this);
		Sphere->SetMaterial(0, MID);
		if (MID)
		{
			MID->SetScalarParameterValue(TEXT("Size"), static_cast<float>(Planet.Scale));
			MID->SetTextureParameterValue(TEXT("Color Texture"), Planet.ColorTexture.LoadSynchronous());
			MID->SetTextureParameterValue(TEXT("Normal Map"), Planet.NormalMap.LoadSynchronous());
			MID->SetScalarParameterValue(TEXT("Normal Map Intensity"), static_cast<float>(Planet.NormalMapStrength));
			MID->SetScalarParameterValue(TEXT("Terminator Offset"), static_cast<float>(Planet.TerminatorOffset));
			MID->SetScalarParameterValue(TEXT("Terminator Thickness"), static_cast<float>(Planet.TerminatorThickness));
			MID->SetScalarParameterValue(TEXT("Terminator Power"), static_cast<float>(Planet.TerminatorPower));
			MID->SetVectorParameterValue(TEXT("Darkside Tint"), Planet.DarkSideTint);
			MID->SetVectorParameterValue(TEXT("Lightside Tint"), Planet.LightSideTint);
			MID->SetVectorParameterValue(TEXT("Terminator Tint"), Planet.TerminatorTint);
			MID->SetScalarParameterValue(TEXT("Spherize Texture"), static_cast<float>(Planet.Scale / 8000.0 + 0.1));
			MID->SetVectorParameterValue(TEXT("Texture Tint"), Planet.ColorTextureTint);
			MID->SetScalarParameterValue(TEXT("Atmosphere Strength"), Planet.bRenderAtmosphere ? static_cast<float>(Planet.AtmosphereStrength) : 0.0f);
			MID->SetScalarParameterValue(TEXT("Atmosphere Height"), Planet.bRenderAtmosphere ? static_cast<float>(Planet.AtmosphereThickness * 150000.0) : 0.0f);
			MID->SetVectorParameterValue(TEXT("Atmosphere Color"), Planet.AtmosphereColor);
			MID->SetTextureParameterValue(TEXT("Emissive"), Planet.EmissiveTexture.LoadSynchronous());
			MID->SetVectorParameterValue(TEXT("Emissive Light Side"), Planet.EmissiveLightSideTint);
			MID->SetVectorParameterValue(TEXT("Emissive Dark Side"), Planet.EmissiveDarkSideTint);
			if (!FMath::IsNearlyZero(Planet.MeshDeformStrength))
			{
				MID->SetVectorParameterValue(TEXT("Deform Offset"), FLinearColor(Planet.MeshDeformTextureOffset.X, Planet.MeshDeformTextureOffset.Y, Planet.MeshDeformTextureOffset.Z, 1.0f));
				MID->SetTextureParameterValue(TEXT("Deform Texture"), Planet.MeshDeformTexture.LoadSynchronous());
				MID->SetScalarParameterValue(TEXT("Shape Deform"), static_cast<float>(Planet.MeshDeformStrength * 100000.0));
				MID->SetScalarParameterValue(TEXT("Deform Center"), static_cast<float>(Planet.MeshDeformCenterLevel));
			}
			MID->SetScalarParameterValue(TEXT("Use Sun Vector"), Planet.LightVector == EArcTimeOfDayPlanetLightSource::Sun ? 1.0f : 0.0f);
			MID->SetScalarParameterValue(TEXT("Use Moon Vector"), Planet.LightVector == EArcTimeOfDayPlanetLightSource::Moon ? 1.0f : 0.0f);
			const FVector CustomVector = Planet.LightVector == EArcTimeOfDayPlanetLightSource::Custom ? Planet.CustomLightVector : FVector::ZeroVector;
			MID->SetVectorParameterValue(TEXT("Custom Light Vector"), FLinearColor(CustomVector.X, CustomVector.Y, CustomVector.Z, 1.0f));
		}
		if (Planet.Glow > 0.0 && !Planet.bRingAroundLocalPlanet)
		{
			UStaticMeshComponent* Glow = CreateSpaceMeshComponent(FString::Printf(TEXT("Space Planet Glow %d"), Index), Root, GlowMesh, FVector(SpacePlanetDistance, 0.0, 0.0), FRotator::ZeroRotator, FVector(Planet.Scale * Planet.GlowScale));
			SpaceDistantPlanetComponents.Add(Glow);
			Glow->SetTranslucentSortPriority(Index + 20);
			UMaterialInstanceDynamic* GlowMID = UMaterialInstanceDynamic::Create(ArcTimeOfDayAlternateModes::Load<UMaterialInterface>(TEXT("/Game/UltraDynamicSky/Materials/Space/Space_Planet_Glow_Mat.Space_Planet_Glow_Mat")), this);
			SpaceGlowMIDs[Index] = GlowMID;
			Glow->SetMaterial(0, GlowMID);
			GlowMID->SetScalarParameterValue(TEXT("Opacity"), static_cast<float>(Planet.Glow * SpaceGlowBrightness));
			GlowMID->SetVectorParameterValue(TEXT("Color"), Planet.GlowColor);
			GlowMID->SetVectorParameterValue(TEXT("Darkside Tint"), Planet.DarkSideTint);
			GlowMID->SetVectorParameterValue(TEXT("Lightside Tint"), Planet.LightSideTint);
		}
		if (Planet.bRenderRing)
		{
			const FRotator RingRotation(-Planet.RingPitch, 0.0, -Planet.RingRoll);
			const float RingScale = static_cast<float>(Planet.Scale * (FMath::Max(Planet.RingDistance, 1.0) + Planet.RingWidth / 5.0));
			UStaticMeshComponent* Front = CreateSpaceMeshComponent(FString::Printf(TEXT("Space Ring Front %d"), Index), Sphere, RingFrontMesh, FVector::ZeroVector, RingRotation, FVector(RingScale));
			UStaticMeshComponent* Back = CreateSpaceMeshComponent(FString::Printf(TEXT("Space Ring Back %d"), Index), Sphere, RingBackMesh, FVector::ZeroVector, RingRotation, FVector(RingScale));
			Front->SetTranslucentSortPriority(Sphere->TranslucencySortPriority + 1);
			Back->SetTranslucentSortPriority(Sphere->TranslucencySortPriority - 1);
			UMaterialInstanceDynamic* RingMID = UMaterialInstanceDynamic::Create(ArcTimeOfDayAlternateModes::Load<UMaterialInterface>(TEXT("/Game/UltraDynamicSky/Materials/Space/Space_Ring_Mat.Space_Ring_Mat")), this);
			Front->SetMaterial(0, RingMID);
			Back->SetMaterial(0, RingMID);
			RingMID->SetScalarParameterValue(TEXT("Ring Width"), static_cast<float>(Planet.RingWidth * 500000.0));
			RingMID->SetScalarParameterValue(TEXT("Ring Inner Distance"), static_cast<float>(FMath::Max(Planet.RingDistance, 1.0) * 2500000.0));
			RingMID->SetTextureParameterValue(TEXT("Ring Texture"), Planet.RingTexture.LoadSynchronous());
			RingMID->SetScalarParameterValue(TEXT("Ring Opacity"), static_cast<float>(Planet.RingOpacity));
			RingMID->SetScalarParameterValue(TEXT("Ring Shadow Brightness"), FMath::Clamp(static_cast<float>(Planet.RingShadowBrightness), 0.0f, 1.0f));
			RingMID->SetScalarParameterValue(TEXT("Ring Shadow Softness"), FMath::Max(static_cast<float>(Planet.RingShadowSoftness), 0.0f) * 25000.0f);
			RingMID->SetVectorParameterValue(TEXT("Ring Color"), Planet.RingTintColor);
			RingMID->SetScalarParameterValue(TEXT("Size"), static_cast<float>(Planet.Scale));
		}
	}

	UStaticMesh* DecalMesh = ArcTimeOfDayAlternateModes::Load<UStaticMesh>(TEXT("/Game/UltraDynamicSky/Meshes/Space_Decal_Plane.Space_Decal_Plane"));
	for (int32 Index = 0; Index < CustomSpaceLayerDecals.Num(); ++Index)
	{
		const FArcTimeOfDaySpaceLayerDecal& Decal = CustomSpaceLayerDecals[Index];
		UMaterialInterface* Parent = Decal.DecalMaterial.LoadSynchronous();
		if (!Parent) continue;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
		for (const TPair<FName, double>& Pair : Decal.ScalarParameterSettings) MID->SetScalarParameterValue(Pair.Key, static_cast<float>(Pair.Value));
		for (const TPair<FName, FLinearColor>& Pair : Decal.ColorParameterSettings) MID->SetVectorParameterValue(Pair.Key, Pair.Value);
		for (const TPair<FName, TSoftObjectPtr<UTexture>>& Pair : Decal.TextureParameterSettings) MID->SetTextureParameterValue(Pair.Key, Pair.Value.LoadSynchronous());
		USceneComponent* Root = CreateSpaceRootComponent(FString::Printf(TEXT("Space Decal Root %d"), Index), Decal.Parent, FVector::ZeroVector, Decal.RelativeRotation - FRotator(0.0, 180.0, -90.0));
		UStaticMeshComponent* Plane = CreateSpaceMeshComponent(FString::Printf(TEXT("Space Decal %d"), Index), Root, DecalMesh, FVector(SpacePlanetDistance, 0.0, 0.0), FRotator::ZeroRotator, FVector(1.0, Decal.Scale * Decal.ScaleYAxis, Decal.Scale));
		SpaceDecalComponents.Add(Plane);
		Plane->SetTranslucentSortPriority(Index * 4 + Decal.SortIndexOffset);
		Plane->SetMaterial(0, MID);
	}

	if (RenderNebula)
	{
		if (UMaterialInterface* NebulaParent = SpaceNebulaParentMaterial.LoadSynchronous())
		{
			SpaceNebulaMID = UMaterialInstanceDynamic::Create(NebulaParent, this);
			if (SpaceNebulaMID)
			{
				SpaceNebulaComponent = CreateSpaceMeshComponent(TEXT("Space Nebula Sphere"), SunParent, ArcTimeOfDayAlternateModes::Load<UStaticMesh>(TEXT("/Game/UltraDynamicSky/Meshes/Nebula_Sphere.Nebula_Sphere")), FVector::ZeroVector, FRotator::ZeroRotator, FVector(2800.0));
				SpaceNebulaComponent->SetTranslucentSortPriority(NebulaSortIndex * 4 + 2);
				SpaceNebulaComponent->SetMaterial(0, SpaceNebulaMID);
				SpaceNebulaMID->SetVectorParameterValue(TEXT("Color 1"), NebulaColor1);
				SpaceNebulaMID->SetVectorParameterValue(TEXT("Color 2"), NebulaColor2);
				SpaceNebulaMID->SetVectorParameterValue(TEXT("Color 3"), NebulaColor3);
				SpaceNebulaMID->SetScalarParameterValue(TEXT("Large Distortion Factor"), static_cast<float>(1.0 / NebulaLargeDistortion));
				SpaceNebulaMID->SetScalarParameterValue(TEXT("Small Distortion Factor"), static_cast<float>(1.0 / NebulaSmallDistortion));
				SpaceNebulaMID->SetScalarParameterValue(TEXT("3D Noise Scale"), static_cast<float>(NebulaNoiseScale));
				SpaceNebulaMID->SetScalarParameterValue(TEXT("Main Structure Exponent"), static_cast<float>(NebulaExponent));
				SpaceNebulaMID->SetScalarParameterValue(TEXT("Intensity"), static_cast<float>(NebulaIntensity));
				SpaceNebulaMID->SetScalarParameterValue(TEXT("Noise Offset"), static_cast<float>(NebulaNoiseOffset));
				SpaceNebulaMID->SetScalarParameterValue(TEXT("Core Highlight"), static_cast<float>(NebulaCoreHighlight));
			}
		}
	}
}
