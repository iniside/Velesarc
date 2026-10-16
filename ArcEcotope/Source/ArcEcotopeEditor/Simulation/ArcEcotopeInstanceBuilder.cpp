// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeInstanceBuilder.h"

#include "ArcEcotopeModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Algo/BinarySearch.h"
#include "Animation/TransformProviderData.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityTrait.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "Ecotope/ArcEcotopeModel.h"
#include "Engine/Level.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Instances/ArcEcotopeInstancePartitionActor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ArchiveMD5.h"
#include "ScopedTransaction.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcEcotopeTerrainSelection.h"
#include "Simulation/ArcForestWorldSnapshot.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Species/ArcForestStageBand.h"
#include "Terrain/ArcEcotopeTerrainWeightStamp.h"
#include "Terrain/ArcEcotopeWeightRasterBuilder.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcEcotopeInstanceBuilder, Log, All);

namespace ArcEcotopeInstanceBuilderWeightPaint
{
	struct FWeightPaint
	{
		UArcEcotopeTerrainWeightStamp* Stamp = nullptr;
		TSoftObjectPtr<AArcTerrain> Terrain;
		FVector2D CoverageXY = FVector2D::ZeroVector;
		TMap<FName, FArcTerrainBakedField> Layers;
		TMap<FName, FName> ChannelMap;
		TArray<FName> SuppressedChannels;
		FArcTerrainBakedField UnionMask;
	};

	bool PaintsTerrainCover(const FArcEcotopeSpeciesSetup& Setup)
	{
		return Setup.bEnabled && Setup.Species != nullptr && !Setup.Species->TerrainCoverChannel.IsNone();
	}

	bool ComputeWeightPaint(
		AArcEcotopeVolume& Volume,
		const FArcEcotopePreparedInstances& Prepared,
		FWeightPaint& OutPaint,
		FText& OutError)
	{
		OutPaint = FWeightPaint();

		UArcEcotopeTerrainWeightStamp* const Stamp = Volume.TerrainWeightStamp;
		if (Stamp == nullptr)
		{
			return true;
		}
		OutPaint.Stamp = Stamp;

		const UArcEcotopeModel& Ecotope = Volume.GetEcotopeOrDefault();
		bool bPaintsAnything = !Stamp->CanopyChannel.IsNone() || !Stamp->LitterChannel.IsNone();
		for (const FArcEcotopeSpeciesSetup& Setup : Ecotope.SpeciesSetups)
		{
			bPaintsAnything = bPaintsAnything || PaintsTerrainCover(Setup);
		}
		UE_CLOGF(!bPaintsAnything, LogArcEcotope, Warning,"ArcEcotope volume '%ls' paints no terrain weight channel: assign CanopyChannel or LitterChannel on its TerrainWeightStamp, or TerrainCoverChannel on a species.",
			*Volume.GetName());
		if (!bPaintsAnything)
		{
			return true;
		}

		UWorld* const World = Volume.GetWorld();
		if (World == nullptr || !IsValid(Volume.VolumeBounds))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope volume '%s' paints terrain weight channels but has no world or bounds component."),
				*Volume.GetName()));
			return false;
		}

		const FVector UnscaledExtent = Volume.VolumeBounds->GetUnscaledBoxExtent();
		const FTransform BoundsTransform = Volume.VolumeBounds->GetComponentTransform();
		const FBox WorldBounds = FBox(-UnscaledExtent, UnscaledExtent).TransformBy(BoundsTransform);
		const UE::ArcEcotope::FArcEcotopeTerrainChoice Choice =
			UE::ArcEcotope::SelectTerrainForVolume(*World, WorldBounds, WorldBounds, BoundsTransform);
		if (Choice.Terrain == nullptr)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope volume '%s' paints terrain weight channels but resolves to no ArcTerrain."),
				*Volume.GetName()));
			return false;
		}

		OutPaint.Terrain = Choice.Terrain;
		// Unscaled: ComputeBounds re-applies the component transform, which already carries the actor scale.
		OutPaint.CoverageXY = FVector2D(UnscaledExtent.X * 2.0, UnscaledExtent.Y * 2.0);

		const FArcEcotopeSiteMetadata Site = Volume.GetActiveSiteMetadata();
		if (!ArcEcotopeWeightRaster::BuildWeightRasters(
				Volume, Site, Choice.Terrain->Config.ChannelSet,
				Stamp->CanopyChannel, Stamp->LitterChannel,
				Stamp->MaskExpandCm, Stamp->MaskFalloffCm, Stamp->MaskFalloffExponent,
				Prepared.WeightFootprints,
				OutPaint.Layers, OutPaint.ChannelMap, OutPaint.SuppressedChannels, OutPaint.UnionMask,
				OutError))
		{
			return false;
		}

		return true;
	}

	void ApplyWeightPaint(FWeightPaint& Paint)
	{
		if (Paint.Stamp == nullptr)
		{
			return;
		}

		Paint.Stamp->Modify();
		Paint.Stamp->BakedLayers = MoveTemp(Paint.Layers);
		Paint.Stamp->LayerChannelMap = MoveTemp(Paint.ChannelMap);
		Paint.Stamp->SuppressedChannels = MoveTemp(Paint.SuppressedChannels);
		Paint.Stamp->UnionMask = MoveTemp(Paint.UnionMask);
		Paint.Stamp->RefreshProviderFields();
		if (!Paint.Terrain.IsNull())
		{
			const bool bTerrainChanged = Paint.Stamp->AffectedTerrain != Paint.Terrain;
			Paint.Stamp->AffectedTerrain = Paint.Terrain;
			Paint.Stamp->CoverageXY = Paint.CoverageXY;
			if (bTerrainChanged)
			{
				Paint.Stamp->NotifyAffectedTerrainChanged();
				return;
			}
		}
		Paint.Stamp->NotifyStampChanged();
	}
}

namespace ArcEcotopeInstanceBuilder
{
	constexpr uint64 GroupHashKey = 0xD32E71A4C9B85F06ull;

	enum class EVisualLookupResult : uint8
	{
		Found,
		Missing,
		Invalid,
	};

	struct FMissingVisualWarning
	{
		FString SpeciesName;
		EArcForestVisualKind Kind = EArcForestVisualKind::Alive;
		int64 SkippedInstanceCount = 0;
	};

	struct FNearestVisualCount
	{
		FString SpeciesName;
		int64 InstanceCount = 0;
	};

	struct FClusterRadiusWarning
	{
		FString SpeciesName;
		int32 LayerIndex = 0;
		double AuthoredRadiusCm = 0.0;
		double MaxRadiusCm = 0.0;
		int64 RecordCount = 0;
	};

	struct FClusterSiteCapWarning
	{
		FString SpeciesName;
		int32 LayerIndex = 0;
		double ClusterRadiusCm = 0.0;
		double RecordCellSizeCm = 0.0;
		uint8 RecordGridLevel = 0;
		int64 CandidateSiteCount = 0;
		int64 RecordCount = 0;
	};

	/**
	 * Our own sentinel: UActorPartitionSubsystem::FCellCoord has no Invalid member. The narrowing to
	 * int32 is deliberate — FCellCoord::X/Y/Z are int64, and int32 is ample at a 256 m grid.
	 */
	const FIntVector UnassignedCell(MIN_int32, MIN_int32, MIN_int32);

	/** Identity of one instanced component, including the partition cell it lands in. */
	struct FArcEcotopeGroupKey
	{
		FIntVector CellCoord = UnassignedCell;
		FGuid SpeciesId;
		uint8 Kind = 0;
		float MinHeightCm = 0.0f;
		float MaxHeightCm = 0.0f;
		uint64 RendererHash = 0;
		int32 VariantIndex = INDEX_NONE;

		bool operator==(const FArcEcotopeGroupKey& Other) const
		{
			return CellCoord == Other.CellCoord
				&& SpeciesId == Other.SpeciesId
				&& Kind == Other.Kind
				&& MinHeightCm == Other.MinHeightCm
				&& MaxHeightCm == Other.MaxHeightCm
				&& RendererHash == Other.RendererHash
				&& VariantIndex == Other.VariantIndex;
		}

