// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainMeshModifier.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainModule.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "IndexTypes.h"

#if WITH_EDITOR
#include "ArcTerrainSubsystem.h"
#include "Hash/Blake3.h"
#include "Spatial/MeshAABBTree3.h"
#include "StaticMeshLODResourcesAdapter.h"
#include "StaticMeshResources.h"
#endif

namespace ArcTerrainMeshModifier_Local
{
	TFunction<bool(TArray<float>&, FIntPoint&)> MakeProvider(const FArcTerrainBakedField& Field)
	{
		FArcTerrainBakedField FieldCopy = Field;
		return [FieldCopy](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
		{
			if (!FieldCopy.IsValid())
			{
				return false;
			}
			OutValues = FieldCopy.Values;
			OutDims = FieldCopy.Dims;
			return true;
		};
	}

	constexpr double GMaxTiltDegrees = 0.1;

	bool BuildCollarGeometry(const FArcTerrainCollarMesh& Source, UE::Geometry::FDynamicMesh3& OutMesh)
	{
		OutMesh.Clear();
		if (!Source.IsValid())
		{
			return false;
		}

		OutMesh.EnableAttributes();
		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = OutMesh.Attributes()->PrimaryNormals();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = OutMesh.Attributes()->PrimaryUV();
		if (NormalOverlay == nullptr || UVOverlay == nullptr)
		{
			OutMesh.Clear();
			return false;
		}

		const int32 SourceVertexCount = Source.Positions.Num();
		TArray<int32> VertexOfSource;
		TArray<int32> NormalOfSource;
		TArray<int32> UVOfSource;
		VertexOfSource.SetNumUninitialized(SourceVertexCount);
		NormalOfSource.SetNumUninitialized(SourceVertexCount);
		UVOfSource.SetNumUninitialized(SourceVertexCount);
		for (int32 SourceIndex = 0; SourceIndex < SourceVertexCount; ++SourceIndex)
		{
			VertexOfSource[SourceIndex] = OutMesh.AppendVertex(FVector3d(Source.Positions[SourceIndex]));
			NormalOfSource[SourceIndex] = NormalOverlay->AppendElement(Source.Normals[SourceIndex]);
			UVOfSource[SourceIndex] = UVOverlay->AppendElement(Source.UVs[SourceIndex]);
		}

		for (int32 IndexBase = 0; IndexBase + 2 < Source.Indices.Num(); IndexBase += 3)
		{
			const int32 CornerA = Source.Indices[IndexBase];
			const int32 CornerB = Source.Indices[IndexBase + 1];
			const int32 CornerC = Source.Indices[IndexBase + 2];
			if (!VertexOfSource.IsValidIndex(CornerA) || !VertexOfSource.IsValidIndex(CornerB)
				|| !VertexOfSource.IsValidIndex(CornerC)
				|| CornerA == CornerB || CornerB == CornerC || CornerA == CornerC)
			{
				continue;
			}

			const int32 TriangleId = OutMesh.AppendTriangle(UE::Geometry::FIndex3i(
				VertexOfSource[CornerA], VertexOfSource[CornerB], VertexOfSource[CornerC]));
			if (TriangleId < 0)
			{
				continue;
			}

			NormalOverlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(
				NormalOfSource[CornerA], NormalOfSource[CornerB], NormalOfSource[CornerC]));
			UVOverlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(
				UVOfSource[CornerA], UVOfSource[CornerB], UVOfSource[CornerC]));
		}

		if (OutMesh.TriangleCount() == 0)
		{
			OutMesh.Clear();
			return false;
		}
		return true;
	}

