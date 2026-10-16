// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace ArcTimeOfDayClouds
{
	float MaterialQualityScale(const int32 Quality, const float Low)
	{
		return Quality == 0 ? Low : 1.0f;
	}

	void SetConsoleInt(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}

	void SetConsoleFloat(const TCHAR* Name, const float Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

FLinearColor AArcTimeOfDayActor::CurrentVolumetricCloudAlbedo() const
{
	const float DawnDusk = MapRangeClamped(FMath::Abs(CachedSunVector.Z + 0.15f), 0.0f, 0.2f, 1.0f, 0.0f);
	const FLinearColor TimeTint = FMath::Lerp(FLinearColor::White, FLinearColor(1.2f, 1.08076f, 0.8748f, 1.0f), DawnDusk);
	const float SkyModeScale = SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora ? MapRangeClamped(CachedDirectionalInscatteringMultiplier, 0.3f, 1.0f, 1.0f, 0.3f) : 1.0f;
	const float DirectionalDimming = FMath::IsNearlyZero(CachedDirectionalDimming) ? 0.0f : static_cast<float>(OverallIntensity) / CachedDirectionalDimming;
	const FLinearColor Base = VolumetricCloudColor * TimeTint * (SkyModeScale * DirectionalDimming);
	const float ContrastScale = (static_cast<float>(Contrast) - 0.1f) * 1.3f + 1.0f;
	return Desaturate(Base, 1.0f - (static_cast<float>(Saturation) + static_cast<float>(Contrast) - 0.1f)) * ContrastScale;
}

void AArcTimeOfDayActor::BuildVolumetricCloudLayers()
{
	const float BaseLayerHeight = 0.7f * static_cast<float>(LayerHeightScale);
	TArray<FArcTimeOfDayAdditionalCloudLayer> LayersBottomToTop;
	TArray<FVector2D> LayerRanges;
	FArcTimeOfDayAdditionalCloudLayer MainLayer;
	MainLayer.FormationTimeOffset = 0.0;
	LayersBottomToTop.Add(MainLayer);
	LayerRanges.Add(FVector2D(0.0, 1.0));
	int32 MainLayerIndex = 0;
	float RangeMinimum = 0.0f;
	float RangeMaximum = 1.0f;
	for (const FArcTimeOfDayAdditionalCloudLayer& Layer : AdditionalCloudLayers)
	{
		if (!Layer.bAboveMainLayer) continue;
		const FVector2D Previous = LayerRanges.Last();
		const float Bottom = FMath::Lerp(Previous.X, Previous.Y, 1.0f + static_cast<float>(Layer.Margin));
		const float Top = Bottom + static_cast<float>(Layer.LayerHeight);
		LayersBottomToTop.Add(Layer);
		LayerRanges.Add(FVector2D(Bottom, Top));
		RangeMaximum = Top;
	}
	for (const FArcTimeOfDayAdditionalCloudLayer& Layer : AdditionalCloudLayers)
	{
		if (Layer.bAboveMainLayer) continue;
		const FVector2D Previous = LayerRanges[0];
		const float Top = FMath::Lerp(Previous.Y, Previous.X, 1.0f + static_cast<float>(Layer.Margin));
		const float Bottom = Top - static_cast<float>(Layer.LayerHeight);
		LayersBottomToTop.Insert(Layer, 0);
		LayerRanges.Insert(FVector2D(Bottom, Top), 0);
		++MainLayerIndex;
		RangeMinimum = Bottom;
	}
	ExtendedCloudHeightRange = FVector2D(RangeMinimum, RangeMaximum);
	TArray<FVector2D> NormalizedRanges;
	NormalizedRanges.Reserve(LayerRanges.Num());
	for (const FVector2D& Range : LayerRanges)
	{
		NormalizedRanges.Add(FVector2D(MapRangeClamped(Range.X, RangeMinimum, RangeMaximum, 0.0f, 1.0f), MapRangeClamped(Range.Y, RangeMinimum, RangeMaximum, 0.0f, 1.0f)));
	}
	CloudLayersAltitudeRangeCache.Reset(101);
	int32 CurrentLayer = 0;
	for (int32 Point = 0; Point <= 100; ++Point)
	{
		const float Height = static_cast<float>(Point) / 100.0f;
		while (CurrentLayer < NormalizedRanges.Num() - 1 && Height > NormalizedRanges[CurrentLayer].Y) ++CurrentLayer;
		CloudLayersAltitudeRangeCache.Add(MapRangeClamped(Height, NormalizedRanges[CurrentLayer].X, NormalizedRanges[CurrentLayer].Y, 0.0f, 1.0f));
	}
	const float FullLayerHeight = BaseLayerHeight * (RangeMaximum - RangeMinimum);
	const float MainLayerNormalizedBottom = MapRangeClamped(0.0f, RangeMinimum, RangeMaximum, 0.0f, 1.0f);
	const float ComponentBottom = static_cast<float>(BottomAltitude * VolumetricCloudsScale) - FullLayerHeight * MainLayerNormalizedBottom;
	VolumetricCloud->SetLayerHeight(FullLayerHeight);
	VolumetricCloud->SetLayerBottomAltitude(ComponentBottom);
	const float BaseHeight = ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere ? SkyAtmosphere->GetComponentLocation().Z : 0.0f;
	CloudMainLayerBottomHeight = BaseHeight + static_cast<float>(BottomAltitude * VolumetricCloudsScale) * 100000.0f;
	CloudMainLayerTopHeight = CloudMainLayerBottomHeight + BaseLayerHeight * 100000.0f;
	CloudFullLayerBottomHeight = BaseHeight + ComponentBottom * 100000.0f;
	CloudFullLayerTopHeight = CloudFullLayerBottomHeight + FullLayerHeight * 100000.0f;
	const UWorld* World = GetWorld();
	const FVector ViewLocation = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0] : GetActorLocation();
	CameraCloudLayerNormalizedHeight = MapRangeClamped(ViewLocation.Z, CloudMainLayerBottomHeight, CloudMainLayerTopHeight, 0.0f, 1.0f);
	const float FullNormalizedHeight = MapRangeClamped(ViewLocation.Z, CloudFullLayerBottomHeight, CloudFullLayerTopHeight, 0.0f, 1.0f);
	if (AdditionalCloudLayers.IsEmpty())
	{
		CameraCloudLayerNormalizedHeightFullRange = CameraCloudLayerNormalizedHeight;
	}
	else
	{
		CameraCloudLayerNormalizedHeightFullRange = CloudLayersAltitudeRangeCache[FMath::Clamp(FMath::RoundToInt(FullNormalizedHeight * 100.0f), 0, 100)];
	}
	if (!AdditionalCloudLayers.IsEmpty() && CloudLayerMapRenderTarget && CloudLayerMapBrushMaterial)
	{
		if (!CloudLayerMapBrushMID) CloudLayerMapBrushMID = UMaterialInstanceDynamic::Create(CloudLayerMapBrushMaterial, this, TEXT("UDS Cloud Layer Map Brush MID"));
		UCanvas* Canvas = nullptr;
		FVector2D CanvasSize = FVector2D::ZeroVector;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, CloudLayerMapRenderTarget, Canvas, CanvasSize, Context);
		if (Canvas && CloudLayerMapBrushMID)
		{
			float FormationOffset = 0.0f;
			for (int32 Index = 0; Index < LayersBottomToTop.Num(); ++Index)
			{
				const FArcTimeOfDayAdditionalCloudLayer& Layer = LayersBottomToTop[Index];
				const FVector2D Range = NormalizedRanges[Index];
				FormationOffset += static_cast<float>(Layer.FormationTimeOffset);
				CloudLayerMapBrushMID->SetScalarParameterValue(TEXT("Bottom Y"), Range.X);
				CloudLayerMapBrushMID->SetScalarParameterValue(TEXT("Top Y"), Range.Y);
				CloudLayerMapBrushMID->SetScalarParameterValue(TEXT("Blend Top Y"), Index == 0 ? 0.0f : FMath::Max(Range.X, NormalizedRanges[Index - 1].Y));
				CloudLayerMapBrushMID->SetVectorParameterValue(TEXT("RGB 1"), FLinearColor(Layer.CloudFormationTextureScale.X, Layer.CloudFormationTextureScale.Y, 0.0f, 1.0f));
				CloudLayerMapBrushMID->SetVectorParameterValue(TEXT("RGB 2"), FLinearColor(static_cast<float>(Layer.InheritedCloudCoverage), static_cast<float>(Layer.IndependentCloudCoverage / 2.898), static_cast<float>(Layer.PaintedCloudCoverage), 1.0f));
				CloudLayerMapBrushMID->SetVectorParameterValue(TEXT("RGB 3"), FLinearColor(static_cast<float>(Layer.ExtinctionScale), Index == MainLayerIndex ? 0.0f : FormationOffset, static_cast<float>(Layer.FormationTextureMipOffset), 1.0f));
				Canvas->K2_DrawMaterial(CloudLayerMapBrushMID, FVector2D::ZeroVector, CanvasSize, FVector2D::ZeroVector, FVector2D::UnitVector);
			}
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	}
	SetWeatherScalar(TEXT("Cloud Main Layer Bottom Altitude"), CloudMainLayerBottomHeight);
	SetCloudScalar(TEXT("Bottom Altitude"), CloudFullLayerBottomHeight);
	SetCloudScalar(TEXT("Top Altitude"), CloudFullLayerTopHeight);
	SetCloudScalar(TEXT("Shadows Altitude"), FMath::Lerp(CloudMainLayerBottomHeight, CloudMainLayerTopHeight, 0.25f));
	if (InsideCloudFogMesh)
	{
		InsideCloudFogMesh->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, FMath::Lerp(CloudFullLayerBottomHeight, CloudFullLayerTopHeight, 0.5f)));
		InsideCloudFogMesh->SetWorldScale3D(FVector(185000000.0f, 185000000.0f, FMath::Abs(CloudFullLayerTopHeight - CloudFullLayerBottomHeight) / 100.0f));
	}
}

