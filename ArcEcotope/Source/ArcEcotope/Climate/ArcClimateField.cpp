// Copyright Lukasz Baran. All Rights Reserved.

#include "Climate/ArcClimateField.h"

#include "Climate/ArcClimateVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineDefines.h"
#include "EngineUtils.h"

namespace ArcClimateFieldFileLocal
{
	constexpr double UnboundedZCm = HALF_WORLD_MAX;

	struct FGatheredVolume
	{
		AArcClimateVolume* Volume = nullptr;
		double Priority = 0.0;
		FString PathName;
	};
}

namespace UE::ArcEcotope::Climate
{
	FBox ComputeContributionBounds(const AArcClimateVolume& Volume)
	{
		if (Volume.VolumeBounds == nullptr)
		{
			return FBox(ForceInit);
		}
		const FVector Extent = Volume.VolumeBounds->GetUnscaledBoxExtent();
		if (Extent.ContainsNaN() || Extent.X <= 0.0 || Extent.Y <= 0.0)
		{
			return FBox(ForceInit);
		}
		const double Band = FMath::IsFinite(Volume.GradientBandCm)
			? FMath::Max(Volume.GradientBandCm, 0.0)
			: 0.0;
		const FVector Inflated(Extent.X + Band, Extent.Y + Band, FMath::Max(Extent.Z, 0.0));
		FBox WorldBounds = FBox(-Inflated, Inflated).TransformBy(Volume.GetActorTransform());
		WorldBounds.Min.Z = -ArcClimateFieldFileLocal::UnboundedZCm;
		WorldBounds.Max.Z = ArcClimateFieldFileLocal::UnboundedZCm;
		return WorldBounds;
	}

	void GatherSortedVolumes(const UWorld& World, const FBox& QueryWorldBounds, TArray<AArcClimateVolume*>& OutSorted)
	{
		OutSorted.Reset();
		if (!QueryWorldBounds.IsValid)
		{
			return;
		}
		TArray<ArcClimateFieldFileLocal::FGatheredVolume> Gathered;
		for (TActorIterator<AArcClimateVolume> VolumeIt(&World); VolumeIt; ++VolumeIt)
		{
			AArcClimateVolume* Volume = *VolumeIt;
			if (!IsValid(Volume) || !Volume->bContributesClimate)
			{
				continue;
			}
			const FBox Bounds = ComputeContributionBounds(*Volume);
			if (!Bounds.IsValid || !Bounds.Intersect(QueryWorldBounds))
			{
				continue;
			}
			ArcClimateFieldFileLocal::FGatheredVolume& Entry = Gathered.AddDefaulted_GetRef();
			Entry.Volume = Volume;
			Entry.Priority = FMath::IsFinite(Volume->ClimatePriority) ? Volume->ClimatePriority : 0.0;
			Entry.PathName = Volume->GetPathName();
		}
		Gathered.Sort([](const ArcClimateFieldFileLocal::FGatheredVolume& Left,
			const ArcClimateFieldFileLocal::FGatheredVolume& Right) -> bool
		{
			return Left.Priority != Right.Priority ? Left.Priority < Right.Priority : Left.PathName < Right.PathName;
		});
		OutSorted.Reserve(Gathered.Num());
		for (const ArcClimateFieldFileLocal::FGatheredVolume& Entry : Gathered)
		{
			OutSorted.Add(Entry.Volume);
		}
	}

	double EvaluateMask(const AArcClimateVolume& Volume, const FVector& WorldPosition)
	{
		if (Volume.VolumeBounds == nullptr)
		{
			return 0.0;
		}
		const FVector Local = Volume.GetActorTransform().InverseTransformPosition(WorldPosition);
		const FVector Half = Volume.VolumeBounds->GetUnscaledBoxExtent();
		if (Local.ContainsNaN() || Half.ContainsNaN())
		{
			return 0.0;
		}
		const double Outside = FMath::Max(FMath::Abs(Local.X) - Half.X, FMath::Abs(Local.Y) - Half.Y);
		if (Outside <= 0.0)
		{
			return 1.0;
		}
		if (!(Volume.GradientBandCm > 0.0))
		{
			return 0.0;
		}
		const double Ramp = FMath::Clamp(1.0 - Outside / Volume.GradientBandCm, 0.0, 1.0);
		return FMath::Pow(Ramp, FMath::Max(Volume.GradientSharpness, 1.0e-4));
	}

	FArcClimateAnalyticSample EvaluateAt(
		const TArray<AArcClimateVolume*>& SortedVolumes,
		const AArcClimateVolume* BakingVolume,
		const double SeedReferenceElevationM,
		const FVector& WorldPosition)
	{
		FArcClimateAnalyticSample Accum;
		Accum.ReferenceElevationM = FMath::IsFinite(SeedReferenceElevationM) ? SeedReferenceElevationM : 0.0;
		bool bFoundBakingVolume = false;
		for (const AArcClimateVolume* Volume : SortedVolumes)
		{
			if (Volume == nullptr)
			{
				continue;
			}
			bFoundBakingVolume = bFoundBakingVolume || Volume == BakingVolume;
			const double Mask = EvaluateMask(*Volume, WorldPosition);
			if (Mask <= 0.0)
			{
				continue;
			}
			if (FMath::IsFinite(Volume->Temperature))
			{
				Accum.Temperature = FMath::Lerp(Accum.Temperature, Volume->Temperature, Mask);
			}
			if (FMath::IsFinite(Volume->Humidity))
			{
				Accum.Humidity = FMath::Lerp(Accum.Humidity, Volume->Humidity, Mask);
			}
			if (FMath::IsFinite(Volume->ReferenceElevationM))
			{
				Accum.ReferenceElevationM = FMath::Lerp(
					Accum.ReferenceElevationM, Volume->ReferenceElevationM, Mask);
			}
		}
		ensureMsgf(BakingVolume == nullptr || bFoundBakingVolume,
			TEXT("The baking climate volume is absent from its gathered contributor set."));
		return Accum;
	}
}
