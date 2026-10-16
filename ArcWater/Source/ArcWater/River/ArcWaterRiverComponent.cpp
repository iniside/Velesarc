// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverComponent.h"

#include "ArcWaterModule.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "SceneView.h"

#include "River/ArcWaterRiverImmersion.h"
#include "River/ArcWaterRiverMesh.h"
#include "River/ArcWaterRiverModifier.h"
#include "River/ArcWaterRiverSceneProxy.h"

namespace ArcWaterRiverComponentPrivate
{
	constexpr float FallbackHalfWidthCm = 300.0f;
	constexpr float FallbackFlowSpeedCmS = 150.0f;

	struct FNewPointValues
	{
		float HalfWidthCm = FallbackHalfWidthCm;
		float FlowSpeedCmS = FallbackFlowSpeedCmS;
	};

	FNewPointValues MakeNewPointValues(const UArcWaterRiverComponent& River)
	{
		FNewPointValues Values;
		Values.HalfWidthCm = River.NewPointHalfWidthCm;
		Values.FlowSpeedCmS = River.NewPointFlowSpeedCmS;
		return Values;
	}

	FNewPointValues ResolveNewPointValues(const UObject& Metadata)
	{
		const UArcWaterRiverComponent* const River = Metadata.GetTypedOuter<UArcWaterRiverComponent>();
		return River != nullptr ? MakeNewPointValues(*River) : FNewPointValues();
	}

	void ReindexInputKeys(FInterpCurveFloat& Curve)
	{
		for (int32 PointIndex = 0; PointIndex < Curve.Points.Num(); ++PointIndex)
		{
			Curve.Points[PointIndex].InVal = static_cast<float>(PointIndex);
		}
	}

	void InsertInterpolated(FInterpCurveFloat& Curve, const int32 Index, const float t, const bool bClosedLoop)
	{
		if (!Curve.Points.IsValidIndex(Index))
		{
			return;
		}

		const int32 NumPoints = Curve.Points.Num();
		const int32 PrevIndex = (bClosedLoop && Index == 0) ? NumPoints - 1 : Index - 1;
		float NewValue = Curve.Points[Index].OutVal;
		if (PrevIndex >= 0 && PrevIndex < NumPoints)
		{
			NewValue = FMath::LerpStable(Curve.Points[PrevIndex].OutVal, NewValue, t);
		}

		Curve.Points.Insert(FInterpCurvePoint<float>(static_cast<float>(Index), NewValue), Index);
		ReindexInputKeys(Curve);
	}

	void UpdateInterpolated(FInterpCurveFloat& Curve, const int32 Index, const float t, const bool bClosedLoop)
	{
		const int32 NumPoints = Curve.Points.Num();
		if (Index < 0 || Index >= NumPoints)
		{
			return;
		}

		const int32 PrevIndex = (bClosedLoop && Index == 0) ? NumPoints - 1 : Index - 1;
		const int32 NextIndex = (bClosedLoop && Index + 1 >= NumPoints) ? 0 : Index + 1;
		if (PrevIndex < 0 || PrevIndex >= NumPoints || NextIndex < 0 || NextIndex >= NumPoints)
		{
			return;
		}

		Curve.Points[Index].OutVal = FMath::LerpStable(Curve.Points[PrevIndex].OutVal, Curve.Points[NextIndex].OutVal, t);
	}

	void AppendLastOrDefault(FInterpCurveFloat& Curve, const float DefaultValue)
	{
		const int32 LastIndex = Curve.Points.Num() - 1;
		const float NewValue = LastIndex >= 0 ? Curve.Points[LastIndex].OutVal : DefaultValue;
		Curve.Points.Emplace(static_cast<float>(LastIndex + 1), NewValue);
	}

	void DuplicateAt(FInterpCurveFloat& Curve, const int32 Index)
	{
		if (!Curve.Points.IsValidIndex(Index))
		{
			return;
		}

		Curve.Points.Insert(FInterpCurvePoint<float>(Curve.Points[Index]), Index);
		ReindexInputKeys(Curve);
	}