void AArcTimeOfDayActor::ApplyInsideCloudFog()
{
	const bool bEnabled = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && bEnableFogInsideClouds && VolumetricCloudMIDComplex && VolumetricCloudMIDSimple;
	UMaterialInterface* FogMaterial = AdditionalCloudLayers.IsEmpty() ? InsideCloudFogMaterialOneLayer : InsideCloudFogMaterialTwoLayer;
	if (bEnabled && FogMaterial && (!InsideCloudsFogMID || AppliedInsideCloudFogMaterial != FogMaterial))
	{
		InsideCloudsFogMID = UMaterialInstanceDynamic::Create(FogMaterial, this, TEXT("UDS Inside Clouds Fog MID"));
		AppliedInsideCloudFogMaterial = FogMaterial;
		InsideCloudFogMesh->SetMaterial(0, InsideCloudsFogMID);
	}
	else if (!bEnabled || !FogMaterial)
	{
		InsideCloudFogMesh->SetMaterial(0, nullptr);
		InsideCloudsFogMID = nullptr;
		AppliedInsideCloudFogMaterial = nullptr;
	}
	const bool bInsideCloudFogEnabled = bEnabled && InsideCloudsFogMID;
	InsideCloudFogMesh->SetVisibility(bInsideCloudFogEnabled);
	InsideCloudFogMesh->SetHiddenInGame(!bInsideCloudFogEnabled);
	const FLinearColor AmbientFogColor = CachedAmbientFogColor * VolumetricCloudColor * InsideCloudFogColor;
	if (InsideCloudsFogMID)
	{
		InsideCloudsFogMID->SetScalarParameterValue(TEXT("Fog Amount"), static_cast<float>(InsideCloudFogStrength) * 0.4f);
		InsideCloudsFogMID->SetVectorParameterValue(TEXT("Color"), AmbientFogColor);
	}
}