		friend uint32 GetTypeHash(const FArcEcotopeGroupKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.CellCoord);
			Hash = HashCombine(Hash, GetTypeHash(Key.SpeciesId));
			Hash = HashCombine(Hash, GetTypeHash(Key.Kind));
			Hash = HashCombine(Hash, GetTypeHash(Key.MinHeightCm));
			Hash = HashCombine(Hash, GetTypeHash(Key.MaxHeightCm));
			Hash = HashCombine(Hash, GetTypeHash(Key.RendererHash));
			Hash = HashCombine(Hash, GetTypeHash(Key.VariantIndex));
			return Hash;
		}
	};

	struct FGroup
	{
		const UArcEcotopeSpeciesModel* Species = nullptr;
		int32 BandIndex = INDEX_NONE;
		FArcEcotopeGroupKey Key;
		uint64 NameHash = 0;
		uint32 GridSizeCm = 0;
		FName RuntimeGridName;
		/** Volume-local, as every transform in the builder is; the emit step crosses into the partition actor's frame. */
		TArray<FTransform> Transforms;
		TArray<int32> AnimationIndices;
	};

	template <typename FVisualType>
	const FVisualType* GetGroupVisual(const FGroup& Group)
	{
		return Group.Species != nullptr && Group.Species->Bands.IsValidIndex(Group.BandIndex)
			? Group.Species->Bands[Group.BandIndex].Visual.GetPtr<FVisualType>()
			: nullptr;
	}

	const TCHAR* DescribeGroupVisual(const FGroup& Group)
	{
		if (GetGroupVisual<FArcForestBandVisual_StaticMesh>(Group) != nullptr)
		{
			return TEXT("ISM");
		}
		if (GetGroupVisual<FArcForestBandVisual_SkinnedMesh>(Group) != nullptr)
		{
			return TEXT("ISKM");
		}
		if (GetGroupVisual<FArcForestBandVisual_Entity>(Group) != nullptr)
		{
			return TEXT("Entity");
		}
		return TEXT("unknown");
	}

	/** The world-side facts every emit path shares, resolved once per bake. */
	struct FEmitContext
	{
		UWorld* World = nullptr;
		ULevel* PersistentLevel = nullptr;
		UActorPartitionSubsystem* PartitionSubsystem = nullptr;
		UWorldPartition* WorldPartition = nullptr;
		bool bIsPartitionedWorld = false;
		FTransform VolumeTransform;
	};

	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A) return Left.A < Right.A;
		if (Left.B != Right.B) return Left.B < Right.B;
		if (Left.C != Right.C) return Left.C < Right.C;
		return Left.D < Right.D;
	}

	/** Seeds the deterministic component name, is the group sort order, and is what the collision guard keys on. */
	uint64 MakeGroupNameHash(const FArcEcotopeGroupKey& Key)
	{
		UE::ArcForest::FKeyedHash64 Hash(GroupHashKey);
		Hash.AddInt32(Key.CellCoord.X);
		Hash.AddInt32(Key.CellCoord.Y);
		Hash.AddInt32(Key.CellCoord.Z);
		Hash.AddGuid(Key.SpeciesId);
		Hash.AddUInt8(Key.Kind);
		Hash.AddUInt32(Key.MinHeightCm == 0.0f ? 0u : FPlatformMath::AsUInt(Key.MinHeightCm));
		Hash.AddUInt32(Key.MaxHeightCm == 0.0f ? 0u : FPlatformMath::AsUInt(Key.MaxHeightCm));
		Hash.AddUInt64(Key.RendererHash);
		Hash.AddInt32(Key.VariantIndex);
		return Hash.Finalize();
	}

	const FArcEcotopeSpeciesSetup* FindEnabledSetup(const AArcEcotopeVolume& Volume, const FGuid& SpeciesId)
	{
		for (const FArcEcotopeSpeciesSetup& Setup : Volume.GetEcotopeOrDefault().SpeciesSetups)
		{
			if (Setup.bEnabled && Setup.Species != nullptr && Setup.Species->SpeciesId == SpeciesId)
			{
				return &Setup;
			}
		}
		return nullptr;
	}

	struct FResolvedBand
	{
		const FArcForestStageBand* Band = nullptr;
		int32 BandIndex = INDEX_NONE;
	};

	struct FVisualValidity
	{
		bool bValid = false;
		FText Error;
	};

	using FVisualValidityCache = TMap<const FArcForestBandVisual*, FVisualValidity>;
	using FSpeciesValidityCache = TMap<const UArcEcotopeSpeciesModel*, FVisualValidity>;

	const FVisualValidity& GetVisualValidity(
		FVisualValidityCache& Cache,
		const FArcForestBandVisual& Visual)
	{
		if (const FVisualValidity* Existing = Cache.Find(&Visual))
		{
			return *Existing;
		}
		FVisualValidity Validity;
		Validity.bValid = Visual.IsValid(Validity.Error);
		return Cache.Add(&Visual, MoveTemp(Validity));
	}

	/**
	 * Separate from the per-visual gate: the grid and the FastGeo descriptor rules are species-wide, no
	 * band's IsValid can see them, and IsDataValid is an editor warning rather than a load gate, so
	 * without this a HISM descriptor would reach the cook.
	 */
	const FVisualValidity& GetSpeciesValidity(
		FSpeciesValidityCache& Cache,
		const UArcEcotopeSpeciesModel& Species)
	{
		if (const FVisualValidity* Existing = Cache.Find(&Species))
		{
			return *Existing;
		}
		TArray<FString> Errors;
		if (!StaticEnum<EArcForestGridSize>()->IsValidEnumValue(static_cast<int64>(Species.GridSize)))
		{
			Errors.Add(TEXT("GridSize must be one of the authored cell sizes."));
		}
		bool bDrawsStaticMesh = false;
		for (const FArcForestStageBand& Band : Species.Bands)
		{
			if (Band.Visual.GetPtr<FArcForestBandVisual_StaticMesh>() != nullptr)
			{
				bDrawsStaticMesh = true;
				break;
			}
		}
		if (bDrawsStaticMesh)
		{
			if (Species.StaticMeshDescriptor.ComponentClass == nullptr
				|| Species.StaticMeshDescriptor.ComponentClass->IsChildOf(UHierarchicalInstancedStaticMeshComponent::StaticClass()))
			{
				Errors.Add(TEXT("StaticMeshDescriptor.ComponentClass must be a plain UInstancedStaticMeshComponent: the cook-time FastGeo conversion rejects a non-Nanite HISM with more than one LOD."));
			}
			if (Species.StaticMeshDescriptor.Mobility != EComponentMobility::Static)
			{
				Errors.Add(TEXT("StaticMeshDescriptor.Mobility must be Static for the cook-time FastGeo conversion."));
			}
		}
		FVisualValidity Validity;
		Validity.bValid = Errors.IsEmpty();
		if (!Validity.bValid)
		{
			Validity.Error = FText::FromString(FString::Join(Errors, TEXT(" ")));
		}
		return Cache.Add(&Species, MoveTemp(Validity));
	}

	EArcForestVisualKind MakeVisualKind(const EArcForestDeadwoodType Type)
	{
		switch (Type)
		{
		case EArcForestDeadwoodType::Snag:
			return EArcForestVisualKind::Snag;
		case EArcForestDeadwoodType::Log:
			return EArcForestVisualKind::Log;
		case EArcForestDeadwoodType::Stump:
			return EArcForestVisualKind::Stump;
		case EArcForestDeadwoodType::Uprooted:
			return EArcForestVisualKind::Uprooted;
		case EArcForestDeadwoodType::Branch:
			return EArcForestVisualKind::Branch;
		case EArcForestDeadwoodType::MAX:
			break;
		}
		return EArcForestVisualKind::Alive;
	}

	EVisualLookupResult FindBandForSize(
		const UArcEcotopeSpeciesModel& Species,
		const EArcForestVisualKind Kind,
		const float SizeCm,
		FVisualValidityCache& VisualValidity,
		FSpeciesValidityCache& SpeciesValidity,
		FResolvedBand& OutResolved,
		bool& bOutNearestMatch,
		FText& OutError)
	{
		OutResolved = FResolvedBand();
		bOutNearestMatch = false;
		const float Size = FMath::IsFinite(SizeCm) ? SizeCm : 0.0f;
		const FArcForestStageBand* Result = nullptr;
		int32 ResultIndex = INDEX_NONE;
		float BestDistanceCm = 0.0f;
		for (int32 BandIndex = 0; BandIndex < Species.Bands.Num(); ++BandIndex)
		{
			const FArcForestStageBand* Band = &Species.Bands[BandIndex];
			if (Band->Kind != Kind || !Band->Visual.IsValid())
			{
				continue;
			}
			if (!FMath::IsFinite(Band->MinHeightCm) || !FMath::IsFinite(Band->MaxHeightCm))
			{
				continue;
			}
			const float DistanceCm = FMath::Max3(Band->MinHeightCm - Size, Size - Band->MaxHeightCm, 0.0f);
			const bool bBetter = Result == nullptr
				|| DistanceCm < BestDistanceCm
				|| (DistanceCm == BestDistanceCm && Band->MinHeightCm < Result->MinHeightCm);
			if (bBetter)
			{
				Result = Band;
				ResultIndex = BandIndex;
				BestDistanceCm = DistanceCm;
			}
		}
		if (Result == nullptr)
		{
			return EVisualLookupResult::Missing;
		}
		bOutNearestMatch = BestDistanceCm > 0.0f;

		const FVisualValidity& SpeciesGate = GetSpeciesValidity(SpeciesValidity, Species);
		if (!SpeciesGate.bValid)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Species %s has invalid species-wide visual settings: %s"),
				*Species.GetName(),
				*SpeciesGate.Error.ToString()));
			return EVisualLookupResult::Invalid;
		}
		const FVisualValidity& Validity = GetVisualValidity(VisualValidity, *Result->Visual.GetPtr());
		if (!Validity.bValid)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Species %s Bands[%d] has an invalid visual on a snapshot-used height band: %s"),
				*Species.GetName(),
				ResultIndex,
				*Validity.Error.ToString()));
			return EVisualLookupResult::Invalid;
		}
		OutResolved.Band = Result;
		OutResolved.BandIndex = ResultIndex;
		return EVisualLookupResult::Found;
	}

	uint64 MakeAnnualInstanceKey(
		const FArcForestWorldSnapshot& Snapshot,
		const UE::ArcForest::EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal,
		const int64 Draw)
	{
		return UE::ArcForest::MakeAnnualKey(
			Snapshot.SimulationId, Snapshot.Seed, Snapshot.CurrentYear,
			Domain, SpeciesId, CellIndex, EntityId, Ordinal, Draw);
	}

	/** Year-free draw: a stem keeps the same variant, lean and jitter for its whole life. */
	uint64 MakeStableInstanceKey(
		const FArcForestWorldSnapshot& Snapshot,
		const UE::ArcForest::EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal,
		const int64 Draw)
	{
		// MakeKey carries no entity fields, so the stem identity folds into its field ordinal.
		UE::ArcForest::FKeyedHash64 StemOrdinal(static_cast<uint64>(Domain));
		StemOrdinal.AddUInt64(EntityId);
		StemOrdinal.AddInt32(Ordinal);
		StemOrdinal.AddInt64(Draw);
		return UE::ArcForest::MakeKey(
			Snapshot.SimulationId, Snapshot.Seed, Domain, SpeciesId, CellIndex,
			static_cast<int64>(StemOrdinal.Finalize()));
	}

	template <typename FVariantType>
	int32 SelectVariantIndex(
		const TArray<FVariantType>& Variants,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal)
	{
		check(!Variants.IsEmpty());
		double TotalWeight = 0.0;
		for (const FVariantType& Variant : Variants)
		{
			TotalWeight += FMath::Max(0.0, static_cast<double>(Variant.SelectionWeight));
		}
		if (!(TotalWeight > 0.0))
		{
			return 0;
		}
		const uint64 Key = MakeStableInstanceKey(Snapshot, UE::ArcForest::EDeterministicDomain::InstanceMeshVariant, SpeciesId, CellIndex, EntityId, Ordinal, 0);
		double Remaining = UE::ArcForest::KeyToUnitOpen(Key) * TotalWeight;
		for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
		{
			Remaining -= FMath::Max(0.0, static_cast<double>(Variants[VariantIndex].SelectionWeight));
			if (Remaining < 0.0)
			{
				return VariantIndex;
			}
		}
		return Variants.Num() - 1;
	}

	FQuat MakeTilt(
		const FArcForestInstanceRotation& Rotation,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal)
	{
		const double MaxTiltRadians = FMath::DegreesToRadians(UE::ArcForest::SanitizeAngleDegrees(Rotation.MaxTiltDeg, 90.0));
		if (!(MaxTiltRadians > 0.0))
		{
			return FQuat::Identity;
		}
		const uint64 AngleKey = MakeStableInstanceKey(Snapshot, UE::ArcForest::EDeterministicDomain::InstanceTilt, SpeciesId, CellIndex, EntityId, Ordinal, 0);
		const uint64 AzimuthKey = MakeStableInstanceKey(Snapshot, UE::ArcForest::EDeterministicDomain::InstanceTilt, SpeciesId, CellIndex, EntityId, Ordinal, 1);
		const double Azimuth = UE::ArcForest::KeyToUnitOpen(AzimuthKey) * 2.0 * PI;
		// A horizontal axis leans the instance's up vector by the drawn angle toward the drawn azimuth.
		const FVector Axis(FMath::Cos(Azimuth), FMath::Sin(Azimuth), 0.0);
		return FQuat(Axis, UE::ArcForest::KeyToUnitOpen(AngleKey) * MaxTiltRadians);
	}

	FQuat MakeYaw(
		const FArcForestInstanceRotation& Rotation,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal)
	{
		const double MaxYawRadians = FMath::DegreesToRadians(UE::ArcForest::SanitizeAngleDegrees(Rotation.MaxYawDeg, 180.0));
		const uint64 YawKey = MakeAnnualInstanceKey(Snapshot, UE::ArcForest::EDeterministicDomain::InstanceYaw, SpeciesId, CellIndex, EntityId, Ordinal, 0);
		return FQuat(FVector::UpVector, (UE::ArcForest::KeyToUnitOpen(YawKey) * 2.0 - 1.0) * MaxYawRadians);
	}

	FQuat MakeRotation(
		const FArcForestInstanceRotation& Rotation,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal)
	{
		return MakeTilt(Rotation, Snapshot, SpeciesId, CellIndex, EntityId, Ordinal)
			* MakeYaw(Rotation, Snapshot, SpeciesId, CellIndex, EntityId, Ordinal);
	}

	TMap<FGuid, int32> BuildSpeciesMaxAgeYearsCache(const AArcEcotopeVolume& Volume)
	{
		TMap<FGuid, int32> Cache;
		Cache.Reserve(Volume.GetEcotopeOrDefault().SpeciesSetups.Num());
		for (const FArcEcotopeSpeciesSetup& Setup : Volume.GetEcotopeOrDefault().SpeciesSetups)
		{
			const UArcEcotopeSpeciesModel* SpeciesModel = Setup.Species;
			if (SpeciesModel == nullptr)
			{
				continue;
			}

			ArcForestSpeciesModel::FArcForestDerivedEcology Derived;
			FText DerivationError;
			const int32 MaxAgeYears = ArcForestSpeciesModel::DeriveEcology(*SpeciesModel, Derived, DerivationError)
				? Derived.MaxAgeYears
				: 0;
			Cache.Add(SpeciesModel->SpeciesId, MaxAgeYears);
		}

		return Cache;
	}

	double MakeScale(
		const FArcForestInstanceScale& Scale,
		const FArcForestWorldSnapshot& Snapshot,
		const TMap<FGuid, int32>& SpeciesMaxAgeYearsCache,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal,
		const float SizeCm,
		const float AgeYears,
		const float BandMinHeightCm,
		const float BandMaxHeightCm)
	{
		// Both bounds are re-derived here because IsDataValid is not a hard load gate:
		// a non-finite or non-positive authored bound would otherwise reach AddInstances.
		constexpr double SmallestScale = 0.0001;
		const double MinScale = FMath::IsFinite(Scale.MinScale)
			? FMath::Max(SmallestScale, static_cast<double>(Scale.MinScale))
			: SmallestScale;
		const double MaxScale = FMath::IsFinite(Scale.MaxScale)
			? FMath::Max(MinScale, static_cast<double>(Scale.MaxScale))
			: MinScale;
		double Alpha = 0.0;
		switch (Scale.Mode)
		{
		case EArcForestInstanceScaleMode::FromHeight:
			Alpha = (BandMaxHeightCm > BandMinHeightCm)
				? FMath::Clamp(
					(static_cast<double>(SizeCm) - static_cast<double>(BandMinHeightCm))
						/ (static_cast<double>(BandMaxHeightCm) - static_cast<double>(BandMinHeightCm)),
					0.0, 1.0)
				: 0.0;
			break;
		case EArcForestInstanceScaleMode::FromAge:
		{
			const int32 MaxAgeYears = SpeciesMaxAgeYearsCache.FindRef(SpeciesId);
			Alpha = MaxAgeYears > 0
				? FMath::Clamp(static_cast<double>(AgeYears) / static_cast<double>(MaxAgeYears), 0.0, 1.0)
				: 0.0;
			break;
		}
		case EArcForestInstanceScaleMode::Random:
		default:
			Alpha = UE::ArcForest::KeyToUnitOpen(
				MakeStableInstanceKey(Snapshot, UE::ArcForest::EDeterministicDomain::InstanceScale, SpeciesId, CellIndex, EntityId, Ordinal, 0));
			break;
		}
		double Value = FMath::Lerp(MinScale, MaxScale, Alpha);
		const double Jitter = FMath::IsFinite(Scale.RandomJitter)
			? FMath::Max(0.0, static_cast<double>(Scale.RandomJitter))
			: 0.0;
		if (Jitter > 0.0)
		{
			const uint64 JitterKey = MakeStableInstanceKey(Snapshot, UE::ArcForest::EDeterministicDomain::InstanceScale, SpeciesId, CellIndex, EntityId, Ordinal, 1);
			Value *= 1.0 + (UE::ArcForest::KeyToUnitOpen(JitterKey) * 2.0 - 1.0) * Jitter;
		}
		// A non-finite SizeCm or AgeYears would survive the clamp as NaN.
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, MinScale, MaxScale) : MinScale;
	}

	TAutoConsoleVariable<float> CVarPatchInstanceDensityPerSqM(
		TEXT("arcecotope.Instances.PatchDensityPerSqM"),
		2.0f,
		TEXT("Ceiling on how densely a record cell's patch instances are drawn, in instances per square ")
		TEXT("metre of its plantable area, summed across every species sharing the cell. Zero draws no ")
		TEXT("patch instances at all. Presentation only: the simulated stem-or-cover equivalent is ")
		TEXT("unchanged."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarMaxPatchInstancesPerRecord(
		TEXT("arcecotope.Instances.MaxPatchInstancesPerRecord"),
		1024,
		TEXT("Hard cap on the instances one patch record draws, whatever its plantable area. A per-record ")
		TEXT("safety valve, not the density cap -- that is arcecotope.Instances.PatchDensityPerSqM, which ")
		TEXT("applies to the record cell's total across all species sharing it."),
		ECVF_Default);

	double GetPatchInstanceDensityPerSqM()
	{
		const float Value = CVarPatchInstanceDensityPerSqM.GetValueOnGameThread();
		return FMath::IsFinite(Value) ? FMath::Max(static_cast<double>(Value), 0.0) : 0.0;
	}

	TAutoConsoleVariable<int32> CVarPatchClusterInstances(
		TEXT("arcecotope.Instances.PatchClusterInstances"),
		12,
		TEXT("Instances one clonal clump of ground cover is worth, used ONLY by a species that authors no ")
		TEXT("CoverClusterLayers; an authored profile ignores this entirely. It divides the local instance ")
		TEXT("density to give the chance one clump site of the arcecotope.Instances.PatchClusterRadiusCm ")
		TEXT("lattice activates. One or less disables clustering entirely and restores the uniform scatter ")
		TEXT("over the record cell's plantable base cells. Presentation only: the simulated stem-or-cover ")
		TEXT("equivalent is unchanged."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarPatchClusterRadiusCm(
		TEXT("arcecotope.Instances.PatchClusterRadiusCm"),
		250.0f,
		TEXT("Radius in centimetres of one ground-cover clump, measured from its centre, and half the ")
		TEXT("spacing of the clump site lattice; instances are drawn uniformly over that disc. Zero ")
		TEXT("restores the uniform scatter. Ignored while arcecotope.Instances.PatchClusterInstances is at ")
		TEXT("or below one, and used ONLY by a species that authors no CoverClusterLayers."),
		ECVF_Default);

	/** The instance builder owns a separate cache because it runs after the simulation reader has closed. */
	TAutoConsoleVariable<int32> CVarInstanceSiteReaderBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.InstanceBuilderBudgetMiB"),
		64,
		TEXT("Cache budget in MiB for ArcEcotope instance materialization; clamped to at least 1 MiB."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarInstanceSiteScheduleBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.InstanceBuilderScheduleBudgetMiB"),
		64,
		TEXT("Memory budget in MiB for ArcEcotope instance chunk schedules; clamped to at least 1 MiB."),
		ECVF_Default);

	constexpr uint32 InstanceSiteFieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::LocalSurfaceZ))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Slope))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Aspect))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::RecordValid))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Moisture))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Fertility))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::BareRock))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ChannelBuffer));

	constexpr int32 MaxClusterHaloBaseCells = 4;

	// Read halo exceeds the cluster reach by one so a bilinear neighbour of the outermost cluster cell is still in view.
	constexpr int32 InstanceSiteHaloBaseCells = MaxClusterHaloBaseCells + 1;

	struct FInstanceSiteGeometry
	{
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		float CellSizeCm = 0.0f;
		FIntPoint BaseDimensions = FIntPoint::ZeroValue;
		FIntPoint RecordDimensions = FIntPoint::ZeroValue;
		uint8 RecordGridLevel = 0;
		uint8 MoistureLevel = 0;
		uint8 FertilityLevel = 0;
		uint8 BareRockLevel = 0;
		FIntPoint MoistureDimensions = FIntPoint::ZeroValue;
		FIntPoint FertilityDimensions = FIntPoint::ZeroValue;
		FIntPoint BareRockDimensions = FIntPoint::ZeroValue;
		uint64 SiteSnapshotHash = 0;

		bool TryGetBaseCellIndex(const FVector2D& LocalPosition, int32& OutCellIndex) const
		{
			OutCellIndex = INDEX_NONE;
			if (!FMath::IsFinite(LocalPosition.X) || !FMath::IsFinite(LocalPosition.Y)
				|| !FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f || !LocalBounds.IsValid
				|| BaseDimensions.X <= 0 || BaseDimensions.Y <= 0
				|| LocalPosition.X < LocalBounds.Min.X || LocalPosition.X > LocalBounds.Max.X
				|| LocalPosition.Y < LocalBounds.Min.Y || LocalPosition.Y > LocalBounds.Max.Y)
			{
				return false;
			}
			const int32 CellX = FMath::Clamp(
				FMath::FloorToInt32((LocalPosition.X - LocalBounds.Min.X) / CellSizeCm), 0, BaseDimensions.X - 1);
			const int32 CellY = FMath::Clamp(
				FMath::FloorToInt32((LocalPosition.Y - LocalBounds.Min.Y) / CellSizeCm), 0, BaseDimensions.Y - 1);
			const int64 CellIndex = static_cast<int64>(CellX) + static_cast<int64>(CellY) * BaseDimensions.X;
			const int64 CellCount = static_cast<int64>(BaseDimensions.X) * BaseDimensions.Y;
			if (CellIndex < 0 || CellIndex >= CellCount || CellCount > MAX_int32)
			{
				return false;
			}
			OutCellIndex = static_cast<int32>(CellIndex);
			return true;
		}

		bool TryGetLevelCellRect(
			const int32 CellIndex,
			const uint8 Level,
			FVector2D& OutMinLocal,
			FVector2D& OutMaxLocal) const
		{
			if (Level > FArcEcotopeSiteMetadata::MaxGridLevel || !LocalBounds.IsValid
				|| !FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f)
			{
				return false;
			}
			const int32 Divisor = 1 << Level;
			const FIntPoint Dimensions(
				FMath::DivideAndRoundUp(BaseDimensions.X, Divisor),
				FMath::DivideAndRoundUp(BaseDimensions.Y, Divisor));
			const int64 CellCount = static_cast<int64>(Dimensions.X) * Dimensions.Y;
			if (Dimensions.X <= 0 || Dimensions.Y <= 0 || CellCount > MAX_int32
				|| CellIndex < 0 || CellIndex >= CellCount)
			{
				return false;
			}
			const int32 CellX = CellIndex % Dimensions.X;
			const int32 CellY = CellIndex / Dimensions.X;
			const double LevelCellSizeCm = static_cast<double>(CellSizeCm) * Divisor;
			const double CellMinX = LocalBounds.Min.X + static_cast<double>(CellX) * LevelCellSizeCm;
			const double CellMinY = LocalBounds.Min.Y + static_cast<double>(CellY) * LevelCellSizeCm;
			OutMinLocal = FVector2D(CellMinX, CellMinY);
			OutMaxLocal = FVector2D(
				FMath::Min(CellMinX + LevelCellSizeCm, static_cast<double>(LocalBounds.Max.X)),
				FMath::Min(CellMinY + LevelCellSizeCm, static_cast<double>(LocalBounds.Max.Y)));
			return true;
		}

		double GetBaseCellAreaSquareCentimeters(const int32 BaseCellIndex) const
		{
			FVector2D MinLocal = FVector2D::ZeroVector;
			FVector2D MaxLocal = FVector2D::ZeroVector;
			return TryGetLevelCellRect(BaseCellIndex, 0, MinLocal, MaxLocal)
				? FMath::Max(0.0, MaxLocal.X - MinLocal.X) * FMath::Max(0.0, MaxLocal.Y - MinLocal.Y)
				: 0.0;
		}
	};

	struct FChunkRecordSchedule
	{
		TArray<int32> Offsets;
		TArray<int32> RecordIndices;

		TConstArrayView<int32> GetRecords(const int32 ChunkIndex) const
		{
			if (!Offsets.IsValidIndex(ChunkIndex) || !Offsets.IsValidIndex(ChunkIndex + 1))
			{
				return TConstArrayView<int32>();
			}
			const int32 Begin = Offsets[ChunkIndex];
			const int32 End = Offsets[ChunkIndex + 1];
			return MakeArrayView(RecordIndices).Slice(Begin, End - Begin);
		}

		int64 GetAllocatedBytes() const
		{
			const uint64 Bytes = static_cast<uint64>(Offsets.GetAllocatedSize())
				+ static_cast<uint64>(RecordIndices.GetAllocatedSize());
			return Bytes <= static_cast<uint64>(MAX_int64) ? static_cast<int64>(Bytes) : MAX_int64;
		}
	};

	bool TryAddReservedInt32ArrayBytes(const int32 RequiredNum, uint64& InOutBytes)
	{
		if (RequiredNum < 0)
		{
			return false;
		}
		if (RequiredNum == 0)
		{
			return true;
		}
		const int32 ReservedNum = DefaultCalculateSlackReserve<int32>(
			RequiredNum, sizeof(int32), true, alignof(int32));
		if (ReservedNum < RequiredNum)
		{
			return false;
		}
		const uint64 Bytes = static_cast<uint64>(ReservedNum) * sizeof(int32);
		if (InOutBytes > MAX_uint64 - Bytes)
		{
			return false;
		}
		InOutBytes += Bytes;
		return true;
	}

	template <typename RecordType, typename ResolveChunkType>
	bool BuildChunkRecordSchedule(
		const TConstArrayView<RecordType> Records,
		const int32 ChunkCount,
		ResolveChunkType&& ResolveChunk,
		const TCHAR* RecordKind,
		FChunkRecordSchedule& OutSchedule,
		int64& InOutMaxScratchBytes,
		FText& OutError)
	{
		OutSchedule = FChunkRecordSchedule();
		if (ChunkCount <= 0)
		{
			OutError = FText::FromString(TEXT("ArcEcotope instance site layout has no chunks."));
			return false;
		}
		TArray<int32> Counts;
		Counts.Reserve(ChunkCount);
		Counts.SetNumZeroed(ChunkCount);
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			int32 ChunkIndex = INDEX_NONE;
			if (!ResolveChunk(Records[RecordIndex], ChunkIndex) || !Counts.IsValidIndex(ChunkIndex)
				|| Counts[ChunkIndex] == MAX_int32)
			{
				OutError = FText::FromString(FString::Printf(
					TEXT("ArcForest %s record %d cannot be assigned to exactly one site chunk."),
					RecordKind,
					RecordIndex));
				return false;
			}
			++Counts[ChunkIndex];
		}

		OutSchedule.Offsets.Reserve(ChunkCount + 1);
		OutSchedule.Offsets.SetNumUninitialized(ChunkCount + 1);
		OutSchedule.Offsets[0] = 0;
		int64 RunningCount = 0;
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			RunningCount += Counts[ChunkIndex];
			if (RunningCount > MAX_int32)
			{
				OutError = FText::FromString(FString::Printf(
					TEXT("ArcForest %s chunk schedule exceeds the supported record count."), RecordKind));
				return false;
			}
			OutSchedule.Offsets[ChunkIndex + 1] = static_cast<int32>(RunningCount);
		}
		if (RunningCount != Records.Num())
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcForest %s chunk schedule lost records while counting."), RecordKind));
			return false;
		}

		OutSchedule.RecordIndices.Reserve(Records.Num());
		OutSchedule.RecordIndices.SetNumUninitialized(Records.Num());
		TArray<int32> Cursors;
		Cursors.Reserve(ChunkCount);
		Cursors.SetNumUninitialized(ChunkCount);
		const uint64 ScratchBytes = static_cast<uint64>(Counts.GetAllocatedSize())
			+ static_cast<uint64>(Cursors.GetAllocatedSize());
		if (ScratchBytes > static_cast<uint64>(MAX_int64))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcForest %s chunk schedule scratch allocation is not representable."), RecordKind));
			return false;
		}
		InOutMaxScratchBytes = FMath::Max(InOutMaxScratchBytes, static_cast<int64>(ScratchBytes));
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			Cursors[ChunkIndex] = OutSchedule.Offsets[ChunkIndex];
		}
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			int32 ChunkIndex = INDEX_NONE;
			if (!ResolveChunk(Records[RecordIndex], ChunkIndex) || !Cursors.IsValidIndex(ChunkIndex)
				|| Cursors[ChunkIndex] >= OutSchedule.Offsets[ChunkIndex + 1])
			{
				OutError = FText::FromString(FString::Printf(
					TEXT("ArcForest %s chunk schedule changed during stable fill."), RecordKind));
				return false;
			}
			OutSchedule.RecordIndices[Cursors[ChunkIndex]++] = RecordIndex;
		}
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			if (Cursors[ChunkIndex] != OutSchedule.Offsets[ChunkIndex + 1])
			{
				OutError = FText::FromString(FString::Printf(
					TEXT("ArcForest %s chunk schedule did not assign every record exactly once."), RecordKind));
				return false;
			}
		}
		return true;
	}

	bool TryGetChunkIndexForBaseCell(
		const FArcEcotopeChunkLayout& Layout,
		const FInstanceSiteGeometry& Geometry,
		const int32 BaseCellIndex,
		int32& OutChunkIndex)
	{
		OutChunkIndex = INDEX_NONE;
		const int64 BaseCellCount = static_cast<int64>(Geometry.BaseDimensions.X) * Geometry.BaseDimensions.Y;
		if (Geometry.BaseDimensions.X <= 0 || BaseCellIndex < 0 || BaseCellIndex >= BaseCellCount)
		{
			return false;
		}
		FArcEcotopeChunkId ChunkId;
		if (!Layout.FindChunkForBaseCell(
				FIntPoint(BaseCellIndex % Geometry.BaseDimensions.X, BaseCellIndex / Geometry.BaseDimensions.X),
				ChunkId))
		{
			return false;
		}
		const FIntPoint ChunkDimensions = Layout.GetChunkGridDimensions();
		const int64 ChunkIndex = static_cast<int64>(ChunkId.X) + static_cast<int64>(ChunkId.Y) * ChunkDimensions.X;
		if (ChunkIndex < 0 || ChunkIndex >= Layout.GetChunkCount())
		{
			return false;
		}
		OutChunkIndex = static_cast<int32>(ChunkIndex);
		return true;
	}

	bool TryGetChunkIndexForRecordCell(
		const FArcEcotopeChunkLayout& Layout,
		const FInstanceSiteGeometry& Geometry,
		const int32 RecordCellIndex,
		int32& OutChunkIndex)
	{
		OutChunkIndex = INDEX_NONE;
		const int64 RecordCellCount = static_cast<int64>(Geometry.RecordDimensions.X) * Geometry.RecordDimensions.Y;
		if (Geometry.RecordDimensions.X <= 0 || RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return false;
		}
		FArcEcotopeChunkId ChunkId;
		if (!Layout.FindChunkForRecordCell(
				FIntPoint(RecordCellIndex % Geometry.RecordDimensions.X, RecordCellIndex / Geometry.RecordDimensions.X),
				ChunkId))
		{
			return false;
		}
		const FIntPoint ChunkDimensions = Layout.GetChunkGridDimensions();
		const int64 ChunkIndex = static_cast<int64>(ChunkId.X) + static_cast<int64>(ChunkId.Y) * ChunkDimensions.X;
		if (ChunkIndex < 0 || ChunkIndex >= Layout.GetChunkCount())
		{
			return false;
		}
		OutChunkIndex = static_cast<int32>(ChunkIndex);
		return true;
	}

	bool TryGatherPlantableBaseCells(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteByteFieldView& ValidSurface,
		const int32 RecordCellIndex,
		TArray<int32>& OutBaseCellIndices)
	{
		OutBaseCellIndices.Reset();
		const int64 RecordCellCount = static_cast<int64>(Geometry.RecordDimensions.X) * Geometry.RecordDimensions.Y;
		if (Geometry.RecordDimensions.X <= 0 || RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return false;
		}
		const int32 RecordX = RecordCellIndex % Geometry.RecordDimensions.X;
		const int32 RecordY = RecordCellIndex / Geometry.RecordDimensions.X;
		const int32 ChildSpan = 1 << Geometry.RecordGridLevel;
		const int32 BaseMinX = RecordX * ChildSpan;
		const int32 BaseMinY = RecordY * ChildSpan;
		OutBaseCellIndices.Reserve(FMath::Min(ChildSpan * ChildSpan, 256));
		for (int32 OffsetY = 0; OffsetY < ChildSpan; ++OffsetY)
		{
			const int32 BaseY = BaseMinY + OffsetY;
			if (BaseY >= Geometry.BaseDimensions.Y)
			{
				break;
			}
			for (int32 OffsetX = 0; OffsetX < ChildSpan; ++OffsetX)
			{
				const int32 BaseX = BaseMinX + OffsetX;
				if (BaseX >= Geometry.BaseDimensions.X)
				{
					break;
				}
				const uint8* Valid = ValidSurface.TryGet(BaseX, BaseY);
				if (Valid == nullptr)
				{
					return false;
				}
				if (*Valid != 0)
				{
					OutBaseCellIndices.Add(BaseX + BaseY * Geometry.BaseDimensions.X);
				}
			}
		}
		return true;
	}

	/**
	 * Separate from TryGatherPlantableBaseCells because that one also feeds ground cover, which keeps
	 * scattering to the water's edge while trees are cut out of the channel buffer's hard zone.
	 */
	bool TryGatherTreePlantableBaseCells(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteByteFieldView& ValidSurface,
		const FArcEcotopeSiteFloatFieldView& ChannelBuffer,
		const int32 RecordCellIndex,
		TArray<int32>& OutBaseCellIndices,
		bool& bOutAllExcludedByChannelBuffer)
	{
		bOutAllExcludedByChannelBuffer = false;
		if (!TryGatherPlantableBaseCells(Geometry, ValidSurface, RecordCellIndex, OutBaseCellIndices))
		{
			return false;
		}
		if (OutBaseCellIndices.IsEmpty())
		{
			return true;
		}
		int32 SurvivorCount = 0;
		for (int32 Index = 0; Index < OutBaseCellIndices.Num(); ++Index)
		{
			const int32 BaseCellIndex = OutBaseCellIndices[Index];
			const int32 BaseX = BaseCellIndex % Geometry.BaseDimensions.X;
			const int32 BaseY = BaseCellIndex / Geometry.BaseDimensions.X;
			const float* Buffer = ChannelBuffer.TryGet(BaseX, BaseY);
			if (Buffer == nullptr)
			{
				return false;
			}
			if (*Buffer < 1.0f)
			{
				OutBaseCellIndices[SurvivorCount] = BaseCellIndex;
				++SurvivorCount;
			}
		}
		OutBaseCellIndices.SetNum(SurvivorCount, EAllowShrinking::No);
		bOutAllExcludedByChannelBuffer = OutBaseCellIndices.IsEmpty();
		return true;
	}

	/**
	 * Separate from TryGatherPlantableBaseCells because a clump straddles into neighbouring record
	 * cells, while that one also sizes the record's own instance count and must stay unwidened.
	 */
	bool TryGatherClusterPlantableBaseCells(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteByteFieldView& ValidSurface,
		const int32 RecordCellIndex,
		const int32 HaloBaseCells,
		TArray<int32>& OutBaseCellIndices)
	{
		OutBaseCellIndices.Reset();
		const int64 RecordCellCount = static_cast<int64>(Geometry.RecordDimensions.X) * Geometry.RecordDimensions.Y;
		if (Geometry.RecordDimensions.X <= 0 || RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount
			|| HaloBaseCells < 0 || Geometry.BaseDimensions.X <= 0 || Geometry.BaseDimensions.Y <= 0)
		{
			return false;
		}
		const int32 RecordX = RecordCellIndex % Geometry.RecordDimensions.X;
		const int32 RecordY = RecordCellIndex / Geometry.RecordDimensions.X;
		const int32 ChildSpan = 1 << Geometry.RecordGridLevel;
		const int32 MinX = FMath::Max(RecordX * ChildSpan - HaloBaseCells, 0);
		const int32 MinY = FMath::Max(RecordY * ChildSpan - HaloBaseCells, 0);
		const int32 MaxX = FMath::Min(RecordX * ChildSpan + ChildSpan - 1 + HaloBaseCells, Geometry.BaseDimensions.X - 1);
		const int32 MaxY = FMath::Min(RecordY * ChildSpan + ChildSpan - 1 + HaloBaseCells, Geometry.BaseDimensions.Y - 1);
		OutBaseCellIndices.Reserve(FMath::Min((MaxX - MinX + 1) * (MaxY - MinY + 1), 1024));
		// Y outer, X inner keeps the indices ascending for the membership binary search.
		for (int32 BaseY = MinY; BaseY <= MaxY; ++BaseY)
		{
			for (int32 BaseX = MinX; BaseX <= MaxX; ++BaseX)
			{
				const uint8* Valid = ValidSurface.TryGet(BaseX, BaseY);
				if (Valid != nullptr && *Valid != 0)
				{
					OutBaseCellIndices.Add(BaseX + BaseY * Geometry.BaseDimensions.X);
				}
			}
		}
		return true;
	}

	double ComputePlantableAreaSquareCm(
		const FInstanceSiteGeometry& Geometry,
		const TArray<int32>& PlantableBaseCells)
	{
		double AreaSquareCm = 0.0;
		for (const int32 BaseCellIndex : PlantableBaseCells)
		{
			AreaSquareCm += Geometry.GetBaseCellAreaSquareCentimeters(BaseCellIndex);
		}
		return AreaSquareCm;
	}

	struct FInstanceSiteContext
	{
		FInstanceSiteGeometry Geometry;
		FArcEcotopeChunkLayout Layout;
		FArcEcotopeSiteChunkReader Reader;
		FArcEcotopeFrozenSiteSourcePtr FrozenSource;
		int64 ReaderBudgetBytes = 0;
	};

	bool OpenInstanceSiteContext(
		const AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FInstanceSiteContext& OutContext,
		FText& OutError)
	{
		OutContext = FInstanceSiteContext();
		OutContext.ReaderBudgetBytes = static_cast<int64>(FMath::Max(
			CVarInstanceSiteReaderBudgetMiB.GetValueOnGameThread(), 1)) * 1024ll * 1024ll;
		if (!Volume.FreezeSiteChunkSource(OutContext.FrozenSource, OutError))
		{
			return false;
		}
		const FArcEcotopeSiteMetadata& ActiveMetadata = OutContext.FrozenSource->Metadata;
		OutContext.Layout = OutContext.FrozenSource->Layout;
		FString ReaderError;
		if (!OutContext.Reader.OpenEditorBulk(
				OutContext.FrozenSource, OutContext.ReaderBudgetBytes, ReaderError))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope Instance Builder could not open its site reader: %s"), *ReaderError));
			return false;
		}
		const FIntPoint RecordDimensions = ActiveMetadata.GetLevelDimensions(ActiveMetadata.RecordGridLevel);
		const int64 RecordCount = static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y;
		if (ActiveMetadata.SiteSnapshotHash == 0
			|| ActiveMetadata.SiteSnapshotHash != Snapshot.SiteSnapshotHash
			|| RecordCount <= 0 || RecordCount > MAX_int32
			|| OutContext.Layout.GetBaseDimensions() != ActiveMetadata.GridDimensions
			|| OutContext.Layout.GetRecordDimensions() != RecordDimensions
			|| OutContext.Layout.GetChunkCount() <= 0)
		{
			OutError = FText::FromString(
				TEXT("ArcEcotope Instance Builder snapshot and site-reader metadata are incompatible."));
			return false;
		}

		OutContext.Geometry.LocalBounds = ActiveMetadata.LocalBounds;
		OutContext.Geometry.CellSizeCm = ActiveMetadata.CellSizeCm;
		OutContext.Geometry.BaseDimensions = ActiveMetadata.GridDimensions;
		OutContext.Geometry.RecordDimensions = RecordDimensions;
		OutContext.Geometry.RecordGridLevel = ActiveMetadata.RecordGridLevel;
		OutContext.Geometry.MoistureLevel = ActiveMetadata.MoistureLevel;
		OutContext.Geometry.FertilityLevel = ActiveMetadata.FertilityLevel;
		OutContext.Geometry.BareRockLevel = ActiveMetadata.BareRockLevel;
		OutContext.Geometry.MoistureDimensions =
			ActiveMetadata.GetLevelDimensions(ActiveMetadata.MoistureLevel);
		OutContext.Geometry.FertilityDimensions =
			ActiveMetadata.GetLevelDimensions(ActiveMetadata.FertilityLevel);
		OutContext.Geometry.BareRockDimensions =
			ActiveMetadata.GetLevelDimensions(ActiveMetadata.BareRockLevel);
		OutContext.Geometry.SiteSnapshotHash = ActiveMetadata.SiteSnapshotHash;
		return true;
	}

	bool ContainsRect(const FIntRect& Outer, const FIntRect& Inner)
	{
		return Outer.Min.X <= Inner.Min.X && Outer.Min.Y <= Inner.Min.Y
			&& Outer.Max.X >= Inner.Max.X && Outer.Max.Y >= Inner.Max.Y;
	}

	bool ValidateInstanceChunkView(
		const FInstanceSiteContext& SiteContext,
		const FArcEcotopeChunkRect& Chunk,
		const FArcEcotopeSiteChunkView& View,
		const int32 BaseHaloCells,
		FText& OutError)
	{
		FIntRect ExpectedRecordRect;
		FIntRect ExpectedMoistureRect;
		FIntRect ExpectedFertilityRect;
		FIntRect ExpectedBareRockRect;
		if (BaseHaloCells < 0
			|| !FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
				Chunk.BaseCoreRect,
				SiteContext.Geometry.BaseDimensions,
				SiteContext.Geometry.RecordGridLevel,
				ExpectedRecordRect)
			|| !FArcEcotopeSiteChunkReader::TryMapBaseReadRectToLevel(
				Chunk.BaseCoreRect,
				SiteContext.Geometry.BaseDimensions,
				SiteContext.Geometry.MoistureLevel,
				ExpectedMoistureRect)
			|| !FArcEcotopeSiteChunkReader::TryMapBaseReadRectToLevel(
				Chunk.BaseCoreRect,
				SiteContext.Geometry.BaseDimensions,
				SiteContext.Geometry.FertilityLevel,
				ExpectedFertilityRect)
			|| !FArcEcotopeSiteChunkReader::TryMapBaseReadRectToLevel(
				Chunk.BaseCoreRect,
				SiteContext.Geometry.BaseDimensions,
				SiteContext.Geometry.BareRockLevel,
				ExpectedBareRockRect)
			|| (View.FieldMask & InstanceSiteFieldMask) != InstanceSiteFieldMask
			|| View.Id.X != Chunk.Id.X || View.Id.Y != Chunk.Id.Y
			|| View.BaseCoreRect != Chunk.BaseCoreRect
			|| !ContainsRect(View.BaseReadRect, Chunk.BaseCoreRect)
			|| View.BaseReadRect != FArcEcotopeSiteChunkReader::MakeBaseReadRect(
				Chunk.BaseCoreRect, SiteContext.Geometry.BaseDimensions, BaseHaloCells)
			|| !View.LocalSurfaceZ.IsValid() || View.LocalSurfaceZ.Level != 0
			|| View.LocalSurfaceZ.GlobalDimensions != SiteContext.Geometry.BaseDimensions
			|| !ContainsRect(View.LocalSurfaceZ.GlobalRect, Chunk.BaseCoreRect)
			|| !View.ValidSurface.IsValid() || View.ValidSurface.Level != 0
			|| View.ValidSurface.GlobalDimensions != SiteContext.Geometry.BaseDimensions
			|| !ContainsRect(View.ValidSurface.GlobalRect, Chunk.BaseCoreRect)
			|| !View.Slope.IsValid() || View.Slope.Level != 0
			|| View.Slope.GlobalDimensions != SiteContext.Geometry.BaseDimensions
			|| !ContainsRect(View.Slope.GlobalRect, Chunk.BaseCoreRect)
			|| !View.Aspect.IsValid() || View.Aspect.Level != 0
			|| View.Aspect.GlobalDimensions != SiteContext.Geometry.BaseDimensions
			|| !ContainsRect(View.Aspect.GlobalRect, Chunk.BaseCoreRect)
			|| !View.ChannelBuffer.IsValid() || View.ChannelBuffer.Level != 0
			|| View.ChannelBuffer.GlobalDimensions != SiteContext.Geometry.BaseDimensions
			|| !ContainsRect(View.ChannelBuffer.GlobalRect, Chunk.BaseCoreRect)
			|| !View.RecordValid.IsValid()
			|| View.RecordValid.Level != SiteContext.Geometry.RecordGridLevel
			|| View.RecordValid.GlobalDimensions != SiteContext.Geometry.RecordDimensions
			|| !ContainsRect(View.RecordValid.GlobalRect, ExpectedRecordRect)
			|| !View.Moisture.IsValid()
			|| View.Moisture.Level != SiteContext.Geometry.MoistureLevel
			|| View.Moisture.GlobalDimensions != SiteContext.Geometry.MoistureDimensions
			|| !ContainsRect(View.Moisture.GlobalRect, ExpectedMoistureRect)
			|| !View.Fertility.IsValid()
			|| View.Fertility.Level != SiteContext.Geometry.FertilityLevel
			|| View.Fertility.GlobalDimensions != SiteContext.Geometry.FertilityDimensions
			|| !ContainsRect(View.Fertility.GlobalRect, ExpectedFertilityRect)
			|| !View.BareRock.IsValid()
			|| View.BareRock.Level != SiteContext.Geometry.BareRockLevel
			|| View.BareRock.GlobalDimensions != SiteContext.Geometry.BareRockDimensions
			|| !ContainsRect(View.BareRock.GlobalRect, ExpectedBareRockRect))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope Instance Builder acquired an invalid field view for site chunk (%d,%d)."),
				Chunk.Id.X,
				Chunk.Id.Y));
			return false;
		}
		return true;
	}

	void AddCanonicalString(UE::ArcForest::FKeyedHash64& Hash, const FString& Value)
	{
		Hash.AddInt32(Value.Len());
		for (const TCHAR Character : Value)
		{
			Hash.AddUInt32(static_cast<uint32>(Character));
		}
	}

	template <typename ValueType>
	uint64 GetCanonicalValueBits(const ValueType Value)
	{
		static_assert(sizeof(ValueType) <= sizeof(uint64));
		if (Value == static_cast<ValueType>(0))
		{
			return 0;
		}
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(ValueType));
		return Bits;
	}

	void AddCanonicalTransform(UE::ArcForest::FKeyedHash64& Hash, const FTransform& Transform)
	{
		const FVector Translation = Transform.GetTranslation();
		const FQuat Rotation = Transform.GetRotation();
		const FVector Scale = Transform.GetScale3D();
		Hash.AddUInt64(GetCanonicalValueBits(Translation.X));
		Hash.AddUInt64(GetCanonicalValueBits(Translation.Y));
		Hash.AddUInt64(GetCanonicalValueBits(Translation.Z));
		Hash.AddUInt64(GetCanonicalValueBits(Rotation.X));
		Hash.AddUInt64(GetCanonicalValueBits(Rotation.Y));
		Hash.AddUInt64(GetCanonicalValueBits(Rotation.Z));
		Hash.AddUInt64(GetCanonicalValueBits(Rotation.W));
		Hash.AddUInt64(GetCanonicalValueBits(Scale.X));
		Hash.AddUInt64(GetCanonicalValueBits(Scale.Y));
		Hash.AddUInt64(GetCanonicalValueBits(Scale.Z));
	}

	uint64 MakeNonEntityGroupIdentity(const FGroup& Group, const uint8 RendererKind)
	{
		constexpr uint64 GroupIdentityKey = 0x8A36E41BC25F970Dull;
		UE::ArcForest::FKeyedHash64 Hash(GroupIdentityKey);
		Hash.AddUInt8(RendererKind);
		Hash.AddInt32(Group.Key.CellCoord.X);
		Hash.AddInt32(Group.Key.CellCoord.Y);
		Hash.AddInt32(Group.Key.CellCoord.Z);
		Hash.AddGuid(Group.Key.SpeciesId);
		Hash.AddUInt8(Group.Key.Kind);
		Hash.AddUInt32(static_cast<uint32>(GetCanonicalValueBits(Group.Key.MinHeightCm)));
		Hash.AddUInt32(static_cast<uint32>(GetCanonicalValueBits(Group.Key.MaxHeightCm)));
		Hash.AddUInt64(Group.Key.RendererHash);
		Hash.AddInt32(Group.Key.VariantIndex);
		Hash.AddUInt64(Group.NameHash);
		Hash.AddUInt32(Group.GridSizeCm);
		AddCanonicalString(Hash, Group.RuntimeGridName.ToString());
		return Hash.Finalize();
	}

	bool AddCanonicalCount(
		TMap<FString, int64>& Counts,
		const FString& Identity,
		const int64 Increment,
		FText& OutError)
	{
		int64& Count = Counts.FindOrAdd(Identity);
		if (Increment < 0 || Count > MAX_int64 - Increment)
		{
			OutError = FText::FromString(TEXT("ArcEcotope canonical instance diagnostic count overflowed."));
			return false;
		}
		Count += Increment;
		return true;
	}

	struct FCanonicalOutputDiagnostic
	{
		uint64 Digest = 0;
		int64 TupleCount = 0;
		int32 GroupCount = 0;
		int32 ActorCount = 0;
		int64 NonEntityTupleCount = 0;
		int64 EntityTupleCount = 0;
	};

	bool BuildCanonicalOutputDiagnostic(
		const TConstArrayView<FGroup> Groups,
		const FArcEcotopePreparedInstances& Prepared,
		FCanonicalOutputDiagnostic& OutDiagnostic,
		FText& OutError)
	{
		constexpr uint64 TupleKey = 0xE96721AC438B50DFull;
		constexpr uint64 CountKey = 0x5C18D4F2A93E706Bull;
		constexpr uint64 AggregateKey = 0xB7438D19E2605CAFull;
		OutDiagnostic = FCanonicalOutputDiagnostic();
		TArray<uint64> TupleDigests;
		TArray<uint64> GroupCountDigests;
		TArray<uint64> ActorCountDigests;
		TMap<FString, int64> ActorCounts;
		TMap<FString, int64> EntityGroupCounts;

		for (const FGroup& Group : Groups)
		{
			if (GetGroupVisual<FArcForestBandVisual_Entity>(Group) != nullptr)
			{
				continue;
			}
			// 0 and 1 are output, not an internal enum: they reach the canonical digest and the actor
			// identity string, and must keep agreeing with MakeInstanceGridGuid's own kind byte.
			const uint8 RendererKind = GetGroupVisual<FArcForestBandVisual_StaticMesh>(Group) != nullptr ? 0u
				: GetGroupVisual<FArcForestBandVisual_SkinnedMesh>(Group) != nullptr ? 1u : MAX_uint8;
			if (RendererKind == MAX_uint8 || Group.Transforms.Num() > MAX_int64 - OutDiagnostic.NonEntityTupleCount)
			{
				OutError = FText::FromString(TEXT("ArcEcotope canonical diagnostic encountered an invalid non-entity group."));
				return false;
			}
			const uint64 GroupIdentity = MakeNonEntityGroupIdentity(Group, RendererKind);
			UE::ArcForest::FKeyedHash64 GroupCountHash(CountKey);
			GroupCountHash.AddUInt8(0);
			GroupCountHash.AddUInt64(GroupIdentity);
			GroupCountHash.AddInt64(Group.Transforms.Num());
			GroupCountDigests.Add(GroupCountHash.Finalize());
			OutDiagnostic.NonEntityTupleCount += Group.Transforms.Num();

			// Renderer kind selects the partition actor class in MakeInstanceGridGuid; together with cell,
			// grid size and runtime grid it is the stable logical actor identity. No pointer is hashed.
			const FString ActorIdentity = FString::Printf(
				TEXT("I|%u|%d|%d|%d|%u|%s"),
				RendererKind,
				Group.Key.CellCoord.X,
				Group.Key.CellCoord.Y,
				Group.Key.CellCoord.Z,
				Group.GridSizeCm,
				*Group.RuntimeGridName.ToString());
			if (!AddCanonicalCount(ActorCounts, ActorIdentity, Group.Transforms.Num(), OutError))
			{
				return false;
			}
			for (int32 TransformIndex = 0; TransformIndex < Group.Transforms.Num(); ++TransformIndex)
			{
				UE::ArcForest::FKeyedHash64 TupleHash(TupleKey);
				TupleHash.AddUInt8(0);
				TupleHash.AddUInt64(GroupIdentity);
				AddCanonicalTransform(TupleHash, Group.Transforms[TransformIndex]);
				TupleHash.AddInt32(Group.AnimationIndices.IsValidIndex(TransformIndex)
					? Group.AnimationIndices[TransformIndex] : INDEX_NONE);
				TupleDigests.Add(TupleHash.Finalize());
			}
		}

		for (const FArcEcotopePreparedEntityWrite& Write : Prepared.EntityWrites)
		{
			const AArcPlacedEntityPartitionActor* Actor = Write.Actor.Get();
			const UObject* Config = Write.Config.Get();
			if (Actor == nullptr || Config == nullptr
				|| Write.RelativeTransforms.Num() > MAX_int64 - OutDiagnostic.EntityTupleCount)
			{
				OutError = FText::FromString(TEXT("ArcEcotope canonical diagnostic encountered an incomplete entity write."));
				return false;
			}
			const FString ConfigPath = Config->GetPathName();
			const FString ActorPackage = Actor->GetPackage() != nullptr ? Actor->GetPackage()->GetName() : FString();
			const FString ActorIdentity = FString::Printf(
				TEXT("E|%s|%s"),
				*Actor->GetActorGuid().ToString(EGuidFormats::Digits),
				*ActorPackage);
			const FString GroupIdentity = ActorIdentity + TEXT("|") + ConfigPath;
			if (ConfigPath.IsEmpty() || ActorPackage.IsEmpty()
				|| !AddCanonicalCount(ActorCounts, ActorIdentity, Write.RelativeTransforms.Num(), OutError)
				|| !AddCanonicalCount(EntityGroupCounts, GroupIdentity, Write.RelativeTransforms.Num(), OutError))
			{
				if (OutError.IsEmpty())
				{
					OutError = FText::FromString(TEXT("ArcEcotope canonical diagnostic could not form stable entity identities."));
				}
				return false;
			}
			OutDiagnostic.EntityTupleCount += Write.RelativeTransforms.Num();
			for (const FTransform& RelativeTransform : Write.RelativeTransforms)
			{
				UE::ArcForest::FKeyedHash64 TupleHash(TupleKey);
				TupleHash.AddUInt8(1);
				AddCanonicalString(TupleHash, ConfigPath);
				AddCanonicalString(TupleHash, ActorIdentity);
				AddCanonicalTransform(TupleHash, RelativeTransform);
				TupleHash.AddInt32(INDEX_NONE);
				TupleDigests.Add(TupleHash.Finalize());
			}
		}

		for (const TPair<FString, int64>& Pair : EntityGroupCounts)
		{
			UE::ArcForest::FKeyedHash64 CountHash(CountKey);
			CountHash.AddUInt8(1);
			AddCanonicalString(CountHash, Pair.Key);
			CountHash.AddInt64(Pair.Value);
			GroupCountDigests.Add(CountHash.Finalize());
		}
		for (const TPair<FString, int64>& Pair : ActorCounts)
		{
			UE::ArcForest::FKeyedHash64 CountHash(CountKey);
			AddCanonicalString(CountHash, Pair.Key);
			CountHash.AddInt64(Pair.Value);
			ActorCountDigests.Add(CountHash.Finalize());
		}

		TupleDigests.Sort();
		GroupCountDigests.Sort();
		ActorCountDigests.Sort();
		UE::ArcForest::FKeyedHash64 Aggregate(AggregateKey);
		Aggregate.AddInt64(TupleDigests.Num());
		for (const uint64 Digest : TupleDigests)
		{
			Aggregate.AddUInt64(Digest);
		}
		Aggregate.AddInt32(GroupCountDigests.Num());
		for (const uint64 Digest : GroupCountDigests)
		{
			Aggregate.AddUInt64(Digest);
		}
		Aggregate.AddInt32(ActorCountDigests.Num());
		for (const uint64 Digest : ActorCountDigests)
		{
			Aggregate.AddUInt64(Digest);
		}
		OutDiagnostic.Digest = Aggregate.Finalize();
		OutDiagnostic.TupleCount = TupleDigests.Num();
		OutDiagnostic.GroupCount = GroupCountDigests.Num();
		OutDiagnostic.ActorCount = ActorCountDigests.Num();
		return true;
	}

	int32 GetMaxPatchInstancesPerRecord()
	{
		return FMath::Max(CVarMaxPatchInstancesPerRecord.GetValueOnGameThread(), 0);
	}

	int32 GetPatchClusterInstances()
	{
		const int32 Value = CVarPatchClusterInstances.GetValueOnGameThread();
		return Value > 0 ? Value : 0;
	}

	double GetPatchClusterRadiusCm()
	{
		const float Value = CVarPatchClusterRadiusCm.GetValueOnGameThread();
		return FMath::IsFinite(Value) ? FMath::Max(static_cast<double>(Value), 0.0) : 0.0;
	}

	/**
	 * A patch equivalent is a cover measure for colonial growth, not a head-count, so it sets a drawn
	 * density over the cell's plantable area instead of one mesh per unit. The density ceiling applies
	 * to the record cell's total across every species sharing it, not to this record alone -- one patch
	 * record exists per (species, cell), so per-record clamping let each cover species draw up to the
	 * ceiling independently and a two-species cell rendered twice as dense as the cap allows.
	 */
	int64 ComputePatchInstanceCount(const double CoverEquivalent, const double CellTotalEquivalent, const double PlantableAreaSquareCm, const int32 MaxInstances)
	{
		const int64 ClampedMax = FMath::Max(static_cast<int64>(MaxInstances), static_cast<int64>(0));
		if (!(CoverEquivalent > 0.0) || !(CellTotalEquivalent > 0.0) || !(PlantableAreaSquareCm > 0.0) || ClampedMax <= 0)
		{
			return 0;
		}
		// Double throughout: Density * Area overflows int32 long before it overflows a double.
		const double AreaSqM = PlantableAreaSquareCm / 10000.0;
		const double CellDensityPerSqM = FMath::Min(CellTotalEquivalent / AreaSqM, GetPatchInstanceDensityPerSqM());
		if (!(CellDensityPerSqM > 0.0))
		{
			return 0;
		}
		// A patch that is drawn at all draws at least once; rounding a sub-unit count to zero would delete it visually.
		const double Count = FMath::Max(FMath::RoundToDouble(CellDensityPerSqM * AreaSqM * (CoverEquivalent / CellTotalEquivalent)), 1.0);
		return static_cast<int64>(FMath::Clamp(Count, 0.0, static_cast<double>(ClampedMax)));
	}

	struct FSurfaceSample
	{
		double SurfaceZCm = 0.0;
		FVector Normal = FVector::UpVector;
	};

	bool SampleSurface(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteFloatFieldView& LocalSurfaceZ,
		const FArcEcotopeSiteByteFieldView& ValidSurface,
		const FArcEcotopeSiteFloatFieldView& Slope,
		const FArcEcotopeSiteFloatFieldView& Aspect,
		const FVector2D& LocalXY,
		FSurfaceSample& OutSample)
	{
		int32 CellIndex = INDEX_NONE;
		if (!Geometry.TryGetBaseCellIndex(LocalXY, CellIndex))
		{
			return false;
		}
		const int32 CellX = CellIndex % Geometry.BaseDimensions.X;
		const int32 CellY = CellIndex / Geometry.BaseDimensions.X;
		FVector2D CellMinLocal = FVector2D::ZeroVector;
		FVector2D CellMaxLocal = FVector2D::ZeroVector;
		if (!Geometry.TryGetLevelCellRect(CellIndex, 0, CellMinLocal, CellMaxLocal))
		{
			return false;
		}
		const FVector2D CellCentre = (CellMinLocal + CellMaxLocal) * 0.5;
		const int32 NeighbourX = FMath::Clamp(
			CellX + (LocalXY.X >= CellCentre.X ? 1 : -1), 0, Geometry.BaseDimensions.X - 1);
		const int32 NeighbourY = FMath::Clamp(
			CellY + (LocalXY.Y >= CellCentre.Y ? 1 : -1), 0, Geometry.BaseDimensions.Y - 1);
		FVector2D NeighbourXMinLocal = FVector2D::ZeroVector;
		FVector2D NeighbourXMaxLocal = FVector2D::ZeroVector;
		FVector2D NeighbourYMinLocal = FVector2D::ZeroVector;
		FVector2D NeighbourYMaxLocal = FVector2D::ZeroVector;
		if (!Geometry.TryGetLevelCellRect(
				NeighbourX + CellY * Geometry.BaseDimensions.X, 0, NeighbourXMinLocal, NeighbourXMaxLocal)
			|| !Geometry.TryGetLevelCellRect(
				CellX + NeighbourY * Geometry.BaseDimensions.X, 0, NeighbourYMinLocal, NeighbourYMaxLocal))
		{
			return false;
		}
		// Weights come from the real spacing of cell centres: the trailing column and row are partial, so CellSizeCm is not the step there.
		const double SpacingX = FMath::Abs((NeighbourXMinLocal.X + NeighbourXMaxLocal.X) * 0.5 - CellCentre.X);
		const double SpacingY = FMath::Abs((NeighbourYMinLocal.Y + NeighbourYMaxLocal.Y) * 0.5 - CellCentre.Y);
		const double Fx = SpacingX > 0.0
			? FMath::Clamp(FMath::Abs(LocalXY.X - CellCentre.X) / SpacingX, 0.0, 1.0)
			: 0.0;
		const double Fy = SpacingY > 0.0
			? FMath::Clamp(FMath::Abs(LocalXY.Y - CellCentre.Y) / SpacingY, 0.0, 1.0)
			: 0.0;
		const int32 TapX[4] = { CellX, NeighbourX, CellX, NeighbourX };
		const int32 TapY[4] = { CellY, CellY, NeighbourY, NeighbourY };
		const double TapWeight[4] = {
			(1.0 - Fx) * (1.0 - Fy),
			Fx * (1.0 - Fy),
			(1.0 - Fx) * Fy,
			Fx * Fy };
		double WeightSum = 0.0;
		double SurfaceZSum = 0.0;
		FVector NormalSum = FVector::ZeroVector;
		for (int32 TapIndex = 0; TapIndex < 4; ++TapIndex)
		{
			const float* SurfaceZ = LocalSurfaceZ.TryGet(TapX[TapIndex], TapY[TapIndex]);
			const uint8* Valid = ValidSurface.TryGet(TapX[TapIndex], TapY[TapIndex]);
			const float* SlopeDegrees = Slope.TryGet(TapX[TapIndex], TapY[TapIndex]);
			const float* AspectTurns = Aspect.TryGet(TapX[TapIndex], TapY[TapIndex]);
			if (SurfaceZ == nullptr || Valid == nullptr || *Valid == 0
				|| SlopeDegrees == nullptr || AspectTurns == nullptr)
			{
				continue;
			}
			const double Weight = TapWeight[TapIndex];
			const double Theta = FMath::DegreesToRadians(static_cast<double>(*SlopeDegrees));
			const double Phi = static_cast<double>(*AspectTurns) * 2.0 * PI;
			WeightSum += Weight;
			SurfaceZSum += Weight * static_cast<double>(*SurfaceZ);
			NormalSum += Weight * FVector(
				FMath::Sin(Theta) * FMath::Cos(Phi),
				FMath::Sin(Theta) * FMath::Sin(Phi),
				FMath::Cos(Theta));
		}
		if (!(WeightSum > 0.0))
		{
			return false;
		}
		OutSample.SurfaceZCm = SurfaceZSum / WeightSum;
		OutSample.Normal = NormalSum.GetSafeNormal();
		if (OutSample.Normal.IsZero())
		{
			OutSample.Normal = FVector::UpVector;
		}
		return true;
	}

	bool TryMakeRepresentativePosition(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteFloatFieldView& LocalSurfaceZ,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 RecordCellIndex,
		const TArray<int32>& PlantableBaseCells,
		const uint64 EntityId,
		const int32 Ordinal,
		FVector& OutPosition,
		FText& OutError)
	{
		const uint64 CellKey = UE::ArcForest::MakeAnnualKey(Snapshot.SimulationId, Snapshot.Seed, Snapshot.CurrentYear, UE::ArcForest::EDeterministicDomain::InstanceRepresentativeCell, SpeciesId, RecordCellIndex, EntityId, Ordinal, 0);
		// The record cell is coarser than the sampled surface, so an instance is placed
		// inside one of its base children that carries a valid surface.
		const int32 BaseCellIndex = PlantableBaseCells.IsEmpty()
			? INDEX_NONE
			: PlantableBaseCells[FMath::Min(
				PlantableBaseCells.Num() - 1,
				static_cast<int32>(UE::ArcForest::KeyToUnitOpen(CellKey) * static_cast<double>(PlantableBaseCells.Num())))];
		FVector2D CellMinLocal = FVector2D::ZeroVector;
		FVector2D CellMaxLocal = FVector2D::ZeroVector;
		const int32 BaseCellX = BaseCellIndex != INDEX_NONE && Geometry.BaseDimensions.X > 0
			? BaseCellIndex % Geometry.BaseDimensions.X : INDEX_NONE;
		const int32 BaseCellY = BaseCellIndex != INDEX_NONE && Geometry.BaseDimensions.X > 0
			? BaseCellIndex / Geometry.BaseDimensions.X : INDEX_NONE;
		const float* SurfaceZ = BaseCellIndex != INDEX_NONE
			? LocalSurfaceZ.TryGet(BaseCellX, BaseCellY) : nullptr;
		if (BaseCellIndex == INDEX_NONE
			|| !Geometry.TryGetLevelCellRect(BaseCellIndex, 0, CellMinLocal, CellMaxLocal)
			|| SurfaceZ == nullptr)
		{
			OutError = FText::FromString(TEXT("ArcEcotope representative record references an invalid site cell."));
			return false;
		}
		const uint64 XKey = UE::ArcForest::MakeAnnualKey(Snapshot.SimulationId, Snapshot.Seed, Snapshot.CurrentYear, UE::ArcForest::EDeterministicDomain::InstanceRepresentativePositionX, SpeciesId, RecordCellIndex, EntityId, Ordinal, 0);
		const uint64 YKey = UE::ArcForest::MakeAnnualKey(Snapshot.SimulationId, Snapshot.Seed, Snapshot.CurrentYear, UE::ArcForest::EDeterministicDomain::InstanceRepresentativePositionY, SpeciesId, RecordCellIndex, EntityId, Ordinal, 0);
		OutPosition = FVector(
			FMath::Lerp(CellMinLocal.X, CellMaxLocal.X, UE::ArcForest::KeyToUnitOpen(XKey)),
			FMath::Lerp(CellMinLocal.Y, CellMaxLocal.Y, UE::ArcForest::KeyToUnitOpen(YKey)),
			0.0);
		return true;
	}

	/** Year-free clump draw: MakeKey carries no entity or year, so a clump stays put for the whole run. */
	uint64 MakePatchClusterKey(
		const FArcForestWorldSnapshot& Snapshot,
		const UE::ArcForest::EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		const int32 RecordCellIndex,
		const int32 LayerIndex,
		const int64 Ordinal,
		const int64 Draw)
	{
		// MakeKey carries no layer field, so the layer folds into the field ordinal ahead of the
		// ordinal itself; without it two layers of one species in one cell draw identical centres.
		UE::ArcForest::FKeyedHash64 FieldOrdinal(static_cast<uint64>(Domain));
		FieldOrdinal.AddInt32(LayerIndex);
		FieldOrdinal.AddInt64(Ordinal);
		FieldOrdinal.AddInt64(Draw);
		return UE::ArcForest::MakeKey(
			Snapshot.SimulationId, Snapshot.Seed, Domain, SpeciesId, RecordCellIndex,
			static_cast<int64>(FieldOrdinal.Finalize()));
	}

	uint64 MakeClusterSiteKey(
		const FArcForestWorldSnapshot& Snapshot,
		const UE::ArcForest::EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		const int32 LayerIndex,
		const int64 SiteX,
		const int64 SiteY)
	{
		UE::ArcForest::FKeyedHash64 FieldOrdinal(static_cast<uint64>(Domain));
		FieldOrdinal.AddInt32(LayerIndex);
		FieldOrdinal.AddInt64(SiteX);
		FieldOrdinal.AddInt64(SiteY);
		return UE::ArcForest::MakeKey(
			Snapshot.SimulationId, Snapshot.Seed, Domain, SpeciesId, INDEX_NONE,
			static_cast<int64>(FieldOrdinal.Finalize()));
	}

	/** The layer axis value of the draw that picks a layer, kept off every real layer index. */
	constexpr int32 LayerSelectionAxis = INDEX_NONE;

	/**
	 * The authored layers of a species, or a single layer built from the live console values when it
	 * authored none — an empty array is "whatever the CVars say", never "no clustering", because
	 * that is what every ground cover predating the authored profile already rendered as.
	 */
	void ResolveCoverClusterLayers(const UArcEcotopeSpeciesModel& Species, TArray<FArcForestCoverClusterLayer>& OutLayers)
	{
		OutLayers.Reset();
		const TConstArrayView<FArcForestCoverClusterLayer> AuthoredLayers = Species.GetCoverClusterLayers();
		if (!AuthoredLayers.IsEmpty())
		{
			OutLayers.Append(AuthoredLayers);
			return;
		}
		FArcForestCoverClusterLayer& Fallback = OutLayers.AddDefaulted_GetRef();
		Fallback.Weight = 1.0f;
		Fallback.ClusterRadiusCm = static_cast<float>(GetPatchClusterRadiusCm());
		Fallback.InstancesPerCluster = GetPatchClusterInstances();
		Fallback.SiteBiasStrength = 0.0f;
	}

	double GetClusterRadiusCeilingCm(const FInstanceSiteGeometry& Geometry)
	{
		return FMath::IsFinite(Geometry.CellSizeCm) && Geometry.CellSizeCm > 0.0f
			? 0.5 * static_cast<double>(Geometry.CellSizeCm) * static_cast<double>(MaxClusterHaloBaseCells)
			: 0.0;
	}

	/** Authored bounds are re-derived because IsDataValid is not a hard load gate. */
	double SanitizeClusterRadiusCm(const float RadiusCm)
	{
		return FMath::IsFinite(RadiusCm) ? FMath::Max(static_cast<double>(RadiusCm), 0.0) : 0.0;
	}

	double SanitizeSiteBiasStrength(const float Strength)
	{
		return FMath::IsFinite(Strength) ? FMath::Clamp(static_cast<double>(Strength), 0.0, 1.0) : 0.0;
	}

	double SanitizeLayerWeight(const float Weight)
	{
		return FMath::IsFinite(Weight) ? FMath::Max(static_cast<double>(Weight), 0.0) : 0.0;
	}

	/** Weighted draw over the layers, the mechanic SelectVariantIndex uses; a weightless set falls to the first. */
	int32 SelectClusterLayer(
		const TConstArrayView<FArcForestCoverClusterLayer> Layers,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 RecordCellIndex,
		const int64 Ordinal)
	{
		check(!Layers.IsEmpty());
		if (Layers.Num() == 1)
		{
			return 0;
		}
		double TotalWeight = 0.0;
		for (const FArcForestCoverClusterLayer& Layer : Layers)
		{
			TotalWeight += SanitizeLayerWeight(Layer.Weight);
		}
		if (!(TotalWeight > 0.0))
		{
			return 0;
		}
		const uint64 Key = MakePatchClusterKey(
			Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterAssignment, SpeciesId, RecordCellIndex,
			LayerSelectionAxis, Ordinal, 0);
		double Remaining = UE::ArcForest::KeyToUnitOpen(Key) * TotalWeight;
		for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
		{
			Remaining -= SanitizeLayerWeight(Layers[LayerIndex].Weight);
			if (Remaining < 0.0)
			{
				return LayerIndex;
			}
		}
		return Layers.Num() - 1;
	}

	double ComputeLayerWeightShare(
		const TConstArrayView<FArcForestCoverClusterLayer> Layers,
		const int32 LayerIndex)
	{
		if (Layers.Num() <= 1)
		{
			return 1.0;
		}
		double TotalWeight = 0.0;
		for (const FArcForestCoverClusterLayer& Layer : Layers)
		{
			TotalWeight += SanitizeLayerWeight(Layer.Weight);
		}
		if (!(TotalWeight > 0.0))
		{
			return LayerIndex == 0 ? 1.0 : 0.0;
		}
		return SanitizeLayerWeight(Layers[LayerIndex].Weight) / TotalWeight;
	}

	/**
	 * Folds an overshooting coordinate back over the edge it crossed. Clamping instead would stack every
	 * overshoot on the edge itself and draw a line of instances along it.
	 */
	double ReflectIntoRange(const double Value, const double MinLocal, const double MaxLocal)
	{
		double Reflected = Value;
		if (Reflected < MinLocal)
		{
			Reflected = MinLocal + (MinLocal - Reflected);
		}
		else if (Reflected > MaxLocal)
		{
			Reflected = MaxLocal - (Reflected - MaxLocal);
		}
		return FMath::Clamp(Reflected, MinLocal, MaxLocal);
	}

	/** Keeps a lattice or record index inside the range a double indexes exactly, so the int64 cast is defined. */
	bool IsSiteCoordinateRepresentable(const double SiteCoordinate)
	{
		return FMath::IsFinite(SiteCoordinate) && FMath::Abs(SiteCoordinate) <= 9.0e15;
	}

	bool TryGetLevelCellCoords(
		const FInstanceSiteGeometry& Geometry,
		const FVector2D& LocalPosition,
		const uint8 Level,
		const FIntPoint& LevelDimensions,
		FIntPoint& OutCoords)
	{
		OutCoords = FIntPoint::ZeroValue;
		if (Level > FArcEcotopeSiteMetadata::MaxGridLevel
			|| LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || !Geometry.LocalBounds.IsValid
			|| !FMath::IsFinite(Geometry.CellSizeCm) || Geometry.CellSizeCm <= 0.0f
			|| !FMath::IsFinite(LocalPosition.X) || !FMath::IsFinite(LocalPosition.Y))
		{
			return false;
		}
		const double LevelCellSizeCm = static_cast<double>(Geometry.CellSizeCm) * static_cast<double>(1 << Level);
		const double OffsetX = (LocalPosition.X - Geometry.LocalBounds.Min.X) / LevelCellSizeCm;
		const double OffsetY = (LocalPosition.Y - Geometry.LocalBounds.Min.Y) / LevelCellSizeCm;
		if (OffsetX < 0.0 || OffsetY < 0.0
			|| OffsetX >= static_cast<double>(LevelDimensions.X) || OffsetY >= static_cast<double>(LevelDimensions.Y))
		{
			return false;
		}
		OutCoords = FIntPoint(FMath::FloorToInt32(OffsetX), FMath::FloorToInt32(OffsetY));
		return true;
	}

	bool TryReadSiteUnitField(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteFloatFieldView& View,
		const FVector2D& LocalPosition,
		const uint8 Level,
		const FIntPoint& LevelDimensions,
		double& OutValue)
	{
		FIntPoint Coords = FIntPoint::ZeroValue;
		if (!TryGetLevelCellCoords(Geometry, LocalPosition, Level, LevelDimensions, Coords))
		{
			return false;
		}
		const float* Value = View.TryGet(Coords.X, Coords.Y);
		if (Value == nullptr || !FMath::IsFinite(*Value))
		{
			return false;
		}
		OutValue = FMath::Clamp(static_cast<double>(*Value), 0.0, 1.0);
		return true;
	}

	double GetRecordCellSizeCm(const FInstanceSiteGeometry& Geometry)
	{
		return static_cast<double>(Geometry.CellSizeCm) * static_cast<double>(1 << Geometry.RecordGridLevel);
	}

	bool TryGetSiteDiscBaseCellRange(
		const FInstanceSiteGeometry& Geometry,
		const FVector2D& MinLocal,
		const FVector2D& MaxLocal,
		FIntPoint& OutMin,
		FIntPoint& OutMax)
	{
		OutMin = FIntPoint::ZeroValue;
		OutMax = FIntPoint::ZeroValue;
		const double CellSizeCm = static_cast<double>(Geometry.CellSizeCm);
		if (!FMath::IsFinite(Geometry.CellSizeCm) || !(CellSizeCm > 0.0) || !Geometry.LocalBounds.IsValid
			|| Geometry.BaseDimensions.X <= 0 || Geometry.BaseDimensions.Y <= 0
			|| !FMath::IsFinite(MinLocal.X) || !FMath::IsFinite(MinLocal.Y)
			|| !FMath::IsFinite(MaxLocal.X) || !FMath::IsFinite(MaxLocal.Y))
		{
			return false;
		}
		const double MinCellXReal = FMath::FloorToDouble((MinLocal.X - Geometry.LocalBounds.Min.X) / CellSizeCm);
		const double MinCellYReal = FMath::FloorToDouble((MinLocal.Y - Geometry.LocalBounds.Min.Y) / CellSizeCm);
		const double MaxCellXReal = FMath::CeilToDouble((MaxLocal.X - Geometry.LocalBounds.Min.X) / CellSizeCm) - 1.0;
		const double MaxCellYReal = FMath::CeilToDouble((MaxLocal.Y - Geometry.LocalBounds.Min.Y) / CellSizeCm) - 1.0;
		if (!IsSiteCoordinateRepresentable(MinCellXReal) || !IsSiteCoordinateRepresentable(MinCellYReal)
			|| !IsSiteCoordinateRepresentable(MaxCellXReal) || !IsSiteCoordinateRepresentable(MaxCellYReal))
		{
			return false;
		}
		const int64 MinCellX = static_cast<int64>(MinCellXReal);
		const int64 MinCellY = static_cast<int64>(MinCellYReal);
		const int64 MaxCellX = static_cast<int64>(MaxCellXReal);
		const int64 MaxCellY = static_cast<int64>(MaxCellYReal);
		if (MaxCellX < MinCellX || MaxCellY < MinCellY || MaxCellX < 0 || MaxCellY < 0
			|| MinCellX > static_cast<int64>(Geometry.BaseDimensions.X) - 1
			|| MinCellY > static_cast<int64>(Geometry.BaseDimensions.Y) - 1)
		{
			return false;
		}
		OutMin = FIntPoint(
			static_cast<int32>(FMath::Clamp<int64>(MinCellX, 0, Geometry.BaseDimensions.X - 1)),
			static_cast<int32>(FMath::Clamp<int64>(MinCellY, 0, Geometry.BaseDimensions.Y - 1)));
		OutMax = FIntPoint(
			static_cast<int32>(FMath::Clamp<int64>(MaxCellX, 0, Geometry.BaseDimensions.X - 1)),
			static_cast<int32>(FMath::Clamp<int64>(MaxCellY, 0, Geometry.BaseDimensions.Y - 1)));
		return true;
	}

	struct FSiteDiscTotals
	{
		double PlantableAreaSquareCm = 0.0;
		double SpeciesEquivalent = 0.0;
		double TotalEquivalent = 0.0;
	};

	void GatherSiteDiscTotals(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteByteFieldView& ValidSurface,
		const TMap<int32, double>& CellTotalEquivalent,
		const TMap<TPair<FGuid, int32>, double>& SpeciesCellEquivalent,
		const FGuid& SpeciesId,
		const FVector2D& MinLocal,
		const FVector2D& MaxLocal,
		FSiteDiscTotals& OutTotals)
	{
		OutTotals = FSiteDiscTotals();
		FIntPoint RangeMin = FIntPoint::ZeroValue;
		FIntPoint RangeMax = FIntPoint::ZeroValue;
		if (Geometry.RecordDimensions.X <= 0
			|| Geometry.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| !TryGetSiteDiscBaseCellRange(Geometry, MinLocal, MaxLocal, RangeMin, RangeMax))
		{
			return;
		}
		const int32 RecordGridShift = static_cast<int32>(Geometry.RecordGridLevel);
		const int64 ChildSpan = static_cast<int64>(1) << RecordGridShift;
		for (int32 BaseY = RangeMin.Y; BaseY <= RangeMax.Y; ++BaseY)
		{
			const int32 RecordY = BaseY >> RecordGridShift;
			const int64 ChildCountY = FMath::Min<int64>(
				ChildSpan, static_cast<int64>(Geometry.BaseDimensions.Y) - static_cast<int64>(RecordY) * ChildSpan);
			for (int32 BaseX = RangeMin.X; BaseX <= RangeMax.X; ++BaseX)
			{
				const int32 RecordX = BaseX >> RecordGridShift;
				const int32 RecordCellIndex = RecordX + RecordY * Geometry.RecordDimensions.X;
				const double* Total = CellTotalEquivalent.Find(RecordCellIndex);
				if (Total == nullptr)
				{
					continue;
				}
				const uint8* Valid = ValidSurface.TryGet(BaseX, BaseY);
				if (Valid == nullptr || *Valid == 0)
				{
					continue;
				}
				const int64 ChildCountX = FMath::Min<int64>(
					ChildSpan, static_cast<int64>(Geometry.BaseDimensions.X) - static_cast<int64>(RecordX) * ChildSpan);
				const double ChildCellCount = static_cast<double>(ChildCountX * ChildCountY);
				if (!(ChildCellCount > 0.0))
				{
					continue;
				}
				const double* Species = SpeciesCellEquivalent.Find(TPair<FGuid, int32>(SpeciesId, RecordCellIndex));
				OutTotals.PlantableAreaSquareCm +=
					Geometry.GetBaseCellAreaSquareCentimeters(BaseX + BaseY * Geometry.BaseDimensions.X);
				OutTotals.TotalEquivalent += *Total / ChildCellCount;
				OutTotals.SpeciesEquivalent += Species != nullptr ? *Species / ChildCellCount : 0.0;
			}
		}
	}

	struct FClusterLayerSites
	{
		TArray<FVector2D> Centres;
	};

	struct FClusterSiteContext
	{
		const FInstanceSiteGeometry& Geometry;
		const FArcEcotopeSiteByteFieldView& ValidSurface;
		const FArcEcotopeSiteFloatFieldView& Moisture;
		const FArcEcotopeSiteFloatFieldView& Fertility;
		const FArcEcotopeSiteFloatFieldView& BareRock;
		const TMap<int32, double>& CellTotalEquivalent;
		const TMap<TPair<FGuid, int32>, double>& SpeciesCellEquivalent;
	};

	constexpr int64 MaxCandidateClusterSites = 8192;

	struct FClusterSiteSweep
	{
		double ClusterRadiusCm = 0.0;
		int64 CandidateSiteCount = 0;
		bool bCandidateCapExceeded = false;
	};

	/**
	 * Activation and centre are functions of the lattice cell alone, never of the asking record: two
	 * records overlapping one site must derive the same centre or a clump cannot straddle them.
	 */
	void GatherActiveClusterSites(
		const FClusterSiteContext& Context,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 RecordCellIndex,
		const FVector2D& RecordMinLocal,
		const FVector2D& RecordMaxLocal,
		const TConstArrayView<FArcForestCoverClusterLayer> Layers,
		const int32 LayerIndex,
		TArray<FVector2D>& OutCentres,
		FClusterSiteSweep& OutSweep)
	{
		OutCentres.Reset();
		OutSweep = FClusterSiteSweep();
		const FInstanceSiteGeometry& Geometry = Context.Geometry;
		const double ClusterRadiusCm = FMath::Min(
			SanitizeClusterRadiusCm(Layers[LayerIndex].ClusterRadiusCm),
			GetClusterRadiusCeilingCm(Geometry));
		OutSweep.ClusterRadiusCm = ClusterRadiusCm;
		const double LayerWeightShare = ComputeLayerWeightShare(Layers, LayerIndex);
		if (Layers[LayerIndex].InstancesPerCluster <= 1 || !(ClusterRadiusCm > 0.0) || !(LayerWeightShare > 0.0)
			|| !Geometry.LocalBounds.IsValid || Geometry.RecordDimensions.X <= 0)
		{
			return;
		}
		const double SpacingCm = 2.0 * ClusterRadiusCm;
		const double SiteAreaSqM = SpacingCm * SpacingCm / 10000.0;
		const double InstancesPerCluster = static_cast<double>(FMath::Max(Layers[LayerIndex].InstancesPerCluster, 1));
		const double SiteBiasStrength = SanitizeSiteBiasStrength(Layers[LayerIndex].SiteBiasStrength);
		const double DensityCeilingPerSqM = GetPatchInstanceDensityPerSqM();
		const int64 RecordCellCount =
			static_cast<int64>(Geometry.RecordDimensions.X) * Geometry.RecordDimensions.Y;
		if (RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return;
		}
		const FIntPoint OwnRecordCoords(
			RecordCellIndex % Geometry.RecordDimensions.X,
			RecordCellIndex / Geometry.RecordDimensions.X);
		const double OriginX = Geometry.LocalBounds.Min.X;
		const double OriginY = Geometry.LocalBounds.Min.Y;
		const double SiteMinXReal = FMath::FloorToDouble((RecordMinLocal.X - OriginX) / SpacingCm);
		const double SiteMaxXReal = FMath::FloorToDouble((RecordMaxLocal.X - OriginX) / SpacingCm);
		const double SiteMinYReal = FMath::FloorToDouble((RecordMinLocal.Y - OriginY) / SpacingCm);
		const double SiteMaxYReal = FMath::FloorToDouble((RecordMaxLocal.Y - OriginY) / SpacingCm);
		if (!IsSiteCoordinateRepresentable(SiteMinXReal) || !IsSiteCoordinateRepresentable(SiteMaxXReal)
			|| !IsSiteCoordinateRepresentable(SiteMinYReal) || !IsSiteCoordinateRepresentable(SiteMaxYReal))
		{
			return;
		}
		const int64 SiteMinX = static_cast<int64>(SiteMinXReal);
		const int64 SiteMaxX = static_cast<int64>(SiteMaxXReal);
		const int64 SiteMinY = static_cast<int64>(SiteMinYReal);
		const int64 SiteMaxY = static_cast<int64>(SiteMaxYReal);
		if (SiteMaxX < SiteMinX || SiteMaxY < SiteMinY)
		{
			return;
		}
		const int64 SiteSpanX = SiteMaxX - SiteMinX + 1;
		const int64 SiteSpanY = SiteMaxY - SiteMinY + 1;
		const int64 CandidateSiteCount = SiteSpanX > MAX_int64 / SiteSpanY
			? MAX_int64
			: SiteSpanX * SiteSpanY;
		if (CandidateSiteCount > MaxCandidateClusterSites)
		{
			OutSweep.CandidateSiteCount = CandidateSiteCount;
			OutSweep.bCandidateCapExceeded = true;
			return;
		}
		OutSweep.CandidateSiteCount = CandidateSiteCount;
		for (int64 SiteY = SiteMinY; SiteY <= SiteMaxY; ++SiteY)
		{
			for (int64 SiteX = SiteMinX; SiteX <= SiteMaxX; ++SiteX)
			{
				const uint64 CentreXKey = MakeClusterSiteKey(
					Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterCentreX, SpeciesId, LayerIndex,
					SiteX, SiteY);
				const uint64 CentreYKey = MakeClusterSiteKey(
					Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterCentreY, SpeciesId, LayerIndex,
					SiteX, SiteY);
				const FVector2D CentreLocal(
					OriginX + (static_cast<double>(SiteX) + UE::ArcForest::KeyToUnitOpen(CentreXKey)) * SpacingCm,
					OriginY + (static_cast<double>(SiteY) + UE::ArcForest::KeyToUnitOpen(CentreYKey)) * SpacingCm);
				// A site belongs to the record cell holding its centre, so every site is filled once.
				FIntPoint CentreRecordCoords = FIntPoint::ZeroValue;
				if (!TryGetLevelCellCoords(
						Geometry,
						CentreLocal,
						Geometry.RecordGridLevel,
						Geometry.RecordDimensions,
						CentreRecordCoords)
					|| CentreRecordCoords != OwnRecordCoords)
				{
					continue;
				}
				const FVector2D DiscMinLocal(CentreLocal.X - ClusterRadiusCm, CentreLocal.Y - ClusterRadiusCm);
				const FVector2D DiscMaxLocal(CentreLocal.X + ClusterRadiusCm, CentreLocal.Y + ClusterRadiusCm);
				FSiteDiscTotals DiscTotals;
				GatherSiteDiscTotals(
					Geometry, Context.ValidSurface, Context.CellTotalEquivalent, Context.SpeciesCellEquivalent,
					SpeciesId, DiscMinLocal, DiscMaxLocal, DiscTotals);
				const double SpeciesEquivalent = DiscTotals.SpeciesEquivalent;
				const double TotalEquivalent = DiscTotals.TotalEquivalent;
				if (!(SpeciesEquivalent > 0.0) || !(TotalEquivalent > 0.0)
					|| !(DiscTotals.PlantableAreaSquareCm > 0.0))
				{
					continue;
				}
				const double CellDensityPerSqM = FMath::Min(
					TotalEquivalent / (DiscTotals.PlantableAreaSquareCm / 10000.0), DensityCeilingPerSqM);
				if (!(CellDensityPerSqM > 0.0))
				{
					continue;
				}
				double Fertility = 1.0;
				double Moisture = 1.0;
				double BareRock = 0.0;
				TryReadSiteUnitField(
					Geometry,
					Context.Fertility,
					CentreLocal,
					Geometry.FertilityLevel,
					Geometry.FertilityDimensions,
					Fertility);
				TryReadSiteUnitField(
					Geometry,
					Context.Moisture,
					CentreLocal,
					Geometry.MoistureLevel,
					Geometry.MoistureDimensions,
					Moisture);
				TryReadSiteUnitField(
					Geometry,
					Context.BareRock,
					CentreLocal,
					Geometry.BareRockLevel,
					Geometry.BareRockDimensions,
					BareRock);
				const double Quality = FMath::Lerp(
					1.0, Fertility * Moisture * (1.0 - BareRock), SiteBiasStrength);
				const double ActivationProbability = FMath::Clamp(
					CellDensityPerSqM * (SpeciesEquivalent / TotalEquivalent) * SiteAreaSqM / InstancesPerCluster,
					0.0,
					1.0) * LayerWeightShare * Quality;
				const uint64 ActivationKey = MakeClusterSiteKey(
					Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterSiteActivation, SpeciesId, LayerIndex,
					SiteX, SiteY);
				if (UE::ArcForest::KeyToUnitOpen(ActivationKey) < ActivationProbability)
				{
					OutCentres.Add(CentreLocal);
				}
			}
		}
	}

	/**
	 * Ground cover spreads clonally, so its instances are drawn as discs around a few shared centres
	 * rather than independently, which would be white noise and could not clump. Trees keep the
	 * independent draw. Any clustered point that misses a plantable base cell falls back to that draw,
	 * so the clump shape never changes the instance count.
	 *
	 * Each instance is assigned to one authored layer first, and the layers run at once: a species can
	 * carry tight clumps and a sparse scatter together. A selected layer of one instance per clump is
	 * the authored bypass and takes the independent draw, which is the only genuinely uniform one.
	 */
	bool TryMakeClusteredPatchPosition(
		const FInstanceSiteGeometry& Geometry,
		const FArcEcotopeSiteFloatFieldView& LocalSurfaceZ,
		const FArcForestWorldSnapshot& Snapshot,
		const FGuid& SpeciesId,
		const int32 RecordCellIndex,
		const TArray<int32>& PlantableBaseCells,
		const TArray<int32>& ClusterPlantableBaseCells,
		const TConstArrayView<FArcForestCoverClusterLayer> Layers,
		const TConstArrayView<FClusterLayerSites> LayerSites,
		const uint64 EntityId,
		const int32 Ordinal,
		FVector& OutPosition,
		FText& OutError)
	{
		const int32 LayerIndex = Layers.IsEmpty()
			? INDEX_NONE
			: SelectClusterLayer(Layers, Snapshot, SpeciesId, RecordCellIndex, Ordinal);
		if (LayerIndex != INDEX_NONE
			&& Layers[LayerIndex].InstancesPerCluster > 1
			&& LayerSites.IsValidIndex(LayerIndex)
			&& !LayerSites[LayerIndex].Centres.IsEmpty())
		{
			const TArray<FVector2D>& Centres = LayerSites[LayerIndex].Centres;
			const uint64 AssignmentKey = MakePatchClusterKey(
				Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterAssignment, SpeciesId, RecordCellIndex, LayerIndex, Ordinal, 0);
			const int32 SiteIndex = FMath::Min(
				Centres.Num() - 1,
				static_cast<int32>(UE::ArcForest::KeyToUnitOpen(AssignmentKey) * static_cast<double>(Centres.Num())));
			const uint64 RadiusKey = MakePatchClusterKey(
				Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterOffset, SpeciesId, RecordCellIndex, LayerIndex, Ordinal, 0);
			const uint64 AngleKey = MakePatchClusterKey(
				Snapshot, UE::ArcForest::EDeterministicDomain::PatchClusterOffset, SpeciesId, RecordCellIndex, LayerIndex, Ordinal, 1);
			const double ClusterRadiusCm = FMath::Min(
				SanitizeClusterRadiusCm(Layers[LayerIndex].ClusterRadiusCm),
				GetClusterRadiusCeilingCm(Geometry));
			// sqrt of the unit draw makes the disc uniform by area rather than crowding the centre.
			const double OffsetRadiusCm = ClusterRadiusCm * FMath::Sqrt(UE::ArcForest::KeyToUnitOpen(RadiusKey));
			const double OffsetAngle = UE::ArcForest::KeyToUnitOpen(AngleKey) * 2.0 * PI;
			const FVector2D ClusteredLocal(
				ReflectIntoRange(
					Centres[SiteIndex].X + OffsetRadiusCm * FMath::Cos(OffsetAngle),
					Geometry.LocalBounds.Min.X, Geometry.LocalBounds.Max.X),
				ReflectIntoRange(
					Centres[SiteIndex].Y + OffsetRadiusCm * FMath::Sin(OffsetAngle),
					Geometry.LocalBounds.Min.Y, Geometry.LocalBounds.Max.Y));
			int32 ClusteredBaseCellIndex = INDEX_NONE;
			if (Geometry.TryGetBaseCellIndex(ClusteredLocal, ClusteredBaseCellIndex)
				&& Algo::BinarySearch(ClusterPlantableBaseCells, ClusteredBaseCellIndex) != INDEX_NONE)
			{
				const int32 BaseCellX = ClusteredBaseCellIndex % Geometry.BaseDimensions.X;
				const int32 BaseCellY = ClusteredBaseCellIndex / Geometry.BaseDimensions.X;
				if (const float* SurfaceZ = LocalSurfaceZ.TryGet(BaseCellX, BaseCellY))
				{
					OutPosition = FVector(ClusteredLocal.X, ClusteredLocal.Y, 0.0);
					return true;
				}
			}
		}
		return TryMakeRepresentativePosition(
			Geometry, LocalSurfaceZ, Snapshot, SpeciesId, RecordCellIndex, PlantableBaseCells,
			EntityId, Ordinal, OutPosition, OutError);
	}

	/**
	 * The World Partition cell an instance lands in, narrowed to int32 so it can live in the group key.
	 * FCellCoord is int64; at the smallest authorable grid size int32 still spans far past world bounds,
	 * so an overflow is a corrupt position rather than a legitimate placement.
	 */
	bool TryMakeCellKey(
		const FVector& WorldPosition,
		const FEmitContext& Context,
		const uint32 GridSizeCm,
		FIntVector& OutCell)
	{
		if (!Context.bIsPartitionedWorld)
		{
			// The level backend keys on (class, guid) alone, so every instance shares one cell.
			OutCell = FIntVector::ZeroValue;
			return true;
		}
		if (GridSizeCm == 0 || WorldPosition.ContainsNaN())
		{
			return false;
		}
		const UActorPartitionSubsystem::FCellCoord CellCoord =
			UActorPartitionSubsystem::FCellCoord::GetCellCoord(WorldPosition, Context.PersistentLevel, GridSizeCm);
		// MIN_int32 itself is the unassigned sentinel and must never be a real coordinate.
		if (CellCoord.X <= MIN_int32 || CellCoord.X > MAX_int32
			|| CellCoord.Y <= MIN_int32 || CellCoord.Y > MAX_int32
			|| CellCoord.Z <= MIN_int32 || CellCoord.Z > MAX_int32)
		{
			return false;
		}
		OutCell = FIntVector(
			static_cast<int32>(CellCoord.X),
			static_cast<int32>(CellCoord.Y),
			static_cast<int32>(CellCoord.Z));
		return true;
	}

	bool TryAddTransform(
		TArray<FGroup>& Groups,
		TMap<FArcEcotopeGroupKey, int32>& GroupIndices,
		const UArcEcotopeSpeciesModel& Species,
		const FResolvedBand& Resolved,
		const FArcForestWorldSnapshot& Snapshot,
		const FEmitContext& Context,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal,
		const FTransform& Transform,
		const FVector& WorldPosition,
		const uint64 RendererHash,
		int64& InOutTotalInstances,
		const int64 Cap,
		FText& OutError)
	{
		if (InOutTotalInstances >= Cap || InOutTotalInstances == MAX_int64)
		{
			OutError = FText::FromString(TEXT("ArcEcotope instance generation exceeded its configured safe cap."));
			return false;
		}

		check(Resolved.Band != nullptr);

		FArcEcotopeGroupKey Key;
		Key.SpeciesId = Species.SpeciesId;
		Key.Kind = static_cast<uint8>(Resolved.Band->Kind);
		Key.MinHeightCm = Resolved.Band->MinHeightCm;
		Key.MaxHeightCm = Resolved.Band->MaxHeightCm;

		const FArcForestBandVisual_StaticMesh* StaticVisual =
			Resolved.Band->Visual.GetPtr<FArcForestBandVisual_StaticMesh>();
		const FArcForestBandVisual_SkinnedMesh* SkinnedVisual =
			Resolved.Band->Visual.GetPtr<FArcForestBandVisual_SkinnedMesh>();
		const FArcForestBandVisual_Entity* EntityVisual =
			Resolved.Band->Visual.GetPtr<FArcForestBandVisual_Entity>();
		int32 AnimationIndex = 0;
		EArcForestMeshLengthAxis LengthAxis = EArcForestMeshLengthAxis::PlusZ;
		const uint32 GridSizeCm = static_cast<uint32>(Species.GridSize);
		const FName RuntimeGridName = Species.RuntimeGridName;
		if (StaticVisual != nullptr)
		{
			Key.VariantIndex = SelectVariantIndex(
				StaticVisual->Variants, Snapshot, Key.SpeciesId, CellIndex, EntityId, Ordinal);
			LengthAxis = StaticVisual->Variants[Key.VariantIndex].LengthAxis;
		}
		else if (SkinnedVisual != nullptr)
		{
			const TArray<FArcForestSkeletalMeshVariant>& Variants = SkinnedVisual->Variants;
			Key.VariantIndex = SelectVariantIndex(Variants, Snapshot, Key.SpeciesId, CellIndex, EntityId, Ordinal);
			AnimationIndex = Variants[Key.VariantIndex].AnimationIndex;
		}
		else if (EntityVisual != nullptr)
		{
			Key.VariantIndex = 0;
		}
		else
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Species %s Bands[%d] uses a visual the instance builder cannot draw."),
				*Species.GetName(),
				Resolved.BandIndex));
			return false;
		}
		Key.RendererHash = RendererHash;
		// The entity backend keeps the unassigned sentinel: its instances are partitioned by ArcMass.
		if (EntityVisual == nullptr && !TryMakeCellKey(WorldPosition, Context, GridSizeCm, Key.CellCoord))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Species %s produced an instance at a world position or grid size with no valid partition cell."),
				*Species.GetName()));
			return false;
		}

		int32* ExistingIndex = GroupIndices.Find(Key);
		int32 GroupIndex = INDEX_NONE;
		if (ExistingIndex == nullptr)
		{
			GroupIndex = Groups.Num();
			FGroup& NewGroup = Groups.AddDefaulted_GetRef();
			NewGroup.Species = &Species;
			NewGroup.BandIndex = Resolved.BandIndex;
			NewGroup.Key = Key;
			NewGroup.NameHash = MakeGroupNameHash(Key);
			NewGroup.GridSizeCm = GridSizeCm;
			NewGroup.RuntimeGridName = RuntimeGridName;
			GroupIndices.Add(Key, GroupIndex);
		}
		else
		{
			GroupIndex = *ExistingIndex;
		}
		FGroup& Group = Groups[GroupIndex];
		FTransform OrientedTransform = Transform;
		OrientedTransform.SetRotation(Transform.GetRotation() * UE::ArcForest::MakeMeshAxisFix(LengthAxis));
		Group.Transforms.Add(OrientedTransform);
		Group.AnimationIndices.Add(AnimationIndex);
		++InOutTotalInstances;
		return true;
	}

	FGuid MakeVolumeGridGuid(const FGuid& VolumeId)
	{
		FGuid Seed = VolumeId;
		FArchiveMD5 ArMD5;
		ArMD5 << Seed;
		return ArMD5.GetGuidFromHash();
	}

	/**
	 * The grid guid is the only identity axis UActorPartitionSubsystem matches on beyond class and cell,
	 * and its cache is keyed on (cell, identity) with no grid size in it, so the size, the runtime grid
	 * and the renderer kind must all fold in here or two families alias onto one actor.
	 */
	FGuid MakeInstanceGridGuid(
		const FGuid& VolumeId,
		const uint32 GridSizeCm,
		const FName RuntimeGridName,
		const bool bSkinned)
	{
		FGuid Seed = VolumeId;
		uint32 Size = GridSizeCm;
		// FName compares case-insensitively, so folding the display casing would split one grid into two families.
		FString GridName = RuntimeGridName.ToString().ToLower();
		uint8 Kind = bSkinned ? 1 : 0;
		FArchiveMD5 ArMD5;
		ArMD5 << Seed;
		ArMD5 << Size;
		ArMD5 << GridName;
		ArMD5 << Kind;
		return ArMD5.GetGuidFromHash();
	}

	struct FPartitionCellKey
	{
		FGuid GridGuid;
		int64 GridIndexX = 0;
		int64 GridIndexY = 0;
		int64 GridIndexZ = 0;

		bool operator==(const FPartitionCellKey& Other) const
		{
			return GridGuid == Other.GridGuid
				&& GridIndexX == Other.GridIndexX
				&& GridIndexY == Other.GridIndexY
				&& GridIndexZ == Other.GridIndexZ;
		}

		friend uint32 GetTypeHash(const FPartitionCellKey& Key)
		{
			return HashCombine(
				GetTypeHash(Key.GridGuid),
				HashCombine(
					GetTypeHash(Key.GridIndexX),
					HashCombine(GetTypeHash(Key.GridIndexY), GetTypeHash(Key.GridIndexZ))));
		}
	};

	struct FDiscoveredPartitionActors
	{
		TMap<FPartitionCellKey, AArcEcotopeInstancePartitionActor*> InstanceActors;
		TMap<FPartitionCellKey, AArcPlacedEntityPartitionActor*> EntityActors;
		TArray<FWorldPartitionReference> Pins;
	};

	FPartitionCellKey MakePartitionCellKey(const FPartitionActorDesc& Desc)
	{
		FPartitionCellKey Key;
		Key.GridGuid = Desc.GridGuid;
		Key.GridIndexX = Desc.GridIndexX;
		Key.GridIndexY = Desc.GridIndexY;
		Key.GridIndexZ = Desc.GridIndexZ;
		return Key;
	}

	FPartitionCellKey MakePartitionCellKey(const FGuid& GridGuid, const UActorPartitionSubsystem::FCellCoord& CellCoord)
	{
		FPartitionCellKey Key;
		Key.GridGuid = GridGuid;
		Key.GridIndexX = CellCoord.X;
		Key.GridIndexY = CellCoord.Y;
		Key.GridIndexZ = CellCoord.Z;
		return Key;
	}

	void CollectCandidateDescs(
		UWorldPartition* WorldPartition,
		const TSubclassOf<AActor>& ActorClass,
		TFunctionRef<bool(const FPartitionActorDesc&, FName)> Matches,
		TArray<TPair<FPartitionCellKey, FGuid>>& OutCandidates)
	{
		// Read-only: pinning an actor from inside the walk would mutate the container the walk is iterating.
		FWorldPartitionHelpers::ForEachActorDescInstance(
			WorldPartition,
			ActorClass,
			[&Matches, &OutCandidates](const FWorldPartitionActorDescInstance* DescInstance) -> bool
			{
				const FPartitionActorDesc* Desc =
					static_cast<const FPartitionActorDesc*>(DescInstance->GetActorDesc());
				if (Desc != nullptr && Matches(*Desc, DescInstance->GetRuntimeGrid()))
				{
					OutCandidates.Emplace(MakePartitionCellKey(*Desc), DescInstance->GetGuid());
				}
				return true;
			});
	}

	// In a partitioned world the level is authoritative and this reconciles against it; in a non-partitioned
	// one it returns nothing and the volume's records are the only rail.
	void DiscoverOwnedPartitionActors(
		const AArcEcotopeVolume& Volume,
		UWorldPartition* WorldPartition,
		FDiscoveredPartitionActors& OutDiscovered)
	{
		if (WorldPartition == nullptr || !Volume.VolumeId.IsValid())
		{
			return;
		}
		const FGuid VolumeId = Volume.VolumeId;

		TArray<TPair<FPartitionCellKey, FGuid>> InstanceCandidates;
		CollectCandidateDescs(
			WorldPartition,
			AArcEcotopeInstancePartitionActor::StaticClass(),
			[&VolumeId](const FPartitionActorDesc& Desc, const FName RuntimeGrid) -> bool
			{
				return Desc.GridGuid == MakeInstanceGridGuid(VolumeId, Desc.GridSize, RuntimeGrid, false)
					|| Desc.GridGuid == MakeInstanceGridGuid(VolumeId, Desc.GridSize, RuntimeGrid, true);
			},
			InstanceCandidates);

		const FGuid EntityGridGuid = MakeVolumeGridGuid(VolumeId);
		TArray<TPair<FPartitionCellKey, FGuid>> EntityCandidates;
		CollectCandidateDescs(
			WorldPartition,
			AArcPlacedEntityPartitionActor::StaticClass(),
			[&EntityGridGuid](const FPartitionActorDesc& Desc, const FName) -> bool
			{
				return Desc.GridGuid == EntityGridGuid;
			},
			EntityCandidates);

		for (const TPair<FPartitionCellKey, FGuid>& Candidate : InstanceCandidates)
		{
			FWorldPartitionReference Reference(WorldPartition, Candidate.Value);
			if (!Reference.IsValid())
			{
				continue;
			}
			AArcEcotopeInstancePartitionActor* Actor =
				Cast<AArcEcotopeInstancePartitionActor>(Reference.GetActor());
			// Another ecotope volume can share this level; taking one of its actors would delete its forest.
			if (Actor == nullptr || Actor->OwnerGuid != VolumeId)
			{
				continue;
			}
			AArcEcotopeInstancePartitionActor* const* Existing = OutDiscovered.InstanceActors.Find(Candidate.Key);
			if (Existing != nullptr && *Existing != Actor)
			{
				UE_LOGF(
					LogArcEcotopeInstanceBuilder,
					Warning,
					"ArcEcotope discovery maps instance partition actors '%ls' and '%ls' onto one cell key; the key carries "
					"no content bundle or data layer context, so '%ls' is dropped from the reconciliation.",
					*(*Existing)->GetName(),
					*Actor->GetName(),
					*(*Existing)->GetName());
			}
			OutDiscovered.Pins.Add(Reference);
			OutDiscovered.InstanceActors.Add(Candidate.Key, Actor);
		}
		for (const TPair<FPartitionCellKey, FGuid>& Candidate : EntityCandidates)
		{
			FWorldPartitionReference Reference(WorldPartition, Candidate.Value);
			if (!Reference.IsValid())
			{
				continue;
			}
			AArcPlacedEntityPartitionActor* Actor =
				Cast<AArcPlacedEntityPartitionActor>(Reference.GetActor());
			if (Actor == nullptr)
			{
				continue;
			}
			AArcPlacedEntityPartitionActor* const* Existing = OutDiscovered.EntityActors.Find(Candidate.Key);
			if (Existing != nullptr && *Existing != Actor)
			{
				UE_LOGF(
					LogArcEcotopeInstanceBuilder,
					Warning,
					"ArcEcotope discovery maps placed-entity partition actors '%ls' and '%ls' onto one cell key; the key carries "
					"no content bundle or data layer context, so '%ls' is dropped from the reconciliation.",
					*(*Existing)->GetName(),
					*Actor->GetName(),
					*(*Existing)->GetName());
			}
			OutDiscovered.Pins.Add(Reference);
			OutDiscovered.EntityActors.Add(Candidate.Key, Actor);
		}
	}

	AArcEcotopeInstancePartitionActor* ResolveInstancePartitionActor(
		AArcEcotopeVolume& Volume,
		const FEmitContext& Context,
		const FDiscoveredPartitionActors& Discovered,
		const UActorPartitionSubsystem::FCellCoord& CellCoord,
		const uint32 GridSizeCm,
		const FName RuntimeGridName,
		const FGuid& GridGuid)
	{
		const bool bIsPartitionedWorld = Context.bIsPartitionedWorld;
		AArcEcotopeVolume* VolumePtr = &Volume;
		// GetActor builds the actor name from the guid it is given, before the actor exists, and reads the
		// desc's copy back out of SetGridGuid; one parameter feeds both, because a divergence re-spawns
		// under a colliding name with ESpawnActorNameMode::Required_Fatal.
		AArcEcotopeInstancePartitionActor* Actor = Cast<AArcEcotopeInstancePartitionActor>(Context.PartitionSubsystem->GetActor(
			AArcEcotopeInstancePartitionActor::StaticClass(),
			CellCoord,
			/*bInCreate=*/true,
			GridGuid,
			GridSizeCm,
			/*bInBoundsSearch=*/true,
			/*InActorCreated=*/[&GridGuid, RuntimeGridName, bIsPartitionedWorld, VolumePtr](APartitionActor* NewActor)
			{
				AArcEcotopeInstancePartitionActor* InstanceActor = CastChecked<AArcEcotopeInstancePartitionActor>(NewActor);
				InstanceActor->SetGridGuid(GridGuid);
				InstanceActor->OwningVolume = VolumePtr;
				InstanceActor->OwnerGuid = VolumePtr->VolumeId;
				if (bIsPartitionedWorld && !RuntimeGridName.IsNone())
				{
					InstanceActor->SetRuntimeGrid(RuntimeGridName);
				}
			}));
		if (Actor != nullptr)
		{
			return Actor;
		}
		// A streamed-out actor already holds this cell, which discovery has pinned; spawning a second
		// would collide with its name.
		AArcEcotopeInstancePartitionActor* const* DiscoveredActor =
			Discovered.InstanceActors.Find(MakePartitionCellKey(GridGuid, CellCoord));
		return DiscoveredActor != nullptr ? *DiscoveredActor : nullptr;
	}

	void CollectWeightFootprints(
		const FGroup& Group,
		const FBoxSphereBounds& MeshBounds,
		TArray<FArcEcotopeInstanceFootprint>& OutFootprints)
	{
		OutFootprints.Reserve(OutFootprints.Num() + Group.Transforms.Num());
		for (const FTransform& LocalTransform : Group.Transforms)
		{
			// Not SphereRadius: a tree's height dominates it and the footprint would come out several times too wide.
			const double RadiusCm = FMath::Max(MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y)
				* LocalTransform.GetScale3D().X;
			if (!(RadiusCm > 0.0) || !FMath::IsFinite(RadiusCm))
			{
				continue;
			}

			const FVector CentreLocal = LocalTransform.GetTranslation()
				+ LocalTransform.TransformVector(FVector(MeshBounds.Origin.X, MeshBounds.Origin.Y, 0.0));

			FArcEcotopeInstanceFootprint& Footprint = OutFootprints.AddDefaulted_GetRef();
			Footprint.LocalXY = FVector2D(CentreLocal.X, CentreLocal.Y);
			Footprint.RadiusCm = RadiusCm;
			Footprint.SpeciesId = Group.Key.SpeciesId;
			Footprint.Kind = Group.Key.Kind;
		}
	}

	/**
	 * Creates one component per group on the partition actor of its cell. Group transforms are
	 * volume-local and a partition actor sits at its cell centre, so each transform crosses two frames:
	 * local -> world -> relative to the resolved actor.
	 */
	bool BuildComponents(
		AArcEcotopeVolume& Volume,
		TArray<FGroup>& Groups,
		const FEmitContext& Context,
		const FDiscoveredPartitionActors& Discovered,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FArcEcotopePreparedInstances& OutPrepared,
		FText& OutError)
	{
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_SortGroups);
			Groups.Sort([](const FGroup& Left, const FGroup& Right) { return Left.NameHash < Right.NameHash; });
		}
		TMap<uint64, FArcEcotopeGroupKey> NameHashKeys;
		TMap<const AArcEcotopeInstancePartitionActor*, int32> ActorSetIndices;
		for (FGroup& Group : Groups)
		{
			if (GetGroupVisual<FArcForestBandVisual_Entity>(Group) != nullptr)
			{
				continue;
			}
			const FArcForestBandVisual_StaticMesh* StaticVisual = GetGroupVisual<FArcForestBandVisual_StaticMesh>(Group);
			const FArcForestBandVisual_SkinnedMesh* SkinnedVisual = GetGroupVisual<FArcForestBandVisual_SkinnedMesh>(Group);
			check(StaticVisual != nullptr || SkinnedVisual != nullptr);
			if (const FArcEcotopeGroupKey* ExistingKey = NameHashKeys.Find(Group.NameHash))
			{
				if (!(*ExistingKey == Group.Key))
				{
					OutError = FText::FromString(TEXT("ArcEcotope instance group-name hash collision."));
					return false;
				}
			}
			else
			{
				NameHashKeys.Add(Group.NameHash, Group.Key);
			}

			AArcEcotopeInstancePartitionActor* PartitionActor = nullptr;
			{
				ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_ResolveActor);
				const UActorPartitionSubsystem::FCellCoord CellCoord(
					Group.Key.CellCoord.X, Group.Key.CellCoord.Y, Group.Key.CellCoord.Z, Context.PersistentLevel);
				const FGuid GridGuid = MakeInstanceGridGuid(
					Volume.VolumeId, Group.GridSizeCm, Group.RuntimeGridName, SkinnedVisual != nullptr);
				PartitionActor = ResolveInstancePartitionActor(
					Volume, Context, Discovered, CellCoord, Group.GridSizeCm, Group.RuntimeGridName, GridGuid);
				if (PartitionActor == nullptr)
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("ArcEcotope could not resolve its instance partition actor in cell (%d, %d, %d); one exists ")
						TEXT("there but will not load, and spawning a second would collide with its name."),
						Group.Key.CellCoord.X, Group.Key.CellCoord.Y, Group.Key.CellCoord.Z));
					return false;
				}
				if (PartitionActor->OwnerGuid != Volume.VolumeId)
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("ArcEcotope resolved instance partition actor '%s', which another volume owns."),
						*PartitionActor->GetName()));
					return false;
				}
			}

			int32* ExistingSetIndex = ActorSetIndices.Find(PartitionActor);
			if (ExistingSetIndex == nullptr)
			{
				ExistingSetIndex = &ActorSetIndices.Add(PartitionActor, OutPrepared.InstanceActors.Num());
				OutPrepared.InstanceActors.AddDefaulted_GetRef().Actor = PartitionActor;
			}
			FArcEcotopePreparedInstanceActor& ActorSet = OutPrepared.InstanceActors[*ExistingSetIndex];
			const FTransform ActorTransform = PartitionActor->GetActorTransform();

			if (StaticVisual != nullptr)
			{
				const FArcForestStaticMeshVariant& Variant = StaticVisual->Variants[Group.Key.VariantIndex];
				if (Variant.StaticMesh != nullptr)
				{
					CollectWeightFootprints(Group, Variant.StaticMesh->GetBounds(), OutPrepared.WeightFootprints);
				}
				const FName FinalName(*FString::Printf(TEXT("ArcEcotopeISM_%016llX"), Group.NameHash));
				UInstancedStaticMeshComponent* Component = nullptr;
				{
					ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_CreateStatic);
					UClass* ComponentClass = Group.Species->StaticMeshDescriptor.ComponentClass != nullptr
						? Group.Species->StaticMeshDescriptor.ComponentClass.Get()
						: UInstancedStaticMeshComponent::StaticClass();
					const FName CandidateName = MakeUniqueObjectName(
						PartitionActor, ComponentClass, FName(TEXT("ArcEcotopeCandidateISM")));
					Component = NewObject<UInstancedStaticMeshComponent>(
						PartitionActor, ComponentClass, CandidateName, RF_NoFlags);
					Component->ClearFlags(RF_Transactional);
					Component->CreationMethod = EComponentCreationMethod::Instance;
					Component->SetupAttachment(PartitionActor->GetRootComponent());
					// InitComponent clears the mesh and the override materials, so the variant is stamped after it.
					Group.Species->StaticMeshDescriptor.InitComponent(Component);
				}
				{
					ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_SetStaticMesh);
					Component->SetStaticMesh(Variant.StaticMesh);
					for (int32 MaterialIndex = 0; MaterialIndex < Variant.MaterialOverrides.Num(); ++MaterialIndex)
					{
						Component->SetMaterial(MaterialIndex, Variant.MaterialOverrides[MaterialIndex]);
					}
				}
				{
					ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_AddStaticInstances);
					for (const FTransform& LocalTransform : Group.Transforms)
					{
						const FTransform WorldTransform = LocalTransform * Context.VolumeTransform;
						Component->AddInstance(WorldTransform.GetRelativeTransform(ActorTransform), false);
					}
				}
				ActorSet.StaticComponents.Add(Component);
				ActorSet.StaticFinalNames.Add(FinalName);
			}
			else
			{
				const FArcForestSkeletalMeshVariant& Variant = SkinnedVisual->Variants[Group.Key.VariantIndex];
				if (Variant.SkeletalMesh != nullptr)
				{
					CollectWeightFootprints(Group, Variant.SkeletalMesh->GetBounds(), OutPrepared.WeightFootprints);
				}
				const FName FinalName(*FString::Printf(TEXT("ArcEcotopeISKM_%016llX"), Group.NameHash));
				UInstancedSkinnedMeshComponent* Component = nullptr;
				{
					ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_CreateSkinned);
					const FName CandidateName = MakeUniqueObjectName(
						PartitionActor, UInstancedSkinnedMeshComponent::StaticClass(), FName(TEXT("ArcEcotopeCandidateISKM")));
					Component = NewObject<UInstancedSkinnedMeshComponent>(
						PartitionActor, CandidateName, RF_NoFlags);
					Component->ClearFlags(RF_Transactional);
					Component->CreationMethod = EComponentCreationMethod::Instance;
					Component->SetupAttachment(PartitionActor->GetRootComponent());
					// InitComponent clears the skinned asset, the provider and the override materials, so the variant is stamped after it.
					Group.Species->SkinnedMeshDescriptor.InitComponent(Component);
				}
				{
					ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_SetSkinnedAsset);
					Component->SetSkinnedAssetAndUpdate(Variant.SkeletalMesh);
					Component->SetTransformProvider(Variant.TransformProvider);
					for (int32 MaterialIndex = 0; MaterialIndex < Variant.MaterialOverrides.Num(); ++MaterialIndex)
					{
						Component->SetMaterial(MaterialIndex, Variant.MaterialOverrides[MaterialIndex]);
					}
				}
				{
					ARCFOREST_SCOPED_PHASE(Diagnostics, BuildComponents_AddSkinnedInstances);
					for (int32 InstanceIndex = 0; InstanceIndex < Group.Transforms.Num(); ++InstanceIndex)
					{
						const FTransform WorldTransform = Group.Transforms[InstanceIndex] * Context.VolumeTransform;
						Component->AddInstance(
							WorldTransform.GetRelativeTransform(ActorTransform),
							Group.AnimationIndices[InstanceIndex],
							false);
					}
				}
				ActorSet.SkinnedComponents.Add(Component);
				ActorSet.SkinnedFinalNames.Add(FinalName);
			}
		}
		return true;
	}

	/** One placed-entity entry to write: every instance of one config landing in one partition cell. */
	struct FEntityBucket
	{
		UArcMassEntityConfigAsset* Config = nullptr;
		UActorPartitionSubsystem::FCellCoord CellCoord;
		TArray<FTransform> WorldTransforms;
	};

	struct FEntityBucketKey
	{
		const UObject* Config = nullptr;
		UActorPartitionSubsystem::FCellCoord CellCoord;

		bool operator==(const FEntityBucketKey& Other) const
		{
			return Config == Other.Config && CellCoord == Other.CellCoord;
		}

		friend uint32 GetTypeHash(const FEntityBucketKey& Key)
		{
			return HashCombine(PointerHash(Key.Config), GetTypeHash(Key.CellCoord));
		}
	};

	AArcPlacedEntityPartitionActor* ResolvePartitionActor(
		UActorPartitionSubsystem& PartitionSubsystem,
		const FDiscoveredPartitionActors& Discovered,
		const UActorPartitionSubsystem::FCellCoord& CellCoord,
		const uint32 GridSize,
		const FGuid& GridGuid,
		const bool bCreate)
	{
		// GetActor builds the actor name from the guid it is given, before the actor exists, and reads the
		// desc's copy back out of SetGridGuid; one parameter feeds both, because a divergence re-spawns
		// under a colliding name with ESpawnActorNameMode::Required_Fatal.
		AArcPlacedEntityPartitionActor* Actor = Cast<AArcPlacedEntityPartitionActor>(PartitionSubsystem.GetActor(
			AArcPlacedEntityPartitionActor::StaticClass(),
			CellCoord,
			bCreate,
			GridGuid,
			GridSize,
			/*bInBoundsSearch=*/true,
			/*InActorCreated=*/[&GridGuid](APartitionActor* NewActor)
			{
				CastChecked<AArcPlacedEntityPartitionActor>(NewActor)->SetGridGuid(GridGuid);
			}));
		if (Actor != nullptr)
		{
			return Actor;
		}
		AArcPlacedEntityPartitionActor* const* DiscoveredActor =
			Discovered.EntityActors.Find(MakePartitionCellKey(GridGuid, CellCoord));
		return DiscoveredActor != nullptr ? *DiscoveredActor : nullptr;
	}

	UArcMassEntityConfigAsset* ResolveEntityConfig(
		const UArcEcotopeSpeciesModel& Species,
		const FArcForestBandVisual_Entity& Visual,
		TArray<TStrongObjectPtr<UArcMassEntityConfigAsset>>& InOutPins,
		FText& OutError)
	{
		// The runtime spawn hard-casts to this subclass and only logs a warning when it fails
		// (ArcPlacedEntityPartitionActor.cpp SpawnEntities), so a base-class config is caught here instead.
		UArcMassEntityConfigAsset* Config = Cast<UArcMassEntityConfigAsset>(Visual.EntityConfig.TryLoad());
		if (Config == nullptr)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Species %s selects entity config '%s', which is missing or is not a UArcMassEntityConfigAsset."),
				*Species.GetName(),
				*Visual.EntityConfig.ToString()));
			return nullptr;
		}
		if (Config->FindTrait(UArcAlwaysLoadedEntityTrait::StaticClass()) != nullptr)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Species %s selects entity config '%s', which carries UArcAlwaysLoadedEntityTrait. That trait routes a ")
				TEXT("placement to the always-loaded singleton actor, which a streamed partition actor contradicts."),
				*Species.GetName(),
				*Config->GetName()));
			return nullptr;
		}
		// Holds the freshly loaded config until the prepared write takes its own reference.
		InOutPins.Emplace(Config);
		return Config;
	}

	bool HasActorDescriptor(const UWorldPartition* WorldPartition, const FGuid& ActorGuid)
	{
		return WorldPartition != nullptr
			&& ActorGuid.IsValid()
			&& WorldPartition->GetActorDescInstance(ActorGuid) != nullptr;
	}

	/** Loads an owned actor record, pinning it when World Partition streamed it out; a dead record yields null. */
	AActor* ResolveEmittedActor(
		const FArcEcotopeEmittedActor& Record,
		UWorldPartition* WorldPartition,
		TArray<FWorldPartitionReference>& InOutPins)
	{
		AActor* Actor = Record.Actor.Get();
		if (Actor == nullptr && WorldPartition != nullptr && Record.ActorGuid.IsValid())
		{
			FWorldPartitionReference Reference(WorldPartition, Record.ActorGuid);
			if (Reference.IsValid())
			{
				Actor = Reference.GetActor();
				if (Actor != nullptr)
				{
					InOutPins.Add(Reference);
				}
			}
		}
		return Actor;
	}

	void DestroyEmittedActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}
		UWorld* World = Actor->GetWorld();
		if (World != nullptr && !World->bIsTearingDown)
		{
			World->DestroyActor(Actor);
		}
	}

	/**
	 * Prepares the entity groups for the placed-entity partition actors this volume owns alone, on
	 * their own 256 m grid. Group transforms are volume-local and the partition actors sit at their cell
	 * centres, so each transform crosses two frames: local -> world -> relative to the resolved actor.
	 * The actors are resolved and pinned here but mutated only by CommitEmittedActorChanges, inside the
	 * caller's transaction.
	 */
	bool EmitEntityInstances(
		AArcEcotopeVolume& Volume,
		const TArray<FGroup>& Groups,
		const FEmitContext& Context,
		const FDiscoveredPartitionActors& Discovered,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FArcEcotopePreparedInstances& OutPrepared,
		FText& OutError)
	{
		const uint32 GridSize = AArcPlacedEntityPartitionActor::StaticClass()
			->GetDefaultObject<AArcPlacedEntityPartitionActor>()->GetDefaultGridSize(Context.World);
		if (GridSize == 0)
		{
			OutError = FText::FromString(TEXT("ArcEcotope entity emission read a zero placed-entity grid size."));
			return false;
		}

		const bool bIsPartitionedWorld = Context.bIsPartitionedWorld;
		const FGuid GridGuid = MakeVolumeGridGuid(Volume.VolumeId);
		const FTransform VolumeTransform = Context.VolumeTransform;
		ULevel* PersistentLevel = Context.PersistentLevel;

		TArray<FEntityBucket> Buckets;
		TMap<FEntityBucketKey, int32> BucketIndices;
		TArray<TStrongObjectPtr<UArcMassEntityConfigAsset>> ConfigPins;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, EmitEntities_Bucket);
			TMap<const FArcForestBandVisual_Entity*, UArcMassEntityConfigAsset*> ResolvedConfigs;
			for (const FGroup& Group : Groups)
			{
				const FArcForestBandVisual_Entity* EntityVisual = GetGroupVisual<FArcForestBandVisual_Entity>(Group);
				if (EntityVisual == nullptr)
				{
					continue;
				}
				UArcMassEntityConfigAsset** Existing = ResolvedConfigs.Find(EntityVisual);
				UArcMassEntityConfigAsset* Config = Existing != nullptr ? *Existing : nullptr;
				if (Config == nullptr)
				{
					const FArcEcotopeSpeciesSetup* Setup = FindEnabledSetup(Volume, Group.Key.SpeciesId);
					if (Setup == nullptr || Setup->Species == nullptr)
					{
						OutError = FText::FromString(TEXT("ArcEcotope entity emission references a species that is not enabled on its volume."));
						return false;
					}
					Config = ResolveEntityConfig(*Setup->Species, *EntityVisual, ConfigPins, OutError);
					if (Config == nullptr)
					{
						return false;
					}
					ResolvedConfigs.Add(EntityVisual, Config);
				}
				for (const FTransform& LocalTransform : Group.Transforms)
				{
					const FTransform WorldTransform = LocalTransform * VolumeTransform;
					FEntityBucketKey Key;
					Key.Config = Config;
					// A non-partitioned level has a single actor per class, so every instance shares one bucket.
					Key.CellCoord = bIsPartitionedWorld
						? UActorPartitionSubsystem::FCellCoord::GetCellCoord(WorldTransform.GetLocation(), PersistentLevel, GridSize)
						: UActorPartitionSubsystem::FCellCoord(0, 0, 0, PersistentLevel);
					int32* ExistingIndex = BucketIndices.Find(Key);
					if (ExistingIndex == nullptr)
					{
						ExistingIndex = &BucketIndices.Add(Key, Buckets.Num());
						FEntityBucket& NewBucket = Buckets.AddDefaulted_GetRef();
						NewBucket.Config = Config;
						NewBucket.CellCoord = Key.CellCoord;
					}
					Buckets[*ExistingIndex].WorldTransforms.Add(WorldTransform);
				}
			}
		}

		if (Buckets.IsEmpty() && Volume.EmittedEntityActors.IsEmpty() && Discovered.EntityActors.IsEmpty())
		{
			// Nothing to emit and nothing owned: an ISM-only bake must not load a single actor.
			return true;
		}

		UWorldPartition* WorldPartition = Context.WorldPartition;

		// Pinning is what turns a streamed-out actor from a silent drop into a failure: an owned actor left
		// unloaded keeps its previous entries and the next bake would add a second copy beside them.
		TArray<AArcPlacedEntityPartitionActor*> PreviousActors;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, EmitEntities_ResolveOwned);
			for (const FArcEcotopeEmittedActor& Record : Volume.EmittedEntityActors)
			{
				AArcPlacedEntityPartitionActor* Actor = Cast<AArcPlacedEntityPartitionActor>(Record.Actor.Get());
				if (Actor == nullptr && WorldPartition != nullptr && Record.ActorGuid.IsValid())
				{
					FWorldPartitionReference Reference(WorldPartition, Record.ActorGuid);
					if (Reference.IsValid())
					{
						Actor = Cast<AArcPlacedEntityPartitionActor>(Reference.GetActor());
						if (Actor == nullptr)
						{
							OutError = FText::FromString(FString::Printf(
								TEXT("ArcEcotope entity emission could not load the placed-entity partition actor '%s' this volume owns; ")
								TEXT("clearing its entries is what keeps a re-bake from doubling the forest."),
								*Record.Actor.ToString()));
							return false;
						}
						OutPrepared.PinnedActors.Add(Reference);
					}
				}
				if (Actor != nullptr)
				{
					PreviousActors.AddUnique(Actor);
				}
			}
			for (const TPair<FPartitionCellKey, AArcPlacedEntityPartitionActor*>& Pair : Discovered.EntityActors)
			{
				if (Pair.Value != nullptr)
				{
					PreviousActors.AddUnique(Pair.Value);
				}
			}
		}

		TArray<AArcPlacedEntityPartitionActor*> OwnedActors;
		bool bEmitted = true;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, EmitEntities_PrepareWrites);
			for (const FEntityBucket& Bucket : Buckets)
			{
				AArcPlacedEntityPartitionActor* Actor = ResolvePartitionActor(
					*Context.PartitionSubsystem, Discovered, Bucket.CellCoord, GridSize, GridGuid, /*bCreate=*/true);
				if (Actor == nullptr)
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("ArcEcotope entity emission could not resolve its placed-entity partition actor in cell (%lld, %lld, %lld); ")
						TEXT("one exists there but will not load, and spawning a second would collide with its name."),
						Bucket.CellCoord.X, Bucket.CellCoord.Y, Bucket.CellCoord.Z));
					bEmitted = false;
					break;
				}
				OwnedActors.AddUnique(Actor);
				const FTransform ActorTransform = Actor->GetActorTransform();
				FArcEcotopePreparedEntityWrite& Write = OutPrepared.EntityWrites.AddDefaulted_GetRef();
				Write.Actor = Actor;
				Write.Config.Reset(Bucket.Config);
				Write.RelativeTransforms.Reserve(Bucket.WorldTransforms.Num());
				for (const FTransform& WorldTransform : Bucket.WorldTransforms)
				{
					Write.RelativeTransforms.Add(WorldTransform.GetRelativeTransform(ActorTransform));
				}
			}
		}

		if (!bEmitted)
		{
			// A failed bake still owns everything it touched, or the next one would never clear it.
			for (AArcPlacedEntityPartitionActor* Actor : PreviousActors)
			{
				OwnedActors.AddUnique(Actor);
			}
		}

		TArray<FArcEcotopeEmittedActor> NewRecords;
		NewRecords.Reserve(OwnedActors.Num());
		for (AArcPlacedEntityPartitionActor* Actor : OwnedActors)
		{
			FArcEcotopeEmittedActor& Record = NewRecords.AddDefaulted_GetRef();
			Record.Actor = Actor;
			Record.ActorGuid = Actor->GetActorGuid();
		}
		if (!bEmitted)
		{
			OutPrepared.FailedBake.EntityActors = MoveTemp(NewRecords);
			return false;
		}
		OutPrepared.EntityActorRecords = MoveTemp(NewRecords);
		// Which of them end up empty is only known after the commit clears this volume's entries.
		OutPrepared.PreviousEntityActors.Reserve(PreviousActors.Num());
		for (AArcPlacedEntityPartitionActor* Actor : PreviousActors)
		{
			OutPrepared.PreviousEntityActors.Add(Actor);
		}
		return true;
	}

	bool TryMakeEmitContext(AArcEcotopeVolume& Volume, FEmitContext& OutContext, FText& OutError)
	{
		UWorld* World = Volume.GetWorld();
		if (World == nullptr || World->PersistentLevel == nullptr)
		{
			OutError = FText::FromString(TEXT("ArcEcotope instance emission requires a world with a persistent level."));
			return false;
		}
		if (!Volume.VolumeId.IsValid())
		{
			// The volume identity is the only thing separating one volume's partition actors and entity
			// entries from another's; an invalid one also reads as "hand-placed" everywhere downstream.
			OutError = FText::FromString(TEXT("ArcEcotope instance emission requires a valid volume identity."));
			return false;
		}
		UActorPartitionSubsystem* PartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
		if (PartitionSubsystem == nullptr)
		{
			OutError = FText::FromString(TEXT("ArcEcotope instance emission requires the actor partition subsystem."));
			return false;
		}
		OutContext.World = World;
		OutContext.PersistentLevel = World->PersistentLevel;
		OutContext.PartitionSubsystem = PartitionSubsystem;
		OutContext.bIsPartitionedWorld = World->IsPartitionedWorld();
		if (OutContext.bIsPartitionedWorld)
		{
			OutContext.WorldPartition = World->GetWorldPartition();
			if (OutContext.WorldPartition == nullptr)
			{
				OutError = FText::FromString(TEXT("ArcEcotope instance emission could not reach the world partition of a partitioned world."));
				return false;
			}
		}
		OutContext.VolumeTransform = Volume.GetActorTransform();
		return true;
	}

	/**
	 * Loads the instance partition actors this volume already owns, before any component is created.
	 * An owned actor left unloaded keeps its previous geometry while the bake writes a second copy
	 * elsewhere, so it fails the bake instead; the pins are released only after adoption.
	 */
	bool ResolveOwnedInstanceActors(
		AArcEcotopeVolume& Volume,
		const FEmitContext& Context,
		const FDiscoveredPartitionActors& Discovered,
		FArcEcotopePreparedInstances& OutPrepared,
		TArray<AArcEcotopeInstancePartitionActor*>& OutPreviousActors,
		FText& OutError)
	{
		for (const FArcEcotopeEmittedActor& Record : Volume.InstancePartitionActors)
		{
			AArcEcotopeInstancePartitionActor* Actor = Cast<AArcEcotopeInstancePartitionActor>(Record.Actor.Get());
			if (Actor == nullptr && Context.WorldPartition != nullptr && Record.ActorGuid.IsValid())
			{
				FWorldPartitionReference Reference(Context.WorldPartition, Record.ActorGuid);
				if (Reference.IsValid())
				{
					Actor = Cast<AArcEcotopeInstancePartitionActor>(Reference.GetActor());
					if (Actor == nullptr)
					{
						OutError = FText::FromString(FString::Printf(
							TEXT("ArcEcotope could not load the instance partition actor '%s' this volume owns; ")
							TEXT("clearing it is what keeps a re-bake from doubling the forest."),
							*Record.Actor.ToString()));
						return false;
					}
					OutPrepared.PinnedActors.Add(Reference);
				}
			}
			if (Actor != nullptr)
			{
				OutPreviousActors.AddUnique(Actor);
			}
		}
		for (const TPair<FPartitionCellKey, AArcEcotopeInstancePartitionActor*>& Pair : Discovered.InstanceActors)
		{
			if (Pair.Value != nullptr)
			{
				OutPreviousActors.AddUnique(Pair.Value);
			}
		}
		return true;
	}

	void MakeInstanceActorRecords(
		TConstArrayView<FArcEcotopePreparedInstanceActor> InstanceActors,
		TArray<FArcEcotopeEmittedActor>& OutRecords)
	{
		OutRecords.Reset(InstanceActors.Num());
		for (const FArcEcotopePreparedInstanceActor& ActorSet : InstanceActors)
		{
			if (ActorSet.Actor == nullptr)
			{
				continue;
			}
			FArcEcotopeEmittedActor& Record = OutRecords.AddDefaulted_GetRef();
			Record.Actor = ActorSet.Actor;
			Record.ActorGuid = ActorSet.Actor->GetActorGuid();
		}
	}

	void FinalizeInstanceActorRecords(
		TConstArrayView<AArcEcotopeInstancePartitionActor*> PreviousActors,
		FArcEcotopePreparedInstances& OutPrepared)
	{
		MakeInstanceActorRecords(OutPrepared.InstanceActors, OutPrepared.InstanceActorRecords);
		TSet<const AArcEcotopeInstancePartitionActor*> WrittenActors;
		for (const FArcEcotopePreparedInstanceActor& ActorSet : OutPrepared.InstanceActors)
		{
			WrittenActors.Add(ActorSet.Actor);
		}
		for (AArcEcotopeInstancePartitionActor* Actor : PreviousActors)
		{
			if (!WrittenActors.Contains(Actor))
			{
				OutPrepared.ActorsToDestroy.Add(TSoftObjectPtr<AActor>(Actor));
			}
		}
	}

	/** A failed bake still owns every actor it touched, or the next one would never clear it. */
	void RecordOwnedInstanceActorsOnFailure(
		TConstArrayView<AArcEcotopeInstancePartitionActor*> PreviousActors,
		FArcEcotopePreparedInstances& Prepared)
	{
		TArray<FArcEcotopeEmittedActor> Records;
		MakeInstanceActorRecords(Prepared.InstanceActors, Records);
		for (AArcEcotopeInstancePartitionActor* Actor : PreviousActors)
		{
			const bool bAlreadyRecorded = Records.ContainsByPredicate(
				[Actor](const FArcEcotopeEmittedActor& Record) { return Record.Actor.Get() == Actor; });
			if (!bAlreadyRecorded)
			{
				FArcEcotopeEmittedActor& Record = Records.AddDefaulted_GetRef();
				Record.Actor = Actor;
				Record.ActorGuid = Actor->GetActorGuid();
			}
		}
		Prepared.FailedBake.InstanceActors = MoveTemp(Records);
	}

	/** Preserves both newly prepared and previously owned placed-entity actors across a late failure. */
	void RecordOwnedEntityActorsOnFailure(FArcEcotopePreparedInstances& Prepared)
	{
		TArray<FArcEcotopeEmittedActor> Records = MoveTemp(Prepared.EntityActorRecords);
		for (AArcPlacedEntityPartitionActor* Actor : Prepared.PreviousEntityActors)
		{
			if (Actor == nullptr)
			{
				continue;
			}
			const FGuid ActorGuid = Actor->GetActorGuid();
			const bool bAlreadyRecorded = Records.ContainsByPredicate(
				[Actor, ActorGuid](const FArcEcotopeEmittedActor& Record)
				{
					return Record.Actor.Get() == Actor
						|| (ActorGuid.IsValid() && Record.ActorGuid == ActorGuid);
				});
			if (!bAlreadyRecorded)
			{
				FArcEcotopeEmittedActor& Record = Records.AddDefaulted_GetRef();
				Record.Actor = Actor;
				Record.ActorGuid = ActorGuid;
			}
		}
		Prepared.FailedBake.EntityActors = MoveTemp(Records);
	}
}

