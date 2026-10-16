// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeModificationMask.h"

#include "ArcEcotopeModule.h"
#include "Engine/World.h"
#include "EngineDefines.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace ArcEcotopeModificationMaskPrivate
{
	/** A contribution bound spans this on Z, because the cut never reads Z. */
	constexpr double UnboundedZCm = HALF_WORLD_MAX;

	struct FGatheredMask
	{
		UArcEcotopeModificationMask* Mask = nullptr;
		double Priority = 0.0;
		FString PathName;
	};

	double SanitizeBand(double Band)
	{
		return FMath::IsFinite(Band) ? FMath::Max(Band, 0.0) : 0.0;
	}

	double SanitizeOffset(double Offset)
	{
		return FMath::IsFinite(Offset) ? Offset : 0.0;
	}
}

UArcEcotopeModificationMask::UArcEcotopeModificationMask()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FBox UArcEcotopeModificationMask::ComputeContributionBounds() const
{
	const FVector2D Half = CoverageXY * 0.5;
	if (!FMath::IsFinite(Half.X) || !FMath::IsFinite(Half.Y) || Half.X <= 0.0 || Half.Y <= 0.0)
	{
		return FBox(ForceInit);
	}

	// The extreme of the cut's outer edge, 0.5 * Band - Offset, over both signs of the offset.
	const double Inflate = ArcEcotopeModificationMaskPrivate::SanitizeBand(GradientBandCm) * 0.5
		+ FMath::Abs(ArcEcotopeModificationMaskPrivate::SanitizeOffset(GradientOffsetCm));

	const FVector Inflated(Half.X + Inflate, Half.Y + Inflate, 0.0);

	// Transformed first so a rotation cannot smear an already-widened Z into XY.
	FBox WorldBounds = FBox(-Inflated, Inflated).TransformBy(GetComponentTransform());
	WorldBounds.Min.Z = -ArcEcotopeModificationMaskPrivate::UnboundedZCm;
	WorldBounds.Max.Z = ArcEcotopeModificationMaskPrivate::UnboundedZCm;
	return WorldBounds;
}

double UArcEcotopeModificationMask::EvaluateCut(const FVector& WorldPosition) const
{
	if (bIsDisabled || !FMath::IsFinite(CutStrength))
	{
		return 0.0;
	}

	const FVector2D Half = CoverageXY * 0.5;
	const FVector Local = GetComponentTransform().InverseTransformPosition(WorldPosition);

	// FMath::Clamp of a NaN returns the upper bound, so a NaN here would read as a full cut.
	if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y)
		|| !FMath::IsFinite(Half.X) || !FMath::IsFinite(Half.Y)
		|| Half.X <= 0.0 || Half.Y <= 0.0)
	{
		return 0.0;
	}

	const double Strength = FMath::Clamp(CutStrength, 0.0, 1.0);
	const double Band = ArcEcotopeModificationMaskPrivate::SanitizeBand(GradientBandCm);
	const double Offset = ArcEcotopeModificationMaskPrivate::SanitizeOffset(GradientOffsetCm);

	// A Chebyshev measure: negative inside the rect, positive outside, so the contour has square corners.
	const double Signed = FMath::Max(FMath::Abs(Local.X) - Half.X, FMath::Abs(Local.Y) - Half.Y);
	const double Shifted = Signed + Offset;
	if (!(Band > 0.0))
	{
		return Shifted <= 0.0 ? Strength : 0.0;
	}

	const double Ramp = FMath::Clamp(0.5 - Shifted / Band, 0.0, 1.0);
	return Strength * FMath::SmoothStep(0.0, 1.0, Ramp);
}

void UArcEcotopeModificationMask::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Editor-authoring fixup only: the mask gather walks TActorIterator, which sees loaded levels alone,
	// so a spatially loaded owner would silently drop its cut out of a bake that still covers it.
	const UWorld* World = GetWorld();
	if (World != nullptr && World->WorldType == EWorldType::Editor && World->GetWorldPartition() != nullptr)
	{
		AActor* Owner = GetOwner();
		if (Owner != nullptr && Owner->GetIsSpatiallyLoaded() && Owner->CanChangeIsSpatiallyLoadedFlag())
		{
			Owner->Modify();
			Owner->SetIsSpatiallyLoaded(false);
			UE_LOGF(
				LogArcEcotope,
				Log,"Forced modification-mask owner '%ls' to always-loaded on partitioned world '%ls'; its cut must stay resident for every bake that reads it.",
				*Owner->GetName(),
				*World->GetName());
		}
	}
#endif // WITH_EDITOR
}

namespace UE::ArcEcotope
{
	void GatherSortedMasks(
		const UWorld& World,
		const FBox& QueryWorldBounds,
		TArray<UArcEcotopeModificationMask*>& OutSorted)
	{
		OutSorted.Reset();
		if (!QueryWorldBounds.IsValid)
		{
			return;
		}

		TArray<ArcEcotopeModificationMaskPrivate::FGatheredMask> Gathered;
		TArray<UArcEcotopeModificationMask*> OwnedMasks;
		for (TActorIterator<AActor> ActorIt(&World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			if (!IsValid(Actor))
			{
				continue;
			}
			Actor->GetComponents(OwnedMasks);
			for (UArcEcotopeModificationMask* Mask : OwnedMasks)
			{
				if (!IsValid(Mask) || Mask->bIsDisabled)
				{
					continue;
				}
				const FBox Bounds = Mask->ComputeContributionBounds();
				if (!Bounds.IsValid || !Bounds.Intersect(QueryWorldBounds))
				{
					continue;
				}
				ArcEcotopeModificationMaskPrivate::FGatheredMask& Entry = Gathered.AddDefaulted_GetRef();
				Entry.Mask = Mask;
				// A NaN key would make the predicate equivalent to every priority at once, which is not a
				// strict weak ordering and is undefined behaviour inside introsort.
				Entry.Priority = FMath::IsFinite(Mask->MaskPriority) ? Mask->MaskPriority : 0.0;
				Entry.PathName = Mask->GetPathName();
			}
		}

		Gathered.Sort([](
			const ArcEcotopeModificationMaskPrivate::FGatheredMask& A,
			const ArcEcotopeModificationMaskPrivate::FGatheredMask& B) -> bool
		{
			if (A.Priority != B.Priority)
			{
				return A.Priority < B.Priority;
			}
			return A.PathName < B.PathName;
		});

		OutSorted.Reserve(Gathered.Num());
		for (const ArcEcotopeModificationMaskPrivate::FGatheredMask& Entry : Gathered)
		{
			OutSorted.Add(Entry.Mask);
		}
	}

	double EvaluateCutAt(
		const TArray<UArcEcotopeModificationMask*>& SortedMasks,
		const FVector& WorldPosition)
	{
		double Cut = 0.0;
		for (UArcEcotopeModificationMask* Mask : SortedMasks)
		{
			if (Mask == nullptr)
			{
				continue;
			}
			// A NaN out of an override survives FMath::Max and reads as a full cut after any clamp, and
			// makes this fold order-dependent, so each contribution is sanitised before it folds.
			const double MaskCut = Mask->EvaluateCut(WorldPosition);
			if (!FMath::IsFinite(MaskCut))
			{
				continue;
			}
			Cut = FMath::Max(Cut, FMath::Clamp(MaskCut, 0.0, 1.0));
		}
		return Cut;
	}
}