void AArcTimeOfDayActor::ApplyCloudWisps()
{
	if (!SkySphereMID) return;
	SkySphereMID->SetTextureParameterValue(TEXT("Cloud_Wisps_Texture"), CloudWispsTextureSetting);
	SkySphereMID->SetScalarParameterValue(TEXT("Cloud Wisps Morph"), bCloudWispsMovementEffect ? 1.0f : 0.0f);
	SkySphereMID->SetScalarParameterValue(TEXT("Cloud Wisps Morph Period"), static_cast<float>(CloudWispsMovementPeriod));
	SkySphereMID->SetScalarParameterValue(TEXT("Cloud Wisps Morph Amount"), static_cast<float>(CloudWispsMovementIntensity));
	float Opacity = FMath::Lerp(static_cast<float>(CloudWispsOpacityClear), static_cast<float>(CloudWispsOpacityCloudy), FMath::Clamp(CloudCoverage03, 0.0f, 1.0f));
	if (SkyMode == EArcTimeOfDaySkyMode::VoxelClouds && !VoxelCloudsUsesCloudWisps) Opacity = 0.0f;
	const bool bApplyFlatCloudiness = SkyMode == EArcTimeOfDaySkyMode::StaticClouds || SkyMode == EArcTimeOfDaySkyMode::NoClouds || SkyMode == EArcTimeOfDaySkyMode::VolumetricAurora || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds || SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && ColorMode == EArcTimeOfDayColorMode::SimplifiedColor;
	if (bApplyFlatCloudiness) Opacity *= 1.0f - LocalOvercast01;
	if (CameraSkyAtmosphereNormalizedDensity < 1.0f) Opacity *= FMath::Square(CameraSkyAtmosphereNormalizedDensity);
	CacheFloatValue(EArcTimeOfDayCachedProperty::CloudWispsOpacity, Opacity, 0.000001f);
	const float SunThreshold = (CachedSunVector.Z - 0.05f) * 10.0f;
	const float Threshold = FMath::Clamp(FMath::Abs(SunThreshold), 0.0f, 1.0f);
	const bool bUsingSun = SunThreshold < 0.0f;
	const FVector LightVector = bUsingSun ? CachedSunVector : CachedMoonVector;
	const float LightIntensity = bUsingSun ? static_cast<float>(IncreaseWispsBrightnessAroundSun) : static_cast<float>(IncreaseWispsBrightnessAroundMoon);
	CacheColorValue(EArcTimeOfDayCachedProperty::CloudWispsGradientVector, FLinearColor(LightVector.X, LightVector.Y, LightVector.Z, LightIntensity * Threshold), 0.01f);
	const FLinearColor SunTint = ThreeColorTimeBlend(CloudWispsTintDay, CloudWispsTintDawnDusk, FLinearColor(0.403922f, 0.423529f, 0.478431f, 1.0f));
	const FLinearColor WispsColor = (CachedMoonFogBaseColor * CloudWispsTintNight + SunTint * CachedSunFogBaseColor) * FLinearColor(0.437516f, 0.493875f, 0.526042f, 1.0f) * (static_cast<float>(CloudWispsColorIntensity) * CurrentSceneLightingBrightnessScale * static_cast<float>(DirectionalBalance));
	CacheColorValue(EArcTimeOfDayCachedProperty::CloudWispsColor, WispsColor, 0.0001f);
	const float MovementAngle = FMath::DegreesToRadians(-static_cast<float>(CloudDirection.GetWrappedValue(360.0)) + GetActorRotation().Yaw);
	CacheColorValue(EArcTimeOfDayCachedProperty::CloudWispsMovementVector, FLinearColor(FMath::Cos(MovementAngle), FMath::Sin(MovementAngle), 0.0f, 0.0f), 0.005f);
}

