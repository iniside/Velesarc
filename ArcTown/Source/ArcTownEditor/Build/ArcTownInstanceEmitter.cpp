// Copyright Lukasz Baran. All Rights Reserved.

#include "Build/ArcTownInstanceEmitter.h"

#include "ActorPartition/ActorPartitionSubsystem.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcTerrainActor.h"
#include "Build/ArcTownGridKeys.h"
#include "Catalog/ArcTownBuildingCatalog.h"
#include "Catalog/ArcTownBuildingDefinition.h"
#include "Catalog/ArcTownBuildingVisual.h"
#include "Catalog/ArcTownRoadCatalog.h"
#include "Catalog/ArcTownRoadDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Hash/CityHash.h"
#include "Instances/ArcTownPartitionActor.h"
#include "Layout/ArcTownLayout.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ArchiveMD5.h"
#include "Paths/ArcTownPathComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownSimulationState.h"
#include "Simulation/ArcTownTensorField.h"
#include "Stamps/ArcTerrainSplineModifier.h"
#include "StaticMeshResources.h"
#include "Terrain/ArcTownPadStamp.h"
#include "Terrain/ArcTownStampKeys.h"
#include "Town/ArcTownSiteSnapshot.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownInstanceEmitter, Log, All);

namespace ArcTownInstanceEmitterPrivate
{
	/**
	 * Our own sentinel: UActorPartitionSubsystem::FCellCoord has no Invalid member. The narrowing to
	 * int32 is deliberate — FCellCoord::X/Y/Z are int64, and int32 is ample at a 256 m grid.
	 */
	const FIntVector UnassignedCell(MIN_int32, MIN_int32, MIN_int32);

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	/** One catalog entry resolved into everything placement needs, with its visual asset loaded. */
	struct FCandidate
	{
		UStaticMesh* Mesh = nullptr;
		UClass* ActorClass = nullptr;
		UArcMassEntityConfigAsset* EntityConfig = nullptr;
		FVector2D HalfFootprint = FVector2D::ZeroVector;
		double FloorHeightAbovePivot = 0.0;
	};

	struct FPlacedBuilding
	{
		int32 CandidateIndex = INDEX_NONE;
		FTransform LocalXform;
	};

	/**
	 * Catalog entries whose mesh loaded, plus the redirection every consumer needs: a simulated
	 * building names an index into Catalog.Buildings, while FPlacedBuilding and BuildPadSpecs index
	 * this filtered array, and one skipped entry shifts every index after it.
	 */
	bool ResolveCandidates(
		const UArcTownBuildingCatalog& Catalog,
		TArray<FCandidate>& OutCandidates,
		TArray<int32>& OutEntryToCandidate,
		FText& OutError)
	{
		OutCandidates.Reset();
		OutEntryToCandidate.Init(INDEX_NONE, Catalog.Buildings.Num());
		for (int32 EntryIndex = 0; EntryIndex < Catalog.Buildings.Num(); ++EntryIndex)
		{
			const UArcTownBuildingDefinition* const Building = Catalog.Buildings[EntryIndex];
			if (Building == nullptr)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d is empty; the entry is skipped.",
					*Catalog.GetName(),
					EntryIndex);
				continue;
			}