#if WITH_EDITOR
	constexpr int32 GMaxBedTexels = 512 * 512;

	FIntPoint ComputeBedDims(const FVector2D& Coverage, double TexelSizeCm)
	{
		return FIntPoint(
			FMath::Max(FMath::CeilToInt(Coverage.X / TexelSizeCm) + 1, 2),
			FMath::Max(FMath::CeilToInt(Coverage.Y / TexelSizeCm) + 1, 2));
	}

	bool FitsBedBudget(FIntPoint Dims)
	{
		return static_cast<int64>(Dims.X) * static_cast<int64>(Dims.Y)
			<= static_cast<int64>(GMaxBedTexels);
	}

	double SuggestBedTexelSizeCm(const FVector2D& Coverage)
	{
		const double Product = Coverage.X * Coverage.Y;
		const double Sum = Coverage.X + Coverage.Y;
		double Candidate = 1.0;
		if (Product > 0.0)
		{
			const double Root =
				FMath::Sqrt(Sum * Sum + Product * (static_cast<double>(GMaxBedTexels) - 4.0));
			const double InverseTexel = (Root - Sum) / Product;
			if (InverseTexel > 0.0)
			{
				Candidate = FMath::Max(FMath::CeilToDouble(1.0 / InverseTexel), 1.0);
			}
		}

		for (int32 Attempt = 0; Attempt < 64 && !FitsBedBudget(ComputeBedDims(Coverage, Candidate)); ++Attempt)
		{
			Candidate += 1.0;
		}
		return Candidate;
	}

	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	FGuid DigestToGuid(const FBlake3Hash& Digest)
	{
		uint32 Words[4] = { 0, 0, 0, 0 };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}

	FGuid ComputeBedContentId(const FStaticMeshLODResourcesMeshAdapter& Adapter, const FString& MeshPath,
		FIntPoint Dims, const FVector2D& Coverage, float TexelSizeCm, float MaxRiseCm, int32 SamplesPerSide)
	{
		static const FGuid FormatVersion(0x4E1D7A63, 0x1B4C4F02, 0xA95E37D8, 0x0C6B2411);

		FBlake3 Hash;
		HashPod(Hash, FormatVersion.A);
		HashPod(Hash, FormatVersion.B);
		HashPod(Hash, FormatVersion.C);
		HashPod(Hash, FormatVersion.D);
		Hash.Update(*MeshPath, MeshPath.Len() * sizeof(TCHAR));

		const int32 TriangleCount = Adapter.TriangleCount();
		const int32 VertexCount = Adapter.VertexCount();
		HashPod(Hash, TriangleCount);
		HashPod(Hash, VertexCount);
		for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			const FVector3f Position = FVector3f(Adapter.GetVertex(VertexIndex));
			HashPod(Hash, Position.X);
			HashPod(Hash, Position.Y);
			HashPod(Hash, Position.Z);
		}
		for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			const UE::Geometry::FIndex3i Triangle = Adapter.GetTriangle(TriangleIndex);
			HashPod(Hash, Triangle.A);
			HashPod(Hash, Triangle.B);
			HashPod(Hash, Triangle.C);
		}

		HashPod(Hash, Dims.X);
		HashPod(Hash, Dims.Y);
		HashPod(Hash, Coverage.X);
		HashPod(Hash, Coverage.Y);
		HashPod(Hash, TexelSizeCm);
		HashPod(Hash, MaxRiseCm);
		HashPod(Hash, SamplesPerSide);
		return DigestToGuid(Hash.Finalize());
	}

	/** A texel with no accepted sample takes the nearest accepted texel's value rather than a sentinel:
	 *  the composite reconstructs this raster with a bicubic Catmull-Rom whose negative lobes would
	 *  turn any out-of-band filler into a spike two texels inside the footprint. */
	void ExtrapolateHoles(TArray<float>& Values, TArray<uint8>& bHasValue, FIntPoint Dims)
	{
		TArray<int32> Queue;
		Queue.Reserve(Values.Num());
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (bHasValue[Index] != 0)
			{
				Queue.Add(Index);
			}
		}

		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const int32 Index = Queue[Head++];
			const int32 TexelX = Index % Dims.X;
			const int32 TexelY = Index / Dims.X;
			const float Value = Values[Index];

			const int32 NeighbourX[4] = { TexelX - 1, TexelX + 1, TexelX, TexelX };
			const int32 NeighbourY[4] = { TexelY, TexelY, TexelY - 1, TexelY + 1 };
			for (int32 Side = 0; Side < 4; ++Side)
			{
				if (NeighbourX[Side] < 0 || NeighbourX[Side] >= Dims.X
					|| NeighbourY[Side] < 0 || NeighbourY[Side] >= Dims.Y)
				{
					continue;
				}
				const int32 NeighbourIndex = NeighbourY[Side] * Dims.X + NeighbourX[Side];
				if (bHasValue[NeighbourIndex] != 0)
				{
					continue;
				}
				bHasValue[NeighbourIndex] = 1;
				Values[NeighbourIndex] = Value;
				Queue.Add(NeighbourIndex);
			}
		}
	}