void AArcTimeOfDayActor::ApplyCloudShadows()
{
	UMaterialInterface* Parent = CustomLightFunctionMaterial ? CustomLightFunctionMaterial : DefaultVolumetricCloudShadowMaterial;
	if (Parent && (!CloudShadowsMID || AppliedCloudShadowsMaterial != Parent))
	{
		CloudShadowsMID = UMaterialInstanceDynamic::Create(Parent, this, TEXT("UDS Cloud Shadows MID"));
		AppliedCloudShadowsMaterial = Parent;
	}
	const float AtlasScale = static_cast<float>(LightFunctionAtlasScale) * 100000.0f;
	for (UDirectionalLightComponent* Light : { GetSunLightComponent(), GetMoonLightComponent() })
	{
		if (!Light) continue;
		Light->SetLightFunctionMaterial(bUseCloudShadows ? CloudShadowsMID : nullptr);
		Light->SetLightFunctionScale(FVector(AtlasScale));
	}
	if (CloudShadowsMID)
	{
		CloudShadowsMID->SetScalarParameterValue(TEXT("Atlas Scale"), AtlasScale);
		CloudShadowsMID->SetScalarParameterValue(TEXT("Shadow Softness"), static_cast<float>(CloudShadowsSoftness));
		CloudShadowsMID->SetScalarParameterValue(TEXT("Clear Intensity"), static_cast<float>(CloudShadowsIntensityWhenSunny));
		CloudShadowsMID->SetScalarParameterValue(TEXT("Cloudy Intensity"), static_cast<float>(CloudShadowsIntensityWhenOvercast));
		CloudShadowsMID->SetScalarParameterValue(TEXT("Shadows Scale"), static_cast<float>(TwoDCloudShadowsScale));
		const bool bUsingVolumeClouds = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds;
		if (bUsingVolumeClouds)
		{
			CloudShadowsMID->SetTextureParameterValue(TEXT("Formation Texture"), VolumetricCloudFormationVolumeTexture);
		}
		else
		{
			CloudShadowsMID->SetTextureParameterValue(TEXT("clouds_basetex"), TwoDDynamicCloudFormationTexture.LoadSynchronous());
			CloudShadowsMID->SetScalarParameterValue(TEXT("Cloud Position Scale Factor"), 450000.0f / static_cast<float>(TwoDCloudShadowsScale) * static_cast<float>(TwoDCloudShadowsSpeed));
			CloudShadowsMID->SetScalarParameterValue(TEXT("Angle Offset Scale"), static_cast<float>(TwoDCloudShadowsLightAngleOffsetScale));
		}
	}
	if (bApplyLightFunctionAtlasResolution) ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.LightFunctionAtlas.SlotResolution"), LightFunctionAtlasResolution);
	if (bForceLightFunctionsOn) ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.LightFunctionQuality"), 2);
	CloudShadowDisableThreshold = 2.1f;
	bAllowDisablingDirectionalShadowsWithCoverage = bDisableDirectionalLightShadowsWhenFullyShadowedByClouds && bUseCloudShadows && CloudShadowsIntensityWhenOvercast >= 1.0;
	const FVector ShadowSource = CachedSunVector.Z < SunShadowsCutoffZ ? CachedSunVector : CachedMoonVector;
	if (bCorrectSpecularScaleForLowAngleCloudShadows)
	{
		const float CoverageScale = MapRangeClamped(CloudCoverage03, 0.7f, 1.14f, 1.0f, 0.0f);
		CachedDirectionalSpecularCancelValue = FMath::Pow(MapRangeClamped(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-ShadowSource.Z, -1.0f, 1.0f))), 83.0f, 75.0f, CoverageScale, 1.0f), 4.0f);
	}
	else CachedDirectionalSpecularCancelValue = 1.0f;
	const float ShadowBlend = 1.0f - FMath::Clamp(ShadowSource.Z * -1.5f - 0.3f, 0.0f, 1.0f);
	const FVector ShadowVector = FVector(ShadowSource.X, ShadowSource.Y, FMath::Lerp(ShadowSource.Z, -0.2f, ShadowBlend)) * -1.0f;
	const float ShadowAlpha = MapRangeClamped(ShadowSource.Z, -0.1f, -0.2f, 1.0f, 0.0f);
	const float TransitionMultiplier = FMath::Max(MapRangeClamped(CachedSunVector.Z - SunShadowsCutoffZ, -0.01f, -0.03f, 0.0f, 1.0f), MapRangeClamped(CachedSunVector.Z - SunShadowsCutoffZ, 0.01f, 0.02f, 0.0f, 1.0f));
	const float FogAndDustShadow = 1.0f - FMath::Max(FMath::Square(MapRangeClamped(static_cast<float>(Fog.GetClampedValue(0.0, 10.0)), 4.0f, 10.0f, 0.0f, 1.0f)) * static_cast<float>(FogShadows), static_cast<float>(DustAmount.GetClampedValue(0.0, 1.0) * DustShadows));
	const UDirectionalLightComponent* SunLight = GetSunLightComponent();
	const float EclipseShadow = SunLight && SunLight->CastShadows ? EclipsePercent : 1.0f;
	CacheColorValue(EArcTimeOfDayCachedProperty::CloudShadowsLightVector, FLinearColor(ShadowVector.X, ShadowVector.Y, ShadowVector.Z, ShadowAlpha), 0.0001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::CloudShadowsCancel, TransitionMultiplier * FogAndDustShadow * EclipseShadow, 0.05f);
}