bool FArcEcotopeInstanceBuilder::Build(
	AArcEcotopeVolume& Volume,
	const FArcForestWorldSnapshot& Snapshot,
	FArcEcotopePreparedInstances& OutPrepared,
	FText& OutError)
{
	using namespace ArcEcotopeInstanceBuilder;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_InstanceBuilderBuild);
	UE::ArcForest::FDiagnosticScopeReport Report(TEXT("ArcEcotope InstanceBuilder::Build"));
	UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics = Report.GetPhases();
	Discard(OutPrepared);
	OutPrepared.FailedBake = FArcEcotopeFailedBakeRecords();

	if (Volume.GetRootComponent() == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcEcotope instance generation requires a volume root component."));
		Report.SetOutcome(TEXT("no root component"));
		return false;
	}
	FEmitContext Context;
	if (!TryMakeEmitContext(Volume, Context, OutError))
	{
		Report.SetOutcome(TEXT("no emit context"));
		return false;
	}
	const TMap<FGuid, int32> SpeciesMaxAgeYearsCache = BuildSpeciesMaxAgeYearsCache(Volume);
	FText SnapshotError;
	const TArray<FArcForestSpeciesEcologyHash> EcologyHashes = Volume.BuildEnabledSpeciesEcologyHashes();
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_ValidateSnapshot);
		if (Snapshot.Validate(SnapshotError, Volume.ComputeSettingsHash(), &EcologyHashes) != EArcForestSimulationStateStatus::Valid)
		{
			OutError = FText::FromString(FString::Printf(TEXT("ArcEcotope cannot build instances for an invalid snapshot: %s"), *SnapshotError.ToString()));
			Report.SetOutcome(TEXT("invalid snapshot"));
			return false;
		}
	}

	FInstanceSiteContext SiteContext;
	const double SiteOpenStartSeconds = FPlatformTime::Seconds();
	if (!OpenInstanceSiteContext(Volume, Snapshot, SiteContext, OutError))
	{
		Report.SetOutcome(TEXT("site reader open failed"));
		return false;
	}
	const double SiteOpenMilliseconds = (FPlatformTime::Seconds() - SiteOpenStartSeconds) * 1000.0;
	const int64 ScheduleBudgetBytes = static_cast<int64>(FMath::Max(
		CVarInstanceSiteScheduleBudgetMiB.GetValueOnGameThread(), 1)) * 1024ll * 1024ll;
	const int32 ScheduleChunkCount = SiteContext.Layout.GetChunkCount();
	uint64 SchedulePreflightBytes = 0;
	bool bSchedulePreflightValid = ScheduleChunkCount > 0 && ScheduleChunkCount < MAX_int32;
	for (const int32 RecordCount : {
		Snapshot.Individuals.Num(), Snapshot.Cohorts.Num(), Snapshot.Patches.Num(), Snapshot.Deadwood.Num() })
	{
		bSchedulePreflightValid = bSchedulePreflightValid
			&& TryAddReservedInt32ArrayBytes(RecordCount, SchedulePreflightBytes);
	}
	for (int32 ScheduleIndex = 0; ScheduleIndex < 4; ++ScheduleIndex)
	{
		bSchedulePreflightValid = bSchedulePreflightValid
			&& (ScheduleChunkCount < MAX_int32
				? TryAddReservedInt32ArrayBytes(ScheduleChunkCount + 1, SchedulePreflightBytes)
				: false);
	}
	for (int32 ScratchIndex = 0; ScratchIndex < 2; ++ScratchIndex)
	{
		bSchedulePreflightValid = bSchedulePreflightValid
			&& TryAddReservedInt32ArrayBytes(ScheduleChunkCount, SchedulePreflightBytes);
	}
	if (!bSchedulePreflightValid || SchedulePreflightBytes > static_cast<uint64>(MAX_int64))
	{
		OutError = FText::FromString(TEXT("ArcEcotope Instance Builder chunk schedule preflight overflowed."));
		Report.SetOutcome(TEXT("site chunk schedule preflight overflow"));
		return false;
	}
	if (SchedulePreflightBytes > static_cast<uint64>(ScheduleBudgetBytes))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope Instance Builder chunk schedule lower bound requires %.2f MiB, exceeding the %.2f MiB budget before allocation."),
			static_cast<double>(SchedulePreflightBytes) / (1024.0 * 1024.0),
			static_cast<double>(ScheduleBudgetBytes) / (1024.0 * 1024.0)));
		Report.SetOutcome(TEXT("site chunk schedule preflight budget exceeded"));
		return false;
	}

	FChunkRecordSchedule IndividualSchedule;
	FChunkRecordSchedule CohortSchedule;
	FChunkRecordSchedule PatchSchedule;
	FChunkRecordSchedule DeadwoodSchedule;
	int64 MaxScheduleScratchBytes = 0;
	const double ScheduleStartSeconds = FPlatformTime::Seconds();
	const auto ResolvePositionChunk = [&SiteContext](const FVector& LocalPosition, int32& OutChunkIndex)
	{
		int32 BaseCellIndex = INDEX_NONE;
		return SiteContext.Geometry.TryGetBaseCellIndex(
				FVector2D(LocalPosition.X, LocalPosition.Y), BaseCellIndex)
			&& TryGetChunkIndexForBaseCell(
				SiteContext.Layout, SiteContext.Geometry, BaseCellIndex, OutChunkIndex);
	};
	const bool bSchedulesValid = BuildChunkRecordSchedule(
		MakeArrayView(Snapshot.Individuals),
		SiteContext.Layout.GetChunkCount(),
		[&ResolvePositionChunk](const FArcForestIndividualRecord& Record, int32& OutChunkIndex)
		{
			return ResolvePositionChunk(Record.LocalPosition, OutChunkIndex);
		},
		TEXT("individual"),
		IndividualSchedule,
		MaxScheduleScratchBytes,
		OutError)
		&& BuildChunkRecordSchedule(
			MakeArrayView(Snapshot.Cohorts),
			SiteContext.Layout.GetChunkCount(),
			[&SiteContext](const FArcForestCohortRecord& Record, int32& OutChunkIndex)
			{
				return TryGetChunkIndexForRecordCell(
					SiteContext.Layout, SiteContext.Geometry, Record.CellIndex, OutChunkIndex);
			},
			TEXT("cohort"),
			CohortSchedule,
			MaxScheduleScratchBytes,
			OutError)
		&& BuildChunkRecordSchedule(
			MakeArrayView(Snapshot.Patches),
			SiteContext.Layout.GetChunkCount(),
			[&SiteContext](const FArcForestPatchRecord& Record, int32& OutChunkIndex)
			{
				return TryGetChunkIndexForRecordCell(
					SiteContext.Layout, SiteContext.Geometry, Record.CellIndex, OutChunkIndex);
			},
			TEXT("patch"),
			PatchSchedule,
			MaxScheduleScratchBytes,
			OutError)
		&& BuildChunkRecordSchedule(
			MakeArrayView(Snapshot.Deadwood),
			SiteContext.Layout.GetChunkCount(),
			[&ResolvePositionChunk](const FArcForestDeadwoodRecord& Record, int32& OutChunkIndex)
			{
				return ResolvePositionChunk(Record.LocalTransform.GetTranslation(), OutChunkIndex);
			},
			TEXT("deadwood"),
			DeadwoodSchedule,
			MaxScheduleScratchBytes,
			OutError);
	if (!bSchedulesValid)
	{
		Report.SetOutcome(TEXT("site chunk scheduling failed"));
		return false;
	}
	const double ScheduleMilliseconds = (FPlatformTime::Seconds() - ScheduleStartSeconds) * 1000.0;
	int64 ScheduleBytes = 0;
	for (const FChunkRecordSchedule* Schedule : {
		&IndividualSchedule, &CohortSchedule, &PatchSchedule, &DeadwoodSchedule })
	{
		const int64 Bytes = Schedule->GetAllocatedBytes();
		if (ScheduleBytes > MAX_int64 - Bytes)
		{
			OutError = FText::FromString(TEXT("ArcEcotope Instance Builder chunk schedules overflow their memory accounting."));
			Report.SetOutcome(TEXT("site chunk schedule memory overflow"));
			return false;
		}
		ScheduleBytes += Bytes;
	}
	if (ScheduleBytes > MAX_int64 - MaxScheduleScratchBytes)
	{
		OutError = FText::FromString(TEXT("ArcEcotope Instance Builder chunk schedule peak memory accounting overflowed."));
		Report.SetOutcome(TEXT("site chunk schedule peak memory overflow"));
		return false;
	}
	const int64 SchedulePeakBytes = ScheduleBytes + MaxScheduleScratchBytes;
	if (SchedulePeakBytes > ScheduleBudgetBytes)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope Instance Builder chunk schedules require %.2f MiB, exceeding the %.2f MiB budget."),
			static_cast<double>(SchedulePeakBytes) / (1024.0 * 1024.0),
			static_cast<double>(ScheduleBudgetBytes) / (1024.0 * 1024.0)));
		Report.SetOutcome(TEXT("site chunk schedule budget exceeded"));
		return false;
	}
	int32 ActiveChunkCount = 0;
	for (int32 ChunkIndex = 0; ChunkIndex < SiteContext.Layout.GetChunkCount(); ++ChunkIndex)
	{
		if (!IndividualSchedule.GetRecords(ChunkIndex).IsEmpty()
			|| !CohortSchedule.GetRecords(ChunkIndex).IsEmpty()
			|| !PatchSchedule.GetRecords(ChunkIndex).IsEmpty()
			|| !DeadwoodSchedule.GetRecords(ChunkIndex).IsEmpty())
		{
			++ActiveChunkCount;
		}
	}

	const int64 LiveCap = Volume.BudgetSettings.MaxLiveStems;
	const int64 DeadwoodCap = static_cast<int64>(Volume.BudgetSettings.MaxDeadwoodRecords) + Volume.BudgetSettings.MaxBranchRecords;
	if (LiveCap < 0 || DeadwoodCap < 0 || LiveCap > MAX_int64 - DeadwoodCap)
	{
		OutError = FText::FromString(TEXT("ArcEcotope instance caps are invalid or overflow their combined safe cap."));
		return false;
	}
	const int64 TotalCap = LiveCap + DeadwoodCap;
	int64 TotalInstances = 0;
	int64 LiveInstances = 0;
	int64 DeadwoodInstances = 0;
	int64 CohortExpandedInstances = 0;
	int64 PatchExpandedInstances = 0;
	int64 ChannelBufferSkippedCohortRecords = 0;
	TArray<FGroup> Groups;
	TMap<FArcEcotopeGroupKey, int32> GroupIndices;
	TMap<FString, FMissingVisualWarning> MissingVisualWarnings;
	TMap<FGuid, FNearestVisualCount> NearestVisualCounts;
	TMap<FString, FClusterRadiusWarning> ClusterRadiusWarnings;
	TMap<FString, FClusterSiteCapWarning> ClusterSiteCapWarnings;
	FVisualValidityCache VisualValidity;
	FSpeciesValidityCache SpeciesValidity;

	auto RecordMissingVisual = [&MissingVisualWarnings](const UArcEcotopeSpeciesModel& Species, const EArcForestVisualKind Kind, const int64 SkippedInstanceCount)
	{
		check(SkippedInstanceCount >= 0);
		const FString Key = FString::Printf(TEXT("%s|%d"), *Species.SpeciesId.ToString(EGuidFormats::DigitsWithHyphens), static_cast<int32>(Kind));
		FMissingVisualWarning& Warning = MissingVisualWarnings.FindOrAdd(Key);
		Warning.SpeciesName = Species.GetName();
		Warning.Kind = Kind;
		Warning.SkippedInstanceCount = Warning.SkippedInstanceCount > MAX_int64 - SkippedInstanceCount
			? MAX_int64
			: Warning.SkippedInstanceCount + SkippedInstanceCount;
	};

	auto RecordNearestVisual = [&NearestVisualCounts](const UArcEcotopeSpeciesModel& Species, const int64 InstanceCount)
	{
		check(InstanceCount >= 0);
		FNearestVisualCount& Count = NearestVisualCounts.FindOrAdd(Species.SpeciesId);
		Count.SpeciesName = Species.GetName();
		Count.InstanceCount = Count.InstanceCount > MAX_int64 - InstanceCount
			? MAX_int64
			: Count.InstanceCount + InstanceCount;
	};

	auto RecordClusterRadius = [&ClusterRadiusWarnings](
		const UArcEcotopeSpeciesModel& Species,
		const int32 LayerIndex,
		const double AuthoredRadiusCm,
		const double MaxRadiusCm)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%d"),
			*Species.SpeciesId.ToString(EGuidFormats::DigitsWithHyphens),
			LayerIndex);
		FClusterRadiusWarning& Warning = ClusterRadiusWarnings.FindOrAdd(Key);
		Warning.SpeciesName = Species.GetName();
		Warning.LayerIndex = LayerIndex;
		Warning.AuthoredRadiusCm = AuthoredRadiusCm;
		Warning.MaxRadiusCm = Warning.RecordCount > 0 ? FMath::Min(Warning.MaxRadiusCm, MaxRadiusCm) : MaxRadiusCm;
		Warning.RecordCount = Warning.RecordCount == MAX_int64 ? MAX_int64 : Warning.RecordCount + 1;
	};

	auto RecordClusterSiteCap = [&ClusterSiteCapWarnings](
		const UArcEcotopeSpeciesModel& Species,
		const int32 LayerIndex,
		const FClusterSiteSweep& Sweep,
		const FInstanceSiteGeometry& Geometry)
	{
		const FString Key = FString::Printf(
			TEXT("%s|%d"),
			*Species.SpeciesId.ToString(EGuidFormats::DigitsWithHyphens),
			LayerIndex);
		FClusterSiteCapWarning& Warning = ClusterSiteCapWarnings.FindOrAdd(Key);
		Warning.SpeciesName = Species.GetName();
		Warning.LayerIndex = LayerIndex;
		Warning.ClusterRadiusCm = Sweep.ClusterRadiusCm;
		Warning.RecordCellSizeCm = GetRecordCellSizeCm(Geometry);
		Warning.RecordGridLevel = Geometry.RecordGridLevel;
		Warning.CandidateSiteCount = FMath::Max(Warning.CandidateSiteCount, Sweep.CandidateSiteCount);
		Warning.RecordCount = Warning.RecordCount == MAX_int64 ? MAX_int64 : Warning.RecordCount + 1;
	};

	auto ResolveLiveBand = [&Volume, &OutError, &VisualValidity, &SpeciesValidity](
		const FGuid& SpeciesId,
		const float HeightCm,
		const FArcEcotopeSpeciesSetup*& OutSetup,
		FResolvedBand& OutResolved,
		bool& bOutNearestMatch) -> EVisualLookupResult
	{
		OutSetup = FindEnabledSetup(Volume, SpeciesId);
		if (OutSetup == nullptr)
		{
			OutError = FText::FromString(TEXT("ArcForest snapshot references a species that is not enabled on its volume."));
			return EVisualLookupResult::Invalid;
		}
		return FindBandForSize(
			*OutSetup->Species, EArcForestVisualKind::Alive, HeightCm, VisualValidity, SpeciesValidity, OutResolved,
			bOutNearestMatch, OutError);
	};

	TMap<const FArcForestBandVisual*, uint64> RendererHashes;
	auto GetRendererHash = [&RendererHashes](const FArcForestBandVisual& Visual) -> uint64
	{
		if (const uint64* Existing = RendererHashes.Find(&Visual))
		{
			return *Existing;
		}
		const uint64 RendererHash = Visual.ComputeHash();
		RendererHashes.Add(&Visual, RendererHash);
		return RendererHash;
	};

	auto AddLive = [&](
		const FArcEcotopeSpeciesSetup& Setup,
		const FResolvedBand& Resolved,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 Ordinal,
		const FVector& Position,
		const FVector& Normal,
		const float HeightCm,
		const float AgeYears) -> bool
	{
		if (LiveInstances >= LiveCap)
		{
			OutError = FText::FromString(TEXT("ArcEcotope live instance generation exceeded MaxLiveStems."));
			return false;
		}
		const FGuid& SpeciesId = Setup.Species->SpeciesId;
		const FArcForestStageBand& Band = *Resolved.Band;
		const FQuat Align = UE::ArcForest::MakeTerrainAlign(Normal, Band.Rotation.MaxAlignDeg);
		const double Scale = MakeScale(
			Band.Scale, Snapshot, SpeciesMaxAgeYearsCache, SpeciesId, CellIndex, EntityId, Ordinal,
			HeightCm, AgeYears, Band.MinHeightCm, Band.MaxHeightCm);
		// Offset axis is the align-only up: vertical at MaxAlignDeg 0, the terrain normal at full alignment. Tilt and yaw are deliberately excluded.
		const FVector Up = Align.RotateVector(FVector::UpVector);
		const FVector Seated = Position + Up * (static_cast<double>(Band.OffsetAlongUpCm) * Scale);
		const FTransform Transform(
			Align * MakeRotation(Band.Rotation, Snapshot, SpeciesId, CellIndex, EntityId, Ordinal),
			Seated,
			FVector(Scale));
		if (!TryAddTransform(
			Groups, GroupIndices, *Setup.Species, Resolved, Snapshot, Context, CellIndex, EntityId, Ordinal,
			Transform, Context.VolumeTransform.TransformPosition(Seated), GetRendererHash(*Band.Visual.GetPtr()),
			TotalInstances, TotalCap, OutError))
		{
			return false;
		}
		++LiveInstances;
		return true;
	};

	// Preserve canonical source-order accumulation. Chunking changes output insertion order only.
	TMap<int32, double> CellTotalPatchEquivalent;
	TMap<TPair<FGuid, int32>, double> SpeciesCellPatchEquivalent;
	for (const FArcForestPatchRecord& Record : Snapshot.Patches)
	{
		if (FMath::IsFinite(Record.StemOrCoverEquivalent) && Record.StemOrCoverEquivalent >= 0.0f)
		{
			CellTotalPatchEquivalent.FindOrAdd(Record.CellIndex) += static_cast<double>(Record.StemOrCoverEquivalent);
			SpeciesCellPatchEquivalent.FindOrAdd(TPair<FGuid, int32>(Record.SpeciesId, Record.CellIndex))
				+= static_cast<double>(Record.StemOrCoverEquivalent);
		}
	}

	uint64 ReaderAcquireCount = 0;
	uint64 ReaderFailedAcquireCount = 0;
	const auto LogReaderFailure = [&](const TCHAR* FailureContext)
	{
		ReaderFailedAcquireCount = ReaderFailedAcquireCount == MAX_uint64
			? MAX_uint64 : ReaderFailedAcquireCount + 1;
		const FArcEcotopeSiteBulkReaderStats FailureStats = SiteContext.Reader.GetBulkStats();
			UE_LOGF(
				LogArcEcotopeInstanceBuilder,
				Error,"ArcEcotope Instance Builder reader failure | context=%ls logicalAcquire=%llu logicalFailed=%llu bulkAcquire=%llu bulkFailed=%llu peak=%.2fMiB budget=%.2fMiB hits=%llu misses=%llu evictions=%llu",
				FailureContext,
				ReaderAcquireCount,
				ReaderFailedAcquireCount,
				FailureStats.AcquireCount,
				FailureStats.FailedAcquireCount,
				static_cast<double>(FailureStats.PeakTotalBytes) / (1024.0 * 1024.0),
				static_cast<double>(FailureStats.BudgetBytes) / (1024.0 * 1024.0),
				FailureStats.CacheHitCount,
				FailureStats.CacheMissCount,
				FailureStats.EvictionCount);
		SiteContext.Reader.LogBulkStats(FailureContext);
	};
	const double MaterializationStartSeconds = FPlatformTime::Seconds();
	for (int32 SourceChunkIndex = 0; SourceChunkIndex < SiteContext.Layout.GetChunkCount(); ++SourceChunkIndex)
	{
		const TConstArrayView<int32> ChunkIndividuals = IndividualSchedule.GetRecords(SourceChunkIndex);
		const TConstArrayView<int32> ChunkCohorts = CohortSchedule.GetRecords(SourceChunkIndex);
		const TConstArrayView<int32> ChunkPatches = PatchSchedule.GetRecords(SourceChunkIndex);
		const TConstArrayView<int32> ChunkDeadwood = DeadwoodSchedule.GetRecords(SourceChunkIndex);
		if (ChunkIndividuals.IsEmpty() && ChunkCohorts.IsEmpty()
			&& ChunkPatches.IsEmpty() && ChunkDeadwood.IsEmpty())
		{
			continue;
		}

		FArcEcotopeChunkRect SourceChunk;
		if (!SiteContext.Layout.GetChunk(SourceChunkIndex, SourceChunk))
		{
			OutError = FText::FromString(TEXT("ArcEcotope Instance Builder schedule references a missing source chunk."));
			Discard(OutPrepared);
			Report.SetOutcome(TEXT("source chunk lookup failed"));
			return false;
		}
		FArcEcotopeSiteChunkLease SiteLease;
		FString AcquireError;
		++ReaderAcquireCount;
		if (!SiteContext.Reader.AcquireFields(
				SourceChunk.Id, InstanceSiteHaloBaseCells, InstanceSiteFieldMask, SiteLease, AcquireError))
		{
			LogReaderFailure(TEXT("primary-acquire"));
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope Instance Builder could not acquire active source chunk (%d,%d): %s"),
				SourceChunk.Id.X, SourceChunk.Id.Y, *AcquireError));
			Discard(OutPrepared);
			Report.SetOutcome(TEXT("source chunk acquire failed"));
			return false;
		}
		const FArcEcotopeSiteChunkView& SiteView = SiteLease.GetView();
		if (!ValidateInstanceChunkView(
				SiteContext, SourceChunk, SiteView, InstanceSiteHaloBaseCells, OutError))
		{
			LogReaderFailure(TEXT("primary-view-validation"));
			Discard(OutPrepared);
			Report.SetOutcome(TEXT("source chunk view invalid"));
			return false;
		}

		const auto ValidateBasePosition = [&](const FVector& LocalPosition) -> bool
		{
			int32 BaseCellIndex = INDEX_NONE;
			if (!SiteContext.Geometry.TryGetBaseCellIndex(
					FVector2D(LocalPosition.X, LocalPosition.Y), BaseCellIndex))
			{
				return false;
			}
			const int32 BaseX = BaseCellIndex % SiteContext.Geometry.BaseDimensions.X;
			const int32 BaseY = BaseCellIndex / SiteContext.Geometry.BaseDimensions.X;
			const uint8* Valid = SiteView.ValidSurface.TryGet(BaseX, BaseY);
			return Valid != nullptr && *Valid != 0;
		};
		const auto ValidateRecordCell = [&](const int32 RecordCellIndex) -> bool
		{
			if (RecordCellIndex < 0 || SiteContext.Geometry.RecordDimensions.X <= 0)
			{
				return false;
			}
			const int32 RecordX = RecordCellIndex % SiteContext.Geometry.RecordDimensions.X;
			const int32 RecordY = RecordCellIndex / SiteContext.Geometry.RecordDimensions.X;
			const uint8* Valid = SiteView.RecordValid.TryGet(RecordX, RecordY);
			return Valid != nullptr && *Valid != 0;
		};
		for (const int32 RecordIndex : ChunkIndividuals)
		{
			if (!Snapshot.Individuals.IsValidIndex(RecordIndex)
				|| !ValidateBasePosition(Snapshot.Individuals[RecordIndex].LocalPosition))
			{
				OutError = FText::FromString(TEXT("An individual is outside a valid baked site cell."));
				LogReaderFailure(TEXT("individual-valid-surface"));
				Discard(OutPrepared);
				Report.SetOutcome(TEXT("individual site validation failed"));
				return false;
			}
		}
		for (const int32 RecordIndex : ChunkCohorts)
		{
			if (!Snapshot.Cohorts.IsValidIndex(RecordIndex)
				|| !ValidateRecordCell(Snapshot.Cohorts[RecordIndex].CellIndex))
			{
				OutError = FText::FromString(TEXT("A cohort references an invalid baked site cell."));
				LogReaderFailure(TEXT("cohort-record-valid"));
				Discard(OutPrepared);
				Report.SetOutcome(TEXT("cohort site validation failed"));
				return false;
			}
		}
		for (const int32 RecordIndex : ChunkPatches)
		{
			if (!Snapshot.Patches.IsValidIndex(RecordIndex)
				|| !ValidateRecordCell(Snapshot.Patches[RecordIndex].CellIndex))
			{
				OutError = FText::FromString(TEXT("A patch references an invalid baked site cell."));
				LogReaderFailure(TEXT("patch-record-valid"));
				Discard(OutPrepared);
				Report.SetOutcome(TEXT("patch site validation failed"));
				return false;
			}
		}
		for (const int32 RecordIndex : ChunkDeadwood)
		{
			if (!Snapshot.Deadwood.IsValidIndex(RecordIndex)
				|| !ValidateBasePosition(Snapshot.Deadwood[RecordIndex].LocalTransform.GetTranslation()))
			{
				OutError = FText::FromString(TEXT("A deadwood record is outside a valid baked site cell."));
				LogReaderFailure(TEXT("deadwood-valid-surface"));
				Discard(OutPrepared);
				Report.SetOutcome(TEXT("deadwood site validation failed"));
				return false;
			}
		}

	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_Individuals);
		for (const int32 RecordIndex : ChunkIndividuals)
		{
			const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
			if (Record.LifeStatus != EArcForestLifeStatus::Alive)
			{
				OutError = FText::FromString(TEXT("ArcForest individual records must be alive; dead records are rendered through deadwood."));
				Discard(OutPrepared);
				return false;
			}
			const FArcEcotopeSpeciesSetup* Setup = nullptr;
			FResolvedBand Resolved;
			bool bNearestMatch = false;
			const EVisualLookupResult VisualResult = ResolveLiveBand(Record.SpeciesId, Record.HeightCm, Setup, Resolved, bNearestMatch);
			if (VisualResult == EVisualLookupResult::Missing)
			{
				RecordMissingVisual(*Setup->Species, EArcForestVisualKind::Alive, 1);
				continue;
			}
			if (VisualResult == EVisualLookupResult::Invalid)
			{
				Discard(OutPrepared);
				return false;
			}
			FVector Position = Record.LocalPosition;
			FSurfaceSample Surface;
			if (!SampleSurface(SiteContext.Geometry, SiteView.LocalSurfaceZ, SiteView.ValidSurface,
					SiteView.Slope, SiteView.Aspect, FVector2D(Position.X, Position.Y), Surface))
			{
				OutError = FText::FromString(TEXT("ArcEcotope instance position resolved no valid baked surface."));
				LogReaderFailure(TEXT("individual-surface-sample"));
				Discard(OutPrepared);
				return false;
			}
			Position.Z = Surface.SurfaceZCm;
			if (!AddLive(*Setup, Resolved, INDEX_NONE, Record.Id, 0, Position, Surface.Normal, Record.HeightCm, static_cast<float>(Record.AgeYears)))
			{
				Discard(OutPrepared);
				return false;
			}
			if (bNearestMatch)
			{
				RecordNearestVisual(*Setup->Species, 1);
			}
		}
	}
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_Cohorts);
		for (const int32 RecordIndex : ChunkCohorts)
		{
			const FArcForestCohortRecord& Record = Snapshot.Cohorts[RecordIndex];
			if (Record.StemCount < 0)
			{
				OutError = FText::FromString(TEXT("ArcForest cohort stem count is negative."));
				Discard(OutPrepared);
				return false;
			}
			const FArcEcotopeSpeciesSetup* Setup = nullptr;
			FResolvedBand Resolved;
			bool bNearestMatch = false;
			const EVisualLookupResult VisualResult = ResolveLiveBand(Record.SpeciesId, Record.MeanHeightCm, Setup, Resolved, bNearestMatch);
			if (VisualResult == EVisualLookupResult::Missing)
			{
				RecordMissingVisual(*Setup->Species, EArcForestVisualKind::Alive, Record.StemCount);
				continue;
			}
			if (VisualResult == EVisualLookupResult::Invalid)
			{
				Discard(OutPrepared);
				return false;
			}
			TArray<int32> PlantableBaseCells;
			bool bAllExcludedByChannelBuffer = false;
			if (!TryGatherTreePlantableBaseCells(
					SiteContext.Geometry, SiteView.ValidSurface, SiteView.ChannelBuffer, Record.CellIndex,
					PlantableBaseCells, bAllExcludedByChannelBuffer)
				|| (PlantableBaseCells.IsEmpty() && !bAllExcludedByChannelBuffer))
			{
				OutError = FText::FromString(TEXT("ArcForest cohort references an invalid site record cell."));
				LogReaderFailure(TEXT("cohort-plantable-children"));
				Discard(OutPrepared);
				return false;
			}
			if (bAllExcludedByChannelBuffer)
			{
				++ChannelBufferSkippedCohortRecords;
				continue;
			}
			for (int64 Stem = 0; Stem < Record.StemCount; ++Stem)
			{
				FVector Position;
				if (!TryMakeRepresentativePosition(
						SiteContext.Geometry, SiteView.LocalSurfaceZ, Snapshot, Record.SpeciesId,
						Record.CellIndex, PlantableBaseCells, Record.Id, static_cast<int32>(Stem), Position, OutError))
				{
					LogReaderFailure(TEXT("cohort-representative-position"));
					Discard(OutPrepared);
					return false;
				}
				FSurfaceSample Surface;
				if (!SampleSurface(SiteContext.Geometry, SiteView.LocalSurfaceZ, SiteView.ValidSurface,
						SiteView.Slope, SiteView.Aspect, FVector2D(Position.X, Position.Y), Surface))
				{
					OutError = FText::FromString(TEXT("ArcEcotope instance position resolved no valid baked surface."));
					LogReaderFailure(TEXT("cohort-surface-sample"));
					Discard(OutPrepared);
					return false;
				}
				Position.Z = Surface.SurfaceZCm;
				if (!AddLive(
						*Setup, Resolved, Record.CellIndex, Record.Id, static_cast<int32>(Stem),
						Position, Surface.Normal, Record.MeanHeightCm, Record.MeanAgeYears))
				{
					Discard(OutPrepared);
					return false;
				}
			}
			if (bNearestMatch)
			{
				RecordNearestVisual(*Setup->Species, Record.StemCount);
			}
			CohortExpandedInstances += Record.StemCount;
		}
	}
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_Patches);
		TArray<FArcForestCoverClusterLayer> ClusterLayers;
		TArray<FClusterLayerSites> LayerSites;
		TArray<int32> ClusterPlantableBaseCells;
		const FClusterSiteContext ClusterSiteContext{
			SiteContext.Geometry,
			SiteView.ValidSurface,
			SiteView.Moisture,
			SiteView.Fertility,
			SiteView.BareRock,
			CellTotalPatchEquivalent,
			SpeciesCellPatchEquivalent};
		for (const int32 RecordIndex : ChunkPatches)
		{
			const FArcForestPatchRecord& Record = Snapshot.Patches[RecordIndex];
			if (!FMath::IsFinite(Record.StemOrCoverEquivalent) || Record.StemOrCoverEquivalent < 0.0f || Record.StemOrCoverEquivalent > static_cast<double>(MAX_int64 - 1))
			{
				OutError = FText::FromString(TEXT("ArcForest patch representative count is invalid or outside the safe integer range."));
				Discard(OutPrepared);
				return false;
			}
			TArray<int32> PlantableBaseCells;
			const bool bHasPlantableBaseCells = TryGatherPlantableBaseCells(
				SiteContext.Geometry, SiteView.ValidSurface, Record.CellIndex, PlantableBaseCells)
				&& !PlantableBaseCells.IsEmpty();
			const double* CellTotalEquivalent = CellTotalPatchEquivalent.Find(Record.CellIndex);
			const int64 RepresentativeCount = bHasPlantableBaseCells
				? ComputePatchInstanceCount(
					static_cast<double>(Record.StemOrCoverEquivalent),
					CellTotalEquivalent != nullptr ? *CellTotalEquivalent : 0.0,
					ComputePlantableAreaSquareCm(SiteContext.Geometry, PlantableBaseCells),
					GetMaxPatchInstancesPerRecord())
				: 0;
			const FArcEcotopeSpeciesSetup* Setup = nullptr;
			FResolvedBand Resolved;
			bool bNearestMatch = false;
			const EVisualLookupResult VisualResult = ResolveLiveBand(Record.SpeciesId, Record.MeanHeightCm, Setup, Resolved, bNearestMatch);
			if (VisualResult == EVisualLookupResult::Missing)
			{
				RecordMissingVisual(*Setup->Species, EArcForestVisualKind::Alive, RepresentativeCount);
				continue;
			}
			if (VisualResult == EVisualLookupResult::Invalid)
			{
				Discard(OutPrepared);
				return false;
			}
			if (!bHasPlantableBaseCells)
			{
				OutError = FText::FromString(TEXT("ArcForest patch references an invalid site record cell."));
				LogReaderFailure(TEXT("patch-plantable-children"));
				Discard(OutPrepared);
				return false;
			}
			ResolveCoverClusterLayers(*Setup->Species, ClusterLayers);
			LayerSites.SetNum(ClusterLayers.Num());
			for (FClusterLayerSites& Sites : LayerSites)
			{
				Sites.Centres.Reset();
			}
			ClusterPlantableBaseCells.Reset();
			FVector2D RecordMinLocal = FVector2D::ZeroVector;
			FVector2D RecordMaxLocal = FVector2D::ZeroVector;
			if (RepresentativeCount > 0
				&& SiteContext.Geometry.TryGetLevelCellRect(
					Record.CellIndex, SiteContext.Geometry.RecordGridLevel, RecordMinLocal, RecordMaxLocal))
			{
				if (!TryGatherClusterPlantableBaseCells(
						SiteContext.Geometry, SiteView.ValidSurface, Record.CellIndex, MaxClusterHaloBaseCells,
						ClusterPlantableBaseCells))
				{
					OutError = FText::FromString(
						TEXT("ArcForest patch cluster plantability rejected a record cell the record path accepted."));
					LogReaderFailure(TEXT("patch-cluster-plantable-children"));
					Discard(OutPrepared);
					return false;
				}
				const double MaxRadiusCm = GetClusterRadiusCeilingCm(SiteContext.Geometry);
				for (int32 LayerIndex = 0; LayerIndex < ClusterLayers.Num(); ++LayerIndex)
				{
					const double AuthoredRadiusCm = SanitizeClusterRadiusCm(ClusterLayers[LayerIndex].ClusterRadiusCm);
					if (ClusterLayers[LayerIndex].InstancesPerCluster > 1 && AuthoredRadiusCm > MaxRadiusCm)
					{
						RecordClusterRadius(*Setup->Species, LayerIndex, AuthoredRadiusCm, MaxRadiusCm);
					}
					FClusterSiteSweep Sweep;
					GatherActiveClusterSites(
						ClusterSiteContext, Snapshot, Record.SpeciesId, Record.CellIndex, RecordMinLocal,
						RecordMaxLocal, ClusterLayers, LayerIndex, LayerSites[LayerIndex].Centres, Sweep);
					if (Sweep.bCandidateCapExceeded)
					{
						RecordClusterSiteCap(*Setup->Species, LayerIndex, Sweep, SiteContext.Geometry);
					}
				}
			}
			for (int64 Stem = 0; Stem < RepresentativeCount; ++Stem)
			{
				FVector Position;
				if (!TryMakeClusteredPatchPosition(
						SiteContext.Geometry, SiteView.LocalSurfaceZ, Snapshot, Record.SpeciesId,
						Record.CellIndex, PlantableBaseCells, ClusterPlantableBaseCells, ClusterLayers,
						LayerSites, Record.Id, static_cast<int32>(Stem), Position, OutError))
				{
					LogReaderFailure(TEXT("patch-clustered-position"));
					Discard(OutPrepared);
					return false;
				}
				FSurfaceSample Surface;
				if (!SampleSurface(SiteContext.Geometry, SiteView.LocalSurfaceZ, SiteView.ValidSurface,
						SiteView.Slope, SiteView.Aspect, FVector2D(Position.X, Position.Y), Surface))
				{
					OutError = FText::FromString(TEXT("ArcEcotope instance position resolved no valid baked surface."));
					LogReaderFailure(TEXT("patch-surface-sample"));
					Discard(OutPrepared);
					return false;
				}
				Position.Z = Surface.SurfaceZCm;
				if (!AddLive(
						*Setup, Resolved, Record.CellIndex, Record.Id, static_cast<int32>(Stem),
						Position, Surface.Normal, Record.MeanHeightCm, Record.MeanAgeYears))
				{
					Discard(OutPrepared);
					return false;
				}
			}
			if (bNearestMatch)
			{
				RecordNearestVisual(*Setup->Species, RepresentativeCount);
			}
			PatchExpandedInstances += RepresentativeCount;
		}
	}
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_Deadwood);
		for (const int32 RecordIndex : ChunkDeadwood)
		{
			const FArcForestDeadwoodRecord& Record = Snapshot.Deadwood[RecordIndex];
			const FArcEcotopeSpeciesSetup* Setup = FindEnabledSetup(Volume, Record.SpeciesId);
			const EArcForestVisualKind Kind = MakeVisualKind(Record.Type);
			if (Setup == nullptr)
			{
				OutError = FText::FromString(TEXT("ArcForest deadwood references a species that is not enabled on its volume."));
				Discard(OutPrepared);
				return false;
			}
			// Z is the stem length for every dead form; lying down is expressed in LocalTransform.
			const float StemLengthCm = static_cast<float>(Record.DimensionsCm.Z);
			FResolvedBand Resolved;
			bool bNearestMatch = false;
			const EVisualLookupResult VisualResult = FindBandForSize(
				*Setup->Species, Kind, StemLengthCm, VisualValidity, SpeciesValidity, Resolved,
				bNearestMatch, OutError);
			if (VisualResult == EVisualLookupResult::Missing)
			{
				RecordMissingVisual(*Setup->Species, Kind, 1);
				continue;
			}
			if (VisualResult == EVisualLookupResult::Invalid)
			{
				Discard(OutPrepared);
				return false;
			}
			if (DeadwoodInstances >= DeadwoodCap)
			{
				OutError = FText::FromString(
					TEXT("ArcEcotope deadwood instance generation exceeded MaxDeadwoodRecords plus MaxBranchRecords."));
				Discard(OutPrepared);
				return false;
			}
			const FArcForestStageBand& Band = *Resolved.Band;
			FTransform Transform = Record.LocalTransform;
			FVector Position = Record.LocalTransform.GetTranslation();
			FSurfaceSample Surface;
			if (!SampleSurface(SiteContext.Geometry, SiteView.LocalSurfaceZ, SiteView.ValidSurface,
					SiteView.Slope, SiteView.Aspect, FVector2D(Position.X, Position.Y), Surface))
			{
				OutError = FText::FromString(TEXT("ArcEcotope instance position resolved no valid baked surface."));
				LogReaderFailure(TEXT("deadwood-surface-sample"));
				Discard(OutPrepared);
				return false;
			}
			Position.Z = Surface.SurfaceZCm;
			const FQuat Align = UE::ArcForest::MakeTerrainAlign(Surface.Normal, Band.Rotation.MaxAlignDeg);
			const double Scale = MakeScale(
				Band.Scale, Snapshot, SpeciesMaxAgeYearsCache, Record.SpeciesId, INDEX_NONE, Record.Id, 0,
				StemLengthCm, 0.0f, Band.MinHeightCm, Band.MaxHeightCm);
			const FVector Up = Align.RotateVector(FVector::UpVector);
			const FQuat RecordRotation = Band.Rotation.bIgnoreStemBearing
				? FQuat::Identity
				: Record.LocalTransform.GetRotation();
			Transform.SetRotation(Align
				* MakeRotation(Band.Rotation, Snapshot, Record.SpeciesId, INDEX_NONE, Record.Id, 0)
				* RecordRotation);
			Transform.SetTranslation(Position + Up * (static_cast<double>(Band.OffsetAlongUpCm) * Scale));
			Transform.SetScale3D(FVector(Scale));
			if (!TryAddTransform(
					Groups, GroupIndices, *Setup->Species, Resolved, Snapshot, Context, INDEX_NONE, Record.Id, 0,
					Transform, Context.VolumeTransform.TransformPosition(Transform.GetTranslation()),
					GetRendererHash(*Band.Visual.GetPtr()), TotalInstances, TotalCap, OutError)) { Discard(OutPrepared); return false; }
			++DeadwoodInstances;
			if (bNearestMatch)
			{
				RecordNearestVisual(*Setup->Species, 1);
			}
		}
	}
	}
	const double MaterializationMilliseconds =
		(FPlatformTime::Seconds() - MaterializationStartSeconds) * 1000.0;
	if (ReaderAcquireCount != static_cast<uint64>(ActiveChunkCount))
	{
		OutError = FText::FromString(TEXT("ArcEcotope Instance Builder did not acquire every active source chunk exactly once."));
		LogReaderFailure(TEXT("primary-acquire-count"));
		Discard(OutPrepared);
		Report.SetOutcome(TEXT("source chunk acquire count mismatch"));
		return false;
	}
	const FArcEcotopeSiteBulkReaderStats ReaderStats = SiteContext.Reader.GetBulkStats();
	SiteContext.Reader.LogBulkStats(TEXT("InstanceBuilder"));
	Report.AddDetail(FString::Printf(
		TEXT("siteReader: source=EditorBulk chunks=%d/%d acquire=%llu failed=%llu schedulePeak=%.2f/%.2fMiB scheduleScratch=%.2fMiB readerPeak=%.2f/%.2fMiB hits=%llu misses=%llu evictions=%llu | open=%.2fms schedule=%.2fms materialize=%.2fms"),
		ActiveChunkCount,
		SiteContext.Layout.GetChunkCount(),
		ReaderAcquireCount,
		ReaderFailedAcquireCount,
		static_cast<double>(SchedulePeakBytes) / (1024.0 * 1024.0),
		static_cast<double>(ScheduleBudgetBytes) / (1024.0 * 1024.0),
		static_cast<double>(MaxScheduleScratchBytes) / (1024.0 * 1024.0),
		static_cast<double>(ReaderStats.PeakTotalBytes) / (1024.0 * 1024.0),
		static_cast<double>(SiteContext.ReaderBudgetBytes) / (1024.0 * 1024.0),
		ReaderStats.CacheHitCount,
		ReaderStats.CacheMissCount,
		ReaderStats.EvictionCount,
		SiteOpenMilliseconds,
		ScheduleMilliseconds,
		MaterializationMilliseconds));
	OutPrepared.SnapshotHash = Snapshot.StateChecksum;
	OutPrepared.VisualHash = Volume.ComputeEnabledSpeciesVisualHash();
	Report.AddDetail(FString::Printf(
		TEXT("records: individuals=%d cohorts=%d patches=%d deadwood=%d | instances: total=%lld live=%lld deadwood=%lld cohortExpansion=%lld patchExpansion=%lld | groups=%d"),
		Snapshot.Individuals.Num(),
		Snapshot.Cohorts.Num(),
		Snapshot.Patches.Num(),
		Snapshot.Deadwood.Num(),
		TotalInstances,
		LiveInstances,
		DeadwoodInstances,
		CohortExpandedInstances,
		PatchExpandedInstances,
		Groups.Num()));
	FDiscoveredPartitionActors Discovered;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_DiscoverOwnedPartitionActors);
		DiscoverOwnedPartitionActors(Volume, Context.WorldPartition, Discovered);
		OutPrepared.PinnedActors.Append(Discovered.Pins);
	}
	TArray<AArcEcotopeInstancePartitionActor*> PreviousInstanceActors;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Build_ResolveOwnedInstanceActors);
		if (!ResolveOwnedInstanceActors(Volume, Context, Discovered, OutPrepared, PreviousInstanceActors, OutError))
		{
			Discard(OutPrepared);
			Report.SetOutcome(TEXT("owned instance actors failed to load"));
			return false;
		}
	}
	if (!BuildComponents(Volume, Groups, Context, Discovered, Diagnostics, OutPrepared, OutError))
	{
		RecordOwnedInstanceActorsOnFailure(PreviousInstanceActors, OutPrepared);
		Discard(OutPrepared);
		Report.SetOutcome(TEXT("component build failed"));
		return false;
	}
	FinalizeInstanceActorRecords(PreviousInstanceActors, OutPrepared);
	// After the components, so a component failure leaves no entity entries behind. Entity entries are
	// written straight onto the placed-entity partition actors and are not part of the prepared set.
	if (!EmitEntityInstances(Volume, Groups, Context, Discovered, Diagnostics, OutPrepared, OutError))
	{
		RecordOwnedInstanceActorsOnFailure(PreviousInstanceActors, OutPrepared);
		Discard(OutPrepared);
		Report.SetOutcome(TEXT("entity emission failed"));
		return false;
	}
	for (const FGroup& Group : Groups)
	{
		Report.AddDetail(FString::Printf(
			TEXT("group %016llX: %s instances=%d"),
			Group.NameHash,
			DescribeGroupVisual(Group),
			Group.Transforms.Num()));
	}
	Report.SetOutcome(TEXT("succeeded"));
	for (const TPair<FString, FMissingVisualWarning>& Pair : MissingVisualWarnings)
	{
		const FMissingVisualWarning& Warning = Pair.Value;
		UE_LOGF(LogArcEcotopeInstanceBuilder, Warning,"Species %ls has no visual entry for kind %ls used by the snapshot; skipped %lld instance(s).",
			*Warning.SpeciesName,
			*StaticEnum<EArcForestVisualKind>()->GetNameStringByValue(static_cast<int64>(Warning.Kind)),
			Warning.SkippedInstanceCount);
	}
	for (const TPair<FGuid, FNearestVisualCount>& Pair : NearestVisualCounts)
	{
		const FNearestVisualCount& Count = Pair.Value;
		UE_LOGF(LogArcEcotopeInstanceBuilder, Warning,"Species %ls drew %lld instance(s) from its nearest band because no band covered their size; its bands do not span the sizes the snapshot produced.",
			*Count.SpeciesName, Count.InstanceCount);
	}
	for (const TPair<FString, FClusterRadiusWarning>& Pair : ClusterRadiusWarnings)
	{
		const FClusterRadiusWarning& Warning = Pair.Value;
		UE_LOGF(LogArcEcotopeInstanceBuilder, Warning,"Species %ls cover cluster layer %d asks for a %.0f cm radius, above the %.0f cm ceiling the site halo can carry, in %lld record(s); it is clamped to that ceiling, so its clumps are tighter and spaced closer than authored.",
			*Warning.SpeciesName, Warning.LayerIndex, Warning.AuthoredRadiusCm, Warning.MaxRadiusCm, Warning.RecordCount);
	}
	for (const TPair<FString, FClusterSiteCapWarning>& Pair : ClusterSiteCapWarnings)
	{
		const FClusterSiteCapWarning& Warning = Pair.Value;
		UE_LOGF(LogArcEcotopeInstanceBuilder, Warning,"Species %ls cover cluster layer %d places no clumps in %lld record(s): a %.2f cm clump radius on a %.0f cm record cell (record grid level %u) offers %lld candidate clump sites per record, above the %lld the builder evaluates. Its instances scatter uniformly inside their own record cell; raise the radius or lower the record grid level.",
			*Warning.SpeciesName, Warning.LayerIndex, Warning.RecordCount, Warning.ClusterRadiusCm,
			Warning.RecordCellSizeCm, static_cast<uint32>(Warning.RecordGridLevel), Warning.CandidateSiteCount,
			MaxCandidateClusterSites);
	}
	if (ChannelBufferSkippedCohortRecords > 0)
	{
		UE_LOGF(LogArcEcotopeInstanceBuilder, Warning,"ArcEcotope baked no stems for %lld cohort record(s) whose record cell lies wholly inside the channel buffer hard zone; they keep their biomass in the simulation but place nothing. A large count at default settings means TreeBufferHardDistanceCm is authored too wide.",
			ChannelBufferSkippedCohortRecords);
	}
	FCanonicalOutputDiagnostic CanonicalDiagnostic;
	FText CanonicalDiagnosticError;
	if (BuildCanonicalOutputDiagnostic(Groups, OutPrepared, CanonicalDiagnostic, CanonicalDiagnosticError))
	{
		UE_LOGF(LogArcEcotopeInstanceBuilder, Log,"ArcEcotope canonical output digest %016llX: tuples=%lld groups=%d actors=%d",
			CanonicalDiagnostic.Digest,
			CanonicalDiagnostic.TupleCount,
			CanonicalDiagnostic.GroupCount,
			CanonicalDiagnostic.ActorCount);
	}
	else
	{
		UE_LOGF(LogArcEcotopeInstanceBuilder, Warning,"ArcEcotope canonical output digest could not be computed: %ls",
			*CanonicalDiagnosticError.ToString());
	}
	OutError = FText::GetEmpty();
	return true;
}