#endif // WITH_EDITOR
}

UArcTerrainMeshModifier::UArcTerrainMeshModifier()
{
	Priority = 50.0;
	BlendMode = EArcTerrainBlendMode::Max;
}

FBox UArcTerrainMeshModifier::ComputeBounds() const
{
	const AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain == nullptr || Mesh == nullptr)
	{
		return FBox(ForceInit);
	}

	const FBox MeshLocalBounds = Mesh->GetBoundingBox();
	if (!MeshLocalBounds.IsValid || MeshLocalBounds.GetExtent().IsNearlyZero())
	{
		return FBox(ForceInit);
	}

	const FVector2D Half = CoverageXY * 0.5;
	if (Half.X <= 0.0 || Half.Y <= 0.0)
	{
		return FBox(ForceInit);
	}

	const FBox LocalFootprint(
		FVector(-Half.X, -Half.Y, MeshLocalBounds.Min.Z),
		FVector(Half.X, Half.Y, MeshLocalBounds.Max.Z));

	const FTransform ModifierToTerrain =
		GetComponentTransform().GetRelativeTransform(Terrain->GetTerrainTransform());
	return LocalFootprint.TransformBy(ModifierToTerrain);
}

bool UArcTerrainMeshModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	OutDesc.Kind = EArcStampSourceKind::None;

	if (!BedField.IsValid() || !IsRasterPoseSupported())
	{
		return true;
	}

	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = BedField.ContentId;
	OutDesc.SourceDims = BedField.Dims;
	OutDesc.BakedProvider = ArcTerrainMeshModifier_Local::MakeProvider(BedField);
	OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
	OutDesc.EncodingScale = 1.0f;
	OutDesc.ZeroInEncoding = 0.0f;

	OutDesc.EdgeFalloffBand = EdgeFalloffBand;
	OutDesc.EdgeSharpness = EdgeSharpness;
	OutDesc.CornerRadius = CornerRadius;
	OutDesc.bCircleMask = bCircleMask;
	OutDesc.CircleCenterOffset = CircleCenterOffset;
	OutDesc.CircleRadius = CircleRadius;
	OutDesc.CircleFalloffBand = CircleFalloffBand;
	return true;
}

bool UArcTerrainMeshModifier::IsRasterPoseSupported() const
{
	const AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain == nullptr)
	{
		return false;
	}

	const FRotator ModifierToTerrainRotation =
		GetComponentTransform().GetRelativeTransform(Terrain->GetTerrainTransform()).Rotator();
	return FMath::Abs(ModifierToTerrainRotation.Pitch) <= ArcTerrainMeshModifier_Local::GMaxTiltDegrees
		&& FMath::Abs(ModifierToTerrainRotation.Roll) <= ArcTerrainMeshModifier_Local::GMaxTiltDegrees;
}

