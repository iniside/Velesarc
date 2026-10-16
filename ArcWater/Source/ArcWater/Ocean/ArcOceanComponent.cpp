// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOceanComponent.h"

#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "EngineDefines.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Math/TranslationMatrix.h"
#include "RenderingThread.h"
#include "SceneView.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "Render/ArcTerrainAtlasPublish.h"
#include "Render/ArcTerrainAtlasTexture.h"

#include "ArcWaterModule.h"
#include "Ocean/ArcOceanSceneProxy.h"

namespace ArcOceanWaterParams
{
	const FName WaterPlanePoint(TEXT("WaterPlanePoint"));
	const FName WaterPlaneNormal(TEXT("WaterPlaneNormal"));
	const FName WaterExtinction(TEXT("WaterExtinction"));
	const FName WaterScatterAlbedo(TEXT("WaterScatterAlbedo"));
	const FName NearPlaneCm(TEXT("NearPlaneCm"));
	const FName MaskBandHalfCm(TEXT("MaskBandHalfCm"));

	constexpr float Ln10 = 2.302585093f;
	constexpr float MinClarityMeters = 0.25f;
	constexpr float CentimetresPerMetre = 100.0f;
	constexpr float WorldUnitCm = 1.0f;
}

UArcOceanComponent::UArcOceanComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Mobility = EComponentMobility::Movable;
	CastShadow = false;
	bCastDynamicShadow = false;
	bUseAsOccluder = false;
	SetGenerateOverlapEvents(false);

	// The custom depth pass does not test against scene depth, so the sheet stamps this stencil value even where terrain occludes it: every consumer must also match CustomDepth against SceneDepth.
	bRenderCustomDepth = true;
	CustomDepthStencilValue = ArcOcean::UnderwaterStencilValue;
}

FPrimitiveSceneProxy* UArcOceanComponent::CreateSceneProxy()
{
	if (BaseCellSize <= 0.0f || RingCount <= 0)
	{
		return nullptr;
	}

	return new FArcOceanSceneProxy(this);
}

FBoxSphereBounds UArcOceanComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// HALF_WORLD_MAX: the sheet re-centres on the camera every frame, so any finite bound culls it the moment the camera leaves.
	return FBoxSphereBounds(FVector::ZeroVector, FVector(HALF_WORLD_MAX), HALF_WORLD_MAX);
}

FMatrix UArcOceanComponent::GetRenderMatrix() const
{
	return FTranslationMatrix(GetComponentTransform().GetTranslation());
}

void UArcOceanComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0 && OceanMaterial != InMaterial)
	{
		OceanMaterial = InMaterial;
		MarkRenderStateDirty();
	}
}

UMaterialInterface* UArcOceanComponent::GetMaterial(int32 ElementIndex) const
{
	return ElementIndex == 0 ? OceanMaterial : nullptr;
}

int32 UArcOceanComponent::GetNumMaterials() const
{
	return 1;
}

void UArcOceanComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (OceanMaterial != nullptr)
	{
		OutMaterials.Add(OceanMaterial);
	}
}

void UArcOceanComponent::PushWaveParams()
{
	if (SceneProxy == nullptr)
	{
		return;
	}

	ArcOcean::FArcOceanWaveFields WaveFields;
	ArcOcean::PackWaveParams(WaveSettings, WaveFields);

	FArcOceanSceneProxy* TypedProxy = static_cast<FArcOceanSceneProxy*>(SceneProxy);
	ENQUEUE_RENDER_COMMAND(ArcOceanUpdateWaveParams)(
		[TypedProxy, WaveFields](FRHICommandListImmediate& RHICmdList)
		{
			TypedProxy->SetWaveFields_RenderThread(RHICmdList, WaveFields);
		});
}

float UArcOceanComponent::GetDeepDepthCm() const
{
	return FMath::Max(WaveSettings.LongestWavelengthCm, ArcOceanWaterParams::WorldUnitCm);
}