void FArcEcotopeInstanceBuilder::CommitFailedBakeRecords(
	AArcEcotopeVolume& Volume,
	FArcEcotopePreparedInstances& Prepared)
{
	check(IsInGameThread());
	if (Prepared.FailedBake.IsEmpty())
	{
		return;
	}
	const FScopedTransaction Transaction(
		NSLOCTEXT("ArcEcotopeInstanceBuilder", "RecordFailedBakeOwnership", "Record Ecotope Bake Ownership"));
	Volume.Modify();
	if (Prepared.FailedBake.EntityActors.IsSet())
	{
		Volume.EmittedEntityActors = MoveTemp(Prepared.FailedBake.EntityActors.GetValue());
	}
	if (Prepared.FailedBake.InstanceActors.IsSet())
	{
		Volume.InstancePartitionActors = MoveTemp(Prepared.FailedBake.InstanceActors.GetValue());
	}
	Prepared.FailedBake = FArcEcotopeFailedBakeRecords();
}

void FArcEcotopeInstanceBuilder::CommitEmittedActorChanges(
	AArcEcotopeVolume& Volume,
	FArcEcotopePreparedInstances& Prepared)
{
	using namespace ArcEcotopeInstanceBuilder;
	check(IsInGameThread());
	Volume.Modify();
	Volume.EmittedEntityActors = MoveTemp(Prepared.EntityActorRecords);
	Volume.InstancePartitionActors = MoveTemp(Prepared.InstanceActorRecords);
	Prepared.EntityActorRecords.Reset();
	Prepared.InstanceActorRecords.Reset();

	TSet<const AArcPlacedEntityPartitionActor*> WrittenEntityActors;
	for (const FArcEcotopePreparedEntityWrite& Write : Prepared.EntityWrites)
	{
		WrittenEntityActors.Add(Write.Actor);
	}
	TArray<AArcPlacedEntityPartitionActor*> EmptiedEntityActors;
	for (AArcPlacedEntityPartitionActor* Actor : Prepared.PreviousEntityActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		// Before the mutation: RemoveEntriesOwnedBy calls Modify only after it has already removed them.
		Actor->Modify();
		if (Actor->RemoveEntriesOwnedBy(Volume.VolumeId) && !WrittenEntityActors.Contains(Actor))
		{
			EmptiedEntityActors.Add(Actor);
		}
	}
	Prepared.PreviousEntityActors.Reset();

	for (FArcEcotopePreparedEntityWrite& Write : Prepared.EntityWrites)
	{
		if (!IsValid(Write.Actor))
		{
			continue;
		}
		UArcMassEntityConfigAsset* Config = Cast<UArcMassEntityConfigAsset>(Write.Config.Get());
		Write.Actor->Modify();
		const int32 EntryIndex =
			Write.Actor->AddInstances(Config, Volume.VolumeId, Write.RelativeTransforms);
		ensureMsgf(
			EntryIndex != INDEX_NONE,
			TEXT("ArcEcotope could not append its prepared placed-entity instances of config '%s'."),
			*GetNameSafe(Config));
	}
	Prepared.EntityWrites.Reset();

	for (const TSoftObjectPtr<AActor>& SoftActor : Prepared.ActorsToDestroy)
	{
		DestroyEmittedActor(SoftActor.Get());
	}
	Prepared.ActorsToDestroy.Reset();
	for (AArcPlacedEntityPartitionActor* Actor : EmptiedEntityActors)
	{
		DestroyEmittedActor(Actor);
	}
}