void UArcTerrainMeshModifier::UpdateDerivedCoverage()
{
	const FBox MeshLocalBounds = Mesh != nullptr ? Mesh->GetBoundingBox() : FBox(ForceInit);
	if (!MeshLocalBounds.IsValid || MeshLocalBounds.GetExtent().IsNearlyZero())
	{
		CoverageXY = FVector2D(1.0, 1.0);
		EdgeFalloffBand = 0.0f;
		return;
	}

	const double HalfBoxX =
		FMath::Max(FMath::Abs(MeshLocalBounds.Min.X), FMath::Abs(MeshLocalBounds.Max.X));
	const double HalfBoxY =
		FMath::Max(FMath::Abs(MeshLocalBounds.Min.Y), FMath::Abs(MeshLocalBounds.Max.Y));
	const double Band = 0.25 * FMath::Min(HalfBoxX, HalfBoxY);

	CoverageXY = FVector2D(2.0 * (HalfBoxX + Band), 2.0 * (HalfBoxY + Band));
	EdgeFalloffBand = static_cast<float>(Band);
}

void UArcTerrainMeshModifier::SyncVisibleMesh()
{
	AActor* Owner = GetOwner();
	if (Mesh == nullptr || bIsDisabled || Owner == nullptr || !IsRegistered())
	{
		DestroyVisibleMesh();
		return;
	}

	if (!IsValid(VisibleMesh.Get()))
	{
		VisibleMesh = NewObject<UStaticMeshComponent>(
			Owner, NAME_None, RF_Transient | RF_TextExportTransient | RF_DuplicateTransient);
		VisibleMesh->CreationMethod = CreationMethod;
		VisibleMesh->Mobility = Mobility;
		// The per-patch bodies SyncPatchMeshModifierCollision builds already carry this mesh's collision;
		// a second body here would report a second hit for every trace through the contact band.
		VisibleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VisibleMesh->SetGenerateOverlapEvents(false);
		VisibleMesh->SetStaticMesh(Mesh);
		VisibleMesh->SetMaterial(0, MaterialOverride);
		VisibleMesh->SetupAttachment(this);
		VisibleMesh->RegisterComponent();
		return;
	}

	VisibleMesh->SetStaticMesh(Mesh);
	VisibleMesh->SetMaterial(0, MaterialOverride);
}

void UArcTerrainMeshModifier::DestroyVisibleMesh()
{
	UStaticMeshComponent* Component = VisibleMesh.Get();
	VisibleMesh = nullptr;
	if (IsValid(Component))
	{
		Component->DestroyComponent();
	}
}

void UArcTerrainMeshModifier::SyncCollarMesh()
{
	AActor* Owner = GetOwner();
	UE::Geometry::FDynamicMesh3 CollarGeometry;
	if (bIsDisabled || Owner == nullptr || !IsRegistered()
		|| !ArcTerrainMeshModifier_Local::BuildCollarGeometry(CollarMesh, CollarGeometry))
	{
		DestroyCollarMesh();
		return;
	}

	if (!IsValid(Collar.Get()))
	{
		Collar = NewObject<UDynamicMeshComponent>(
			Owner, NAME_None, RF_Transient | RF_TextExportTransient | RF_DuplicateTransient);
		Collar->CreationMethod = CreationMethod;
		Collar->Mobility = Mobility;
		// The per-patch bodies SyncPatchMeshModifierCollision builds already carry this modifier's
		// collision; a second body here would report a second hit for every trace through the contact band.
		Collar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Collar->SetGenerateOverlapEvents(false);
		Collar->SetupAttachment(this);
		Collar->RegisterComponent();
	}

	Collar->SetMaterial(0, MaterialOverride);
	Collar->SetMesh(MoveTemp(CollarGeometry));
}

void UArcTerrainMeshModifier::DestroyCollarMesh()
{
	UDynamicMeshComponent* Component = Collar.Get();
	Collar = nullptr;
	if (IsValid(Component))
	{
		Component->DestroyComponent();
	}
}

