// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayActor.h"

#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/VolumeTexture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace ArcTimeOfDayConfiguration
{
	const TCHAR* ClearSkyMaterialPath(const EArcTimeOfDayColorMode ColorMode, const bool bRealStars, const bool bAurora)
	{
		if (ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere)
		{
			if (bRealStars) return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCA.Arc_UDS_KCA") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KC.Arc_UDS_KC");
			return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KA.Arc_UDS_KA") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_K.Arc_UDS_K");
		}
		if (bRealStars) return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CA.Arc_UDS_CA") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_C.Arc_UDS_C");
		return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_A.Arc_UDS_A") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_B.Arc_UDS_B");
	}

	const TCHAR* DynamicSkyMaterialPath(const EArcTimeOfDayColorMode ColorMode, const bool bRealStars, const bool bAurora, const bool bSingleCloudLayer)
	{
		if (ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere)
		{
			if (bRealStars) return bAurora ? bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCADO.Arc_UDS_KCADO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCAD.Arc_UDS_KCAD") : bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCDO.Arc_UDS_KCDO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCD.Arc_UDS_KCD");
			return bAurora ? bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KADO.Arc_UDS_KADO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KAD.Arc_UDS_KAD") : bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KDO.Arc_UDS_KDO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KD.Arc_UDS_KD");
		}
		if (bRealStars) return bAurora ? bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CADO.Arc_UDS_CADO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CAD.Arc_UDS_CAD") : bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CDO.Arc_UDS_CDO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CD.Arc_UDS_CD");
		return bAurora ? bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_ADO.Arc_UDS_ADO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_AD.Arc_UDS_AD") : bSingleCloudLayer ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_DO.Arc_UDS_DO") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_D.Arc_UDS_D");
	}

	const TCHAR* StaticSkyMaterialPath(const EArcTimeOfDayColorMode ColorMode, const bool bRealStars, const bool bAurora)
	{
		if (ColorMode == EArcTimeOfDayColorMode::SkyAtmosphere)
		{
			if (bRealStars) return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCAS.Arc_UDS_KCAS") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KCS.Arc_UDS_KCS");
			return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KAS.Arc_UDS_KAS") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_KS.Arc_UDS_KS");
		}
		if (bRealStars) return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CAS.Arc_UDS_CAS") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_CS.Arc_UDS_CS");
		return bAurora ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_AS.Arc_UDS_AS") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_S.Arc_UDS_S");
	}

	const TCHAR* SkyMaterialPath(const EArcTimeOfDaySkyMode SkyMode, const EArcTimeOfDayColorMode ColorMode, const bool bRealStars, const bool bAurora, const bool bSingleCloudLayer)
	{
		switch (SkyMode)
		{
		case EArcTimeOfDaySkyMode::VolumetricClouds:
			return ClearSkyMaterialPath(ColorMode, bRealStars, bAurora);
		case EArcTimeOfDaySkyMode::DynamicClouds2D:
			return DynamicSkyMaterialPath(ColorMode, bRealStars, bAurora, bSingleCloudLayer);
		case EArcTimeOfDaySkyMode::StaticClouds:
			return StaticSkyMaterialPath(ColorMode, bRealStars, bAurora);
		case EArcTimeOfDaySkyMode::VoxelClouds:
			return ClearSkyMaterialPath(ColorMode, bRealStars, bAurora);
		case EArcTimeOfDaySkyMode::Space:
			return bRealStars ? TEXT("/ArcTimeOfDay/Materials/Arc_UDS_PC.Arc_UDS_PC") : TEXT("/ArcTimeOfDay/Materials/Arc_UDS_P.Arc_UDS_P");
		case EArcTimeOfDaySkyMode::VolumetricAurora:
			return ClearSkyMaterialPath(ColorMode, bRealStars, false);
		case EArcTimeOfDaySkyMode::NoClouds:
			return ClearSkyMaterialPath(ColorMode, bRealStars, bAurora);
		default:
			checkNoEntry();
			return nullptr;
		}
	}

#if WITH_EDITORONLY_DATA
	void ConfigureEditorMesh(UStaticMeshComponent* Component, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		Component->SetStaticMesh(Mesh);
		Component->SetMaterial(0, Material);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->bCastDynamicShadow = false;
		Component->bCastStaticShadow = false;
		Component->SetHiddenInGame(true);
		Component->SetIsVisualizationComponent(true);
	}