			const UArcTownBuildingDefinition& Entry = *Building;
			const FArcTownBuildingVisual* const Visual = Entry.Visual.GetPtr();
			if (Visual == nullptr)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d names no visual; the entry is skipped.",
					*Catalog.GetName(),
					EntryIndex);
				continue;
			}

			const bool bHasAuthoredFootprint =
				FMath::IsFinite(Entry.FootprintXY.X) && FMath::IsFinite(Entry.FootprintXY.Y)
				&& Entry.FootprintXY.X > 0.0 && Entry.FootprintXY.Y > 0.0;

			FCandidate Candidate;
			Candidate.FloorHeightAbovePivot = Entry.FloorHeightAbovePivot;

			if (const FArcTownBuildingVisual_StaticMesh* const MeshVisual =
				Entry.Visual.GetPtr<FArcTownBuildingVisual_StaticMesh>())
			{
				UStaticMesh* const Mesh = MeshVisual->Mesh.LoadSynchronous();
				if (Mesh == nullptr)
				{
					UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d names mesh '%ls', which will not load; the entry is skipped.",
						*Catalog.GetName(),
						EntryIndex,
						*MeshVisual->Mesh.ToString());
					continue;
				}

				Candidate.Mesh = Mesh;
				if (bHasAuthoredFootprint)
				{
					Candidate.HalfFootprint = Entry.FootprintXY * 0.5;
				}
				else
				{
					const FBox Bounds = Mesh->GetBoundingBox();
					Candidate.HalfFootprint = FVector2D(
						FMath::Max(Bounds.GetExtent().X, 1.0),
						FMath::Max(Bounds.GetExtent().Y, 1.0));
					UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d has no usable FootprintXY; mesh bounds are used instead, which include eaves and porches.",
						*Catalog.GetName(),
						EntryIndex);
				}
			}
			else if (const FArcTownBuildingVisual_Actor* const ActorVisual =
				Entry.Visual.GetPtr<FArcTownBuildingVisual_Actor>())
			{
				UClass* const ActorClass = ActorVisual->ActorClass.LoadSynchronous();
				if (ActorClass == nullptr)
				{
					UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d names actor class '%ls', which will not load; the entry is skipped.",
						*Catalog.GetName(),
						EntryIndex,
						*ActorVisual->ActorClass.ToString());
					continue;
				}
				if (!bHasAuthoredFootprint)
				{
					UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d has no usable FootprintXY and an actor class offers no bounds to fall back on; the entry is skipped.",
						*Catalog.GetName(),
						EntryIndex);
					continue;
				}

				Candidate.ActorClass = ActorClass;
				Candidate.HalfFootprint = Entry.FootprintXY * 0.5;
			}
			else if (const FArcTownBuildingVisual_MassEntity* const EntityVisual =
				Entry.Visual.GetPtr<FArcTownBuildingVisual_MassEntity>())
			{
				UArcMassEntityConfigAsset* const EntityConfig =
					Cast<UArcMassEntityConfigAsset>(EntityVisual->EntityConfig.TryLoad());
				if (EntityConfig == nullptr)
				{
					UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d names entity config '%ls', which will not load or is not a UArcMassEntityConfigAsset; the entry is skipped.",
						*Catalog.GetName(),
						EntryIndex,
						*EntityVisual->EntityConfig.ToString());
					continue;
				}
				if (!bHasAuthoredFootprint)
				{
					UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d has no usable FootprintXY and an entity config offers no bounds to fall back on; the entry is skipped.",
						*Catalog.GetName(),
						EntryIndex);
					continue;
				}

				Candidate.EntityConfig = EntityConfig;
				Candidate.HalfFootprint = Entry.FootprintXY * 0.5;
			}
			else
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"Catalog '%ls' entry %d names visual '%ls', which this build cannot emit; the entry is skipped.",
					*Catalog.GetName(),
					EntryIndex,
					*GetNameSafe(Entry.Visual.GetScriptStruct()));
				continue;
			}

			OutEntryToCandidate[EntryIndex] = OutCandidates.Add(Candidate);
		}

		if (OutCandidates.IsEmpty())
		{
			return SetError(OutError, FString::Printf(
				TEXT("ArcTown catalog '%s' offers no building with a loadable visual."),
				*Catalog.GetName()));
		}
		return true;
	}

	bool ConvertPlacedBuildings(
		const FArcTownSiteSnapshot& Site,
		const FTransform& TownXform,
		const UArcTownBuildingCatalog& Catalog,
		TConstArrayView<FCandidate> Candidates,
		TConstArrayView<int32> EntryToCandidate,
		TConstArrayView<FArcTownPlacedBuilding> LayoutBuildings,
		TArray<FPlacedBuilding>& OutPlaced,
		FText& OutError)
	{
		OutPlaced.Reset(LayoutBuildings.Num());
		for (int32 BuildingIndex = 0; BuildingIndex < LayoutBuildings.Num(); ++BuildingIndex)
		{
			const FArcTownPlacedBuilding& Source = LayoutBuildings[BuildingIndex];
			if (!EntryToCandidate.IsValidIndex(Source.CatalogIndex))
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown building %d names entry %d of catalog '%s', which holds %d entr(ies). Reset Simulation and simulate again."),
					BuildingIndex,
					Source.CatalogIndex,
					*Catalog.GetName(),
					Catalog.Buildings.Num()));
			}

			const int32 CandidateIndex = EntryToCandidate[Source.CatalogIndex];
			if (CandidateIndex == INDEX_NONE)
			{
				const UArcTownBuildingDefinition* const Building = Catalog.Buildings[Source.CatalogIndex];
				if (Building == nullptr)
				{
					return SetError(OutError, FString::Printf(
						TEXT("ArcTown building %d stands on entry %d of catalog '%s', which is empty. Fill the entry, then rebuild."),
						BuildingIndex,
						Source.CatalogIndex,
						*Catalog.GetName()));
				}

				const FArcTownBuildingVisual* const Visual = Building->Visual.GetPtr();
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown building %d stands on entry %d of catalog '%s', whose visual '%s' will not load or has no usable footprint. Fix the entry, then rebuild."),
					BuildingIndex,
					Source.CatalogIndex,
					*Catalog.GetName(),
					Visual != nullptr ? *Visual->GetAssetPath().ToString() : TEXT("<none>")));
			}

			const FVector TownLocal = Source.LocalXform.GetLocation();
			const FVector2D TerrainLocalXY =
				ArcTownSiteFrame::TownLocalToTerrainLocalXY(Site, FVector2D(TownLocal.X, TownLocal.Y));
			const FVector OnTerrainPlane =
				Site.TerrainToWorld.TransformPosition(FVector(TerrainLocalXY.X, TerrainLocalXY.Y, 0.0));
			const FVector2D WorldXY(OnTerrainPlane.X, OnTerrainPlane.Y);

			double GroundZ = 0.0;
			if (!ArcTownSiteSampling::SampleGroundZ(Site, WorldXY, GroundZ))
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown read a non-finite sampled height at world (%.1f, %.1f). Sample Site again."),
					WorldXY.X,
					WorldXY.Y));
			}

			const FTransform WorldXform(
				Site.TerrainToWorld.GetRotation() * Source.LocalXform.GetRotation(),
				FVector(WorldXY.X, WorldXY.Y, GroundZ - Candidates[CandidateIndex].FloorHeightAbovePivot),
				FVector::OneVector);

			FPlacedBuilding& Placed = OutPlaced.AddDefaulted_GetRef();
			Placed.CandidateIndex = CandidateIndex;
			Placed.LocalXform = WorldXform.GetRelativeTransform(TownXform);
		}
		return true;
	}

	AArcTerrain* ResolvePadTerrain(UWorld& World, const FGuid& TerrainGuid)
	{
		if (!TerrainGuid.IsValid())
		{
			return nullptr;
		}
		for (TActorIterator<AArcTerrain> TerrainIt(&World); TerrainIt; ++TerrainIt)
		{
			AArcTerrain* const Terrain = *TerrainIt;
			if (IsValid(Terrain) && Terrain->GetActorGuid() == TerrainGuid)
			{
				return Terrain;
			}
		}
		return nullptr;
	}

	/**
	 * One pad per placed building. The edge mask reaches full strength one falloff band inside the
	 * footprint rectangle, so the pad is grown by that band on every side to leave the walls standing on
	 * flat ground. The pad is expressed in WORLD space, matching where the building was placed rather
	 * than where the terrain currently is: a terrain moved since the site was sampled would otherwise
	 * pull the floors away from the buildings resting on them.
	 */
	void BuildPadSpecs(
		const AArcTownVolume& Town,
		const FTransform& TownXform,
		TConstArrayView<FCandidate> Candidates,
		TConstArrayView<FPlacedBuilding> Placed,
		TArray<FArcTownPadSpec>& OutSpecs)
	{
		const double Band = FMath::Max(Town.PadFalloffBand, 0.0);
		OutSpecs.Reset(Placed.Num());
		for (int32 BuildingIndex = 0; BuildingIndex < Placed.Num(); ++BuildingIndex)
		{
			const FCandidate& Candidate = Candidates[Placed[BuildingIndex].CandidateIndex];
			const FTransform WorldXform = Placed[BuildingIndex].LocalXform * TownXform;
			const FVector2D FootprintXY = Candidate.HalfFootprint * 2.0;

			FArcTownPadSpec& Spec = OutSpecs.AddDefaulted_GetRef();
			Spec.WorldRotation = WorldXform.GetRotation();
			// The layout puts the pivot FloorHeightAbovePivot below the sampled ground, so the floor
			// plane the pad conforms to is that far back above the pivot.
			Spec.WorldLocation =
				WorldXform.GetLocation() + FVector(0.0, 0.0, Candidate.FloorHeightAbovePivot);
			Spec.CoverageXY = FootprintXY + FVector2D(2.0 * Band, 2.0 * Band);
			Spec.ContentId = ArcTownStampKeys::MakePadContentId(
				Town.TownId, BuildingIndex, FootprintXY, Candidate.FloorHeightAbovePivot);
		}
	}

	/** Path components in a stable order; component order is not stable between sessions. */
	void GatherSortedPaths(const AArcTownVolume& Town, TArray<UArcTownPathComponent*>& OutPaths)
	{
		Town.GetComponents<UArcTownPathComponent>(OutPaths);
		OutPaths.RemoveAll([](const UArcTownPathComponent* Path) -> bool { return !IsValid(Path); });
		OutPaths.Sort([](const UArcTownPathComponent& A, const UArcTownPathComponent& B) -> bool
		{
			return A.GetName().Compare(B.GetName(), ESearchCase::CaseSensitive) < 0;
		});
	}

	void BuildRoadModifierSpec(const AArcTownVolume& Town, FArcTownRoadModifierSpec& OutSpec)
	{
		OutSpec = FArcTownRoadModifierSpec();

		TArray<UArcTownPathComponent*> Paths;
		GatherSortedPaths(Town, Paths);
		if (Paths.IsEmpty())
		{
			return;
		}

		OutSpec.Paths.Reserve(Paths.Num());
		for (UArcTownPathComponent* Path : Paths)
		{
			OutSpec.Paths.Add(Path);
		}

		const FVector ScaledExtent = Town.VolumeBounds->GetScaledBoxExtent();
		OutSpec.WorldLocation = Town.VolumeBounds->GetComponentLocation();
		OutSpec.WorldRotation = Town.VolumeBounds->GetComponentQuat();
		OutSpec.CoverageXY = FVector2D(ScaledExtent.X * 2.0, ScaledExtent.Y * 2.0);
	}

	/**
	 * Drops the town's stamps of one kind. The rename is what frees the deterministic name this
	 * rebuild is about to reuse: DestroyComponent only marks the object garbage, and a name still held
	 * would make StaticAllocateObject overwrite the old object in place.
	 */
	template <typename StampType, typename NamePredicate>
	void DestroyExistingStamps(AArcTownVolume& Town, NamePredicate NameMatches, const TCHAR* DiscardBaseName)
	{
		TArray<StampType*> Existing;
		Town.GetComponents<StampType>(Existing);
		// Destroying walks the snapshot taken by the gather above, never the owned-component set itself.
		for (StampType* Stamp : Existing)
		{
			if (!IsValid(Stamp) || !NameMatches(Stamp->GetName()))
			{
				continue;
			}
			const FName DiscardName =
				MakeUniqueObjectName(Stamp->GetOuter(), Stamp->GetClass(), FName(DiscardBaseName));
			Stamp->Rename(
				*DiscardName.ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			Stamp->DestroyComponent();
		}
	}

	bool MatchesGeneratedPadName(const FString& Name)
	{
		return Name.StartsWith(ArcTownPadComponentPrefix, ESearchCase::CaseSensitive);
	}

	bool MatchesGeneratedRoadName(const FString& Name)
	{
		return Name.Compare(ArcTownRoadComponentName, ESearchCase::CaseSensitive) == 0;
	}

	/**
	 * Replaces the town's pads with the prepared set. Every OnRegister notifies the terrain subsystem,
	 * which batches the invalidation of intersecting footprints, so the registrations run in one block.
	 */
	void EmitPadStamps(AArcTownVolume& Town, const FArcTownPreparedInstances& Prepared)
	{
		DestroyExistingStamps<UArcTownPadStamp>(Town, &MatchesGeneratedPadName, TEXT("ArcTownDiscardedPad"));

		USceneComponent* const TownRoot = Town.GetRootComponent();
		const float FalloffBand = static_cast<float>(FMath::Max(Town.PadFalloffBand, 0.0));
		const FTransform TownXform = Town.GetActorTransform();
		for (int32 PadIndex = 0; PadIndex < Prepared.PadSpecs.Num(); ++PadIndex)
		{
			const FArcTownPadSpec& Spec = Prepared.PadSpecs[PadIndex];
			const FName PadName(*FString::Printf(TEXT("%s%04d"), ArcTownPadComponentPrefix, PadIndex));
			UArcTownPadStamp* const Pad = NewObject<UArcTownPadStamp>(&Town, PadName, RF_NoFlags);
			Pad->ClearFlags(RF_Transactional);
			Pad->CreationMethod = EComponentCreationMethod::Instance;
			Pad->AffectedTerrain = Prepared.PadTerrain;
			Pad->Priority = Town.PadStampPriority;
			Pad->PadContentId = Spec.ContentId;
			Pad->CoverageXY = Spec.CoverageXY;
			Pad->EdgeFalloffBand = FalloffBand;
			// The stamp reads its own transform relative to the terrain, so without an absolute scale
			// stretching the volume would multiply every pad's footprint, falloff band and height scale.
			Pad->SetUsingAbsoluteScale(true);
			Pad->SetupAttachment(TownRoot);

			// Placed by world pose: the composite biases the pad by its TERRAIN-LOCAL Z, which this
			// yields exactly, while a Z written straight into the component would be a world height sent
			// into a local frame and land the pad wrong under any terrain with a transform of its own.
			const FTransform PadWorld(Spec.WorldRotation, Spec.WorldLocation, FVector::OneVector);
			const FTransform PadRelative = PadWorld.GetRelativeTransform(TownXform);
			Pad->SetRelativeLocationAndRotation(PadRelative.GetLocation(), PadRelative.GetRotation());

			Town.AddInstanceComponent(Pad);
			Pad->RegisterComponent();
		}

		UE_LOGF(LogArcTownInstanceEmitter, Log,"ArcTown '%ls' emitted %d pad stamp(s).",
			*Town.GetName(), Prepared.PadSpecs.Num());
	}

	int32 EmitRoadModifier(AArcTownVolume& Town, const FArcTownPreparedInstances& Prepared)
	{
		DestroyExistingStamps<UArcTerrainSplineModifier>(
			Town, &MatchesGeneratedRoadName, TEXT("ArcTownDiscardedRoads"));

		const FArcTownRoadModifierSpec& Spec = Prepared.RoadSpec;
		if (Spec.Paths.IsEmpty() || Spec.CoverageXY.X <= 0.0 || Spec.CoverageXY.Y <= 0.0)
		{
			return 0;
		}

		UArcTerrainSplineModifier* const Roads =
			NewObject<UArcTerrainSplineModifier>(&Town, FName(ArcTownRoadComponentName), RF_NoFlags);
		Roads->ClearFlags(RF_Transactional);
		Roads->CreationMethod = EComponentCreationMethod::Instance;
		Roads->AffectedTerrain = Prepared.PadTerrain;
		Roads->Priority = Town.PathStampPriority;
		Roads->BlendMode = EArcTerrainBlendMode::AlphaBlend;
		Roads->CoverageXY = Spec.CoverageXY;
		Roads->SetUsingAbsoluteScale(true);
		Roads->SetupAttachment(Town.GetRootComponent());

		const UArcTownRoadCatalog* const Catalog = Town.RoadCatalog;
		const float FalloffCm = static_cast<float>(FMath::Max(Town.PathFalloffCm, 0.0));
		Roads->Splines.Reserve(Spec.Paths.Num());
		for (const TObjectPtr<UArcTownPathComponent>& Path : Spec.Paths)
		{
			const UArcTownPathComponent* const PathComponent = Path.Get();
			if (!IsValid(PathComponent))
			{
				continue;
			}
			FArcTerrainSplineEntry& Entry = Roads->Splines.AddDefaulted_GetRef();
			Entry.Spline.PathToComponent = PathComponent->GetName();
			Entry.HalfWidthCm = PathComponent->DefaultHalfWidth;
			Entry.FalloffCm = FalloffCm;
			const UArcTownPathMetadata* const Metadata = PathComponent->GetPathMetadata();
			if (Metadata != nullptr)
			{
				Entry.HalfWidthCurve = Metadata->HalfWidth;
			}

			const UArcTownRoadDefinition* const Definition =
				(Catalog != nullptr && Catalog->ClassesByHierarchy.IsValidIndex(PathComponent->Hierarchy))
					? Catalog->ClassesByHierarchy[PathComponent->Hierarchy].Get()
					: nullptr;
			if (Definition != nullptr && !Definition->SurfaceChannel.IsNone())
			{
				Entry.SurfaceChannel = Definition->SurfaceChannel;
				continue;
			}

			FString Reason;
			if (Catalog == nullptr)
			{
				Reason = TEXT("the town names no road catalog");
			}
			else if (!Catalog->ClassesByHierarchy.IsValidIndex(PathComponent->Hierarchy))
			{
				Reason = FString::Printf(
					TEXT("road class %d is past the end of the catalog's %d class(es)"),
					static_cast<int32>(PathComponent->Hierarchy),
					Catalog->ClassesByHierarchy.Num());
			}
			else if (Definition == nullptr)
			{
				Reason = FString::Printf(
					TEXT("catalog road class %d holds no definition"),
					static_cast<int32>(PathComponent->Hierarchy));
			}
			else
			{
				Reason = FString::Printf(
					TEXT("road definition '%s' names no surface channel"),
					*Definition->GetName());
			}

			UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown '%ls' path '%ls' paints no terrain channel because %ls; the spline still deforms the terrain, but no surface layer is written.",
				*Town.GetName(), *PathComponent->GetName(), *Reason);
		}

		const FTransform RoadsWorld(Spec.WorldRotation, Spec.WorldLocation, FVector::OneVector);
		const FTransform RoadsRelative = RoadsWorld.GetRelativeTransform(Town.GetActorTransform());
		Roads->SetRelativeLocationAndRotation(RoadsRelative.GetLocation(), RoadsRelative.GetRotation());

		Town.AddInstanceComponent(Roads);
		Roads->RegisterComponent();

		UE_LOGF(LogArcTownInstanceEmitter, Log,"ArcTown '%ls' emitted one road field carrying %d spline(s).",
			*Town.GetName(), Roads->Splines.Num());
		return Roads->Splines.Num();
	}

	/** Identity of one instanced component: the mesh it draws and the partition cell it lands in. */
	struct FGroupKey
	{
		FIntVector CellCoord = UnassignedCell;
		UStaticMesh* Mesh = nullptr;

		bool operator==(const FGroupKey& Other) const
		{
			return CellCoord == Other.CellCoord && Mesh == Other.Mesh;
		}

		friend uint32 GetTypeHash(const FGroupKey& Key)
		{
			return HashCombine(GetTypeHash(Key.CellCoord), PointerHash(Key.Mesh));
		}
	};

	struct FGroup
	{
		FGroupKey Key;
		TArray<FTransform> WorldXforms;
	};

	FName MakeComponentName(const FGroupKey& Key)
	{
		const FString Source = FString::Printf(
			TEXT("%d,%d,%d|%s"), Key.CellCoord.X, Key.CellCoord.Y, Key.CellCoord.Z, *Key.Mesh->GetPathName());
		const uint64 NameHash = CityHash64(
			reinterpret_cast<const char*>(*Source), static_cast<uint32>(Source.Len() * sizeof(TCHAR)));
		return FName(*FString::Printf(TEXT("%s%016llX"), ArcTownGeneratedISMPrefix, NameHash));
	}

	AArcTownPartitionActor* ResolvePartitionActor(
		AArcTownVolume& Town,
		UActorPartitionSubsystem& PartitionSubsystem,
		FArcTownPreparedInstances& OutPrepared,
		const UActorPartitionSubsystem::FCellCoord& CellCoord,
		const uint32 GridSize,
		const FGuid& InGridGuid,
		const FName RuntimeGridName,
		const bool bPartitioned)
	{
		AArcTownVolume* const TownPtr = &Town;
		FArcTownPreparedInstances* const PreparedPtr = &OutPrepared;
		const FGuid GridGuid = InGridGuid;
		const FName GridName = RuntimeGridName;
		const bool bIsPartitioned = bPartitioned;
		// GetActor builds the actor name from the guid it is given, before the actor exists, and reads the
		// desc's copy back out of SetGridGuid; one parameter feeds both, because a divergence re-spawns
		// under a colliding name with ESpawnActorNameMode::Required_Fatal.
		return Cast<AArcTownPartitionActor>(PartitionSubsystem.GetActor(
			AArcTownPartitionActor::StaticClass(),
			CellCoord,
			/*bInCreate=*/true,
			GridGuid,
			GridSize,
			/*bInBoundsSearch=*/true,
			/*InActorCreated=*/[GridGuid, TownPtr, PreparedPtr, GridName, bIsPartitioned](APartitionActor* NewActor)
			{
				AArcTownPartitionActor* const Created = CastChecked<AArcTownPartitionActor>(NewActor);
				Created->SetGridGuid(GridGuid);
				Created->OwnerGuid = TownPtr->TownId;
				Created->OwningTown = TownPtr;
				if (bIsPartitioned && !GridName.IsNone())
				{
					Created->SetRuntimeGrid(GridName);
				}
				PreparedPtr->CreatedActors.Add(Created);
			}));
	}

	/** One partition cell's building-actor spawns, in the order the layout placed them. */
	struct FCellBuildingActors
	{
		FIntVector CellCoord = UnassignedCell;
		TArray<FArcTownPreparedBuildingActor> Spawns;
	};

	/**
	 * Every resolved partition actor lands in OutPrepared.Actors, mesh-bearing or not: the ownership
	 * ledger is written from that array, and a cell missing from it is never emptied again.
	 */
	bool ResolveCellActorSet(
		AArcTownVolume& Town,
		UWorld& World,
		UActorPartitionSubsystem& PartitionSubsystem,
		FArcTownPreparedInstances& OutPrepared,
		const FIntVector& CellCoord,
		const uint32 GridSize,
		const FGuid& CellGridGuid,
		const bool bPartitioned,
		TMap<FIntVector, AArcTownPartitionActor*>& InOutActorsByCell,
		TMap<const AArcTownPartitionActor*, int32>& InOutActorSetIndices,
		int32& OutSetIndex,
		FText& OutError)
	{
		AArcTownPartitionActor** ExistingActor = InOutActorsByCell.Find(CellCoord);
		if (ExistingActor == nullptr)
		{
			const UActorPartitionSubsystem::FCellCoord PartitionCell(
				CellCoord.X, CellCoord.Y, CellCoord.Z, World.PersistentLevel);
			AArcTownPartitionActor* const Resolved = ResolvePartitionActor(
				Town, PartitionSubsystem, OutPrepared, PartitionCell, GridSize, CellGridGuid,
				Town.GetRuntimeGrid(), bPartitioned);
			if (Resolved == nullptr)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown could not resolve its partition actor in cell (%d, %d, %d); one exists there but will not ")
					TEXT("load, and spawning a second would collide with its name."),
					CellCoord.X,
					CellCoord.Y,
					CellCoord.Z));
			}
			if (Resolved->OwnerGuid != Town.TownId)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown resolved partition actor '%s' in cell (%d, %d, %d), which another town owns."),
					*Resolved->GetName(),
					CellCoord.X,
					CellCoord.Y,
					CellCoord.Z));
			}
			ExistingActor = &InOutActorsByCell.Add(CellCoord, Resolved);
		}
		AArcTownPartitionActor* const PartitionActor = *ExistingActor;

		int32* SetIndex = InOutActorSetIndices.Find(PartitionActor);
		if (SetIndex == nullptr)
		{
			SetIndex = &InOutActorSetIndices.Add(PartitionActor, OutPrepared.Actors.Num());
			OutPrepared.Actors.AddDefaulted_GetRef().Actor = PartitionActor;
		}
		OutSetIndex = *SetIndex;
		return true;
	}

	struct FRoadMeshCandidate
	{
		UStaticMesh* Mesh = nullptr;
		TArray<TObjectPtr<UMaterialInterface>> Materials;
		TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;
		float Weight = 0.0f;
	};

	int32 GetRoadWidthAxisIndex(const ESplineMeshAxis::Type ForwardAxis)
	{
		switch (ForwardAxis)
		{
		case ESplineMeshAxis::X: return 1;
		case ESplineMeshAxis::Y: return 2;
		default: return 0;
		}
	}

	int32 GetRoadForwardAxisIndex(const ESplineMeshAxis::Type ForwardAxis)
	{
		switch (ForwardAxis)
		{
		case ESplineMeshAxis::X: return 0;
		case ESplineMeshAxis::Y: return 1;
		default: return 2;
		}
	}

	// A road mesh has to be measured on every axis the way the spline mesh proxy fits it: with
	// r.SplineMesh.FitToSourceMeshBounds the proxy spans LOD 0's source bounds, not the all-LOD
	// bounds inflated by bounds extensions that UStaticMesh::GetBounds() reports.
	FBoxSphereBounds GetRoadMeshBounds(const UStaticMesh& Mesh)
	{
		const IConsoleVariable* const FitToSourceMeshBounds =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.SplineMesh.FitToSourceMeshBounds"));
		const bool bFitToSourceMeshBounds =
			FitToSourceMeshBounds == nullptr || FitToSourceMeshBounds->GetBool();
		const FStaticMeshRenderData* const RenderData = Mesh.GetRenderData();
		if (bFitToSourceMeshBounds && RenderData != nullptr && !RenderData->LODResources.IsEmpty()
			&& RenderData->LODResources[0].SourceMeshBounds.SphereRadius > 0.0f)
		{
			return RenderData->LODResources[0].SourceMeshBounds;
		}
		return Mesh.GetBounds();
	}

	struct FRoadClass
	{
		TArray<FRoadMeshCandidate> Candidates;
		FString Rejection;
	};

	bool IsRoadMeshTileable(
		const UStaticMesh& Mesh,
		TConstArrayView<TObjectPtr<UMaterialInterface>> MaterialOverrides,
		const ESplineMeshAxis::Type ForwardAxis,
		FString& OutReason)
	{
		if (!Mesh.HasValidNaniteData())
		{
			OutReason = TEXT("it carries no Nanite data");
			return false;
		}
		const int32 SlotCount = Mesh.GetStaticMaterials().Num();
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			UMaterialInterface* const Material =
				MaterialOverrides.IsValidIndex(SlotIndex) && MaterialOverrides[SlotIndex] != nullptr
					? MaterialOverrides[SlotIndex].Get()
					: Mesh.GetMaterial(SlotIndex);
			// Ask for the usage rather than reading it: in the editor this sets the flag and recompiles,
			// which a passive read would never trigger, leaving the gate to reject the very material the
			// first spline mesh would have fixed.
			if (Material == nullptr || !Material->CheckMaterialUsage(MATUSAGE_SplineMesh))
			{
				OutReason = FString::Printf(
					TEXT("material slot %d ('%s') is not marked Used with Spline Meshes"),
					SlotIndex,
					Material != nullptr ? *Material->GetName() : TEXT("none"));
				return false;
			}
		}
		if (GetRoadMeshBounds(Mesh).BoxExtent[GetRoadForwardAxisIndex(ForwardAxis)] <= 0.0)
		{
			OutReason = TEXT("its bounds are flat along the authored forward axis, so a tile would have no length");
			return false;
		}
		return true;
	}

	const TArray<FRoadMeshCandidate>* ResolveRoadClass(
		const UArcTownRoadCatalog& Catalog,
		const uint8 Hierarchy,
		TMap<const UArcTownRoadDefinition*, FRoadClass>& InOutCache,
		FString& OutReason)
	{
		int32 HighestAuthored = INDEX_NONE;
		for (int32 EntryIndex = Catalog.ClassesByHierarchy.Num() - 1; EntryIndex >= 0; --EntryIndex)
		{
			if (Catalog.ClassesByHierarchy[EntryIndex] != nullptr)
			{
				HighestAuthored = EntryIndex;
				break;
			}
		}
		if (HighestAuthored == INDEX_NONE)
		{
			OutReason = TEXT("its road catalog authors no road class at all");
			return nullptr;
		}
		// An unauthored higher hierarchy falls back to the highest authored class, so a single-class
		// catalog serves every road; the alternative is a road that silently vanishes.
		const int32 Effective = FMath::Min(static_cast<int32>(Hierarchy), HighestAuthored);
		const UArcTownRoadDefinition* const Definition = Catalog.ClassesByHierarchy[Effective];
		if (Definition == nullptr)
		{
			OutReason = FString::Printf(
				TEXT("the road catalog entry for hierarchy %u is empty"), static_cast<uint32>(Effective));
			return nullptr;
		}
		if (const FRoadClass* const Cached = InOutCache.Find(Definition))
		{
			OutReason = Cached->Rejection;
			return Cached->Candidates.IsEmpty() ? nullptr : &Cached->Candidates;
		}

		FRoadClass& RoadClass = InOutCache.Add(Definition);
		TArray<FString> Rejections;
		for (int32 EntryIndex = 0; EntryIndex < Definition->SegmentMeshes.Num(); ++EntryIndex)
		{
			const FArcTownRoadMeshEntry& Entry = Definition->SegmentMeshes[EntryIndex];
			UStaticMesh* const Mesh = Entry.Mesh.LoadSynchronous();
			if (Mesh == nullptr)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"Road definition '%ls' segment entry %d names mesh '%ls', which will not load; the entry is skipped.",
					*Definition->GetName(),
					EntryIndex,
					*Entry.Mesh.ToString());
				Rejections.Add(FString::Printf(TEXT("entry %d will not load"), EntryIndex));
				continue;
			}

			TArray<TObjectPtr<UMaterialInterface>> Materials;
			Materials.Reserve(Entry.MaterialOverrides.Num());
			for (const TSoftObjectPtr<UMaterialInterface>& SoftMaterial : Entry.MaterialOverrides)
			{
				Materials.Add(SoftMaterial.LoadSynchronous());
			}

			// Tiling multiplies component count, and that stays draw-call-neutral only on the Nanite
			// spline path, whose raster permutation is not compiled unless the material carries the
			// spline-mesh usage flag (NaniteCullRaster.cpp:1650) - so a miss is an authoring error.
			FString Reason;
			if (!IsRoadMeshTileable(*Mesh, Materials, Entry.ForwardAxis, Reason))
			{
				UE_LOGF(LogArcTownInstanceEmitter, Error,"Road definition '%ls' segment entry %d cannot tile mesh '%ls': %ls.",
					*Definition->GetName(),
					EntryIndex,
					*Mesh->GetName(),
					*Reason);
				Rejections.Add(FString::Printf(TEXT("'%s' %s"), *Mesh->GetName(), *Reason));
				continue;
			}

			FRoadMeshCandidate& Candidate = RoadClass.Candidates.AddDefaulted_GetRef();
			Candidate.Mesh = Mesh;
			Candidate.ForwardAxis = Entry.ForwardAxis;
			Candidate.Weight = FMath::Max(Entry.SelectionWeight, 0.0f);
			Candidate.Materials = MoveTemp(Materials);
		}

		if (RoadClass.Candidates.IsEmpty())
		{
			RoadClass.Rejection = Rejections.IsEmpty()
				? FString::Printf(TEXT("road definition '%s' authors no segment mesh"), *Definition->GetName())
				: FString::Printf(
					TEXT("road definition '%s' left no tileable mesh (%s)"),
					*Definition->GetName(),
					*FString::Join(Rejections, TEXT("; ")));
			OutReason = RoadClass.Rejection;
			return nullptr;
		}
		return &RoadClass.Candidates;
	}

	int32 PickRoadCandidate(TConstArrayView<FRoadMeshCandidate> Candidates, const double Unit)
	{
		double TotalWeight = 0.0;
		int32 LastWeighted = INDEX_NONE;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Candidates[Index].Weight > 0.0f)
			{
				TotalWeight += Candidates[Index].Weight;
				LastWeighted = Index;
			}
		}
		if (LastWeighted == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		const double Target = Unit * TotalWeight;
		double Running = 0.0;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Running += Candidates[Index].Weight;
			if (Running > Target)
			{
				return Index;
			}
		}
		return LastWeighted;
	}

	/**
	 * A laid node mesh gets no SetForwardAxis, so its basis is built here the way the engine builds a
	 * deformed one (SplineMeshSceneProxyDesc.cpp:141-151): lateral is Up x Forward, and which mesh axis
	 * takes the road direction follows the entry's ForwardAxis.
	 */
	FQuat MakeRoadNodeRotation(const ESplineMeshAxis::Type ForwardAxis, const FVector& ArmDirection)
	{
		const FVector Up = FVector::ZAxisVector;
		const FVector Lateral = (Up ^ ArmDirection).GetSafeNormal();
		switch (ForwardAxis)
		{
		case ESplineMeshAxis::X:
			return FMatrix(ArmDirection, Lateral, Up, FVector::ZeroVector).ToQuat();
		case ESplineMeshAxis::Y:
			return FMatrix(Up, ArmDirection, Lateral, FVector::ZeroVector).ToQuat();
		default:
			return FMatrix(Lateral, Up, ArmDirection, FVector::ZeroVector).ToQuat();
		}
	}

	constexpr float MinDrawnSegmentLengthCm = 1.0f;

	/**
	 * On a private copy: the same UArcTownPathMetadata::HalfWidth feeds Entry.HalfWidthCurve of the
	 * terrain stamp, so widening a junction in place would move the terrain of a town with no catalog.
	 */
	void BuildReconciledHalfWidths(
		TConstArrayView<UArcTownPathComponent*> Paths,
		TArray<TArray<float>>& OutHalfWidths)
	{
		OutHalfWidths.Reset();
		OutHalfWidths.SetNum(Paths.Num());
		for (int32 PathIndex = 0; PathIndex < Paths.Num(); ++PathIndex)
		{
			const UArcTownPathComponent& Path = *Paths[PathIndex];
			TArray<float>& Widths = OutHalfWidths[PathIndex];
			Widths.Init(Path.DefaultHalfWidth, Path.GetNumberOfSplinePoints());

			const UArcTownPathMetadata* const Metadata = Path.GetPathMetadata();
			if (Metadata == nullptr)
			{
				continue;
			}
			const int32 PointCount = FMath::Min(Widths.Num(), Metadata->HalfWidth.Points.Num());
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				Widths[PointIndex] = Metadata->HalfWidth.Points[PointIndex].OutVal;
			}
		}

		TMap<int32, float> WidestAtNode;
		for (int32 PathIndex = 0; PathIndex < Paths.Num(); ++PathIndex)
		{
			const TArray<float>& Widths = OutHalfWidths[PathIndex];
			if (Widths.IsEmpty())
			{
				continue;
			}
			const UArcTownPathComponent& Path = *Paths[PathIndex];
			if (Path.StartNodeId != INDEX_NONE)
			{
				float& Widest = WidestAtNode.FindOrAdd(Path.StartNodeId, 0.0f);
				Widest = FMath::Max(Widest, Widths[0]);
			}
			if (Path.EndNodeId != INDEX_NONE)
			{
				float& Widest = WidestAtNode.FindOrAdd(Path.EndNodeId, 0.0f);
				Widest = FMath::Max(Widest, Widths.Last());
			}
		}

		for (int32 PathIndex = 0; PathIndex < Paths.Num(); ++PathIndex)
		{
			TArray<float>& Widths = OutHalfWidths[PathIndex];
			if (Widths.IsEmpty())
			{
				continue;
			}
			const UArcTownPathComponent& Path = *Paths[PathIndex];
			if (const float* const Widest = WidestAtNode.Find(Path.StartNodeId))
			{
				Widths[0] = *Widest;
			}
			if (const float* const Widest = WidestAtNode.Find(Path.EndNodeId))
			{
				Widths.Last() = *Widest;
			}
		}
	}

	double GetRoadHalfWidthAtDistance(
		const UArcTownPathComponent& Path,
		TConstArrayView<float> HalfWidths,
		const double Distance)
	{
		const int32 PointCount = HalfWidths.Num();
		if (PointCount < 2)
		{
			return PointCount == 1 ? static_cast<double>(HalfWidths[0]) : 0.0;
		}
		const float Key = Path.GetInputKeyValueAtDistanceAlongSpline(static_cast<float>(Distance));
		const int32 Lower = FMath::Clamp(FMath::FloorToInt(Key), 0, PointCount - 1);
		const int32 Upper = Path.IsClosedLoop()
			? (Lower + 1) % PointCount
			: FMath::Min(Lower + 1, PointCount - 1);
		const double Alpha = FMath::Clamp<double>(static_cast<double>(Key) - static_cast<double>(Lower), 0.0, 1.0);
		return FMath::Lerp(static_cast<double>(HalfWidths[Lower]), static_cast<double>(HalfWidths[Upper]), Alpha);
	}

	const TArray<TArray<FRoadMeshCandidate>>* ResolveJunctionClass(
		const UArcTownRoadCatalog& Catalog,
		const uint8 Hierarchy,
		TMap<const UArcTownRoadDefinition*, TArray<TArray<FRoadMeshCandidate>>>& InOutCache)
	{
		int32 HighestAuthored = INDEX_NONE;
		for (int32 EntryIndex = Catalog.ClassesByHierarchy.Num() - 1; EntryIndex >= 0; --EntryIndex)
		{
			if (Catalog.ClassesByHierarchy[EntryIndex] != nullptr)
			{
				HighestAuthored = EntryIndex;
				break;
			}
		}
		if (HighestAuthored == INDEX_NONE)
		{
			return nullptr;
		}
		// An unauthored higher hierarchy falls back to the highest authored class, so a single-class
		// catalog serves every road; the alternative is a road that silently vanishes.
		const int32 Effective = FMath::Min(static_cast<int32>(Hierarchy), HighestAuthored);
		const UArcTownRoadDefinition* const Definition = Catalog.ClassesByHierarchy[Effective];
		if (Definition == nullptr)
		{
			return nullptr;
		}
		if (const TArray<TArray<FRoadMeshCandidate>>* const Cached = InOutCache.Find(Definition))
		{
			return Cached;
		}

		TArray<TArray<FRoadMeshCandidate>>& ByValence = InOutCache.Add(Definition);
		ByValence.SetNum(Definition->JunctionMeshesByValence.Num());
		for (int32 Valence = 0; Valence < Definition->JunctionMeshesByValence.Num(); ++Valence)
		{
			const FArcTownRoadMeshEntry& Entry = Definition->JunctionMeshesByValence[Valence];
			if (Entry.Mesh.IsNull())
			{
				continue;
			}
			UStaticMesh* const Mesh = Entry.Mesh.LoadSynchronous();
			if (Mesh == nullptr)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"Road definition '%ls' junction entry %d names mesh '%ls', which will not load; that valence places nothing.",
					*Definition->GetName(),
					Valence,
					*Entry.Mesh.ToString());
				continue;
			}

			FRoadMeshCandidate& Candidate = ByValence[Valence].AddDefaulted_GetRef();
			Candidate.Mesh = Mesh;
			Candidate.ForwardAxis = Entry.ForwardAxis;
			Candidate.Weight = FMath::Max(Entry.SelectionWeight, 0.0f);
			Candidate.Materials.Reserve(Entry.MaterialOverrides.Num());
			for (const TSoftObjectPtr<UMaterialInterface>& SoftMaterial : Entry.MaterialOverrides)
			{
				Candidate.Materials.Add(SoftMaterial.LoadSynchronous());
			}
		}
		return &ByValence;
	}

	bool BuildRoadJunctions(
		AArcTownVolume& Town,
		UWorld& World,
		UActorPartitionSubsystem& PartitionSubsystem,
		const FArcTownWorldSnapshot& Snapshot,
		const UArcTownRoadCatalog& Catalog,
		TConstArrayView<UArcTownPathComponent*> Paths,
		const TArray<TArray<float>>& HalfWidths,
		const uint32 GridSize,
		const FGuid& CellGridGuid,
		const bool bPartitioned,
		FArcTownPreparedInstances& OutPrepared,
		TMap<FIntVector, AArcTownPartitionActor*>& InOutActorsByCell,
		TMap<const AArcTownPartitionActor*, int32>& InOutActorSetIndices,
		TMap<int32, double>& OutTrimByNode,
		int32& OutNodeCount,
		FText& OutError)
	{
		OutTrimByNode.Reset();
		OutNodeCount = 0;

		TMap<const UArcTownRoadDefinition*, TArray<TArray<FRoadMeshCandidate>>> JunctionCache;
		for (const FArcTownGeneratedJunction& Junction : OutPrepared.Junctions)
		{
			if (Junction.NodeId == INDEX_NONE)
			{
				continue;
			}
			const TArray<TArray<FRoadMeshCandidate>>* const ByValence =
				ResolveJunctionClass(Catalog, Junction.Hierarchy, JunctionCache);
			// No fallback to a neighbouring valence: a three-way wedged into a four-way is a hole
			// shaped like correct geometry, so an unauthored valence places nothing and trims nothing.
			if (ByValence == nullptr || static_cast<int32>(Junction.Valence) >= ByValence->Num())
			{
				continue;
			}
			const TArray<FRoadMeshCandidate>& Candidates = (*ByValence)[Junction.Valence];
			if (Candidates.IsEmpty())
			{
				continue;
			}

			const uint64 SelectionKey = ArcTownDeterministic::MakeKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::RoadSegmentMeshPick,
				Junction.NodeId,
				static_cast<int32>(Junction.Valence));
			const int32 CandidateIndex =
				PickRoadCandidate(Candidates, ArcTownDeterministic::KeyToUnitOpen(SelectionKey));
			if (CandidateIndex == INDEX_NONE)
			{
				continue;
			}
			const FRoadMeshCandidate& Candidate = Candidates[CandidateIndex];

			FVector NodeWorld = FVector::ZeroVector;
			FVector ArmTangent = FVector::ZeroVector;
			double NodeHalfWidth = 0.0;
			bool bFoundArm = false;
			for (int32 PathIndex = 0; PathIndex < Paths.Num() && !bFoundArm; ++PathIndex)
			{
				const UArcTownPathComponent& Path = *Paths[PathIndex];
				const int32 PointCount = Path.GetNumberOfSplinePoints();
				if (PointCount < 2 || HalfWidths[PathIndex].Num() != PointCount)
				{
					continue;
				}
				int32 ArmPoint = INDEX_NONE;
				if (Path.StartNodeId == Junction.NodeId)
				{
					ArmPoint = 0;
				}
				else if (Path.EndNodeId == Junction.NodeId)
				{
					ArmPoint = PointCount - 1;
				}
				if (ArmPoint == INDEX_NONE)
				{
					continue;
				}
				Path.GetLocationAndTangentAtSplinePoint(
					ArmPoint, NodeWorld, ArmTangent, ESplineCoordinateSpace::World);
				NodeHalfWidth = HalfWidths[PathIndex][ArmPoint];
				bFoundArm = true;
			}
			if (!bFoundArm)
			{
				continue;
			}

			FVector ArmDirection = ArmTangent;
			if (!ArmDirection.Normalize())
			{
				ArmDirection = FVector::XAxisVector;
			}

			const UActorPartitionSubsystem::FCellCoord CellCoord = bPartitioned
				? UActorPartitionSubsystem::FCellCoord::GetCellCoord(
					NodeWorld, World.PersistentLevel, GridSize)
				: UActorPartitionSubsystem::FCellCoord(0, 0, 0, World.PersistentLevel);
			const FIntVector Cell(
				static_cast<int32>(CellCoord.X), static_cast<int32>(CellCoord.Y), static_cast<int32>(CellCoord.Z));

			int32 SetIndex = INDEX_NONE;
			if (!ResolveCellActorSet(
				Town, World, PartitionSubsystem, OutPrepared, Cell, GridSize, CellGridGuid,
				bPartitioned, InOutActorsByCell, InOutActorSetIndices, SetIndex, OutError))
			{
				return false;
			}
			FArcTownPreparedActor& ActorSet = OutPrepared.Actors[SetIndex];
			AArcTownPartitionActor* const PartitionActor = ActorSet.Actor;
			USceneComponent* const ActorRoot = PartitionActor->GetRootComponent();
			if (ActorRoot == nullptr)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown partition actor '%s' has no root component to carry its road junctions."),
					*PartitionActor->GetName()));
			}

			const FName CandidateName = MakeUniqueObjectName(
				PartitionActor, UStaticMeshComponent::StaticClass(), FName(TEXT("ArcTownCandidateRoadNode")));
			UStaticMeshComponent* const Component =
				NewObject<UStaticMeshComponent>(PartitionActor, CandidateName, RF_NoFlags);
			Component->ClearFlags(RF_Transactional);
			Component->CreationMethod = EComponentCreationMethod::Instance;
			Component->SetMobility(ActorRoot->Mobility);
			Component->SetupAttachment(ActorRoot);
			Component->SetStaticMesh(Candidate.Mesh);
			for (int32 MaterialIndex = 0; MaterialIndex < Candidate.Materials.Num(); ++MaterialIndex)
			{
				if (Candidate.Materials[MaterialIndex] != nullptr)
				{
					Component->SetMaterial(MaterialIndex, Candidate.Materials[MaterialIndex]);
				}
			}
			Component->SetCastShadow(false);
			Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetGenerateOverlapEvents(false);
			Component->SetCanEverAffectNavigation(false);

			const FVector MeshExtent = Candidate.Mesh->GetBounds().BoxExtent;
			const int32 WidthAxis = GetRoadWidthAxisIndex(Candidate.ForwardAxis);
			const double MeshHalfWidth =
				FMath::Max<double>(MeshExtent[WidthAxis], UE_DOUBLE_KINDA_SMALL_NUMBER);
			FVector NodeScale = FVector::OneVector;
			NodeScale[WidthAxis] = NodeHalfWidth / MeshHalfWidth;

			const FTransform NodeWorldXform(
				MakeRoadNodeRotation(Candidate.ForwardAxis, ArmDirection), NodeWorld, NodeScale);
			Component->SetRelativeTransform(
				NodeWorldXform.GetRelativeTransform(PartitionActor->GetActorTransform()));

			ActorSet.RoadNodeComponents.Add(Component);
			ActorSet.RoadNodeFinalNames.Add(
				FName(*FString::Printf(TEXT("%s%05d"), ArcTownRoadNodePrefix, OutNodeCount)));
			++OutNodeCount;

			OutTrimByNode.Add(
				Junction.NodeId, MeshExtent[GetRoadForwardAxisIndex(Candidate.ForwardAxis)]);
		}
		return true;
	}

	bool BuildRoadSegments(
		AArcTownVolume& Town,
		UWorld& World,
		UActorPartitionSubsystem& PartitionSubsystem,
		const FArcTownWorldSnapshot& Snapshot,
		const uint32 GridSize,
		const FGuid& CellGridGuid,
		const bool bPartitioned,
		FArcTownPreparedInstances& OutPrepared,
		TMap<FIntVector, AArcTownPartitionActor*>& InOutActorsByCell,
		TMap<const AArcTownPartitionActor*, int32>& InOutActorSetIndices,
		FText& OutError)
	{
		TArray<UArcTownPathComponent*> Paths;
		GatherSortedPaths(Town, Paths);
		if (Paths.IsEmpty())
		{
			return true;
		}

		const UArcTownRoadCatalog* const Catalog = Town.RoadCatalog;
		if (Catalog == nullptr)
		{
			for (const UArcTownPathComponent* const Path : Paths)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown path '%ls' emits no road: town '%ls' names no road catalog.",
					*Path->GetName(),
					*Town.GetName());
			}
			return true;
		}

		TArray<TArray<float>> HalfWidths;
		BuildReconciledHalfWidths(Paths, HalfWidths);

		TMap<int32, double> TrimByNode;
		int32 NodeOrdinal = 0;
		if (!BuildRoadJunctions(
			Town, World, PartitionSubsystem, Snapshot, *Catalog, Paths, HalfWidths, GridSize, CellGridGuid,
			bPartitioned, OutPrepared, InOutActorsByCell, InOutActorSetIndices, TrimByNode, NodeOrdinal,
			OutError))
		{
			return false;
		}

		TMap<const UArcTownRoadDefinition*, FRoadClass> CandidateCache;
		TMap<FIntVector, int32> SegmentsPerCell;
		int32 SegmentOrdinal = 0;
		int32 PathsWithSegments = 0;
		double MinTileScale = TNumericLimits<double>::Max();
		double MaxTileScale = 0.0;
		FString WorstScalePathName;
		double WorstScaleValue = 1.0;
		double WorstScaleDistance = -1.0;

		for (int32 PathIndex = 0; PathIndex < Paths.Num(); ++PathIndex)
		{
			const UArcTownPathComponent& Path = *Paths[PathIndex];
			const int32 PointCount = Path.GetNumberOfSplinePoints();
			if (PointCount < 2 || HalfWidths[PathIndex].Num() != PointCount)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown path '%ls' emits no road: it carries %d spline point(s) and %d reconciled half width(s).",
					*Path.GetName(),
					PointCount,
					HalfWidths[PathIndex].Num());
				continue;
			}

			FString ClassRejection;
			const TArray<FRoadMeshCandidate>* const Candidates =
				ResolveRoadClass(*Catalog, Path.Hierarchy, CandidateCache, ClassRejection);
			if (Candidates == nullptr)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown path '%ls' emits no road: %ls.",
					*Path.GetName(),
					*ClassRejection);
				continue;
			}

			const TArray<float>& PathWidths = HalfWidths[PathIndex];
			const double* const TrimAtStart =
				Path.StartNodeId != INDEX_NONE ? TrimByNode.Find(Path.StartNodeId) : nullptr;
			const double* const TrimAtEnd =
				Path.EndNodeId != INDEX_NONE ? TrimByNode.Find(Path.EndNodeId) : nullptr;
			const double PathLength = Path.GetSplineLength();
			const double DrawFrom = TrimAtStart != nullptr ? *TrimAtStart : 0.0;
			const double DrawTo = TrimAtEnd != nullptr ? PathLength - *TrimAtEnd : PathLength;
			const double DrawnSpan = DrawTo - DrawFrom;
			if (DrawnSpan <= MinDrawnSegmentLengthCm)
			{
				UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown path '%ls' emits no road: its junction meshes trim its %.1f cm down to %.1f cm.",
					*Path.GetName(),
					PathLength,
					DrawnSpan);
				continue;
			}

			const int32 MaxTilesForPath =
				FMath::Max(1, FMath::FloorToInt(DrawnSpan / MinDrawnSegmentLengthCm));

			// Tiles are cut per key interval so none spans a control point: inside one cubic the hermite
			// through a tile's ends reproduces the curve exactly, which the key-span tangent rescale assumes.
			TArray<double> IntervalEdges;
			IntervalEdges.Reserve(PointCount + 2);
			IntervalEdges.Add(DrawFrom);
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const double PointDistance = Path.GetDistanceAlongSplineAtSplinePoint(PointIndex);
				if (PointDistance > DrawFrom && PointDistance < DrawTo)
				{
					IntervalEdges.Add(PointDistance);
				}
			}
			IntervalEdges.Add(DrawTo);
			IntervalEdges.Sort();

			TArray<int32> TilePicks;
			TArray<double> TileLengths;
			double PathMinTileScale = TNumericLimits<double>::Max();
			double PathMaxTileScale = 0.0;
			int32 EmittedForPath = 0;
			int32 TileOrdinal = 0;
			bool bPathTileable = true;
			double IntervalFrom = DrawFrom;
			for (int32 EdgeIndex = 1; EdgeIndex < IntervalEdges.Num(); ++EdgeIndex)
			{
				const double IntervalTo = IntervalEdges[EdgeIndex];
				const double IntervalLength = IntervalTo - IntervalFrom;
				if (IntervalLength <= MinDrawnSegmentLengthCm)
				{
					continue;
				}

				TilePicks.Reset();
				TileLengths.Reset();
				double AuthoredLength = 0.0;
				while (AuthoredLength < IntervalLength)
				{
					const uint64 SelectionKey = ArcTownDeterministic::MakeKey(
						Snapshot.SimulationId,
						Snapshot.Seed,
						ArcTownDeterministic::EDeterministicDomain::RoadSegmentMeshPick,
						PathIndex,
						TileOrdinal);
					const int32 CandidateIndex =
						PickRoadCandidate(*Candidates, ArcTownDeterministic::KeyToUnitOpen(SelectionKey));
					if (CandidateIndex == INDEX_NONE)
					{
						UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown path '%ls' emits no road: every segment mesh of its road class carries a zero selection weight.",
							*Path.GetName());
						bPathTileable = false;
						break;
					}
					const FRoadMeshCandidate& Candidate = (*Candidates)[CandidateIndex];
					TilePicks.Add(CandidateIndex);
					TileLengths.Add(
						GetRoadMeshBounds(*Candidate.Mesh).BoxExtent[GetRoadForwardAxisIndex(Candidate.ForwardAxis)]
						* 2.0);
					AuthoredLength += TileLengths.Last();
					++TileOrdinal;
					if (TileOrdinal > MaxTilesForPath)
					{
						UE_LOGF(LogArcTownInstanceEmitter, Error,"ArcTown path '%ls' stops after %d tile(s): covering its %.1f cm of drawn length takes more than one tile per %.1f cm, so its road class authors a segment mesh shorter than that.",
							*Path.GetName(),
							MaxTilesForPath,
							DrawnSpan,
							MinDrawnSegmentLengthCm);
						bPathTileable = false;
						break;
					}
				}
				if (!bPathTileable)
				{
					break;
				}

				const double TileScale = IntervalLength / AuthoredLength;
				PathMinTileScale = FMath::Min(PathMinTileScale, TileScale);
				PathMaxTileScale = FMath::Max(PathMaxTileScale, TileScale);

				double TileCursor = IntervalFrom;
				double PrefixLength = 0.0;
				for (int32 TileIndex = 0; TileIndex < TilePicks.Num(); ++TileIndex)
				{
					PrefixLength += TileLengths[TileIndex];
					const double DrawnFrom = TileCursor;
					double DrawnTo = TileIndex == TilePicks.Num() - 1
						? IntervalTo
						: IntervalFrom + PrefixLength * TileScale;
					if (TileIndex == TilePicks.Num() - 2 && IntervalTo - DrawnTo <= MinDrawnSegmentLengthCm)
					{
						DrawnTo = IntervalTo;
					}
					if (DrawnTo - DrawnFrom <= MinDrawnSegmentLengthCm)
					{
						continue;
					}
					TileCursor = DrawnTo;
					const FRoadMeshCandidate& Candidate = (*Candidates)[TilePicks[TileIndex]];

					// The tangents are derivatives by input key, so a shortened hermite rescales by the key
					// span it now covers, never by arc length: speed along a key is not constant.
					const double TangentScale =
						static_cast<double>(Path.GetInputKeyValueAtDistanceAlongSpline(static_cast<float>(DrawnTo)))
						- static_cast<double>(Path.GetInputKeyValueAtDistanceAlongSpline(static_cast<float>(DrawnFrom)));
					const FVector StartWorld = Path.GetLocationAtDistanceAlongSpline(
						static_cast<float>(DrawnFrom), ESplineCoordinateSpace::World);
					const FVector StartTangent = Path.GetTangentAtDistanceAlongSpline(
						static_cast<float>(DrawnFrom), ESplineCoordinateSpace::World) * TangentScale;
					const FVector EndWorld = Path.GetLocationAtDistanceAlongSpline(
						static_cast<float>(DrawnTo), ESplineCoordinateSpace::World);
					const FVector EndTangent = Path.GetTangentAtDistanceAlongSpline(
						static_cast<float>(DrawnTo), ESplineCoordinateSpace::World) * TangentScale;

					const FVector TileCentre = 0.5 * (StartWorld + EndWorld);
					const UActorPartitionSubsystem::FCellCoord CellCoord = bPartitioned
						? UActorPartitionSubsystem::FCellCoord::GetCellCoord(
							TileCentre, World.PersistentLevel, GridSize)
						: UActorPartitionSubsystem::FCellCoord(0, 0, 0, World.PersistentLevel);
					const FIntVector Cell(
						static_cast<int32>(CellCoord.X), static_cast<int32>(CellCoord.Y), static_cast<int32>(CellCoord.Z));

					int32 SetIndex = INDEX_NONE;
					if (!ResolveCellActorSet(
						Town, World, PartitionSubsystem, OutPrepared, Cell, GridSize, CellGridGuid,
						bPartitioned, InOutActorsByCell, InOutActorSetIndices, SetIndex, OutError))
					{
						return false;
					}
					FArcTownPreparedActor& ActorSet = OutPrepared.Actors[SetIndex];
					AArcTownPartitionActor* const PartitionActor = ActorSet.Actor;
					USceneComponent* const ActorRoot = PartitionActor->GetRootComponent();
					if (ActorRoot == nullptr)
					{
						return SetError(OutError, FString::Printf(
							TEXT("ArcTown partition actor '%s' has no root component to carry its road segments."),
							*PartitionActor->GetName()));
					}

					const FName CandidateName = MakeUniqueObjectName(
						PartitionActor, USplineMeshComponent::StaticClass(), FName(TEXT("ArcTownCandidateRoad")));
					USplineMeshComponent* const Component =
						NewObject<USplineMeshComponent>(PartitionActor, CandidateName, RF_NoFlags);
					Component->ClearFlags(RF_Transactional);
					Component->CreationMethod = EComponentCreationMethod::Instance;
					Component->SetMobility(ActorRoot->Mobility);
					Component->SetupAttachment(ActorRoot);
					Component->SetStaticMesh(Candidate.Mesh);
					for (int32 MaterialIndex = 0; MaterialIndex < Candidate.Materials.Num(); ++MaterialIndex)
					{
						if (Candidate.Materials[MaterialIndex] != nullptr)
						{
							Component->SetMaterial(MaterialIndex, Candidate.Materials[MaterialIndex]);
						}
					}
					Component->SetCastShadow(false);
					// A road's collision is the terrain under it, and a deformed spline mesh cooks its own
					// unshareable trimesh per component: this town emits thousands of them.
					Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
					Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Component->SetGenerateOverlapEvents(false);
					Component->SetCanEverAffectNavigation(false);

					const int32 WidthAxis = GetRoadWidthAxisIndex(Candidate.ForwardAxis);
					const double MeshHalfWidth = FMath::Max<double>(
						GetRoadMeshBounds(*Candidate.Mesh).BoxExtent[WidthAxis], UE_DOUBLE_KINDA_SMALL_NUMBER);
					const FTransform ActorXform = PartitionActor->GetActorTransform();
					Component->SetForwardAxis(Candidate.ForwardAxis, /*bUpdateMesh=*/false);
					const double StartHalfWidth = GetRoadHalfWidthAtDistance(Path, PathWidths, DrawnFrom);
					const double EndHalfWidth = GetRoadHalfWidthAtDistance(Path, PathWidths, DrawnTo);
					Component->SetStartScale(
						FVector2D(StartHalfWidth / MeshHalfWidth, 1.0),
						/*bUpdateMesh=*/false);
					Component->SetEndScale(
						FVector2D(EndHalfWidth / MeshHalfWidth, 1.0),
						/*bUpdateMesh=*/false);
					Component->SetStartAndEnd(
						ActorXform.InverseTransformPosition(StartWorld),
						ActorXform.InverseTransformVector(StartTangent),
						ActorXform.InverseTransformPosition(EndWorld),
						ActorXform.InverseTransformVector(EndTangent),
						/*bUpdateMesh=*/false);
					Component->UpdateMesh();

					ActorSet.RoadComponents.Add(Component);
					ActorSet.RoadFinalNames.Add(
						FName(*FString::Printf(TEXT("%s%05d"), ArcTownRoadSegmentPrefix, SegmentOrdinal)));
					++SegmentOrdinal;
					++EmittedForPath;
					++SegmentsPerCell.FindOrAdd(Cell, 0);
				}
				IntervalFrom = IntervalTo;
			}

			if (EmittedForPath > 0)
			{
				++PathsWithSegments;
			}
			if (PathMinTileScale <= PathMaxTileScale)
			{
				MinTileScale = FMath::Min(MinTileScale, PathMinTileScale);
				MaxTileScale = FMath::Max(MaxTileScale, PathMaxTileScale);
				const double PathExtreme =
					FMath::Abs(PathMinTileScale - 1.0) > FMath::Abs(PathMaxTileScale - 1.0)
						? PathMinTileScale
						: PathMaxTileScale;
				if (FMath::Abs(PathExtreme - 1.0) > WorstScaleDistance)
				{
					WorstScaleDistance = FMath::Abs(PathExtreme - 1.0);
					WorstScaleValue = PathExtreme;
					WorstScalePathName = Path.GetName();
				}
			}
		}

		int32 BusiestCell = 0;
		for (const TPair<FIntVector, int32>& CellCount : SegmentsPerCell)
		{
			BusiestCell = FMath::Max(BusiestCell, CellCount.Value);
		}
		const double ReportedMinScale = MinTileScale <= MaxTileScale ? MinTileScale : 0.0;

		if (WorstScaleDistance >= 0.0)
		{
			UE_LOGF(LogArcTownInstanceEmitter, Warning,"ArcTown path '%ls' draws its road tiles the furthest from their authored length, at a tile scale of %.3f.",
				*WorstScalePathName,
				WorstScaleValue);
		}

		UE_LOGF(LogArcTownInstanceEmitter, Log,"ArcTown '%ls' prepared %d road segment component(s) and %d junction component(s) from %d path(s) across %d partition cell(s); the busiest cell carries %d segment(s), and tile scale ran from %.3f to %.3f across every path.",
			*Town.GetName(),
			SegmentOrdinal,
			NodeOrdinal,
			PathsWithSegments,
			SegmentsPerCell.Num(),
			BusiestCell,
			ReportedMinScale,
			MaxTileScale);
		return true;
	}

	/**
	 * Loads the partition actors this town already owns, before any component is created. Pinning is
	 * what turns a streamed-out actor from a silent drop into a failure: it keeps its previous instances
	 * and the next rebuild would place a second copy beside them.
	 */
	bool ResolveOwnedActors(
		const AArcTownVolume& Town,
		UWorldPartition* WorldPartition,
		FArcTownPreparedInstances& OutPrepared,
		TArray<AArcTownPartitionActor*>& OutPreviousActors,
		FText& OutError)
	{
		for (const FArcTownEmittedActor& Record : Town.EmittedActors)
		{
			AArcTownPartitionActor* Actor = Cast<AArcTownPartitionActor>(Record.Actor.Get());
			if (Actor == nullptr && WorldPartition != nullptr && Record.ActorGuid.IsValid())
			{
				FWorldPartitionReference Reference(WorldPartition, Record.ActorGuid);
				if (Reference.IsValid())
				{
					Actor = Cast<AArcTownPartitionActor>(Reference.GetActor());
					if (Actor == nullptr)
					{
						return SetError(OutError, FString::Printf(
							TEXT("ArcTown could not load the partition actor '%s' this town owns; clearing its cell is what ")
							TEXT("keeps a rebuild from placing a second copy of the settlement beside the first."),
							*Record.Actor.ToString()));
					}
					OutPrepared.PinnedActors.Add(Reference);
				}
			}
			if (Actor != nullptr)
			{
				OutPreviousActors.AddUnique(Actor);
			}
		}
		return true;
	}

	void DestroyEmittedActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}
		UWorld* const World = Actor->GetWorld();
		if (World != nullptr && !World->bIsTearingDown)
		{
			World->DestroyActor(Actor);
		}
	}

	struct FEntityBucketKey
	{
		FIntVector CellCoord = UnassignedCell;
		UArcMassEntityConfigAsset* Config = nullptr;

		bool operator==(const FEntityBucketKey& Other) const
		{
			return CellCoord == Other.CellCoord && Config == Other.Config;
		}

		friend uint32 GetTypeHash(const FEntityBucketKey& Key)
		{
			return HashCombine(GetTypeHash(Key.CellCoord), PointerHash(Key.Config));
		}
	};

	struct FEntityBucket
	{
		FEntityBucketKey Key;
		TArray<FTransform> WorldXforms;
	};

	AArcPlacedEntityPartitionActor* ResolveEntityHost(
		AArcTownVolume& Town,
		UActorPartitionSubsystem& PartitionSubsystem,
		FArcTownPreparedInstances& OutPrepared,
		const UActorPartitionSubsystem::FCellCoord& CellCoord,
		const uint32 GridSize,
		const FGuid& InGridGuid,
		const FName RuntimeGridName,
		const bool bPartitioned)
	{
		AArcTownVolume* const TownPtr = &Town;
		FArcTownPreparedInstances* const PreparedPtr = &OutPrepared;
		const FGuid GridGuid = InGridGuid;
		const FName GridName = RuntimeGridName;
		const bool bIsPartitioned = bPartitioned;
		return Cast<AArcPlacedEntityPartitionActor>(PartitionSubsystem.GetActor(
			AArcPlacedEntityPartitionActor::StaticClass(),
			CellCoord,
			/*bInCreate=*/true,
			GridGuid,
			GridSize,
			/*bInBoundsSearch=*/true,
			/*InActorCreated=*/[GridGuid, TownPtr, PreparedPtr, GridName, bIsPartitioned](APartitionActor* NewActor)
			{
				AArcPlacedEntityPartitionActor* const Created = CastChecked<AArcPlacedEntityPartitionActor>(NewActor);
				Created->SetGridGuid(GridGuid);
				Created->SetGridName(GridName);
				Created->OutlinerParent = TownPtr;
				if (bIsPartitioned && !GridName.IsNone())
				{
					Created->SetRuntimeGrid(GridName);
				}
				PreparedPtr->CreatedActors.Add(Created);
			}));
	}

	void MakeEntityHostRecords(
		TConstArrayView<AArcPlacedEntityPartitionActor*> Hosts,
		TArray<FArcTownEmittedActor>& OutRecords)
	{
		OutRecords.Reset(Hosts.Num());
		for (AArcPlacedEntityPartitionActor* Host : Hosts)
		{
			FArcTownEmittedActor& Record = OutRecords.AddDefaulted_GetRef();
			Record.Actor = Host;
			Record.ActorGuid = Host->GetActorGuid();
		}
	}

	bool ResolveOwnedEntityHosts(
		const AArcTownVolume& Town,
		UWorldPartition* WorldPartition,
		TConstArrayView<FGuid> DescGuids,
		FArcTownPreparedInstances& OutPrepared,
		TArray<AArcPlacedEntityPartitionActor*>& OutPreviousHosts,
		FText& OutError)
	{
		TSet<FGuid> ResolvedGuids;
		for (const FArcTownEmittedActor& Record : Town.EmittedEntityHosts)
		{
			AArcPlacedEntityPartitionActor* Host = Cast<AArcPlacedEntityPartitionActor>(Record.Actor.Get());
			if (Host == nullptr && WorldPartition != nullptr && Record.ActorGuid.IsValid())
			{
				FWorldPartitionReference Reference(WorldPartition, Record.ActorGuid);
				if (Reference.IsValid())
				{
					Host = Cast<AArcPlacedEntityPartitionActor>(Reference.GetActor());
					if (Host == nullptr)
					{
						return SetError(OutError, FString::Printf(
							TEXT("ArcTown could not load the placed-entity host '%s' this town owns; clearing its entries is ")
							TEXT("what keeps a rebuild from appending a second copy of the settlement beside the first."),
							*Record.Actor.ToString()));
					}
					OutPrepared.PinnedActors.Add(Reference);
				}
			}
			if (Host != nullptr)
			{
				ResolvedGuids.Add(Host->GetActorGuid());
				OutPreviousHosts.AddUnique(Host);
			}
		}

		for (const FGuid& DescGuid : DescGuids)
		{
			if (ResolvedGuids.Contains(DescGuid))
			{
				continue;
			}
			FWorldPartitionReference Reference(WorldPartition, DescGuid);
			if (!Reference.IsValid())
			{
				continue;
			}
			AArcPlacedEntityPartitionActor* const Host =
				Cast<AArcPlacedEntityPartitionActor>(Reference.GetActor());
			if (Host == nullptr)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown could not load the placed-entity host %s carrying this town's family guid; clearing its ")
					TEXT("entries is what keeps a rebuild from appending a second copy of the settlement beside the first."),
					*DescGuid.ToString()));
			}
			OutPrepared.PinnedActors.Add(Reference);
			ResolvedGuids.Add(DescGuid);
			OutPreviousHosts.AddUnique(Host);
		}
		return true;
	}

	void CommitFailedBuildRecords(AArcTownVolume& Town, FArcTownPreparedInstances& Prepared)
	{
		if (Prepared.FailedBuild.IsEmpty())
		{
			return;
		}
		Town.Modify();
		Town.EmittedEntityHosts = MoveTemp(Prepared.FailedBuild.EntityHosts.GetValue());
		Prepared.FailedBuild = FArcTownFailedBuildRecords();
	}

	bool EmitEntityInstances(
		AArcTownVolume& Town,
		UWorld& World,
		UActorPartitionSubsystem& PartitionSubsystem,
		UWorldPartition* WorldPartition,
		const bool bPartitioned,
		const uint32 GridSize,
		TConstArrayView<FEntityBucket> Buckets,
		FArcTownPreparedInstances& OutPrepared,
		FText& OutError)
	{
		const FGuid GridGuid = FArcTownInstanceEmitter::MakeEntityHostGridGuid(Town.TownId);
		TArray<FGuid> DescGuids;
		FArcTownInstanceEmitter::CollectEntityHostDescGuids(WorldPartition, GridGuid, DescGuids);
		if (Buckets.IsEmpty() && Town.EmittedEntityHosts.IsEmpty() && DescGuids.IsEmpty())
		{
			return true;
		}

		TArray<AArcPlacedEntityPartitionActor*> PreviousHosts;
		if (!ResolveOwnedEntityHosts(Town, WorldPartition, DescGuids, OutPrepared, PreviousHosts, OutError))
		{
			return false;
		}

		TArray<AArcPlacedEntityPartitionActor*> WrittenHosts;
		bool bEmitted = true;
		for (const FEntityBucket& Bucket : Buckets)
		{
			const UActorPartitionSubsystem::FCellCoord CellCoord(
				Bucket.Key.CellCoord.X, Bucket.Key.CellCoord.Y, Bucket.Key.CellCoord.Z, World.PersistentLevel);
			AArcPlacedEntityPartitionActor* const Host = ResolveEntityHost(
				Town, PartitionSubsystem, OutPrepared, CellCoord, GridSize, GridGuid,
				Town.GetRuntimeGrid(), bPartitioned);
			if (Host == nullptr)
			{
				SetError(OutError, FString::Printf(
					TEXT("ArcTown could not resolve its placed-entity host in cell (%d, %d, %d); one exists there but will ")
					TEXT("not load, and spawning a second would collide with its name."),
					Bucket.Key.CellCoord.X,
					Bucket.Key.CellCoord.Y,
					Bucket.Key.CellCoord.Z));
				bEmitted = false;
				break;
			}
			WrittenHosts.AddUnique(Host);

			const FTransform HostXform = Host->GetActorTransform();
			FArcTownPreparedEntityWrite& Write = OutPrepared.EntityWrites.AddDefaulted_GetRef();
			Write.Actor = Host;
			Write.Config = Bucket.Key.Config;
			Write.RelativeTransforms.Reserve(Bucket.WorldXforms.Num());
			for (const FTransform& WorldXform : Bucket.WorldXforms)
			{
				Write.RelativeTransforms.Add(WorldXform.GetRelativeTransform(HostXform));
			}
		}

		if (!bEmitted)
		{
			TArray<FArcTownEmittedActor> FailedRecords;
			MakeEntityHostRecords(PreviousHosts, FailedRecords);
			OutPrepared.FailedBuild.EntityHosts = MoveTemp(FailedRecords);
			return false;
		}

		MakeEntityHostRecords(WrittenHosts, OutPrepared.EntityHostRecords);
		OutPrepared.PreviousEntityHosts.Reserve(PreviousHosts.Num());
		for (AArcPlacedEntityPartitionActor* Host : PreviousHosts)
		{
			OutPrepared.PreviousEntityHosts.Add(Host);
		}
		return true;
	}

	const FArcTownWorldSnapshot* ResolveSimulatedSnapshot(const AArcTownVolume& Town, FText& OutError)
	{
		if (Town.SimulationState == nullptr || !Town.SimulationState->Snapshot.SimulationId.IsValid())
		{
			SetError(OutError, FString::Printf(
				TEXT("ArcTown '%s' has no simulated settlement to lay out. Run Simulate first."),
				*Town.GetName()));
			return nullptr;
		}
		return &Town.SimulationState->Snapshot;
	}

	bool SpawnAndAdoptBuildingActors(
		AArcTownPartitionActor& PartitionActor,
		TConstArrayView<FArcTownPreparedBuildingActor> Spawns,
		FArcTownPreparedInstances& Prepared,
		FText& OutError)
	{
		UWorld* const World = PartitionActor.GetWorld();
		if (World == nullptr)
		{
			return SetError(OutError, TEXT("ArcTown cannot spawn its building actors without a world."));
		}

		TArray<FArcTownEmittedActor> Records;
		Records.Reserve(Spawns.Num());
		for (const FArcTownPreparedBuildingActor& Spawn : Spawns)
		{
			if (Spawn.ActorClass == nullptr)
			{
				return SetError(OutError, TEXT("ArcTown prepared a building actor with no class."));
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = PartitionActor.GetLevel();
			SpawnParams.ObjectFlags = RF_NoFlags;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* const Spawned = World->SpawnActor<AActor>(Spawn.ActorClass, Spawn.WorldXform, SpawnParams);
			if (Spawned == nullptr)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcTown could not spawn building actor of class '%s'."),
					*GetPathNameSafe(Spawn.ActorClass)));
			}

			Prepared.SpawnedBuildingActors.Add(Spawned);
			FArcTownEmittedActor& Record = Records.AddDefaulted_GetRef();
			Record.Actor = Spawned;
			Record.ActorGuid = Spawned->GetActorGuid();
		}

		if (!PartitionActor.AdoptPreparedBuildingActors(MoveTemp(Records), OutError))
		{
			return false;
		}
		Prepared.SpawnedBuildingActors.Reset();
		return true;
	}

	template <typename ComponentType>
	void DestroyCandidate(ComponentType* Component, const TCHAR* DiscardBaseName)
	{
		if (!IsValid(Component))
		{
			return;
		}
		const FName DiscardName =
			MakeUniqueObjectName(Component->GetOuter(), Component->GetClass(), FName(DiscardBaseName));
		Component->Rename(
			*DiscardName.ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		Component->DestroyComponent();
	}
}