void UArcOceanComponent::PushSeabedSnapshot(const FSceneView& InView)
{
	if (SceneProxy == nullptr)
	{
		return;
	}

	FArcOceanSeabedFields SeabedFields;
	SeabedFields.DeepDepthCm = GetDeepDepthCm();
	SeabedFields.bAtlasFilterable = ArcTerrainAtlasPublish::IsHeightAtlasFilterable();

	AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain != nullptr)
	{
		UArcTerrainAtlasTexture* Facade = ArcTerrainAtlasPublish::AcquireHeightAtlasFacade(*Terrain);
		const bool bSnapshotBuilt = Facade != nullptr
			&& ArcTerrainAtlasPublish::BuildAtlasSnapshot(*Terrain, InView.ViewMatrices.GetViewOrigin(), SeabedFields.Snapshot);
		if (bSnapshotBuilt)
		{
			SeabedFields.AtlasResource = Facade->GetAtlasTextureResource();
		}
	}

	FArcOceanSceneProxy* TypedProxy = static_cast<FArcOceanSceneProxy*>(SceneProxy);
	ENQUEUE_RENDER_COMMAND(ArcOceanUpdateSeabedFields)(
		[TypedProxy, SeabedFields](FRHICommandListImmediate& RHICmdList)
		{
			TypedProxy->SetSeabedFields_RenderThread(RHICmdList, SeabedFields);
		});
}

void UArcOceanComponent::OnRegister()
{
	Super::OnRegister();

	UWorld* World = GetWorld();
	if (World != nullptr && !SeabedBeginPostProcessSettingsHandle.IsValid())
	{
		SeabedBeginPostProcessSettingsHandle = World->OnBeginPostProcessSettings.AddUObject(
			this, &UArcOceanComponent::HandleSeabedBeginPostProcessSettings);
	}

	if (World != nullptr && !BeginPostProcessSettingsHandle.IsValid())
	{
		// The push has to stay on OnBeginPostProcessSettings: SendAllEndOfFrameUpdates flushes the deferred collection writes after this broadcast and before the render commands, so a tick or any earlier hook lands one frame late.
		BeginPostProcessSettingsHandle = World->OnBeginPostProcessSettings.AddUObject(this, &UArcOceanComponent::HandleBeginPostProcessSettings);
	}

	PushClarity();
	RefreshUnderwaterBlendable();
}

void UArcOceanComponent::OnUnregister()
{
	UWorld* World = GetWorld();
	if (World != nullptr && SeabedBeginPostProcessSettingsHandle.IsValid())
	{
		World->OnBeginPostProcessSettings.Remove(SeabedBeginPostProcessSettingsHandle);
	}
	SeabedBeginPostProcessSettingsHandle.Reset();

	if (World != nullptr && BeginPostProcessSettingsHandle.IsValid())
	{
		World->OnBeginPostProcessSettings.Remove(BeginPostProcessSettingsHandle);
	}
	BeginPostProcessSettingsHandle.Reset();

	Super::OnUnregister();
}

void UArcOceanComponent::PostLoad()
{
	Super::PostLoad();

	PushClarity();
}

void UArcOceanComponent::PushClarity()
{
	UWorld* World = GetWorld();
	if (World == nullptr || WaterParameterCollection == nullptr)
	{
		return;
	}

	UMaterialParameterCollectionInstance* Collection = World->GetParameterCollectionInstance(WaterParameterCollection);
	if (Collection == nullptr)
	{
		return;
	}

	const float ClarityCm = FMath::Max(ClarityDistanceMeters, ArcOceanWaterParams::MinClarityMeters) * ArcOceanWaterParams::CentimetresPerMetre;
	const float ExtinctionScale = ArcOceanWaterParams::Ln10 / ClarityCm;

	Collection->SetVectorParameterValue(
		ArcOceanWaterParams::WaterExtinction,
		FLinearColor(AbsorptionTint.R * ExtinctionScale, AbsorptionTint.G * ExtinctionScale, AbsorptionTint.B * ExtinctionScale, 0.0f));
	Collection->SetScalarParameterValue(ArcOceanWaterParams::WaterScatterAlbedo, FMath::Clamp(ScatterAlbedo, 0.0f, 1.0f));
}

