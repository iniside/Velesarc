// Copyright Lukasz Baran. All Rights Reserved.

#include "Build/ArcRiverWaterEmitter.h"

#include "ActorFactories/ActorFactory.h"
#include "ArcRiverGeneratorComponent.h"
#include "ArcRiverModule.h"
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Layers/LayersSubsystem.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "WaterBodyComponent.h"
#include "WaterBodyRiverActor.h"
#include "WaterMeshComponent.h"
#include "WaterSplineComponent.h"
#include "WaterSplineMetadata.h"
#include "WaterZoneActor.h"

#define LOCTEXT_NAMESPACE "ArcRiverWaterEmitter"

namespace ArcRiverWaterEmitterPrivate
{
	int32 ReadMaxTilesPerHalfExtent()
	{
		const IConsoleVariable* const Variable =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.WaterMesh.MaxDimensionInTiles"));
		return Variable != nullptr ? FMath::Max(1, Variable->GetInt()) : 256;
	}

	FName MakeScopedTag(const FGuid& GeneratorId)
	{
		return FName(*FString::Printf(TEXT("ArcRiverGenerated_%s"), *GeneratorId.ToString(EGuidFormats::Digits)));
	}

	int32 DestroyCollected(UWorld& World, const TArray<AActor*>& Doomed)
	{
		if (Doomed.IsEmpty())
		{
			return 0;
		}

		USelection* const ActorSelection = GEditor->GetSelectedActors();
		UTypedElementSelectionSet* const SelectionSet =
			ActorSelection != nullptr ? ActorSelection->GetElementSelectionSet() : nullptr;
		if (SelectionSet != nullptr)
		{
			TArray<FTypedElementHandle> Handles;
			Handles.Reserve(Doomed.Num());
			for (AActor* const River : Doomed)
			{
				FTypedElementHandle Handle =
					UEngineElementsLibrary::AcquireEditorActorElementHandle(River, /*bAllowCreate=*/false);
				if (Handle)
				{
					Handles.Add(MoveTemp(Handle));
				}
			}

			const FTypedElementSelectionOptions SelectionOptions = FTypedElementSelectionOptions()
				.SetAllowHidden(true)
				.SetAllowGroups(false)
				.SetWarnIfLocked(false)
				.SetChildElementInclusionMethod(ETypedElementChildInclusionMethod::Recursive);
			SelectionSet->DeselectElements(Handles, SelectionOptions);
		}

		if (ULayersSubsystem* const Layers = GEditor->GetEditorSubsystem<ULayersSubsystem>())
		{
			Layers->DisassociateActorsFromLayers(Doomed);
		}

		int32 DestroyedCount = 0;
		for (AActor* const River : Doomed)
		{
			if (World.EditorDestroyActor(River, /*bShouldModifyLevel=*/true))
			{
				++DestroyedCount;
			}
		}
		return DestroyedCount;
	}

	bool AuthorSpline(
		AWaterBodyRiver& River,
		const ArcRiverGeoreference::FArcRiverPolyline& Polyline,
		const int32 RiverIndex,
		FText& OutError)
	{
		UWaterSplineComponent* const Spline = River.GetWaterSpline();
		UWaterSplineMetadata* const Metadata = River.GetWaterSplineMetadata();
		if (Spline == nullptr || Metadata == nullptr)
		{
			OutError = FText::Format(
				LOCTEXT("NoSpline", "The water body spawned for river {0} carries no water spline or metadata."),
				FText::AsNumber(RiverIndex));
			return false;
		}

		const int32 NumPoints = Polyline.WorldPoints.Num();
		const FTransform ActorXform = River.GetActorTransform();
		TArray<FVector> LocalPoints;
		LocalPoints.Reserve(NumPoints);
		for (const FVector& WorldPoint : Polyline.WorldPoints)
		{
			LocalPoints.Add(ActorXform.InverseTransformPosition(WorldPoint));
		}

		Spline->SetSplinePoints(LocalPoints, ESplineCoordinateSpace::Local, /*bUpdateSpline=*/true);

		if (Metadata->RiverWidth.Points.Num() != NumPoints || Metadata->Depth.Points.Num() != NumPoints)
		{
			OutError = FText::Format(
				LOCTEXT("MetadataCountMismatch",
					"River {0} authored {1} spline points but its water spline metadata holds {2} width and {3} depth keys."),
				FText::AsNumber(RiverIndex),
				FText::AsNumber(NumPoints),
				FText::AsNumber(Metadata->RiverWidth.Points.Num()),
				FText::AsNumber(Metadata->Depth.Points.Num()));
			return false;
		}

		for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			// RiverWidth is the FULL width: every consumer halves it, and the header comment calling it
			// a distance from the centre is wrong.
			Metadata->RiverWidth.Points[PointIndex].OutVal = 2.0f * Polyline.HalfWidthCm[PointIndex];
			Metadata->Depth.Points[PointIndex].OutVal = Polyline.DepthCm[PointIndex];
		}