bool FArcTownInstanceEmitter::ResolveGeneratedPaths(
	const AArcTownVolume& Town,
	TArray<FArcTownGeneratedPath>& OutPaths,
	TArray<FArcTownGeneratedJunction>& OutJunctions,
	FText& OutError)
{
	using namespace ArcTownInstanceEmitterPrivate;

	OutPaths.Reset();
	OutJunctions.Reset();

	const FArcTownWorldSnapshot* const Snapshot = ResolveSimulatedSnapshot(Town, OutError);
	if (Snapshot == nullptr)
	{
		return false;
	}

	return ArcTownLayout::BuildPathsFromSnapshot(*Snapshot, Town.SiteSnapshot, OutPaths, OutJunctions, OutError);
}

FGuid FArcTownInstanceEmitter::MakeEntityHostGridGuid(const FGuid& TownId)
{
	FGuid Seed = TownId;
	FArchiveMD5 ArMD5;
	ArMD5 << Seed;
	return ArMD5.GetGuidFromHash();
}

void FArcTownInstanceEmitter::CollectEntityHostDescGuids(
	UWorldPartition* WorldPartition,
	const FGuid& EntityHostGridGuid,
	TArray<FGuid>& OutGuids)
{
	if (WorldPartition == nullptr || !EntityHostGridGuid.IsValid())
	{
		return;
	}
	FWorldPartitionHelpers::ForEachActorDescInstance(
		WorldPartition,
		AArcPlacedEntityPartitionActor::StaticClass(),
		[&EntityHostGridGuid, &OutGuids](const FWorldPartitionActorDescInstance* DescInstance) -> bool
		{
			const FPartitionActorDesc* const Desc = DescInstance != nullptr
				? static_cast<const FPartitionActorDesc*>(DescInstance->GetActorDesc())
				: nullptr;
			if (Desc != nullptr && Desc->GridGuid == EntityHostGridGuid && DescInstance->GetGuid().IsValid())
			{
				OutGuids.Add(DescInstance->GetGuid());
			}
			return true;
		});
}