namespace ArcOceanSeabedProbeLocal
{
	float BilinearLocalZ(const TArray<float>& InHeights, FIntPoint InDims, FIntPoint InMinTexel,
		double InTexelSizeCm, const FVector2D& InLocalXY)
	{
		const double Tx = InLocalXY.X / InTexelSizeCm - static_cast<double>(InMinTexel.X);
		const double Ty = InLocalXY.Y / InTexelSizeCm - static_cast<double>(InMinTexel.Y);
		const int32 Ix = FMath::Clamp(FMath::FloorToInt32(Tx), 0, InDims.X - 2);
		const int32 Iy = FMath::Clamp(FMath::FloorToInt32(Ty), 0, InDims.Y - 2);
		const float Fx = static_cast<float>(FMath::Clamp(Tx - static_cast<double>(Ix), 0.0, 1.0));
		const float Fy = static_cast<float>(FMath::Clamp(Ty - static_cast<double>(Iy), 0.0, 1.0));

		const int32 Row = InDims.X;
		const float Z00 = InHeights[Iy * Row + Ix];
		const float Z10 = InHeights[Iy * Row + Ix + 1];
		const float Z01 = InHeights[(Iy + 1) * Row + Ix];
		const float Z11 = InHeights[(Iy + 1) * Row + Ix + 1];
		return FMath::Lerp(FMath::Lerp(Z00, Z10, Fx), FMath::Lerp(Z01, Z11, Fx), Fy);
	}

	bool ProbeSeabedWorldZ(UArcTerrainSubsystem& InSubsystem, AArcTerrain& InTerrain,
		const FTransform& InTerrainTransform, double InTexelSize, const FVector& InProbePoint,
		double& OutSeabedWorldZ)
	{
		const FVector Local = InTerrainTransform.InverseTransformPosition(InProbePoint);
		const FVector2D LocalXY(Local.X, Local.Y);

		const FIntPoint MinTexel(
			FMath::FloorToInt32(LocalXY.X / InTexelSize),
			FMath::FloorToInt32(LocalXY.Y / InTexelSize));

		TArray<float> Heights;
		FIntPoint Dims = FIntPoint::ZeroValue;
		bool bComplete = false;
		if (!InSubsystem.SampleTerrainHeightTexels(
				InTerrain, MinTexel, MinTexel + FIntPoint(1, 1), Heights, Dims, bComplete))
		{
			return false;
		}
		if (Dims.X < 2 || Dims.Y < 2 || Heights.Num() != Dims.X * Dims.Y)
		{
			return false;
		}

		const float LocalZ = BilinearLocalZ(Heights, Dims, MinTexel, InTexelSize, LocalXY);
		OutSeabedWorldZ = InTerrainTransform.TransformPosition(
			FVector(LocalXY.X, LocalXY.Y, static_cast<double>(LocalZ))).Z;
		return true;
	}