bool FArcEcotopeInstanceBuilder::ClearEmittedActors(
	AArcEcotopeVolume& Volume,
	const bool bRequiresRebuild,
	FText& OutError)
{
	using namespace ArcEcotopeInstanceBuilder;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_InstanceBuilderClearEmittedActors);

	OutError = FText::GetEmpty();

	UWorld* World = Volume.GetWorld();
	UWorldPartition* WorldPartition =
		World != nullptr && World->IsPartitionedWorld() ? World->GetWorldPartition() : nullptr;

	// Keeps a streamed-out actor loaded for the whole mutation; released once the last destroy has run.
	TArray<FWorldPartitionReference> Pins;
	TArray<FString> Failures;

	FDiscoveredPartitionActors Discovered;
	DiscoverOwnedPartitionActors(Volume, WorldPartition, Discovered);

	TArray<AActor*> InstanceActors;
	TArray<FArcEcotopeEmittedActor> KeptInstanceRecords;
	for (const FArcEcotopeEmittedActor& Record : Volume.InstancePartitionActors)
	{
		AArcEcotopeInstancePartitionActor* Actor =
			Cast<AArcEcotopeInstancePartitionActor>(ResolveEmittedActor(Record, WorldPartition, Pins));
		if (Actor == nullptr)
		{
			if (HasActorDescriptor(WorldPartition, Record.ActorGuid))
			{
				Failures.Add(FString::Printf(
					TEXT("Instance partition actor '%s' still has a level descriptor but will not load; it is left intact and its record is kept."),
					*Record.Actor.ToString()));
				KeptInstanceRecords.Add(Record);
				continue;
			}
			UE_LOGF(
				LogArcEcotopeInstanceBuilder,
				Warning,"ArcEcotope dropped the record of instance partition actor '%ls', which no longer has a level descriptor.",
				*Record.Actor.ToString());
			continue;
		}
		if (Actor->OwnerGuid != Volume.VolumeId)
		{
			Failures.Add(FString::Printf(
				TEXT("Instance partition actor '%s' is recorded by this volume but names owner %s; it is left intact."),
				*Actor->GetName(),
				*Actor->OwnerGuid.ToString()));
			KeptInstanceRecords.Add(Record);
			continue;
		}
		InstanceActors.AddUnique(static_cast<AActor*>(Actor));
	}
	for (const TPair<FPartitionCellKey, AArcEcotopeInstancePartitionActor*>& Pair : Discovered.InstanceActors)
	{
		if (Pair.Value != nullptr)
		{
			InstanceActors.AddUnique(static_cast<AActor*>(Pair.Value));
		}
	}

	TArray<AArcPlacedEntityPartitionActor*> EntityActors;
	TArray<FArcEcotopeEmittedActor> KeptEntityRecords;
	for (const FArcEcotopeEmittedActor& Record : Volume.EmittedEntityActors)
	{
		AArcPlacedEntityPartitionActor* Actor =
			Cast<AArcPlacedEntityPartitionActor>(ResolveEmittedActor(Record, WorldPartition, Pins));
		if (Actor == nullptr)
		{
			if (HasActorDescriptor(WorldPartition, Record.ActorGuid))
			{
				Failures.Add(FString::Printf(
					TEXT("Placed-entity partition actor '%s' still has a level descriptor but will not load; its entries are left intact and its record is kept."),
					*Record.Actor.ToString()));
				KeptEntityRecords.Add(Record);
				continue;
			}
			UE_LOGF(
				LogArcEcotopeInstanceBuilder,
				Warning,"ArcEcotope dropped the record of placed-entity partition actor '%ls', which no longer has a level descriptor.",
				*Record.Actor.ToString());
			continue;
		}
		EntityActors.AddUnique(Actor);
	}
	for (const TPair<FPartitionCellKey, AArcPlacedEntityPartitionActor*>& Pair : Discovered.EntityActors)
	{
		if (Pair.Value != nullptr)
		{
			EntityActors.AddUnique(Pair.Value);
		}
	}

	Volume.Modify();
	Volume.InstancePartitionActors = MoveTemp(KeptInstanceRecords);
	Volume.EmittedEntityActors = MoveTemp(KeptEntityRecords);
	Volume.ClearInstanceRepresentation(bRequiresRebuild);

	if (UArcEcotopeTerrainWeightStamp* WeightStamp = Volume.TerrainWeightStamp)
	{
		WeightStamp->ClearBakedLayers();
	}

	for (AActor* Actor : InstanceActors)
	{
		DestroyEmittedActor(Actor);
	}
	for (AArcPlacedEntityPartitionActor* Actor : EntityActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		// Shared with hand placement and other generators, so only this volume's entries go.
		Actor->Modify();
		if (Actor->RemoveEntriesOwnedBy(Volume.VolumeId))
		{
			DestroyEmittedActor(Actor);
		}
	}
	Discovered.Pins.Reset();
	Pins.Reset();

	if (Failures.IsEmpty())
	{
		return true;
	}
	OutError = FText::FromString(FString::Join(Failures, TEXT("\n")));
	UE_LOGF(LogArcEcotopeInstanceBuilder, Error,"%ls", *OutError.ToString());
	return false;
}