bool FArcTownInstanceEmitter::Prepare(
	AArcTownVolume& Town,
	FArcTownPreparedInstances& OutPrepared,
	FText& OutError)
{
	using namespace ArcTownInstanceEmitterPrivate;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_PrepareInstances);

	OutError = FText::GetEmpty();

	UWorld* const World = Town.GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires a world with a persistent level."));
	}
	UActorPartitionSubsystem* const PartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
	if (PartitionSubsystem == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires the actor partition subsystem."));
	}
	const bool bPartitioned = World->IsPartitionedWorld();
	UWorldPartition* const WorldPartition = bPartitioned ? World->GetWorldPartition() : nullptr;
	if (bPartitioned && WorldPartition == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown rebuild could not reach the world partition of a partitioned world."));
	}

	if (!Town.TownId.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires a valid TownId; it is the only key its partition actors carry."));
	}
	if (Town.VolumeBounds == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires a VolumeBounds component."));
	}
	if (Town.Catalog == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires a building catalog."));
	}
	if (!Town.SiteSnapshot.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires a valid sampled site. Run Sample Site first."));
	}

	const FTransform TownXform = Town.GetActorTransform();
	const FVector TownScale = TownXform.GetScale3D();
	if (!TownXform.IsValid() || TownScale.IsNearlyZero())
	{
		return SetError(OutError, TEXT("ArcTown rebuild requires a finite volume transform with non-zero scale."));
	}

	const uint32 GridSize = AArcTownPartitionActor::StaticClass()
		->GetDefaultObject<AArcTownPartitionActor>()->GetDefaultGridSize(World);
	if (GridSize == 0)
	{
		return SetError(OutError, TEXT("ArcTown rebuild read a zero partition grid size."));
	}

	const uint32 EntityGridSize = AArcPlacedEntityPartitionActor::StaticClass()
		->GetDefaultObject<AArcPlacedEntityPartitionActor>()->GetDefaultGridSize(World);
	if (EntityGridSize == 0)
	{
		return SetError(OutError, TEXT("ArcTown rebuild read a zero placed-entity grid size."));
	}

	TArray<FCandidate> Candidates;
	TArray<int32> EntryToCandidate;
	if (!ResolveCandidates(*Town.Catalog, Candidates, EntryToCandidate, OutError))
	{
		return false;
	}

	const FArcTownWorldSnapshot* const Snapshot = ResolveSimulatedSnapshot(Town, OutError);
	if (Snapshot == nullptr)
	{
		return false;
	}

	TArray<FArcTownPlacedBuilding> LayoutBuildings;
	if (!ArcTownLayout::BuildBuildingsFromSnapshot(*Snapshot, Town.SiteSnapshot, LayoutBuildings, OutError))
	{
		return false;
	}

	TArray<FPlacedBuilding> PlacedBuildings;
	if (!ConvertPlacedBuildings(
		Town.SiteSnapshot,
		TownXform,
		*Town.Catalog,
		Candidates,
		EntryToCandidate,
		LayoutBuildings,
		PlacedBuildings,
		OutError))
	{
		return false;
	}

	AArcTerrain* const PadTerrain = ResolvePadTerrain(*World, Town.SiteSnapshot.TerrainGuid);
	if (PadTerrain == nullptr)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown rebuild found no loaded ArcTerrain with guid %s, the one its site was sampled from; the ")
			TEXT("pads that flatten the ground under its buildings composite into that terrain."),
			*Town.SiteSnapshot.TerrainGuid.ToString()));
	}
	OutPrepared.PadTerrain = PadTerrain;
	BuildPadSpecs(Town, TownXform, Candidates, PlacedBuildings, OutPrepared.PadSpecs);
	BuildRoadModifierSpec(Town, OutPrepared.RoadSpec);

	TArray<AArcTownPartitionActor*> PreviousActors;
	if (!ResolveOwnedActors(Town, WorldPartition, OutPrepared, PreviousActors, OutError))
	{
		Discard(OutPrepared);
		return false;
	}

	TArray<FGroup> Groups;
	TMap<FGroupKey, int32> GroupIndices;
	TArray<FCellBuildingActors> BuildingActorCells;
	TMap<FIntVector, int32> BuildingActorCellIndices;
	TArray<FEntityBucket> EntityBuckets;
	TMap<FEntityBucketKey, int32> EntityBucketIndices;
	for (const FPlacedBuilding& Placed : PlacedBuildings)
	{
		const FTransform WorldXform = Placed.LocalXform * TownXform;
		// A non-partitioned level holds a single actor per class, so every building shares one cell.
		const UActorPartitionSubsystem::FCellCoord CellCoord = bPartitioned
			? UActorPartitionSubsystem::FCellCoord::GetCellCoord(
				WorldXform.GetLocation(), World->PersistentLevel, GridSize)
			: UActorPartitionSubsystem::FCellCoord(0, 0, 0, World->PersistentLevel);
		const FIntVector Cell(
			static_cast<int32>(CellCoord.X), static_cast<int32>(CellCoord.Y), static_cast<int32>(CellCoord.Z));

		const FCandidate& Candidate = Candidates[Placed.CandidateIndex];
		if (Candidate.ActorClass != nullptr)
		{
			int32* CellIndex = BuildingActorCellIndices.Find(Cell);
			if (CellIndex == nullptr)
			{
				CellIndex = &BuildingActorCellIndices.Add(Cell, BuildingActorCells.Num());
				BuildingActorCells.AddDefaulted_GetRef().CellCoord = Cell;
			}
			FArcTownPreparedBuildingActor& Spawn = BuildingActorCells[*CellIndex].Spawns.AddDefaulted_GetRef();
			Spawn.ActorClass = Candidate.ActorClass;
			Spawn.WorldXform = WorldXform;
			continue;
		}
		if (Candidate.EntityConfig != nullptr)
		{
			const UActorPartitionSubsystem::FCellCoord EntityCellCoord = bPartitioned
				? UActorPartitionSubsystem::FCellCoord::GetCellCoord(
					WorldXform.GetLocation(), World->PersistentLevel, EntityGridSize)
				: UActorPartitionSubsystem::FCellCoord(0, 0, 0, World->PersistentLevel);

			FEntityBucketKey BucketKey;
			BucketKey.CellCoord = FIntVector(
				static_cast<int32>(EntityCellCoord.X),
				static_cast<int32>(EntityCellCoord.Y),
				static_cast<int32>(EntityCellCoord.Z));
			BucketKey.Config = Candidate.EntityConfig;

			int32* BucketIndex = EntityBucketIndices.Find(BucketKey);
			if (BucketIndex == nullptr)
			{
				BucketIndex = &EntityBucketIndices.Add(BucketKey, EntityBuckets.Num());
				EntityBuckets.AddDefaulted_GetRef().Key = BucketKey;
			}
			EntityBuckets[*BucketIndex].WorldXforms.Add(WorldXform);
			continue;
		}

		FGroupKey Key;
		Key.CellCoord = Cell;
		Key.Mesh = Candidate.Mesh;

		int32* ExistingIndex = GroupIndices.Find(Key);
		if (ExistingIndex == nullptr)
		{
			ExistingIndex = &GroupIndices.Add(Key, Groups.Num());
			Groups.AddDefaulted_GetRef().Key = Key;
		}
		Groups[*ExistingIndex].WorldXforms.Add(WorldXform);
	}

	const FGuid CellGridGuid = ArcTownGridKeys::MakeTownGridGuid(Town.TownId, GridSize, Town.GetRuntimeGrid());
	TMap<FIntVector, AArcTownPartitionActor*> ActorsByCell;
	TMap<const AArcTownPartitionActor*, int32> ActorSetIndices;
	for (const FGroup& Group : Groups)
	{
		int32 SetIndex = INDEX_NONE;
		if (!ResolveCellActorSet(
			Town, *World, *PartitionSubsystem, OutPrepared, Group.Key.CellCoord, GridSize, CellGridGuid,
			bPartitioned, ActorsByCell, ActorSetIndices, SetIndex, OutError))
		{
			Discard(OutPrepared);
			return false;
		}
		FArcTownPreparedActor& ActorSet = OutPrepared.Actors[SetIndex];
		AArcTownPartitionActor* const PartitionActor = ActorSet.Actor;

		const FName FinalName = MakeComponentName(Group.Key);
		if (ActorSet.FinalNames.Contains(FinalName))
		{
			SetError(OutError, FString::Printf(
				TEXT("ArcTown instance component name '%s' collides inside cell (%d, %d, %d)."),
				*FinalName.ToString(),
				Group.Key.CellCoord.X,
				Group.Key.CellCoord.Y,
				Group.Key.CellCoord.Z));
			Discard(OutPrepared);
			return false;
		}

		const FName CandidateName = MakeUniqueObjectName(
			PartitionActor, UInstancedStaticMeshComponent::StaticClass(), FName(TEXT("ArcTownCandidateISM")));
		UInstancedStaticMeshComponent* const Component =
			NewObject<UInstancedStaticMeshComponent>(PartitionActor, CandidateName, RF_NoFlags);
		Component->ClearFlags(RF_Transactional);
		Component->CreationMethod = EComponentCreationMethod::Instance;
		Component->SetMobility(EComponentMobility::Static);
		Component->SetupAttachment(PartitionActor->GetRootComponent());
		Component->SetStaticMesh(Group.Key.Mesh);

		const FTransform ActorXform = PartitionActor->GetActorTransform();
		for (const FTransform& WorldXform : Group.WorldXforms)
		{
			// A partition actor sits at its cell centre: without this the settlement is offset by
			// (volume origin - cell centre), differently in every cell.
			Component->AddInstance(WorldXform.GetRelativeTransform(ActorXform), /*bWorldSpace=*/false);
		}

		ActorSet.Components.Add(Component);
		ActorSet.FinalNames.Add(FinalName);
	}

	for (FCellBuildingActors& BuildingCell : BuildingActorCells)
	{
		int32 SetIndex = INDEX_NONE;
		if (!ResolveCellActorSet(
			Town, *World, *PartitionSubsystem, OutPrepared, BuildingCell.CellCoord, GridSize, CellGridGuid,
			bPartitioned, ActorsByCell, ActorSetIndices, SetIndex, OutError))
		{
			Discard(OutPrepared);
			return false;
		}
		OutPrepared.Actors[SetIndex].BuildingActors = MoveTemp(BuildingCell.Spawns);
	}

	if (!BuildRoadSegments(
		Town, *World, *PartitionSubsystem, *Snapshot, GridSize, CellGridGuid, bPartitioned,
		OutPrepared, ActorsByCell, ActorSetIndices, OutError))
	{
		Discard(OutPrepared);
		return false;
	}

	if (!EmitEntityInstances(
		Town, *World, *PartitionSubsystem, WorldPartition, bPartitioned, EntityGridSize, EntityBuckets,
		OutPrepared, OutError))
	{
		Discard(OutPrepared);
		CommitFailedBuildRecords(Town, OutPrepared);
		return false;
	}

	TSet<const AArcTownPartitionActor*> WrittenActors;
	OutPrepared.ActorRecords.Reset(OutPrepared.Actors.Num());
	for (const FArcTownPreparedActor& ActorSet : OutPrepared.Actors)
	{
		if (ActorSet.Actor == nullptr)
		{
			continue;
		}
		FArcTownEmittedActor& Record = OutPrepared.ActorRecords.AddDefaulted_GetRef();
		Record.Actor = ActorSet.Actor.Get();
		Record.ActorGuid = ActorSet.Actor->GetActorGuid();
		WrittenActors.Add(ActorSet.Actor.Get());
	}
	for (AArcTownPartitionActor* Previous : PreviousActors)
	{
		if (!WrittenActors.Contains(Previous))
		{
			OutPrepared.ActorsToDestroy.Add(TSoftObjectPtr<AActor>(Previous));
		}
	}

	OutPrepared.BuildingCount = PlacedBuildings.Num();
	OutPrepared.CellCount = OutPrepared.Actors.Num();
	return true;
}