		for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			Spline->SetSplinePointType(PointIndex, ESplinePointType::CurveCustomTangent, /*bUpdateSpline=*/false);

			const FVector& Current = LocalPoints[PointIndex];
			const FVector IncomingDelta = (PointIndex > 0)
				? (Current - LocalPoints[PointIndex - 1])
				: ((PointIndex + 1 < NumPoints) ? (LocalPoints[PointIndex + 1] - Current) : FVector::ZeroVector);
			const FVector OutgoingDelta = (PointIndex + 1 < NumPoints)
				? (LocalPoints[PointIndex + 1] - Current)
				: IncomingDelta;

			const FVector IncomingXY(IncomingDelta.X, IncomingDelta.Y, 0.0);
			const FVector OutgoingXY(OutgoingDelta.X, OutgoingDelta.Y, 0.0);
			const double IncomingLength = IncomingXY.Size();
			const double OutgoingLength = OutgoingXY.Size();
			const FVector InDir = IncomingXY.GetSafeNormal();
			const FVector OutDir = OutgoingXY.GetSafeNormal();

			FVector Miter = InDir + OutDir;
			if (Miter.SizeSquared() <= KINDA_SMALL_NUMBER)
			{
				Miter = OutDir;
			}
			Miter = Miter.GetSafeNormal();

			const double Scale = FMath::Max(0.5 * (IncomingLength + OutgoingLength), static_cast<double>(KINDA_SMALL_NUMBER));
			const FVector Tangent = Miter * Scale;
			Spline->SetTangentsAtSplinePoint(
				PointIndex, Tangent, Tangent, ESplineCoordinateSpace::Local, /*bUpdateSpline=*/false);
			Spline->SetUpVectorAtSplinePoint(
				PointIndex, FVector::UpVector, ESplineCoordinateSpace::Local, /*bUpdateSpline=*/false);
		}
		Spline->UpdateSpline();

		// Required, not cosmetic: it pushes the authored width and depth into the spline point scale that
		// GenerateMeshes reads through GetScaleAtSplinePoint, and broadcasts the change that runs it.
		Spline->K2_SynchronizeAndBroadcastDataChange();
		return true;
	}
}

