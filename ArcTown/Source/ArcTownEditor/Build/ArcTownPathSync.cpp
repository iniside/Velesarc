// Copyright Lukasz Baran. All Rights Reserved.

#include "Build/ArcTownPathSync.h"

#include "Components/SceneComponent.h"
#include "Layout/ArcTownLayout.h"
#include "Paths/ArcTownPathComponent.h"
#include "Simulation/ArcTownTensorField.h"
#include "Town/ArcTownSiteSnapshot.h"
#include "Town/ArcTownVolume.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownPathSync, Log, All);

namespace ArcTownPathSyncPrivate
{
namespace
{
	constexpr float MinHalfWidthCm = 1.0f;

	FVector TownLocalToWorld(const FArcTownSiteSnapshot& Site, const FVector2f& TownLocalXY, const double WorldZ)
	{
		const FVector2D TerrainLocalXY = ArcTownSiteFrame::TownLocalToTerrainLocalXY(
			Site, FVector2D(static_cast<double>(TownLocalXY.X), static_cast<double>(TownLocalXY.Y)));
		const FVector OnTerrainPlane =
			Site.TerrainToWorld.TransformPosition(FVector(TerrainLocalXY.X, TerrainLocalXY.Y, 0.0));
		return FVector(OnTerrainPlane.X, OnTerrainPlane.Y, WorldZ);
	}
}
}

int32 ArcTownPathSync::DestroyGeneratedPaths(AArcTownVolume& Town)
{
	check(IsInGameThread());

	TArray<UArcTownPathComponent*> Existing;
	Town.GetComponents<UArcTownPathComponent>(Existing);

	TArray<UArcTownPathComponent*> Generated;
	Generated.Reserve(Existing.Num());
	for (UArcTownPathComponent* Path : Existing)
	{
		// A path without the generated prefix is the authored override this rebuild promises to keep.
		if (IsValid(Path) && Path->GetName().StartsWith(ArcTownGeneratedPathPrefix, ESearchCase::CaseSensitive))
		{
			Generated.Add(Path);
		}
	}

	for (UArcTownPathComponent* Path : Generated)
	{
		const FName DiscardName =
			MakeUniqueObjectName(Path->GetOuter(), Path->GetClass(), FName(TEXT("ArcTownDiscardedGenPath")));
		Path->Rename(
			*DiscardName.ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		Path->DestroyComponent();
	}
	return Generated.Num();
}

void ArcTownPathSync::SyncGeneratedPaths(AArcTownVolume& Town, const TConstArrayView<FArcTownGeneratedPath> Paths)
{
	using namespace ArcTownPathSyncPrivate;
	check(IsInGameThread());

	Town.Modify();
	DestroyGeneratedPaths(Town);

	USceneComponent* const TownRoot = Town.GetRootComponent();
	const FArcTownSiteSnapshot& Site = Town.SiteSnapshot;
	const double RaiseCm = FMath::Max(Town.PathRaiseCm, 0.0);
	TArray<FVector> WorldPoints;
	for (int32 PathIndex = 0; PathIndex < Paths.Num(); ++PathIndex)
	{
		const FArcTownGeneratedPath& Path = Paths[PathIndex];
		if (Path.Points.Num() < 2 || Path.Points.Num() != Path.HalfWidth.Num())
		{
			continue;
		}

		// The terrain levels to the road bed these points carry: sampled ground plus the embankment raise.
		WorldPoints.Reset(Path.Points.Num());
		for (int32 PointIndex = 0; PointIndex < Path.Points.Num(); ++PointIndex)
		{
			const FVector OnPlane = TownLocalToWorld(Site, Path.Points[PointIndex], 0.0);
			FVector2D WorldXY(OnPlane.X, OnPlane.Y);
			if (!ArcTownSiteSampling::IsInsideSampledSite(Site, WorldXY))
			{
				ArcTownSiteSampling::ClampIntoSampledSite(Site, WorldXY);
			}
			double GroundZ = 0.0;
			if (!ArcTownSiteSampling::SampleGroundZ(Site, WorldXY, GroundZ))
			{
				UE_LOGF(LogArcTownPathSync, Warning,"ArcTown '%ls' dropped generated path %d: the sampled site holds no ground height at world (%.1f, %.1f).",
					*Town.GetName(), PathIndex, WorldXY.X, WorldXY.Y);
				WorldPoints.Reset();
				break;
			}
			WorldPoints.Add(FVector(WorldXY.X, WorldXY.Y, GroundZ + RaiseCm));
		}
		if (WorldPoints.IsEmpty())
		{
			continue;
		}

		const FName ComponentName(*FString::Printf(TEXT("%s%04d"), ArcTownGeneratedPathPrefix, PathIndex));
		UArcTownPathComponent* const Component =
			NewObject<UArcTownPathComponent>(&Town, ComponentName, RF_NoFlags);
		Component->ClearFlags(RF_Transactional);
		Component->CreationMethod = EComponentCreationMethod::Instance;
		Component->SetupAttachment(TownRoot);
		Town.AddInstanceComponent(Component);
		Component->RegisterComponent();

		Component->ClearSplinePoints(/*bUpdateSpline=*/false);
		Component->SetClosedLoop(Path.bClosed, /*bUpdateSpline=*/false);
		Component->Hierarchy = Path.Hierarchy;
		Component->StartNodeId = Path.StartNodeId;
		Component->EndNodeId = Path.EndNodeId;

		UArcTownPathMetadata* const Metadata = Component->GetPathMetadata();
		for (int32 PointIndex = 0; PointIndex < WorldPoints.Num(); ++PointIndex)
		{
			Component->AddSplinePoint(WorldPoints[PointIndex], ESplineCoordinateSpace::World, /*bUpdateSpline=*/false);
			if (Metadata != nullptr && !Metadata->HalfWidth.Points.IsEmpty())
			{
				Metadata->HalfWidth.Points.Last().OutVal =
					FMath::Max(Path.HalfWidth[PointIndex], MinHalfWidthCm);
			}
		}
		Component->UpdateSpline();
	}
}