bool FArcEcotopeInstanceBuilder::AdoptPreparedInstances(
	AArcEcotopeVolume& Volume,
	const FArcForestWorldSnapshot& Snapshot,
	FArcEcotopePreparedInstances& Prepared,
	FText& OutError)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_InstanceBuilderAdopt);

	ArcEcotopeInstanceBuilderWeightPaint::FWeightPaint WeightPaint;
	if (!ArcEcotopeInstanceBuilderWeightPaint::ComputeWeightPaint(
			Volume, Prepared, WeightPaint, OutError))
	{
		Discard(Prepared);
		return false;
	}

	for (FArcEcotopePreparedInstanceActor& ActorSet : Prepared.InstanceActors)
	{
		if (!IsValid(ActorSet.Actor))
		{
			OutError = FText::FromString(TEXT("ArcEcotope prepared instance set names a partition actor that no longer exists."));
			Discard(Prepared);
			return false;
		}
		ActorSet.Actor->Modify();
		if (!ActorSet.Actor->AdoptPreparedInstanceComponents(
			MoveTemp(ActorSet.StaticComponents),
			MoveTemp(ActorSet.StaticFinalNames),
			MoveTemp(ActorSet.SkinnedComponents),
			MoveTemp(ActorSet.SkinnedFinalNames),
			OutError))
		{
			Discard(Prepared);
			return false;
		}
	}
	Prepared.InstanceActors.Reset();
	const bool bCommitted = Volume.CommitInstanceRepresentation(Prepared.SnapshotHash, Prepared.VisualHash, OutError);
	// Released only here: dropping a reference unregisters the actor it names, and every write to a pinned
	// actor - the destroy, the adoption above - has to happen while it is still loaded.
	Prepared.PinnedActors.Reset();
	if (bCommitted)
	{
		ArcEcotopeInstanceBuilderWeightPaint::ApplyWeightPaint(WeightPaint);
	}
	return bCommitted;
}