void FArcTownInstanceEmitter::CommitEmittedActorChanges(
	AArcTownVolume& Town,
	FArcTownPreparedInstances& Prepared)
{
	using namespace ArcTownInstanceEmitterPrivate;
	check(IsInGameThread());

	Town.Modify();
	Town.EmittedActors = MoveTemp(Prepared.ActorRecords);
	Town.EmittedEntityHosts = MoveTemp(Prepared.EntityHostRecords);
	Prepared.ActorRecords.Reset();
	Prepared.EntityHostRecords.Reset();

	EmitPadStamps(Town, Prepared);
	Prepared.RoadSplineCount = EmitRoadModifier(Town, Prepared);
	Prepared.PadSpecs.Reset();
	Prepared.RoadSpec = FArcTownRoadModifierSpec();
	Prepared.PadTerrain.Reset();

	for (const TSoftObjectPtr<AActor>& SoftActor : Prepared.ActorsToDestroy)
	{
		DestroyEmittedActor(SoftActor.Get());
	}
	Prepared.ActorsToDestroy.Reset();

	TSet<const AArcPlacedEntityPartitionActor*> WrittenHosts;
	for (const FArcTownPreparedEntityWrite& Write : Prepared.EntityWrites)
	{
		WrittenHosts.Add(Write.Actor);
	}
	TArray<AArcPlacedEntityPartitionActor*> EmptiedHosts;
	for (AArcPlacedEntityPartitionActor* Host : Prepared.PreviousEntityHosts)
	{
		if (!IsValid(Host))
		{
			continue;
		}
		// Before the mutation: RemoveEntriesOwnedBy calls Modify only after it has already removed them.
		Host->Modify();
		if (Host->RemoveEntriesOwnedBy(Town.TownId) && !WrittenHosts.Contains(Host))
		{
			EmptiedHosts.Add(Host);
		}
	}
	Prepared.PreviousEntityHosts.Reset();

	for (FArcTownPreparedEntityWrite& Write : Prepared.EntityWrites)
	{
		if (!IsValid(Write.Actor))
		{
			continue;
		}
		Write.Actor->Modify();
		const int32 EntryIndex =
			Write.Actor->AddInstances(Write.Config, Town.TownId, Write.RelativeTransforms);
		ensureMsgf(
			EntryIndex != INDEX_NONE,
			TEXT("ArcTown could not append its prepared placed-entity instances of config '%s'."),
			*GetNameSafe(Write.Config));
	}
	Prepared.EntityWrites.Reset();

	for (AArcPlacedEntityPartitionActor* Host : EmptiedHosts)
	{
		DestroyEmittedActor(Host);
	}

	// Released only here: dropping a reference unregisters the actor it names, and every write to a
	// pinned actor - the adoption, the destroys above - has to happen while it is still loaded.
	Prepared.PinnedActors.Reset();
}