	void RemoveAt(FInterpCurveFloat& Curve, const int32 Index)
	{
		if (!Curve.Points.IsValidIndex(Index))
		{
			return;
		}

		Curve.Points.RemoveAt(Index);
		ReindexInputKeys(Curve);
	}

	void CopyValue(const FInterpCurveFloat& From, FInterpCurveFloat& To, const int32 FromIndex, const int32 ToIndex)
	{
		if (!From.Points.IsValidIndex(FromIndex) || !To.Points.IsValidIndex(ToIndex))
		{
			return;
		}

		To.Points[ToIndex].OutVal = From.Points[FromIndex].OutVal;
	}

	void FixupCurve(FInterpCurveFloat& Curve, const int32 NumPoints, const float DefaultValue)
	{
		ReindexInputKeys(Curve);
		while (Curve.Points.Num() < NumPoints)
		{
			Curve.Points.Emplace(static_cast<float>(Curve.Points.Num()), DefaultValue);
		}
		if (Curve.Points.Num() > NumPoints)
		{
			Curve.Points.RemoveAt(NumPoints, Curve.Points.Num() - NumPoints);
		}
	}

	float EvalOrDefault(const FInterpCurveFloat& Curve, const float Key, const float DefaultValue)
	{
		return Curve.Points.Num() > 0 ? Curve.Eval(Key, DefaultValue) : DefaultValue;
	}

	UMaterialParameterCollectionInstance* ResolveCollection(UWorld* const World, UMaterialParameterCollection* const Collection)
	{
		return World != nullptr && Collection != nullptr ? World->GetParameterCollectionInstance(Collection) : nullptr;
	}

	void WarnIfMaterialUnusable(const UMaterialInterface* const Material)
	{
		UE_CLOGF(Material != nullptr && !Material->CheckMaterialUsage_Concurrent(MATUSAGE_Water),
			LogArcWater, Warning,"River material '%ls' is not usable with water; the river falls back to the default surface material.",
			*Material->GetPathName());
	}
}