void UArcTerrainMeshModifier::OnRegister()
{
	UpdateDerivedCoverage();

#if WITH_EDITOR
	if (Mesh != nullptr && !Mesh->IsCompiling() && NeedsBedRebuild())
	{
		BuildBedField();
	}
#endif

	Super::OnRegister();

	SyncVisibleMesh();

#if WITH_EDITOR
	UpdateCollar(false);
	RebindMeshBoundsDelegate();
#endif

	SyncCollarMesh();
}

void UArcTerrainMeshModifier::OnUnregister()
{
#if WITH_EDITOR
	UnbindMeshBoundsDelegate();
#endif
	DestroyCollarMesh();
	DestroyVisibleMesh();
	Super::OnUnregister();
}

void UArcTerrainMeshModifier::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITOR
	UnbindMeshBoundsDelegate();
#endif
	DestroyCollarMesh();
	DestroyVisibleMesh();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
void UArcTerrainMeshModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName ChangedName =
		PropertyChangedEvent.Property != nullptr ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (ChangedName == GET_MEMBER_NAME_CHECKED(UArcTerrainMeshModifier, Mesh))
	{
		RebindMeshBoundsDelegate();
	}

	UpdateDerivedCoverage();

	const bool bBedInputChanged =
		ChangedName == GET_MEMBER_NAME_CHECKED(UArcTerrainMeshModifier, Mesh)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UArcTerrainMeshModifier, BedTexelSizeCm)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UArcTerrainMeshModifier, BedMaxRiseCm)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UArcTerrainMeshModifier, BedSamplesPerTexel);
	if (bBedInputChanged
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (Mesh == nullptr || !Mesh->IsCompiling()))
	{
		Modify();
		BuildBedField();
	}

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		UpdateCollar(true);
	}

	SyncVisibleMesh();
	SyncCollarMesh();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UArcTerrainMeshModifier::PostEditUndo()
{
	if (IsRegistered() && IsValidChecked(this))
	{
		RebindMeshBoundsDelegate();
	}
	else
	{
		UnbindMeshBoundsDelegate();
	}
	UpdateDerivedCoverage();
	SyncVisibleMesh();
	SyncCollarMesh();

	Super::PostEditUndo();
}

void UArcTerrainMeshModifier::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);

	if (bFinished)
	{
		UpdateCollar(true);
		SyncCollarMesh();
	}
}

bool UArcTerrainMeshModifier::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}
	if (InProperty != nullptr)
	{
		const FName PropertyName = InProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, CoverageXY)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainMeshModifier, EdgeFalloffBand))
		{
			return false;
		}
	}
	return true;
}

FGuid UArcTerrainMeshModifier::ComputeMeshSourceKey() const
{
	if (Mesh == nullptr)
	{
		return FGuid();
	}

	const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (RenderData == nullptr || RenderData->DerivedDataKey.IsEmpty())
	{
		return FGuid();
	}

	FBlake3 Hash;
	Hash.Update(*RenderData->DerivedDataKey, RenderData->DerivedDataKey.Len() * sizeof(TCHAR));
	return ArcTerrainMeshModifier_Local::DigestToGuid(Hash.Finalize());
}

FGuid UArcTerrainMeshModifier::ComputeCurrentBedContentId() const
{
	if (Mesh == nullptr)
	{
		return FGuid();
	}

	const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return FGuid();
	}

	FStaticMeshLODResourcesMeshAdapter Adapter(&RenderData->LODResources[0]);
	if (Adapter.TriangleCount() == 0 || Adapter.VertexCount() == 0)
	{
		return FGuid();
	}

	if (BedTexelSizeCm <= 0.0f || CoverageXY.X <= 0.0 || CoverageXY.Y <= 0.0)
	{
		return FGuid();
	}

	return ArcTerrainMeshModifier_Local::ComputeBedContentId(Adapter, Mesh->GetPathName(),
		ArcTerrainMeshModifier_Local::ComputeBedDims(CoverageXY, static_cast<double>(BedTexelSizeCm)),
		CoverageXY, BedTexelSizeCm, BedMaxRiseCm, FMath::Clamp(BedSamplesPerTexel, 1, 8));
}