void AArcTimeOfDayActor::ApplyVolumetricCloudLightRays()
{
	bUsingVolumetricLightRays = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && bEnableVolumetricCloudLightRays;
	if (!VolumetricCloudLightRays) return;
	if (VolumetricCloudLightRays->GetAsset() != VolumetricCloudLightRaysSystem) VolumetricCloudLightRays->SetAsset(VolumetricCloudLightRaysSystem);
	VolumetricCloudLightRays->SetVariableTexture(TEXT("Clouds Texture"), VolumetricCloudFormationVolumeTexture);
	const float Scale = static_cast<float>(VolumetricCloudsScale);
	const float GridDistance = FMath::FloorToFloat(static_cast<float>(LightRaysPointSpacing) * Scale);
	const float CloudHeight = FMath::Lerp(CloudMainLayerBottomHeight, CloudMainLayerTopHeight, 0.2f) + static_cast<float>(LightRaysPointSpacing) * Scale;
	const int32 GridCount = FMath::FloorToInt(static_cast<float>(LightRaysMaxDistanceKm) * 200000.0f * Scale / FMath::Max(GridDistance, 1.0f));
	MaxVolumetricCloudLightRayLength = static_cast<float>(LightRayLength) * FMath::Max(CloudHeight - GetActorLocation().Z, 60000.0f) - static_cast<float>(LightRaysPointSpacing) * Scale + 80000.0f * Scale;
	VolumetricCloudLightRays->SetVariableInt(TEXT("Grid Count"), GridCount);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Grid Distance"), GridDistance);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Max Distance"), static_cast<float>(LightRaysMaxDistanceKm) * 100000.0f);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Cloud Height"), CloudHeight);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Cloud Layer Thickness"), CloudMainLayerTopHeight - CloudMainLayerBottomHeight);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Depth Fade Distance"), static_cast<float>(LightRaysDepthFadeDistance));
	VolumetricCloudLightRays->SetVariableFloat(TEXT("3D Noise Scale"), static_cast<float>(LightRaysThreeDNoiseScale) * Scale);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Scale Width"), static_cast<float>(ScaleLightRayWidth));
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Cloud Coverage Factor"), MapRangeClamped(static_cast<float>(IndividualCloudsLightRays), 0.0f, 1.0f, -0.1f, 0.15f));
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Test Distance"), 70000.0f * Scale);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Test Offset"), 75000.0f * Scale);
	const bool bUsingSun = CachedSunVector.Z < 0.0f;
	const FVector LightVector = bUsingSun ? CachedSunVector : CachedMoonVector;
	const float Strength = MapRangeClamped(LightVector.Z, -0.05f, -0.3f, 0.0f, 1.0f) * (bUsingSun || bRenderMoon ? 1.0f : 0.0f);
	const FLinearColor FogBase = bUsingSun ? CachedSunFogBaseColor : CachedMoonFogBaseColor;
	FLinearColor RayColor = FogBase * (static_cast<float>(LightRayIntensity) / 7.0f);
	RayColor.A = Strength;
	CacheColorValue(EArcTimeOfDayCachedProperty::VolumetricCloudLightRaysColor, RayColor, 0.0001f);
	VolumetricCloudLightRays->SetVariableLinearColor(TEXT("Light Color"), RayColor);
	VolumetricCloudLightRays->SetVariableFloat(TEXT("Ray Length"), MaxVolumetricCloudLightRayLength * Strength);
	VolumetricCloudLightRays->SetVisibility(bUsingVolumetricLightRays && Strength > 0.0f, false);
	if (VolumetricCloudLightRays->IsVisible()) VolumetricCloudLightRays->Activate(); else VolumetricCloudLightRays->Deactivate();
}

void AArcTimeOfDayActor::AdvanceCloudMovement(const float DeltaSeconds)
{
	const float WindMultiplier = FMath::Max(FMath::Pow(UDWWindValue.GetClampedValue(0.0f, 10.0f) / 10.0f, 1.4f) * 0.7f, 0.05f);
	const float CloudTimeOffset = static_cast<float>(CloudSpeed) * WindMultiplier * DeltaSeconds;
	const FVector Direction = FRotator(0.0f, static_cast<float>(CloudDirection.GetWrappedValue(360.0)) - 180.0f, 0.0f).RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector TextureVelocity = (Direction + FVector(0.0f, 0.0f, -static_cast<float>(ThreeDNoiseVerticalMovement))) * 0.02f;
	const float BaseCloudScale = static_cast<float>(CloudFormationTextureScale) * 1200000.0f;
	CloudsBTime += CloudTimeOffset;
	CloudsPosition += TextureVelocity * CloudTimeOffset * BaseCloudScale;
	const float PrecipitationFall = FMath::Max(FMath::Clamp((UDWRainValue.GetClampedValue(0.0f, 10.0f) - 1.0f) / 1400.0f, 0.0f, 0.005f), FMath::Clamp((UDWSnowValue.GetClampedValue(0.0f, 10.0f) - 1.0f) / 5600.0f, 0.0f, 0.00125f));
	WeatherFogPosition += (TextureVelocity * FVector(1.0f, 1.0f, 0.0f) * (DeltaSeconds * WindMultiplier * 0.35f) + FVector(0.0f, 0.0f, PrecipitationFall * DeltaSeconds)) * 80000.0f;
}

void AArcTimeOfDayActor::UpdateCloudMovementParameters()
{
	const FVector StaticCloudPosition = CloudsPosition - GetActorLocation() + CloudPhaseVectorMultiplier * static_cast<float>(CloudPhase);
	SetCloudVector(TEXT("Clouds Position"), FLinearColor(StaticCloudPosition.X, StaticCloudPosition.Y, StaticCloudPosition.Z, 1.0f));
	SetCloudScalar(TEXT("Clouds B Time"), CloudsBTime + static_cast<float>(CloudPhase));
	const FVector FogPosition = -GetActorLocation() + FogNoiseOffset + WeatherFogPosition;
	SetCloudVector(TEXT("Fog Position"), FLinearColor(FogPosition.X, FogPosition.Y, FogPosition.Z, 1.0f));
}