UArcWaterRiverMetadata::UArcWaterRiverMetadata(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcWaterRiverMetadata::InsertPoint(const int32 Index, const float t, const bool bClosedLoop)
{
	check(Index >= 0);

	if (Index >= HalfWidth.Points.Num())
	{
		AddPoint(static_cast<float>(Index));
		return;
	}

	Modify();

	ArcWaterRiverComponentPrivate::InsertInterpolated(HalfWidth, Index, t, bClosedLoop);
	ArcWaterRiverComponentPrivate::InsertInterpolated(FlowSpeed, Index, t, bClosedLoop);
}

void UArcWaterRiverMetadata::UpdatePoint(const int32 Index, const float t, const bool bClosedLoop)
{
	if (!HalfWidth.Points.IsValidIndex(Index))
	{
		return;
	}

	Modify();

	ArcWaterRiverComponentPrivate::UpdateInterpolated(HalfWidth, Index, t, bClosedLoop);
	ArcWaterRiverComponentPrivate::UpdateInterpolated(FlowSpeed, Index, t, bClosedLoop);
}

void UArcWaterRiverMetadata::AddPoint(const float InputKey)
{
	Modify();

	const ArcWaterRiverComponentPrivate::FNewPointValues NewPoint =
		ArcWaterRiverComponentPrivate::ResolveNewPointValues(*this);

	ArcWaterRiverComponentPrivate::AppendLastOrDefault(HalfWidth, NewPoint.HalfWidthCm);
	ArcWaterRiverComponentPrivate::AppendLastOrDefault(FlowSpeed, NewPoint.FlowSpeedCmS);
}

void UArcWaterRiverMetadata::RemovePoint(const int32 Index)
{
	if (!HalfWidth.Points.IsValidIndex(Index))
	{
		return;
	}

	Modify();

	ArcWaterRiverComponentPrivate::RemoveAt(HalfWidth, Index);
	ArcWaterRiverComponentPrivate::RemoveAt(FlowSpeed, Index);
}

void UArcWaterRiverMetadata::DuplicatePoint(const int32 Index)
{
	if (!HalfWidth.Points.IsValidIndex(Index))
	{
		return;
	}

	Modify();

	ArcWaterRiverComponentPrivate::DuplicateAt(HalfWidth, Index);
	ArcWaterRiverComponentPrivate::DuplicateAt(FlowSpeed, Index);
}

void UArcWaterRiverMetadata::CopyPoint(const USplineMetadata* FromSplineMetadata, const int32 FromIndex, const int32 ToIndex)
{
	const UArcWaterRiverMetadata* const From = Cast<UArcWaterRiverMetadata>(FromSplineMetadata);
	if (From == nullptr)
	{
		return;
	}

	Modify();

	ArcWaterRiverComponentPrivate::CopyValue(From->HalfWidth, HalfWidth, FromIndex, ToIndex);
	ArcWaterRiverComponentPrivate::CopyValue(From->FlowSpeed, FlowSpeed, FromIndex, ToIndex);
}

void UArcWaterRiverMetadata::Reset(const int32 NumPoints)
{
	Modify();

	HalfWidth.Points.Reset(NumPoints);
	FlowSpeed.Points.Reset(NumPoints);
}

void UArcWaterRiverMetadata::Fixup(const int32 NumPoints, USplineComponent* SplineComp)
{
	const UArcWaterRiverComponent* const River = Cast<UArcWaterRiverComponent>(SplineComp);
	const ArcWaterRiverComponentPrivate::FNewPointValues NewPoint = River != nullptr
		? ArcWaterRiverComponentPrivate::MakeNewPointValues(*River)
		: ArcWaterRiverComponentPrivate::ResolveNewPointValues(*this);

	ArcWaterRiverComponentPrivate::FixupCurve(HalfWidth, NumPoints, NewPoint.HalfWidthCm);
	ArcWaterRiverComponentPrivate::FixupCurve(FlowSpeed, NumPoints, NewPoint.FlowSpeedCmS);
}

#if WITH_EDITOR

void UArcWaterRiverMetadata::PostEditUndo()
{
	Super::PostEditUndo();

	UArcWaterRiverComponent* const OwningRiver = GetTypedOuter<UArcWaterRiverComponent>();
	if (OwningRiver != nullptr)
	{
		OwningRiver->UpdateSpline();
	}
}

#endif

UArcWaterRiverComponent::UArcWaterRiverComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RiverMetadata = ObjectInitializer.CreateDefaultSubobject<UArcWaterRiverMetadata>(this, TEXT("RiverMetadata"));
	PrimaryComponentTick.bCanEverTick = false;
	CastShadow = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
}

float UArcWaterRiverComponent::GetHalfWidthAtKey(const float Key) const
{
	return RiverMetadata != nullptr
		? ArcWaterRiverComponentPrivate::EvalOrDefault(RiverMetadata->HalfWidth, Key, NewPointHalfWidthCm)
		: NewPointHalfWidthCm;
}

float UArcWaterRiverComponent::GetFlowSpeedAtKey(const float Key) const
{
	return RiverMetadata != nullptr
		? ArcWaterRiverComponentPrivate::EvalOrDefault(RiverMetadata->FlowSpeed, Key, NewPointFlowSpeedCmS)
		: NewPointFlowSpeedCmS;
}

void UArcWaterRiverComponent::SetUnderwaterPostProcess(UPostProcessComponent* InPostProcess)
{
	UnderwaterPostProcess = InPostProcess;
	RefreshUnderwaterBlendable();
}

void UArcWaterRiverComponent::SetGeneratedChannel(
	TSharedRef<const FArcWaterRiverGeneratedData> InGeneratedData, int32 InReachIndex)
{
	GeneratedChannelData = InGeneratedData;
	GeneratedReachIndex = InReachIndex;
	GeneratedSource = Cast<UArcWaterRiverModifier>(GetAttachParent());
	RegenerationNotice = TEXT("Rebuild Rivers replaces edits made on this generated component.");
	InvalidateRiverGeometry();
}

const FArcWaterRiverGeneratedData* UArcWaterRiverComponent::GetGeneratedChannelData() const
{
	return GeneratedChannelData.Get();
}