	void SolveSeabed(UWorld* InWorld, AArcTerrain* InTerrain, const FVector& InSurfacePoint,
		float InDeepDepthCm, float InShortestWavelengthCm, float& OutDepthCm, FVector2f& OutShoreward)
	{
		OutDepthCm = InDeepDepthCm;
		OutShoreward = FVector2f::ZeroVector;

		if (InWorld == nullptr || InTerrain == nullptr)
		{
			return;
		}

		UArcTerrainSubsystem* Subsystem = InWorld->GetSubsystem<UArcTerrainSubsystem>();
		if (Subsystem == nullptr)
		{
			return;
		}

		const int32 TexelSizeCm = InTerrain->Config.TexelSizeCm();
		if (TexelSizeCm <= 0)
		{
			return;
		}
		const double TexelSize = static_cast<double>(TexelSizeCm);

		const FTransform TerrainTransform = InTerrain->GetTerrainTransform();

		double CentreWorldZ = 0.0;
		if (!ProbeSeabedWorldZ(*Subsystem, *InTerrain, TerrainTransform, TexelSize, InSurfacePoint, CentreWorldZ))
		{
			return;
		}

		OutDepthCm = static_cast<float>(InSurfacePoint.Z - CentreWorldZ);

		const double StepCm = FMath::Max(0.5 * static_cast<double>(InShortestWavelengthCm), 4.0 * TexelSize);

		double EastWorldZ = 0.0;
		double NorthWorldZ = 0.0;
		if (!ProbeSeabedWorldZ(*Subsystem, *InTerrain, TerrainTransform, TexelSize,
				InSurfacePoint + FVector(StepCm, 0.0, 0.0), EastWorldZ)
			|| !ProbeSeabedWorldZ(*Subsystem, *InTerrain, TerrainTransform, TexelSize,
				InSurfacePoint + FVector(0.0, StepCm, 0.0), NorthWorldZ))
		{
			return;
		}

		OutShoreward = FVector2f(
			static_cast<float>(EastWorldZ - CentreWorldZ),
			static_cast<float>(NorthWorldZ - CentreWorldZ)).GetSafeNormal();
	}
}

FArcOceanViewPlane UArcOceanComponent::SolveViewPlane(const FSceneView& InView) const
{
	TArray<FArcGerstnerWave> Waves;
	ArcOceanWaves::BuildWaveSet(WaveSettings, Waves);

	const FVector SurfaceOrigin = GetComponentTransform().GetTranslation();
	const FVector ViewOrigin = InView.ViewMatrices.GetViewOrigin();

	// The solver works in the same actor-relative XY as the vertex factory, which phases the waves in LatticeTranslatedWorldXY - SurfaceOriginTranslatedWorld.xy.
	const FVector2f ObservedXY(
		static_cast<float>(ViewOrigin.X - SurfaceOrigin.X),
		static_cast<float>(ViewOrigin.Y - SurfaceOrigin.Y));

	const float TimeSeconds = InView.Family != nullptr ? static_cast<float>(InView.Family->Time.GetWorldTimeSeconds()) : 0.0f;
	const float DeltaSeconds = InView.Family != nullptr ? InView.Family->Time.GetDeltaWorldTimeSeconds() : 0.0f;

	const float ShortestWavelengthCm = Waves.Num() > 0 ? Waves.Last().Wavelength : 0.0f;

	float DepthCm = 0.0f;
	FVector2f Shoreward = FVector2f::ZeroVector;
	ArcOceanSeabedProbeLocal::SolveSeabed(
		GetWorld(),
		AffectedTerrain.Get(),
		FVector(ViewOrigin.X, ViewOrigin.Y, SurfaceOrigin.Z),
		GetDeepDepthCm(),
		ShortestWavelengthCm,
		DepthCm,
		Shoreward);

	float HeightZ = 0.0f;
	FVector3f Normal = FVector3f::ZAxisVector;
	float VerticalSpeed = 0.0f;
	float AmplitudeSumCm = 0.0f;
	ArcOceanWaves::SolveSurfaceAt(Waves, ObservedXY, TimeSeconds, BaseCellSize, DepthCm, Shoreward,
		HeightZ, Normal, VerticalSpeed, AmplitudeSumCm);

	const float NearPlaneCm = static_cast<float>(InView.NearClippingDistance);
	const FVector2f TanHalfFov = InView.ViewMatrices.GetTanHalfFov();
	const float NormalZFloor = 1.0f - ArcOcean::ARC_OCEAN_MAX_STEEPNESS_SUM;
	const float NearPlaneExtentCm = NearPlaneCm * TanHalfFov.Size();
	const float SlopeMagnitude = FMath::Sqrt(Normal.X * Normal.X + Normal.Y * Normal.Y) / FMath::Max(Normal.Z, NormalZFloor);
	const float MaskBandHalfCm = FMath::Min(
		VerticalSpeed * DeltaSeconds + NearPlaneExtentCm * SlopeMagnitude,
		NearPlaneExtentCm + AmplitudeSumCm);

	FArcOceanViewPlane ViewPlane;
	ViewPlane.PlanePoint = FVector(ViewOrigin.X, ViewOrigin.Y, SurfaceOrigin.Z + static_cast<double>(HeightZ));
	ViewPlane.PlaneNormal = Normal;
	ViewPlane.NearPlaneCm = NearPlaneCm;
	ViewPlane.TanHalfFov = TanHalfFov;
	ViewPlane.SlopeMagnitude = SlopeMagnitude;
	ViewPlane.MaskBandHalfCm = MaskBandHalfCm;
	return ViewPlane;
}