bool FArcTownInstanceEmitter::Adopt(
	AArcTownVolume& Town,
	FArcTownPreparedInstances& Prepared,
	FText& OutError)
{
	using namespace ArcTownInstanceEmitterPrivate;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_AdoptInstances);

	for (FArcTownPreparedActor& ActorSet : Prepared.Actors)
	{
		if (!IsValid(ActorSet.Actor))
		{
			SetError(OutError, FString::Printf(
				TEXT("ArcTown prepared instance set of town '%s' names a partition actor that no longer exists."),
				*Town.GetName()));
			Discard(Prepared);
			return false;
		}
		ActorSet.Actor->Modify();
		if (!ActorSet.Actor->AdoptPreparedInstanceComponents(
			MoveTemp(ActorSet.Components), MoveTemp(ActorSet.FinalNames), OutError))
		{
			Discard(Prepared);
			return false;
		}
		if (!ActorSet.Actor->AdoptPreparedRoadComponents(
			MoveTemp(ActorSet.RoadComponents), MoveTemp(ActorSet.RoadFinalNames), OutError))
		{
			Discard(Prepared);
			return false;
		}
		if (!ActorSet.Actor->AdoptPreparedRoadNodeComponents(
			MoveTemp(ActorSet.RoadNodeComponents), MoveTemp(ActorSet.RoadNodeFinalNames), OutError))
		{
			Discard(Prepared);
			return false;
		}
		if (!SpawnAndAdoptBuildingActors(*ActorSet.Actor, ActorSet.BuildingActors, Prepared, OutError))
		{
			Discard(Prepared);
			return false;
		}
	}

	Prepared.Actors.Reset();
	Prepared.CreatedActors.Reset();
	Prepared.SpawnedBuildingActors.Reset();
	return true;
}