bool UArcTerrainMeshModifier::NeedsBedRebuild() const
{
	if (!bBedFieldBuilt || !BedCoverageXY.Equals(CoverageXY, UE_KINDA_SMALL_NUMBER))
	{
		return true;
	}

	const FGuid SourceKey = ComputeMeshSourceKey();
	if (SourceKey.IsValid() || BedSourceKey.IsValid())
	{
		return BedSourceKey != SourceKey;
	}

	return BedContentKey != ComputeCurrentBedContentId();
}

void UArcTerrainMeshModifier::BuildBedField()
{
	BedField.Reset();
	bBedFieldBuilt = true;
	BedCoverageXY = CoverageXY;
	BedSourceKey = ComputeMeshSourceKey();
	BedContentKey.Invalidate();

	if (Mesh == nullptr)
	{
		return;
	}

	const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: '%ls' has no LOD0 render data; no height bed built.",
			*GetPathName(), *Mesh->GetPathName());
		return;
	}

	FStaticMeshLODResourcesMeshAdapter Adapter(&RenderData->LODResources[0]);
	if (Adapter.TriangleCount() == 0 || Adapter.VertexCount() == 0)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: LOD0 of '%ls' is not CPU-readable; no height bed built.",
			*GetPathName(), *Mesh->GetPathName());
		return;
	}

	if (BedTexelSizeCm <= 0.0f || CoverageXY.X <= 0.0 || CoverageXY.Y <= 0.0)
	{
		return;
	}

	const FIntPoint Dims =
		ArcTerrainMeshModifier_Local::ComputeBedDims(CoverageXY, static_cast<double>(BedTexelSizeCm));
	const int32 SamplesPerSide = FMath::Clamp(BedSamplesPerTexel, 1, 8);
	BedContentKey = ArcTerrainMeshModifier_Local::ComputeBedContentId(
		Adapter, Mesh->GetPathName(), Dims, CoverageXY, BedTexelSizeCm, BedMaxRiseCm, SamplesPerSide);

	if (!ArcTerrainMeshModifier_Local::FitsBedBudget(Dims))
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: '%ls' covers %.0f x %.0f cm, which needs a %d x %d bed at %.1f cm texels and exceeds the "
				 "%d texel budget; raise BedTexelSizeCm to at least %.0f cm. No height bed built.",
			*GetPathName(), *Mesh->GetPathName(), CoverageXY.X, CoverageXY.Y, Dims.X, Dims.Y, BedTexelSizeCm,
			ArcTerrainMeshModifier_Local::GMaxBedTexels,
			ArcTerrainMeshModifier_Local::SuggestBedTexelSizeCm(CoverageXY));
		return;
	}

	UE::Geometry::TMeshAABBTree3<FStaticMeshLODResourcesMeshAdapter> Tree(&Adapter);
	const UE::Geometry::FAxisAlignedBox3d GeometryBox = Tree.GetBoundingBox();
	if (GeometryBox.Max.Z < GeometryBox.Min.Z)
	{
		return;
	}

	const double HalfX = CoverageXY.X * 0.5;
	const double HalfY = CoverageXY.Y * 0.5;
	const double SpanX = CoverageXY.X / static_cast<double>(Dims.X - 1);
	const double SpanY = CoverageXY.Y / static_cast<double>(Dims.Y - 1);
	const double RayStartZ = GeometryBox.Max.Z + 1.0;
	const double AcceptCeilingZ = GeometryBox.Min.Z + static_cast<double>(BedMaxRiseCm);

	TArray<float> Values;
	Values.SetNumZeroed(Dims.X * Dims.Y);
	TArray<uint8> bHasValue;
	bHasValue.SetNumZeroed(Dims.X * Dims.Y);
	TArray<MeshIntersection::FHitIntersectionResult> Hits;
	bool bAnyAccepted = false;

	for (int32 TexelY = 0; TexelY < Dims.Y; ++TexelY)
	{
		const double TexelCenterY = -HalfY + static_cast<double>(TexelY) * SpanY;
		for (int32 TexelX = 0; TexelX < Dims.X; ++TexelX)
		{
			const double TexelCenterX = -HalfX + static_cast<double>(TexelX) * SpanX;

			double TexelZ = 0.0;
			bool bTexelAccepted = false;
			for (int32 SubY = 0; SubY < SamplesPerSide; ++SubY)
			{
				const double SampleY = TexelCenterY
					+ ((static_cast<double>(SubY) + 0.5) / static_cast<double>(SamplesPerSide) - 0.5) * SpanY;
				for (int32 SubX = 0; SubX < SamplesPerSide; ++SubX)
				{
					const double SampleX = TexelCenterX
						+ ((static_cast<double>(SubX) + 0.5) / static_cast<double>(SamplesPerSide) - 0.5) * SpanX;

					const FRay3d Ray(FVector3d(SampleX, SampleY, RayStartZ), FVector3d(0.0, 0.0, -1.0), true);
					Hits.Reset();
					if (!Tree.FindAllHitTriangles(Ray, Hits))
					{
						continue;
					}

					// Rejection is per SAMPLE, then a max over the survivors: rejecting whole texels
					// instead would take the max first, so a texel straddling an arch leg and its
					// opening would score on the soffit and be dropped along with the leg it covers.
					const double LowestHitZ = Ray.PointAt(Hits.Last().Distance).Z;
					if (LowestHitZ > AcceptCeilingZ)
					{
						continue;
					}

					TexelZ = bTexelAccepted ? FMath::Max(TexelZ, LowestHitZ) : LowestHitZ;
					bTexelAccepted = true;
				}
			}

			if (bTexelAccepted)
			{
				const int32 Index = TexelY * Dims.X + TexelX;
				Values[Index] = static_cast<float>(TexelZ);
				bHasValue[Index] = 1;
				bAnyAccepted = true;
			}
		}
	}

	if (!bAnyAccepted)
	{
		UE_LOGF(LogArcTerrain, Warning,"%ls: no sample of '%ls' landed within %.1f cm of its lowest geometry; no height bed built.",
			*GetPathName(), *Mesh->GetPathName(), BedMaxRiseCm);
		return;
	}

	ArcTerrainMeshModifier_Local::ExtrapolateHoles(Values, bHasValue, Dims);

	BedField.Values = MoveTemp(Values);
	BedField.Dims = Dims;
	BedField.ContentId = BedContentKey;
}