void UArcOceanComponent::PushSurfacePlane(const FArcOceanViewPlane& InViewPlane)
{
	UWorld* World = GetWorld();
	if (World == nullptr || WaterParameterCollection == nullptr)
	{
		return;
	}

	UMaterialParameterCollectionInstance* Collection = World->GetParameterCollectionInstance(WaterParameterCollection);
	if (Collection == nullptr)
	{
		return;
	}

	Collection->SetVectorParameterValue(
		ArcOceanWaterParams::WaterPlanePoint,
		FLinearColor(
			static_cast<float>(InViewPlane.PlanePoint.X),
			static_cast<float>(InViewPlane.PlanePoint.Y),
			static_cast<float>(InViewPlane.PlanePoint.Z),
			0.0f));
	Collection->SetVectorParameterValue(
		ArcOceanWaterParams::WaterPlaneNormal,
		FLinearColor(InViewPlane.PlaneNormal.X, InViewPlane.PlaneNormal.Y, InViewPlane.PlaneNormal.Z, 0.0f));
	Collection->SetScalarParameterValue(ArcOceanWaterParams::NearPlaneCm, InViewPlane.NearPlaneCm);
	Collection->SetScalarParameterValue(ArcOceanWaterParams::MaskBandHalfCm, InViewPlane.MaskBandHalfCm);
}

void UArcOceanComponent::SetUnderwaterPostProcess(UPostProcessComponent* InPostProcess)
{
	UnderwaterPostProcess = InPostProcess;
	RefreshUnderwaterBlendable();
}

void UArcOceanComponent::RefreshUnderwaterBlendable()
{
	if (UnderwaterPostProcess == nullptr)
	{
		return;
	}

	UnderwaterPostProcess->Settings.WeightedBlendables.Array.Reset();
	if (UnderwaterPostProcessMaterial != nullptr)
	{
		UnderwaterPostProcess->AddOrUpdateBlendable(UnderwaterPostProcessMaterial, 1.0f);
	}
}

void UArcOceanComponent::UpdateUnderwaterCull(const FVector& InViewLocation, const FArcOceanViewPlane& InViewPlane)
{
	if (UnderwaterPostProcess == nullptr)
	{
		return;
	}

	// A cull, not the mask: it only skips the full-screen pass when no near-plane point can reach the surface.
	const float NearPlaneRadiusScale = FMath::Sqrt(1.0f + InViewPlane.TanHalfFov.X * InViewPlane.TanHalfFov.X + InViewPlane.TanHalfFov.Y * InViewPlane.TanHalfFov.Y);
	const float InverseNormalZ = FMath::Sqrt(1.0f + InViewPlane.SlopeMagnitude * InViewPlane.SlopeMagnitude);
	const double MarginCm = static_cast<double>(InViewPlane.NearPlaneCm * NearPlaneRadiusScale * InverseNormalZ + InViewPlane.MaskBandHalfCm);
	UnderwaterPostProcess->bEnabled = InViewLocation.Z <= InViewPlane.PlanePoint.Z + MarginCm;
}

