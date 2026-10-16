#include "Nodes/ArcTargetingOverlap_Sphere.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingModule.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"

namespace ArcTargetingOverlapPrivate
{
	UWorld* WorldFromContext(const FArcTargetingContext& Ctx)
	{
		if (const AActor* Source = Ctx.SourceActor.Get())
		{
			return Source->GetWorld();
		}
		if (const UObject* Object = Ctx.SourceObject.Get())
		{
			return Object->GetWorld();
		}
		return nullptr;
	}
}

void FArcTargetingOverlap_Sphere::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingOverlapPrivate;

	UWorld* World = WorldFromContext(Ctx);
	if (World == nullptr)
	{
		return;
	}

	const bool bAroundSource = Center == EArcTargetingOverlapCenter::SourceLocation;
	if (!bAroundSource && Candidates.Num() == 0)
	{
		return;
	}
	const FVector OverlapCenter = bAroundSource ? Ctx.SourceLocation : Candidates[0].HitResult.ImpactPoint;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTargetingSphereOverlap), bTraceComplex);
	if (const AActor* SourceActor = Ctx.SourceActor.Get())
	{
		Params.AddIgnoredActor(SourceActor);
	}

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	const bool bAnyBlocking = World->OverlapMultiByChannel(Overlaps, OverlapCenter, FQuat::Identity, Channel, Shape, Params);
	if (bAroundSource)
	{
		UE_LOGF(LogArcTargeting, Verbose, "Overlap: Sphere around source: %d overlaps.", Overlaps.Num());
		if (Overlaps.Num() == 0)
		{
			return;
		}
	}
	else if (!bAnyBlocking)
	{
		return;
	}

	Candidates.Reserve(Candidates.Num() + Overlaps.Num());
	for (const FOverlapResult& Overlap : Overlaps)
	{
		FArcTargetCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.HitResult.HitObjectHandle = FActorInstanceHandle(Overlap.GetActor());
		Candidate.HitResult.Component = Overlap.GetComponent();
		Candidate.HitResult.Item = Overlap.ItemIndex;
		Candidate.HitResult.bBlockingHit = Overlap.bBlockingHit;
		Candidate.HitResult.PhysicsObject = Overlap.PhysicsObject;
		Candidate.HitResult.PhysicsObjectOwner = Overlap.PhysicsObjectOwner;
		if (bAroundSource)
		{
			const UPrimitiveComponent* Component = Overlap.GetComponent();
			const AActor* Actor = Overlap.GetActor();
			FVector HitLocation = OverlapCenter;
			if (Component != nullptr)
			{
				HitLocation = Component->GetComponentLocation();
			}
			else if (Actor != nullptr)
			{
				HitLocation = Actor->GetActorLocation();
			}
			else if (Overlap.PhysicsObject != nullptr)
			{
				HitLocation = ArcMassPhysicsEntityLink::ResolveHitToBodyTransform(Candidate.HitResult).GetLocation();
			}
			Candidate.HitResult.Location = HitLocation;
			Candidate.HitResult.ImpactPoint = HitLocation;
			Candidate.HitResult.TraceStart = OverlapCenter;
			Candidate.HitResult.TraceEnd = HitLocation;
		}
		else
		{
			Candidate.HitResult.Location = Candidates[0].HitResult.Location;
			Candidate.HitResult.ImpactPoint = Candidates[0].HitResult.ImpactPoint;
			Candidate.HitResult.TraceStart = Candidates[0].HitResult.ImpactPoint;
			Candidate.HitResult.TraceEnd = Candidates[0].HitResult.TraceEnd;
		}
		Candidate.Score = 0.f;
		EArcMassResult ResolveResult = EArcMassResult::NotValid;
		Candidate.Entity = UArcMassEntityLibrary::ResolveHitToEntity(Candidate.HitResult, ResolveResult);
	}
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingOverlap_Sphere::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	const bool bAroundSource = Center == EArcTargetingOverlapCenter::SourceLocation;
	if (!bAroundSource && CandidatesBefore.Num() == 0)
	{
		return;
	}

	const FColor Color = (DebugColor == FColor()) ? FColor::Green : DebugColor;
	const FVector OverlapCenter = bAroundSource ? Ctx.SourceLocation : CandidatesBefore[0].HitResult.ImpactPoint;

	UE::DebugDraw::Sphere(Scene, OverlapCenter, Radius, 24, Color);

	for (int32 Index = CandidatesBefore.Num(); Index < CandidatesAfter.Num(); ++Index)
	{
		UE::DebugDraw::Sphere(Scene, CandidatesAfter[Index].HitResult.ImpactPoint, 12.f, 12, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