void AArcTimeOfDayActor::ApplyVolumetricClouds()
{
	const bool bUsingCloudData = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds || SkyMode == EArcTimeOfDaySkyMode::VoxelClouds;
	const bool bUsingVolumetricComponent = SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds;
	const FArcTimeOfDayCachedValue& CachedAuroraIntensity = CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::AuroraIntensity)];
	VolumetricCloud->SetHiddenInGame(!bUsingVolumetricComponent);
	VolumetricAurora->SetHiddenInGame(SkyMode != EArcTimeOfDaySkyMode::VolumetricAurora || !CachedAuroraIntensity.bInitialized || CachedAuroraIntensity.FloatValue < 0.0001f);
#if WITH_EDITOR
	VolumetricCloud->SetIsTemporarilyHiddenInEditor(!bUsingVolumetricComponent);
	VolumetricAurora->SetIsTemporarilyHiddenInEditor(SkyMode != EArcTimeOfDaySkyMode::VolumetricAurora || !CachedAuroraIntensity.bInitialized || CachedAuroraIntensity.FloatValue < 0.0001f);
#endif
	ApplyCloudWisps();
	ApplyVolumetricCloudLightRays();
	if (!bUsingCloudData || !VolumetricCloudMIDComplex || !VolumetricCloudMIDSimple)
	{
		ApplyInsideCloudFog();
		return;
	}
	const int32 Quality = UKismetSystemLibrary::GetRenderingMaterialQualityLevel();
	const float QualityScale = ArcTimeOfDayClouds::MaterialQualityScale(Quality, static_cast<float>(LowMaterialQualitySampleCountScale));
	UVolumeTexture* NoiseTexture = Quality == 0 ? LowQualityVolumeTexture : HighQualityVolumeTexture;
	const float Scale = FMath::Clamp(static_cast<float>(VolumetricCloudsScale), 0.05f, TNumericLimits<float>::Max());
	const UWorld* World = GetWorld();
	const bool bCinematicRuntime = ProjectMode == EArcTimeOfDayProjectMode::CinematicOffline && World && World->IsGameWorld();
	const float MinimumCoverage = FMath::Min(LocalCloudCoverage, CloudCoverage03);
	const float SimplifiedAlpha = bChangeToSimplifiedMaterialWhenOvercast ? MapRangeClamped(MinimumCoverage, static_cast<float>(SimplifiedMaterialCloudCoverageThreshold - 0.1 - SimplifiedMaterialFadeLength), static_cast<float>(SimplifiedMaterialCloudCoverageThreshold - 0.1), 0.0f, 1.0f) : 0.0f;
	if (SkyMode == EArcTimeOfDaySkyMode::VolumetricClouds && VolumetricCloud->GetMaterial() != VolumetricCloudMIDComplex && VolumetricCloud->GetMaterial() != VolumetricCloudMIDSimple)
	{
		CachedPropertyValues[static_cast<int32>(EArcTimeOfDayCachedProperty::VolumetricCloudMaterialSwitch)].bInitialized = false;
	}
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudMaterialSwitch, bChangeToSimplifiedMaterialWhenOvercast && MinimumCoverage > static_cast<float>(SimplifiedMaterialCloudCoverageThreshold) ? 1.0f : 0.0f, 0.5f);
	VolumetricCloud->SetTracingStartMaxDistance(static_cast<float>(TracingMaxStartDistance) * Scale);
	VolumetricCloud->SetShadowViewSampleCountScale(bCinematicRuntime ? static_cast<float>(CinematicCloudsShadowSampleScale) : QualityScale * static_cast<float>(ShadowSampleScale) * FMath::Lerp(2.0f, 1.0f, FMath::Clamp(Scale, 0.0f, 1.0f)));
	VolumetricCloud->SetReflectionViewSampleCountScale(QualityScale * static_cast<float>(ReflectionSampleCountScale) * 2.0f);
	VolumetricCloud->SetShadowReflectionViewSampleCountScale(QualityScale * static_cast<float>(ReflectionShadowSampleCountScale));
	ArcTimeOfDayClouds::SetConsoleFloat(TEXT("r.VolumetricCloud.DistanceToSampleMaxCount"), static_cast<float>(DistanceToSampleMaxCount) * Scale);
	ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.VolumetricCloud.ReflectionRaySampleMaxCount"), 200);
	ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.VolumetricCloud.SampleMinCount"), FMath::RoundToInt(static_cast<float>(ViewSampleScaleDay) * 56.25f * static_cast<float>(AdditionalCloudLayers.Num() + 1)));
	ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.VolumetricCloud.ViewRaySampleMaxCount"), bCinematicRuntime ? CinematicCloudsViewRaySampleMaxCount : ViewRaySampleMaxCount);
	ArcTimeOfDayClouds::SetConsoleFloat(TEXT("r.VolumetricCloud.StepSizeOnZeroConservativeDensity"), bCinematicRuntime || AdjustForPathTracer ? 1.0f : 3.0f);
	const int32 VolumetricRenderTargetMode = bCinematicRuntime ? 3 : VolumetricCloudRenderingMode == EArcTimeOfDayVolumetricRenderingMode::FidelityAndPerformance ? 0 : 2;
	ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.VolumetricRenderTarget.Mode"), VolumetricRenderTargetMode);
	ArcTimeOfDayClouds::SetConsoleInt(TEXT("r.VolumetricRenderTarget"), 1);
	const float CloudDensity = FMath::Clamp((CloudCoverage03 - MapRangeClamped(CloudCoverage03, 0.0f, 0.2f, 0.2f, 0.0f)) * 1.15f, -0.2f, 3.0f);
	const float MultiscatteringOcclusion = static_cast<float>(MultiscatteringLightOcclusion) * MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.1f, 0.3f, 0.8f, MapRangeClamped(LocalCloudCoverage, 1.3f, 1.85f, 1.0f, 0.75f));
	const float HeightMultiplier = MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.07f, 0.2f, 0.15f, 1.0f) * 0.8f;
	const float ColorFloorVariation = static_cast<float>(FloorLuminanceScale) * MapRangeClamped(static_cast<float>(Contrast), 0.1f, 1.0f, 0.8f, 1.2f) * (1.0f - CameraCloudLayerNormalizedHeight);
	const float PhaseBlend = MapRangeClamped(CloudCoverage03, 1.7f, 2.1f, 0.65f, 0.9f);
	const float InnerEmitLimit = MapRangeClamped(LocalCloudCoverage, 1.6f, 2.1f, 0.2f, 0.08f) / Scale;
	const float MultiscatteringContribution = static_cast<float>(MultiscatteringLightIntensity) * MapRangeClamped(LocalCloudCoverage, 1.3f, 1.8f, 1.0f, 0.85f);
	const float SkyAtmosphereContribution = ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere ? static_cast<float>(AmbientLightFromSkyAtmosphere) + MapRangeClamped(LocalCloudCoverage, 1.5f, 2.0f, 0.0f, MapRangeClamped(FMath::Abs(CachedSunVector.Z), 0.025f, 0.2f, 0.0f, 0.25f)) : 0.0f;
	const float OvercastTurbulence = MapRangeClamped(MinimumCoverage, 1.8f, 2.05f, 0.0f, static_cast<float>(TwoDOvercastTurbulence) / 1.666667f);
	const float ViewHeight = World && !World->ViewLocationsRenderedLastFrame.IsEmpty() ? World->ViewLocationsRenderedLastFrame[0].Z : GetActorLocation().Z;
	const float CameraDistanceFromBottom = ViewHeight - CloudMainLayerBottomHeight;
	const float CameraInsideMask = CameraCloudLayerNormalizedHeight > 0.0f && CameraCloudLayerNormalizedHeight < 1.0f ? 1.0f : 0.0f;
	const float TraceBlend = CameraInsideMask * FMath::Clamp(FMath::Abs(CameraDistanceFromBottom / FMath::Max((CloudMainLayerTopHeight - CloudMainLayerBottomHeight) * 0.3f, 1.0f)), 0.0f, 1.0f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudMaxTraceDistance, bCinematicRuntime ? static_cast<float>(CinematicCloudsTracingMaxDistance) * Scale : FMath::Lerp(static_cast<float>(TracingMaxDistanceInsideCloudLayer), static_cast<float>(TracingMaxDistanceOutsideCloudLayer), TraceBlend) * Scale, 0.005f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::CloudShadowTraceDistance, bCinematicRuntime ? static_cast<float>(CinematicCloudsTracingMaxDistance) * Scale : static_cast<float>(ShadowTracingDistance) * Scale, 0.001f);
	float ViewSampleCount = FMath::Lerp(static_cast<float>(ViewSampleScaleDay), static_cast<float>(ViewSampleScaleNight), MapRangeClamped(CachedSunVector.Z, -0.0161f, 0.011f, 0.0f, 1.0f)) * 1.7f * MapRangeClamped(LocalCloudCoverage, 1.9f, 2.2f, 1.0f, static_cast<float>(ScaleViewSamplesWhenOvercast));
	ViewSampleCount *= QualityScale;
	if (bCinematicRuntime) ViewSampleCount = static_cast<float>(CinematicCloudsViewSampleScale);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudViewSampleScale, ViewSampleCount, 0.05f);
	const FVector Direction = FRotator(0.0f, static_cast<float>(CloudDirection.GetWrappedValue(360.0)) - 180.0f, 0.0f).RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector TextureVelocity = (Direction + FVector(0.0f, 0.0f, -static_cast<float>(ThreeDNoiseVerticalMovement))) * 0.02f;
	const float BaseCloudScale = static_cast<float>(CloudFormationTextureScale) * 1200000.0f;
	CloudPhaseVectorMultiplier = TextureVelocity * BaseCloudScale;
	if (CloudSpeed == 0.0)
	{
		CloudsPosition = FVector::ZeroVector;
		CloudsBTime = 0.0f;
	}
	UpdateCloudMovementParameters();
	SetCloudScalar(TEXT("Volumetric Cloud Target Active"), 0.0f);
	SetCloudVector(TEXT("Drawn Target Mapping"), FLinearColor::Black);
	SetCloudScalar(TEXT("Clouds B Speed"), SkyMode == EArcTimeOfDaySkyMode::VoxelClouds && !VoxelCloudsFormationChanges ? 0.0f : static_cast<float>(FormationChangeSpeed));
	SetCloudScalar(TEXT("Z Formation Shift"), static_cast<float>(CloudFormationZShift * LayerHeightScale));
	SetCloudScalar(TEXT("Reflection Density Scale"), ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere ? 1.0f : 0.0f);
	SetCloudScalar(TEXT("Clouds Scale"), BaseCloudScale);
	SetCloudScalar(TEXT("Cloud Shadow Falloff"), Scale * static_cast<float>(LayerHeightScale));
	SetCloudScalar(TEXT("Cloud Layer Height"), SkyMode == EArcTimeOfDaySkyMode::VoxelClouds ? Scale * static_cast<float>(VoxelCloudsVoxelSize) : Scale * static_cast<float>(LayerHeightScale));
	SetCloudScalar(TEXT("PhaseG2"), static_cast<float>(MultiscatteringPhase2));
	SetCloudScalar(TEXT("Eccentricity"), static_cast<float>(MultiscatteringEccentricity) * (static_cast<float>(Contrast) - 0.1f + 1.0f));
	SetCloudScalar(TEXT("3D Erosion"), static_cast<float>(ThreeDErosionIntensity));
	const float CurrentExtinctionScale = static_cast<float>(ExtinctionScale) / Scale;
	SetCloudScalar(TEXT("Extinction Scale"), CurrentExtinctionScale);
	SetCloudScalar(TEXT("Outer Emit Limit"), FMath::Max(CurrentExtinctionScale / 10.0f, 1.0f) * 0.07f);
	SetCloudScalar(TEXT("Macro Scale"), static_cast<float>(MacroScale) * 1.35f);
	SetCloudScalar(TEXT("Clouds Mip Level"), static_cast<float>(CloudFormationMipLevel));
	SetCloudScalar(TEXT("HF Octaves"), static_cast<float>(MaxHighFrequencyNoiseLevels));
	SetCloudScalar(TEXT("HF Octave Zero Distance"), static_cast<float>(HighFrequencyNoiseDistanceScale) * Scale * (Quality == 0 ? 0.6f : 1.0f));
	const FVector NoiseScale = FVector(1200000.0f / 9.0f, 1200000.0f / 9.0f, 1200000.0f / 9.0f * 0.58f) * static_cast<float>(ThreeDNoiseScale) * Scale * (Quality == 0 ? 0.7f : 1.0f);
	SetCloudVector(TEXT("3D Noise Scale"), FLinearColor(NoiseScale.X, NoiseScale.Y, NoiseScale.Z, 1.0f));
	SetCloudScalar(TEXT("Close Fade Distance"), static_cast<float>(CloseFadeDistance) * Scale);
	SetCloudScalar(TEXT("Close Fade Offset"), static_cast<float>(CloseFadeOffset) * Scale);
	SetCloudScalar(TEXT("Minimum Erosion"), static_cast<float>(MinimumErosion));
	SetCloudScalar(TEXT("HF Distortion"), static_cast<float>(DistortHighFrequencyNoise));
	SetCloudScalar(TEXT("Cloud Density"), CloudDensity);
	ApplyInsideCloudFog();
	UMaterialInstanceDynamic* CloudMIDs[] = { VolumetricCloudMIDComplex, VolumetricCloudMIDSimple, InsideCloudsFogMID };
	for (UMaterialInstanceDynamic* CloudMID : CloudMIDs)
	{
		if (!CloudMID) continue;
		CloudMID->SetTextureParameterValue(TEXT("Cloud_Profile"), CloudProfileLUT);
		CloudMID->SetTextureParameterValue(TEXT("Formation Texture"), VolumetricCloudFormationVolumeTexture);
		CloudMID->SetTextureParameterValue(TEXT("3D_Texture"), NoiseTexture);
		CloudMID->SetTextureParameterValue(TEXT("2D Turbulence"), TwoDOvercastTurbulenceTexture);
		CloudMID->SetScalarParameterValue(TEXT("Path Tracer"), AdjustForPathTracer ? 1.0f : 0.0f);
	}
	CacheFloatValue(EArcTimeOfDayCachedProperty::MacroVariation, static_cast<float>(MacroVariation) * MapRangeClamped(CloudCoverage03, 0.0f, 0.3f, 0.0f, 1.0f), 0.001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudLerpToSimplified, SimplifiedAlpha, 0.005f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::HighFrequencyNoiseAmount, static_cast<float>(HighFrequencyNoiseAmount) * (1.0f - SimplifiedAlpha), 0.002f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::OvercastTurbulenceStrength, OvercastTurbulence, 0.005f);
	CacheColorValue(EArcTimeOfDayCachedProperty::VolumetricCloudAlbedo, CurrentVolumetricCloudAlbedo(), 0.0001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::MultiscatteringOcclusion, MultiscatteringOcclusion, 0.0001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudMultiscatteringPhase1, static_cast<float>(MultiscatteringPhase1) * FMath::Lerp(1.0f, 0.8f, CachedNightFilter), 0.001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationClear, static_cast<float>(BaseFloorVariation) * HeightMultiplier, 0.001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationCloudy, FMath::Max(static_cast<float>(OvercastFloorVariation) * HeightMultiplier, 0.001f), 0.001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudFloorVariationColor, ColorFloorVariation, 0.001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::PhaseBlend, PhaseBlend, 0.005f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolCloudInnerEmitLimit, InnerEmitLimit, 0.001f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::MultiscatteringIntensity, MultiscatteringContribution, 0.002f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::VolumetricCloudSkyAtmoContribution, SkyAtmosphereContribution, 0.01f);
	CacheFloatValue(EArcTimeOfDayCachedProperty::PaintedCloudCoverageOpacityLayer1, 1.0f, 0.000001f);
	VolumetricCloud->MarkRenderStateDirty();
}