void UArcOceanComponent::HandleSeabedBeginPostProcessSettings(FVector InViewLocation, FSceneView* InView)
{
	if (InView == nullptr || InView->bIsSceneCapture || InView->bIsSceneCaptureCube
		|| InView->bIsReflectionCapture || InView->bIsPlanarReflection)
	{
		return;
	}

	if (!InView->ViewMatrices.IsPerspectiveProjection())
	{
		return;
	}

	PushSeabedSnapshot(*InView);
}

void UArcOceanComponent::HandleBeginPostProcessSettings(FVector InViewLocation, FSceneView* InView)
{
	if (InView == nullptr || InView->bIsSceneCapture || InView->bIsSceneCaptureCube
		|| InView->bIsReflectionCapture || InView->bIsPlanarReflection
		|| (WaterParameterCollection == nullptr && UnderwaterPostProcess == nullptr))
	{
		return;
	}

	if (!InView->ViewMatrices.IsPerspectiveProjection())
	{
		return;
	}

	const FArcOceanViewPlane ViewPlane = SolveViewPlane(*InView);
	PushSurfacePlane(ViewPlane);
	UpdateUnderwaterCull(InViewLocation, ViewPlane);
}

#if WITH_EDITOR

void UArcOceanComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	if (MemberName == GET_MEMBER_NAME_CHECKED(UArcOceanComponent, WaveSettings))
	{
		ReportClampedWaveSettings();
		PushWaveParams();
		return;
	}

	if (MemberName == GET_MEMBER_NAME_CHECKED(UArcOceanComponent, WaterParameterCollection)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UArcOceanComponent, AbsorptionTint)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UArcOceanComponent, ClarityDistanceMeters)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UArcOceanComponent, ScatterAlbedo))
	{
		PushClarity();
		return;
	}

	if (MemberName == GET_MEMBER_NAME_CHECKED(UArcOceanComponent, UnderwaterPostProcessMaterial))
	{
		RefreshUnderwaterBlendable();
		return;
	}

	MarkRenderStateDirty();
}

void UArcOceanComponent::ReportClampedWaveSettings() const
{
	FArcOceanBandLimits BandLimits;
	ArcOceanWaves::ResolveBandLimits(WaveSettings, BandLimits);

	const FArcOceanShallowBandSettings& Authored = WaveSettings.ShallowBand;

	UE_CLOGF(BandLimits.ShallowNumWaves != Authored.NumWaves, LogArcWater, Warning,"Ocean '%ls': shallow band NumWaves %d was reduced to %d (per-band cap, or the %d-wave budget shared with %d swell waves).",
		*GetPathName(), Authored.NumWaves, BandLimits.ShallowNumWaves, ArcOcean::MaxWaves, BandLimits.SwellNumWaves);

	UE_CLOGF(BandLimits.ShallowNumWaves > 0 && BandLimits.ShallowLongestWavelengthCm != Authored.LongestWavelengthCm,
		LogArcWater, Warning,"Ocean '%ls': shallow band LongestWavelengthCm %.1f is outside the 1.0..%.1f cm the swell's wavelength series leaves; %.1f cm is used.",
		*GetPathName(), Authored.LongestWavelengthCm, BandLimits.ShallowCeilingCm, BandLimits.ShallowLongestWavelengthCm);

	UE_CLOGF(BandLimits.ShallowNumWaves > 0 && BandLimits.ShallowSteepness != Authored.Steepness, LogArcWater, Warning,"Ocean '%ls': shallow band Steepness %.3f plus the swell's %.3f exceeds the %.3f steepness sum budget; %.3f is used.",
		*GetPathName(), Authored.Steepness, BandLimits.SwellSteepness, ArcOcean::ARC_OCEAN_MAX_STEEPNESS_SUM,
		BandLimits.ShallowSteepness);
}

#endif // WITH_EDITOR