#endif
}

void AArcTimeOfDayActor::LoadFoundationResources()
{
	using namespace ArcTimeOfDayConfiguration;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkySphereMesh(TEXT("/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere.Ultra_Dynamic_Sky_Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyMaterialFinder(TEXT("/ArcTimeOfDay/Materials/Arc_UDS_K.Arc_UDS_K"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudComplexFinder(TEXT("/ArcTimeOfDay/Materials/Clouds/Arc_UDS_VolumetricClouds_Default.Arc_UDS_VolumetricClouds_Default"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudSimpleFinder(TEXT("/ArcTimeOfDay/Materials/Clouds/Arc_UDS_VolumetricClouds_Default_S.Arc_UDS_VolumetricClouds_Default_S"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudComplexTwoLayerFinder(TEXT("/ArcTimeOfDay/Materials/Clouds/Arc_UDS_VolumetricClouds_2L.Arc_UDS_VolumetricClouds_2L"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudSimpleTwoLayerFinder(TEXT("/ArcTimeOfDay/Materials/Clouds/Arc_UDS_VolumetricClouds_2L_S.Arc_UDS_VolumetricClouds_2L_S"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudShadowFinder(TEXT("/Game/UltraDynamicSky/Materials/Material_Instances/Light_Functions/LF_VolCloud_Shadows.LF_VolCloud_Shadows"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InsideCloudFogOneLayerFinder(TEXT("/Game/UltraDynamicSky/Materials/Inside_Clouds_Fog_Particle.Inside_Clouds_Fog_Particle"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InsideCloudFogTwoLayerFinder(TEXT("/Game/UltraDynamicSky/Materials/Material_Instances/Inside_Clouds_Fog_Particle_2L.Inside_Clouds_Fog_Particle_2L"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudLayerMapBrushFinder(TEXT("/Game/UltraDynamicSky/Materials/CloudLayerMapBrush.CloudLayerMapBrush"));
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> CloudParametersFinder(TEXT("/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC.UDS_VolumetricClouds_MPC"));
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> WeatherParametersFinder(TEXT("/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters.UltraDynamicWeather_Parameters"));
	static ConstructorHelpers::FObjectFinder<UTexture> CloudProfileFinder(TEXT("/Game/UltraDynamicSky/Textures/Volumetric_Clouds/Cloud_Profile.Cloud_Profile"));
	static ConstructorHelpers::FObjectFinder<UVolumeTexture> FormationFinder(TEXT("/Game/UltraDynamicSky/Textures/3D_Clouds/FormationVolume.FormationVolume"));
	static ConstructorHelpers::FObjectFinder<UVolumeTexture> NoiseHighFinder(TEXT("/Game/UltraDynamicSky/Textures/3D_Clouds/3D_Cells_64.3D_Cells_64"));
	static ConstructorHelpers::FObjectFinder<UVolumeTexture> NoiseLowFinder(TEXT("/Game/UltraDynamicSky/Textures/3D_Clouds/3D_Cells_32.3D_Cells_32"));
	static ConstructorHelpers::FObjectFinder<UVolumeTexture> NoiseEpicFinder(TEXT("/Game/UltraDynamicSky/Textures/3D_Clouds/3DCells_128.3DCells_128"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> CloudLayerMapFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/CloudLayersMap.CloudLayersMap"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> TurbulenceFinder(TEXT("/Game/UltraDynamicSky/Textures/Clouds/CloudsTurbulence.CloudsTurbulence"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> TwoDCloudFormationFinder(TEXT("/Game/UltraDynamicSky/Textures/Clouds/clouds_diverse.clouds_diverse"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> MoonTextureFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Moon_Color.Moon_Color"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> MoonNormalFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Moon_PhaseNormal.Moon_PhaseNormal"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> CloudWispsTextureFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps.Cloud_Wisps"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> StarsTextureFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Tiling_Stars.Tiling_Stars"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> RealStarsTextureFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Real_Stars.Real_Stars"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> RealStarsStarmapFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Starmap500k.Starmap500k"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> RealStarsSystemFinder(TEXT("/Game/UltraDynamicSky/Particles/RealStarsSprites.RealStarsSprites"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> VolumetricCloudLightRaysFinder(TEXT("/Game/UltraDynamicSky/Particles/VolumetricCloud_LightRays.VolumetricCloud_LightRays"));
	static ConstructorHelpers::FObjectFinder<UPrimaryDataAsset> CalendarFinder(TEXT("/Game/UltraDynamicSky/Blueprints/System/Calendars/Gregorian_Calendar.Gregorian_Calendar"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> ExposureCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Exposure_Compensation_Curve.Exposure_Compensation_Curve"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> ExposurePhysicalCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Exposure_Compensation_Curve_Physical.Exposure_Compensation_Curve_Physical"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> TwoDShineCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Shine_Intensity.Shine_Intensity"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> TwoDHighlightIntensityCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Sun_Highlight_Intensity.Sun_Highlight_Intensity"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> TwoDHighlightRadiusCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Sun_Highlight_Radius.Sun_Highlight_Radius"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> DirectionalIntensityCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Directional_Light_Intensity.Directional_Light_Intensity"));
	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> DirectionalLightScatteringCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Color_Curves/Sun_Light_Color.Sun_Light_Color"));
	static ConstructorHelpers::FObjectFinder<UVolumeTexture> SimplifiedSunLUTFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Sun_Atmosphere_LUT_Volume.Sun_Atmosphere_LUT_Volume"));
	static ConstructorHelpers::FObjectFinder<UVolumeTexture> SimplifiedMoonLUTFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/Moon_Atmosphere_LUT_Volume.Moon_Atmosphere_LUT_Volume"));
	static ConstructorHelpers::FObjectFinder<UTextureCube> SkyLightCubemapFinder(TEXT("/Game/UltraDynamicSky/Textures/Sky/FlatCubemap.FlatCubemap"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GlobalFogMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlobalFogMaterialFinder(TEXT("/Game/UltraDynamicSky/Materials/Global_Volumetric_Fog.Global_Volumetric_Fog"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlobalGroundFogMaterialFinder(TEXT("/Game/UltraDynamicSky/Materials/Material_Instances/Global_Volumetric_Fog_w_Ground_Fog.Global_Volumetric_Fog_w_Ground_Fog"));
	static ConstructorHelpers::FObjectFinder<UCurveFloat> SkyAtmosphereDensityCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Float_Curves/Skyatmosphere_Density.Skyatmosphere_Density"));
	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> SunDiskCurveFinder(TEXT("/Game/UltraDynamicSky/Materials/Color_Curves/Sun_Disk_Color.Sun_Disk_Color"));
	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> FogInscatteringFinder(TEXT("/Game/UltraDynamicSky/Materials/Color_Curves/Fog_Scattering_Color.Fog_Scattering_Color"));
	static ConstructorHelpers::FObjectFinder<UCurveLinearColor> FogDirectionalFinder(TEXT("/Game/UltraDynamicSky/Materials/Color_Curves/Fog_Directional_Scattering_Color.Fog_Directional_Scattering_Color"));
#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinder<UStaticMesh> UDSLabelMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/UDS_Text.UDS_Text"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CompassMesh(TEXT("/Game/UltraDynamicSky/Meshes/Compass.Compass"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WestMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/Compass_W.Compass_W"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ClockDiskMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/ClockDisk.ClockDisk"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NorthMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/Compass_N.Compass_N"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SouthMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/Compass_S.Compass_S"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EastMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/Compass_E.Compass_E"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HandleMesh(TEXT("/Game/UltraDynamicSky/Meshes/EditorLabels/Editor_SunMoon_Handle.Editor_SunMoon_Handle"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UDSLabelMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/UDS_EditorLabel.UDS_EditorLabel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CompassMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/UDS_Compass_Material.UDS_Compass_Material"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CompassLettersMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/UDS_Compass_Material_Letters.UDS_Compass_Material_Letters"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ClockMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/UDS_Clock_Material.UDS_Clock_Material"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MoonHandleMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/Editor_MoonHandle_Material.Editor_MoonHandle_Material"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MoonGlowMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/Editor_Moon_Glow.Editor_Moon_Glow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SunHandleMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/Editor_SunHandle_Material.Editor_SunHandle_Material"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SunGlowMaterial(TEXT("/Game/UltraDynamicSky/Blueprints/System/Editor_UI/Editor_Materials/Editor_Sun_Glow.Editor_Sun_Glow"));
#endif

	SkySphere->SetStaticMesh(SkySphereMesh.Object);
	SkyMaterial = SkyMaterialFinder.Object;
	VolumetricCloudMaterialComplex = CloudComplexFinder.Object;
	VolumetricCloudMaterialSimple = CloudSimpleFinder.Object;
	VolumetricCloudMaterialComplexTwoLayer = CloudComplexTwoLayerFinder.Object;
	VolumetricCloudMaterialSimpleTwoLayer = CloudSimpleTwoLayerFinder.Object;
	DefaultVolumetricCloudShadowMaterial = CloudShadowFinder.Object;
	InsideCloudFogMaterialOneLayer = InsideCloudFogOneLayerFinder.Object;
	InsideCloudFogMaterialTwoLayer = InsideCloudFogTwoLayerFinder.Object;
	CloudLayerMapBrushMaterial = CloudLayerMapBrushFinder.Object;
	VolumetricCloudParameters = CloudParametersFinder.Object;
	WeatherParameters = WeatherParametersFinder.Object;
	CloudProfileLUT = CloudProfileFinder.Object;
	VolumetricCloudFormationVolumeTexture = FormationFinder.Object;
	HighQualityVolumeTexture = NoiseHighFinder.Object;
	LowQualityVolumeTexture = NoiseLowFinder.Object;
	EpicQualityVolumeTexture = NoiseEpicFinder.Object;
	TwoDOvercastTurbulenceTexture = TurbulenceFinder.Object;
	TwoDDynamicCloudFormationTexture = TwoDCloudFormationFinder.Object;
	CloudLayerMapRenderTarget = CloudLayerMapFinder.Object;
	MoonTexture = MoonTextureFinder.Object;
	MoonNormalTexture = MoonNormalFinder.Object;
	CloudWispsTextureSetting = CloudWispsTextureFinder.Object;
	StarsTexture = StarsTextureFinder.Object;
	RealStarsTexture = RealStarsTextureFinder.Object;
	RealStarsSpritesStarmap = RealStarsStarmapFinder.Object;
	RealStarsSpritesSystem = RealStarsSystemFinder.Object;
	VolumetricCloudLightRaysSystem = VolumetricCloudLightRaysFinder.Object;
	Calendar = CalendarFinder.Object;
	ExposureCurve = ExposureCurveFinder.Object;
	ExposureCompensationCurve = ExposureCurveFinder.Object;
	ExposureCompensationCurvePhysical = ExposurePhysicalCurveFinder.Object;
	TwoDDynamicCloudsShineIntensityCurve = TwoDShineCurveFinder.Object;
	TwoDDynamicCloudsSunHighlightIntensityCurve = TwoDHighlightIntensityCurveFinder.Object;
	TwoDDynamicCloudsSunHighlightRadiusCurve = TwoDHighlightRadiusCurveFinder.Object;
	DirectionalIntensityCurve = DirectionalIntensityCurveFinder.Object;
	DirectionalLightScatteringCurve = DirectionalLightScatteringCurveFinder.Object;
	SimplifiedColorSunAtmosphereLUT = SimplifiedSunLUTFinder.Object;
	SimplifiedColorMoonAtmosphereLUT = SimplifiedMoonLUTFinder.Object;
	CapturedSceneSkyLight->Cubemap = SkyLightCubemapFinder.Object;
	GlobalVolumetricFogMaterial = GlobalFogMaterialFinder.Object;
	GlobalVolumetricGroundFogMaterial = GlobalGroundFogMaterialFinder.Object;
	GlobalVolumetricFogMesh->SetStaticMesh(GlobalFogMeshFinder.Object);
	InsideCloudFogMesh->SetStaticMesh(GlobalFogMeshFinder.Object);
	SkyAtmosphereDensityCurve = SkyAtmosphereDensityCurveFinder.Object;
	SunDiskColorCurve = SunDiskCurveFinder.Object;
	FogInscatteringColorCurve = FogInscatteringFinder.Object;
	FogDirectionalInscatteringColorCurve = FogDirectionalFinder.Object;

#if WITH_EDITORONLY_DATA
	ConfigureEditorMesh(UDSLabel, UDSLabelMesh.Object, UDSLabelMaterial.Object);
	ConfigureEditorMesh(Compass, CompassMesh.Object, CompassMaterial.Object);
	ConfigureEditorMesh(WestLabel, WestMesh.Object, CompassLettersMaterial.Object);
	ConfigureEditorMesh(ClockDisk, ClockDiskMesh.Object, ClockMaterial.Object);
	ConfigureEditorMesh(NorthLabel, NorthMesh.Object, CompassLettersMaterial.Object);
	ConfigureEditorMesh(SouthLabel, SouthMesh.Object, CompassLettersMaterial.Object);
	ConfigureEditorMesh(EastLabel, EastMesh.Object, CompassLettersMaterial.Object);
	ConfigureEditorMesh(MoonHandle, HandleMesh.Object, MoonHandleMaterial.Object);
	MoonHandle->SetMaterial(1, MoonGlowMaterial.Object);
	ConfigureEditorMesh(SunHandle, HandleMesh.Object, SunHandleMaterial.Object);
	SunHandle->SetMaterial(1, SunGlowMaterial.Object);
#endif
}

void AArcTimeOfDayActor::EnsureVisualResources()
{
	const TCHAR* SelectedSkyMaterialPath = ArcTimeOfDayConfiguration::SkyMaterialPath(SkyMode, ColorMode, bSimulateRealStars, UseAuroras, TwoDDynamicCloudsUseSingleLayer);
	UMaterialInterface* SelectedSkyMaterial = SelectedSkyMaterialPath ? LoadObject<UMaterialInterface>(nullptr, SelectedSkyMaterialPath) : nullptr;
	if (SelectedSkyMaterial && (!SkySphereMID || SkyMaterial != SelectedSkyMaterial))
	{
		SkyMaterial = SelectedSkyMaterial;
		SkySphereMID = SkySphere->CreateDynamicMaterialInstance(0, SkyMaterial);
		HardResetCache();
	}
	UMaterialInterface* ComplexParent = CustomVolumetricCloudMaterial;
	UMaterialInterface* SimpleParent = CustomVolumetricCloudMaterial;
	if (!ComplexParent)
	{
		ComplexParent = AdditionalCloudLayers.IsEmpty() ? VolumetricCloudMaterialComplex : VolumetricCloudMaterialComplexTwoLayer;
		SimpleParent = AdditionalCloudLayers.IsEmpty() ? VolumetricCloudMaterialSimple : VolumetricCloudMaterialSimpleTwoLayer;
	}
	bool bCloudMaterialChanged = false;
	if (ComplexParent && (!VolumetricCloudMIDComplex || AppliedVolumetricCloudMaterialComplex != ComplexParent))
	{
		VolumetricCloudMIDComplex = UMaterialInstanceDynamic::Create(ComplexParent, VolumetricCloud, TEXT("UDS Vol Clouds MID Complex"));
		AppliedVolumetricCloudMaterialComplex = ComplexParent;
		bCloudMaterialChanged = true;
	}
	else if (!ComplexParent && (VolumetricCloudMIDComplex || AppliedVolumetricCloudMaterialComplex))
	{
		VolumetricCloudMIDComplex = nullptr;
		AppliedVolumetricCloudMaterialComplex = nullptr;
		bCloudMaterialChanged = true;
	}
	if (SimpleParent && (!VolumetricCloudMIDSimple || AppliedVolumetricCloudMaterialSimple != SimpleParent))
	{
		VolumetricCloudMIDSimple = UMaterialInstanceDynamic::Create(SimpleParent, VolumetricCloud, TEXT("UDS Vol Clouds MID Simple"));
		AppliedVolumetricCloudMaterialSimple = SimpleParent;
		bCloudMaterialChanged = true;
	}
	else if (!SimpleParent && (VolumetricCloudMIDSimple || AppliedVolumetricCloudMaterialSimple))
	{
		VolumetricCloudMIDSimple = nullptr;
		AppliedVolumetricCloudMaterialSimple = nullptr;
		bCloudMaterialChanged = true;
	}
	if (bCloudMaterialChanged) HardResetCache();
}