void FArcTownInstanceEmitter::Discard(FArcTownPreparedInstances& Prepared)
{
	using namespace ArcTownInstanceEmitterPrivate;
	for (FArcTownPreparedActor& ActorSet : Prepared.Actors)
	{
		for (UInstancedStaticMeshComponent* Component : ActorSet.Components)
		{
			DestroyCandidate(Component, TEXT("ArcTownDiscardedISM"));
		}
		for (USplineMeshComponent* Component : ActorSet.RoadComponents)
		{
			DestroyCandidate(Component, TEXT("ArcTownDiscardedRoadSeg"));
		}
		for (UStaticMeshComponent* Component : ActorSet.RoadNodeComponents)
		{
			DestroyCandidate(Component, TEXT("ArcTownDiscardedRoadNode"));
		}
	}
	for (const TObjectPtr<AActor>& Spawned : Prepared.SpawnedBuildingActors)
	{
		DestroyEmittedActor(Spawned.Get());
	}
	Prepared.SpawnedBuildingActors.Reset();
	for (const TObjectPtr<AActor>& Created : Prepared.CreatedActors)
	{
		DestroyEmittedActor(Created.Get());
	}
	Prepared.CreatedActors.Reset();
	Prepared.Actors.Reset();
	Prepared.ActorsToDestroy.Reset();
	Prepared.ActorRecords.Reset();
	Prepared.EntityHostRecords.Reset();
	Prepared.PreviousEntityHosts.Reset();
	Prepared.EntityWrites.Reset();
	Prepared.PinnedActors.Reset();
	// Nothing to undo: stamps are only ever created once the adoption has already succeeded.
	Prepared.PadSpecs.Reset();
	Prepared.RoadSpec = FArcTownRoadModifierSpec();
	Prepared.PadTerrain.Reset();
	Prepared.BuildingCount = 0;
	Prepared.CellCount = 0;
	Prepared.RoadSplineCount = 0;
	Prepared.Junctions.Reset();
}