void UArcTerrainMeshModifier::UpdateCollar(bool bModifyBeforeWrite)
{
	if (Mesh == nullptr || Mesh->IsCompiling())
	{
		return;
	}

	AArcTerrain* Terrain = AffectedTerrain.Get();
	UWorld* World = GetWorld();
	UArcTerrainSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
	if (Terrain == nullptr || Subsystem == nullptr)
	{
		return;
	}

	const double TexelSizeCm = static_cast<double>(Terrain->Config.TexelSizeCm());
	if (TexelSizeCm <= 0.0)
	{
		return;
	}

	const FBox Footprint = ComputeBounds();
	if (!Footprint.IsValid)
	{
		return;
	}

	const double RadiusCm = static_cast<double>(CollarParams.CollarRadiusCm);
	const FIntPoint MinTexel(
		FMath::FloorToInt32((Footprint.Min.X - RadiusCm) / TexelSizeCm),
		FMath::FloorToInt32((Footprint.Min.Y - RadiusCm) / TexelSizeCm));
	const FIntPoint MaxTexel(
		FMath::CeilToInt32((Footprint.Max.X + RadiusCm) / TexelSizeCm),
		FMath::CeilToInt32((Footprint.Max.Y + RadiusCm) / TexelSizeCm));

	FArcTerrainCollarHeightField HeightField;
	HeightField.MinTexel = MinTexel;
	HeightField.TexelSizeCm = static_cast<float>(TexelSizeCm);
	if (!Subsystem->SampleTerrainHeightTexels(
			*Terrain, MinTexel, MaxTexel, HeightField.Heights, HeightField.Dims, HeightField.bComplete))
	{
		return;
	}

	if (!HeightField.bComplete)
	{
		UE_LOGF(LogArcTerrain, Verbose,"%ls: the ground under '%ls' has an unresolved stamp; the collar is left as it was and no key "
				 "is stamped.",
			*GetPathName(), *Mesh->GetPathName());
		return;
	}

	if (!HeightField.IsValid())
	{
		return;
	}

	const FTransform ComponentToTerrain =
		GetComponentTransform().GetRelativeTransform(Terrain->GetTerrainTransform());
	const FGuid ContentId =
		ArcTerrainCollarBake::ComputeContentId(*Mesh, ComponentToTerrain, HeightField, CollarParams);
	if (!ContentId.IsValid() || ContentId == CollarBakeKey)
	{
		return;
	}

	if (bModifyBeforeWrite)
	{
		Modify();
	}

	CollarBakeKey = ContentId;
	if (!ArcTerrainCollarBake::Bake(
			*Mesh, ComponentToTerrain, HeightField, CollarParams, GetPathName(), CollarMesh))
	{
		CollarMesh.Reset();
	}
}