bool ArcRiverWaterEmitter::CreateOrUpdateWaterZone(
	UArcRiverGeneratorComponent& Generator,
	const UArcTerrainTexturePatchModifier& Modifier,
	FText& OutError)
{
	using namespace ArcRiverWaterEmitterPrivate;

	UWorld* const World = Generator.GetWorld();
	if (World == nullptr || World->GetCurrentLevel() == nullptr)
	{
		OutError = LOCTEXT("NoWorldForZone", "The river generator component has no editor world to place a water zone in.");
		return false;
	}
	if (GEditor == nullptr)
	{
		OutError = LOCTEXT("NoEditorForZone", "Water zones can only be placed from the editor.");
		return false;
	}

	const FTransform ModifierTransform = Modifier.GetComponentTransform();
	const FVector ModifierScale = ModifierTransform.GetScale3D();
	const FVector2D FootprintCm(
		Modifier.CoverageXY.X * FMath::Abs(ModifierScale.X),
		Modifier.CoverageXY.Y * FMath::Abs(ModifierScale.Y));
	if (!(FootprintCm.X > 0.0) || !(FootprintCm.Y > 0.0))
	{
		OutError = LOCTEXT("BadZoneFootprint",
			"The source modifier's CoverageXY folded with its world scale resolves a non-positive footprint, so there is "
			"nothing for a water zone to cover.");
		return false;
	}

	// The water mesh halves its tile size until the quad tree fits the platform cap, and over a footprint
	// this size that bias collapses the surface, so the tile size is derived from the cap up front.
	const int32 MaxTilesPerHalfExtent = ReadMaxTilesPerHalfExtent();
	const double LargestHalfExtentCm = FMath::Max(FootprintCm.X, FootprintCm.Y) * 0.5;
	const double DefaultTileSizeCm = static_cast<double>(GetDefault<UWaterMeshComponent>()->GetTileSize());
	const double TileSizeCm = FMath::Max(
		DefaultTileSizeCm,
		FMath::CeilToDouble(LargestHalfExtentCm / static_cast<double>(MaxTilesPerHalfExtent) / 100.0) * 100.0);

	AWaterZone* Zone = Generator.WaterZone.LoadSynchronous();
	if (Zone != nullptr && (!IsValid(Zone) || Zone->GetWorld() != World))
	{
		Zone = nullptr;
	}

	const FVector ZoneLocation = ModifierTransform.GetLocation();
	if (Zone == nullptr)
	{
		UActorFactory* const Factory = GEditor->FindActorFactoryForActorClass(AWaterZone::StaticClass());
		if (Factory == nullptr)
		{
			OutError = LOCTEXT("NoZoneFactory",
				"No actor factory is registered for AWaterZone; without it the placed zone would carry no far distance "
				"material or water info resolution.");
			return false;
		}

		FActorSpawnParameters SpawnInfo;
		AActor* const Spawned = Factory->CreateActor(World, World->GetCurrentLevel(), FTransform(ZoneLocation), SpawnInfo);
		Zone = Cast<AWaterZone>(Spawned);
		if (Zone == nullptr)
		{
			OutError = LOCTEXT("ZoneSpawnFailed", "The water zone actor factory produced no zone.");
			return false;
		}
	}
	else
	{
		Zone->Modify();
		Zone->SetActorLocation(ZoneLocation);
	}

	const FVector ZoneScale = Zone->GetActorScale3D();
	const double ZoneScaleX = FMath::Abs(ZoneScale.X);
	const double ZoneScaleY = FMath::Abs(ZoneScale.Y);
	if (!(ZoneScaleX > 0.0) || !(ZoneScaleY > 0.0))
	{
		OutError = FText::Format(
			LOCTEXT("DegenerateZoneScale",
				"Water zone '{0}' is scaled to zero on X or Y, so its extent cannot be fitted to the stamp footprint."),
			FText::FromString(Zone->GetActorNameOrLabel()));
		return false;
	}
	Zone->SetZoneExtent(FVector2D(FootprintCm.X / ZoneScaleX, FootprintCm.Y / ZoneScaleY));

	UWaterMeshComponent* const WaterMesh = Zone->GetWaterMeshComponent();
	if (WaterMesh == nullptr)
	{
		OutError = FText::Format(
			LOCTEXT("NoWaterMesh", "Water zone '{0}' carries no water mesh component."),
			FText::FromString(Zone->GetActorNameOrLabel()));
		return false;
	}
	WaterMesh->Modify();
	WaterMesh->SetTileSize(static_cast<float>(TileSizeCm));

	Zone->MarkForRebuild(EWaterZoneRebuildFlags::All, &Generator);

	Generator.Modify();
	Generator.WaterZone = Zone;

	const FIntPoint TilesPerHalfExtent(
		static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FootprintCm.X * 0.5 / TileSizeCm))),
		static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FootprintCm.Y * 0.5 / TileSizeCm))));

	UE_LOGF(LogArcRiver, Log,
		"ArcRiver water zone '%ls' centred on (%.0f, %.0f, %.0f) over a %.0f x %.0f cm footprint: tile size "
		"%.0f cm gives %d x %d tiles per half extent, against the r.Water.WaterMesh.MaxDimensionInTiles cap of %d.",
		*Zone->GetActorNameOrLabel(), ZoneLocation.X, ZoneLocation.Y, ZoneLocation.Z, FootprintCm.X, FootprintCm.Y,
		TileSizeCm, TilesPerHalfExtent.X, TilesPerHalfExtent.Y, MaxTilesPerHalfExtent);

	return true;
}

int32 ArcRiverWaterEmitter::DestroyWaterBodies(UArcRiverGeneratorComponent& Generator)
{
	UWorld* const World = Generator.GetWorld();
	if (World == nullptr || GEditor == nullptr)
	{
		return 0;
	}

	const FGuid GeneratorId = Generator.GetGeneratorId();
	if (!GeneratorId.IsValid())
	{
		return 0;
	}

	const FName ScopedTag = ArcRiverWaterEmitterPrivate::MakeScopedTag(GeneratorId);
	TArray<AActor*> Generated;
	for (TActorIterator<AWaterBodyRiver> It(World); It; ++It)
	{
		AWaterBodyRiver* const River = *It;
		if (IsValid(River) && River->Tags.Contains(ScopedTag))
		{
			Generated.Add(River);
		}
	}

	return ArcRiverWaterEmitterPrivate::DestroyCollected(*World, Generated);
}

int32 ArcRiverWaterEmitter::DestroyAllWaterBodies(UArcRiverGeneratorComponent& Generator)
{
	UWorld* const World = Generator.GetWorld();
	if (World == nullptr || GEditor == nullptr)
	{
		return 0;
	}

	TArray<AActor*> Rivers;
	for (TActorIterator<AWaterBodyRiver> It(World); It; ++It)
	{
		AWaterBodyRiver* const River = *It;
		if (IsValid(River))
		{
			Rivers.Add(River);
		}
	}

	return ArcRiverWaterEmitterPrivate::DestroyCollected(*World, Rivers);
}