void UArcWaterRiverComponent::PushClarity()
{
	UMaterialParameterCollectionInstance* const Collection =
		ArcWaterRiverComponentPrivate::ResolveCollection(GetWorld(), RiverParameterCollection);
	if (Collection == nullptr)
	{
		return;
	}

	ArcWaterRiverImmersion::PushClarity(*Collection,
		ArcWaterRiverImmersion::DeriveClarity(AbsorptionTint, ClarityDistanceMeters, ScatterAlbedo));
}

FPrimitiveSceneProxy* UArcWaterRiverComponent::CreateSceneProxy()
{
	TArray<FDynamicMeshVertex> Vertices;
	TArray<uint32> Indices;
	ArcWaterRiverMesh::BuildRibbon(*this, Vertices, Indices);

	if (Vertices.IsEmpty() || Indices.IsEmpty())
	{
		return nullptr;
	}

	return new FArcWaterRiverSceneProxy(this, MoveTemp(Vertices), MoveTemp(Indices));
}

FBoxSphereBounds UArcWaterRiverComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!bLocalBoundsValid)
	{
		CachedLocalBounds = ArcWaterRiverMesh::ComputeLocalBounds(*this);
		bLocalBoundsValid = true;
	}

	if (!CachedLocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
	}

	return FBoxSphereBounds(CachedLocalBounds).TransformBy(LocalToWorld);
}

int32 UArcWaterRiverComponent::GetNumMaterials() const
{
	return 1;
}

UMaterialInterface* UArcWaterRiverComponent::GetMaterial(const int32 ElementIndex) const
{
	return ElementIndex == 0 ? RiverMaterial : nullptr;
}

void UArcWaterRiverComponent::SetMaterial(const int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex != 0 || RiverMaterial == Material)
	{
		return;
	}

	RiverMaterial = Material;
	ArcWaterRiverComponentPrivate::WarnIfMaterialUnusable(RiverMaterial);
	MarkRenderStateDirty();
}

void UArcWaterRiverComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, const bool bGetDebugMaterials) const
{
	if (RiverMaterial != nullptr)
	{
		OutMaterials.Add(RiverMaterial);
	}
}

USplineMetadata* UArcWaterRiverComponent::GetSplinePointsMetadata()
{
	return RiverMetadata;
}

const USplineMetadata* UArcWaterRiverComponent::GetSplinePointsMetadata() const
{
	return RiverMetadata;
}

void UArcWaterRiverComponent::PostLoad()
{
	Super::PostLoad();
	SynchronizeMetadata();
	PushClarity();
}

void UArcWaterRiverComponent::OnRegister()
{
	Super::OnRegister();
	SynchronizeMetadata();

	if (!SplineChangedHandle.IsValid())
	{
		SplineChangedHandle = OnSplineChanged.AddUObject(this, &UArcWaterRiverComponent::HandleOwnSplineChanged);
	}

	UWorld* const World = GetWorld();
	if (World != nullptr && !BeginPostProcessSettingsHandle.IsValid())
	{
		BeginPostProcessSettingsHandle = World->OnBeginPostProcessSettings.AddUObject(
			this, &UArcWaterRiverComponent::HandleBeginPostProcessSettings);
	}

	PushClarity();
	RefreshUnderwaterBlendable();
}

void UArcWaterRiverComponent::OnUnregister()
{
	if (SplineChangedHandle.IsValid())
	{
		OnSplineChanged.Remove(SplineChangedHandle);
		SplineChangedHandle.Reset();
	}

	UWorld* const World = GetWorld();
	if (World != nullptr && BeginPostProcessSettingsHandle.IsValid())
	{
		World->OnBeginPostProcessSettings.Remove(BeginPostProcessSettingsHandle);
	}
	BeginPostProcessSettingsHandle.Reset();

	Super::OnUnregister();
}

#if WITH_EDITOR

void UArcWaterRiverComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SynchronizeMetadata();
	InvalidateRiverGeometry();

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	if (MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverComponent, RiverMaterial))
	{
		ArcWaterRiverComponentPrivate::WarnIfMaterialUnusable(RiverMaterial);
		return;
	}

	if (MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverComponent, UnderwaterMaterial))
	{
		RefreshUnderwaterBlendable();
		return;
	}

	if (MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverComponent, RiverParameterCollection)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverComponent, AbsorptionTint)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverComponent, ClarityDistanceMeters)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverComponent, ScatterAlbedo))
	{
		PushClarity();
	}
}

void UArcWaterRiverComponent::PostEditImport()
{
	Super::PostEditImport();
	SynchronizeMetadata();
}

#endif

void UArcWaterRiverComponent::SynchronizeMetadata()
{
	if (RiverMetadata != nullptr)
	{
		RiverMetadata->Fixup(GetNumberOfSplinePoints(), this);
	}
}

void UArcWaterRiverComponent::HandleOwnSplineChanged()
{
	InvalidateRiverGeometry();
}

void UArcWaterRiverComponent::InvalidateRiverGeometry()
{
	bLocalBoundsValid = false;
	MarkRenderStateDirty();
	UpdateBounds();
}

void UArcWaterRiverComponent::RefreshUnderwaterBlendable()
{
	if (UnderwaterPostProcess == nullptr)
	{
		return;
	}

	// The blendable holds UnderwaterMaterial itself: a dynamic instance would serialize into the .umap through Settings.WeightedBlendables, so the per-view values ride RiverParameterCollection instead.
	UnderwaterPostProcess->Settings.WeightedBlendables.Array.Reset();
	if (UnderwaterMaterial != nullptr)
	{
		UnderwaterPostProcess->AddOrUpdateBlendable(UnderwaterMaterial, 1.0f);
	}
}

void UArcWaterRiverComponent::SetUnderwaterEnabled(const bool bInEnabled)
{
	if (UnderwaterPostProcess != nullptr)
	{
		UnderwaterPostProcess->bEnabled = bInEnabled;
	}
}

void UArcWaterRiverComponent::HandleBeginPostProcessSettings(FVector ViewLocation, FSceneView* View)
{
	if (View == nullptr || (RiverParameterCollection == nullptr && UnderwaterPostProcess == nullptr))
	{
		return;
	}

	if (!View->ViewMatrices.IsPerspectiveProjection())
	{
		return;
	}

	const ArcWaterRiverImmersion::FViewGeometry ViewGeometry = ArcWaterRiverImmersion::SolveViewGeometry(*View);
	if (!ArcWaterRiverImmersion::IsNearCorridorXY(*this, ViewLocation, ViewGeometry.NearPlaneRadiusCm))
	{
		SetUnderwaterEnabled(false);
		return;
	}

	ArcWaterRiverImmersion::FCorridorSample Sample;
	ArcWaterRiverImmersion::SolveCorridor(*this, ViewLocation, Sample);

	const ArcWaterRiverImmersion::FClarity Clarity =
		ArcWaterRiverImmersion::DeriveClarity(AbsorptionTint, ClarityDistanceMeters, ScatterAlbedo);
	CachedMaskReachCm = ArcWaterRiverImmersion::DeriveMaskReachCm(
		Clarity, CachedMinCurvatureRadiusCm, Sample.HalfWidthCm, GetSplineLength());
	Sample.ReachCm = CachedMaskReachCm;

	const ArcWaterRiverImmersion::FViewMargin Margin =
		ArcWaterRiverImmersion::DeriveViewMargin(ViewGeometry, Sample.SurfaceNormal);

	const bool bImmersed = ArcWaterRiverImmersion::IsViewImmersed(*this, ViewLocation, Sample, Margin.MarginCm);
	if (bImmersed)
	{
		UMaterialParameterCollectionInstance* const Collection =
			ArcWaterRiverComponentPrivate::ResolveCollection(GetWorld(), RiverParameterCollection);
		if (Collection != nullptr)
		{
			ArcWaterRiverImmersion::PushCorridor(*Collection, Sample, ViewGeometry, Margin);
		}
	}

	SetUnderwaterEnabled(bImmersed);
}