void UArcTerrainMeshModifier::RebindMeshBoundsDelegate()
{
	UnbindMeshBoundsDelegate();

	if (Mesh == nullptr)
	{
		return;
	}

	BoundsDelegateMesh = Mesh;
	BoundsDelegateHandle = Mesh->GetOnExtendedBoundsChanged().AddUObject(
		this, &UArcTerrainMeshModifier::OnMeshExtendedBoundsChanged);
	PostMeshBuildHandle = Mesh->OnPostMeshBuild().AddUObject(
		this, &UArcTerrainMeshModifier::OnMeshPostBuild);
}

void UArcTerrainMeshModifier::UnbindMeshBoundsDelegate()
{
	UStaticMesh* PreviouslyBound = BoundsDelegateMesh.Get();
	if (BoundsDelegateHandle.IsValid())
	{
		if (PreviouslyBound != nullptr)
		{
			PreviouslyBound->GetOnExtendedBoundsChanged().Remove(BoundsDelegateHandle);
		}
		BoundsDelegateHandle.Reset();
	}
	if (PostMeshBuildHandle.IsValid())
	{
		if (PreviouslyBound != nullptr)
		{
			PreviouslyBound->OnPostMeshBuild().Remove(PostMeshBuildHandle);
		}
		PostMeshBuildHandle.Reset();
	}
	BoundsDelegateMesh = nullptr;
}

void UArcTerrainMeshModifier::OnMeshPostBuild(UStaticMesh* /*BuiltMesh*/)
{
	Modify();
	BuildBedField();
	UpdateCollar(true);
	SyncCollarMesh();
	NotifyStampChanged();
}

void UArcTerrainMeshModifier::OnMeshExtendedBoundsChanged(const FBoxSphereBounds& /*NewBounds*/)
{
	// Broadcast from UStaticMesh::SetExtendedBounds, which FStaticMeshCompilationContext documents as
	// running on the game thread at the end of the build.
	UpdateDerivedCoverage();

	if (Mesh != nullptr && !Mesh->IsCompiling())
	{
		const FGuid SourceKey = ComputeMeshSourceKey();
		if (SourceKey.IsValid() && SourceKey == BedSourceKey && NeedsBedRebuild())
		{
			Modify();
			BuildBedField();
		}
	}

	UpdateCollar(true);
	SyncCollarMesh();

	NotifyStampChanged();
}
#endif // WITH_EDITOR