bool ArcRiverWaterEmitter::SpawnWaterBodies(
	UArcRiverGeneratorComponent& Generator,
	TConstArrayView<ArcRiverGeoreference::FArcRiverPolyline> Polylines,
	TFunctionRef<bool()> ShouldContinue,
	int32& OutSpawnedCount,
	FText& OutError)
{
	using namespace ArcRiverWaterEmitterPrivate;

	OutSpawnedCount = 0;

	UWorld* const World = Generator.GetWorld();
	if (World == nullptr || World->GetCurrentLevel() == nullptr)
	{
		OutError = LOCTEXT("NoWorld", "The river generator component has no editor world to spawn water bodies into.");
		return false;
	}
	if (GEditor == nullptr)
	{
		OutError = LOCTEXT("NoEditor", "Water bodies can only be spawned from the editor.");
		return false;
	}

	// The factory is the only assignment of the water, static-mesh, HLOD, underwater post-process, water
	// info and river-to-lake/ocean transition materials, so a bare SpawnActor leaves the river unshaded.
	UActorFactory* const Factory = GEditor->FindActorFactoryForActorClass(AWaterBodyRiver::StaticClass());
	if (Factory == nullptr)
	{
		OutError = LOCTEXT("NoFactory",
			"No actor factory is registered for AWaterBodyRiver; without it the spawned rivers would carry no materials.");
		return false;
	}

	const FName ScopedTag = MakeScopedTag(Generator.GetOrCreateGeneratorId());

	DestroyWaterBodies(Generator);

	for (int32 RiverIndex = 0; RiverIndex < Polylines.Num(); ++RiverIndex)
	{
		if (!ShouldContinue())
		{
			OutError = LOCTEXT("Cancelled", "River generation was cancelled while spawning water bodies.");
			return false;
		}

		const ArcRiverGeoreference::FArcRiverPolyline& Polyline = Polylines[RiverIndex];
		const int32 NumPoints = Polyline.WorldPoints.Num();
		if (NumPoints < 2)
		{
			continue;
		}
		if (Polyline.HalfWidthCm.Num() != NumPoints || Polyline.DepthCm.Num() != NumPoints)
		{
			OutError = FText::Format(
				LOCTEXT("MalformedPolyline",
					"River {0} carries mismatched point, width or depth arrays and cannot be turned into a water body."),
				FText::AsNumber(RiverIndex));
			return false;
		}

		FActorSpawnParameters SpawnInfo;
		// Must run before the spline is authored: the factory's PostSpawnActor ends in an unconditional
		// 3-point ResetSpline (WaterBodyActorFactory.cpp:106) that would wipe the generated points.
		AActor* const Spawned = Factory->CreateActor(
			World, World->GetCurrentLevel(), FTransform(Polyline.WorldPoints[0]), SpawnInfo);
		AWaterBodyRiver* const River = Cast<AWaterBodyRiver>(Spawned);
		if (River == nullptr)
		{
			OutError = FText::Format(
				LOCTEXT("SpawnFailed", "The water body actor factory produced no river for chain {0}."),
				FText::AsNumber(RiverIndex));
			return false;
		}

		River->SetActorLabel(FString::Printf(TEXT("ArcRiver_%03d"), RiverIndex));
		River->Tags.Add(ArcRiver::GeneratedTag);
		River->Tags.Add(ScopedTag);

		UWaterBodyComponent* const WaterBodyComponent = River->GetWaterBodyComponent();
		if (WaterBodyComponent != nullptr)
		{
			// Governs landscape deformation and landscape-derived water depth; this project has no
			// ALandscape, so leaving it true only produces a Map Check "requires a Landscape" error.
			WaterBodyComponent->bAffectsLandscape = false;
			WaterBodyComponent->SetWaterZoneOverride(Generator.WaterZone);
		}

		if (!AuthorSpline(*River, Polyline, RiverIndex, OutError))
		{
			return false;
		}

		if (WaterBodyComponent != nullptr)
		{
			// After the spline, never before: enabling rebuilds the static mesh from the spline as it
			// stands, and before AuthorSpline that is still the factory's 3-point reset spline.
			WaterBodyComponent->SetWaterBodyStaticMeshEnabled(true);
			const TArray<UPrimitiveComponent*> RenderableComponents =
				WaterBodyComponent->GetStandardRenderableComponents();
			for (UPrimitiveComponent* const RenderableComponent : RenderableComponents)
			{
				if (USplineMeshComponent* const SplineMesh = Cast<USplineMeshComponent>(RenderableComponent))
				{
					SplineMesh->SetVisibility(false);
					SplineMesh->SetHiddenInGame(true);
				}
			}
		}
		++OutSpawnedCount;
	}

	UE_LOGF(LogArcRiver, Log,"ArcRiver spawned %d water body river(s).", OutSpawnedCount);
	return true;
}

#undef LOCTEXT_NAMESPACE