void FArcEcotopeInstanceBuilder::Discard(FArcEcotopePreparedInstances& Prepared)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_InstanceBuilderDiscard);
	int32 DiscardedComponentCount = 0;
	const double DiscardStartSeconds = FPlatformTime::Seconds();
	for (FArcEcotopePreparedInstanceActor& ActorSet : Prepared.InstanceActors)
	{
		DiscardedComponentCount += ActorSet.StaticComponents.Num() + ActorSet.SkinnedComponents.Num();
		for (UInstancedStaticMeshComponent* Component : ActorSet.StaticComponents)
		{
			if (IsValid(Component))
			{
				const FName DiscardName = MakeUniqueObjectName(Component->GetOuter(), Component->GetClass(), FName(TEXT("ArcEcotopeDiscardedISM")));
				Component->Rename(*DiscardName.ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				Component->DestroyComponent();
			}
		}
		for (UInstancedSkinnedMeshComponent* Component : ActorSet.SkinnedComponents)
		{
			if (IsValid(Component))
			{
				const FName DiscardName = MakeUniqueObjectName(Component->GetOuter(), Component->GetClass(), FName(TEXT("ArcEcotopeDiscardedISKM")));
				Component->Rename(*DiscardName.ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
				Component->DestroyComponent();
			}
		}
	}
	Prepared.InstanceActors.Reset();
	Prepared.ActorsToDestroy.Reset();
	Prepared.EntityActorRecords.Reset();
	Prepared.InstanceActorRecords.Reset();
	Prepared.PreviousEntityActors.Reset();
	Prepared.EntityWrites.Reset();
	Prepared.PinnedActors.Reset();
	Prepared.WeightFootprints.Reset();
	Prepared.SnapshotHash = 0;
	Prepared.VisualHash = 0;
	if (DiscardedComponentCount > 0)
	{
		UE_LOGF(
			LogArcEcotope,
			Log,"InstanceBuilder::Discard destroyed %d prepared component(s) in %.2f ms",
			DiscardedComponentCount,
			UE::ArcForest::GetElapsedMilliseconds(DiscardStartSeconds));
	}
}
