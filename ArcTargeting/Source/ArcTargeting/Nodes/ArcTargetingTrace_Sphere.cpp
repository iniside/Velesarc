#include "Nodes/ArcTargetingTrace_Sphere.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ArcMass/ArcMassEntityLibrary.h"

namespace ArcTargetingTraceSpherePrivate
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

	void AppendHit(TArray<FArcTargetCandidate>& Candidates, const FHitResult& Hit, int32 CandidatePriority)
	{
		FArcTargetCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.HitResult = Hit;
		Candidate.Score = 0.f;
		Candidate.Priority = CandidatePriority;
		EArcMassResult ResolveResult = EArcMassResult::NotValid;
		Candidate.Entity = UArcMassEntityLibrary::ResolveHitToEntity(Hit, ResolveResult);
	}
}

void FArcTargetingTrace_Sphere::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingTraceSpherePrivate;

	UWorld* World = WorldFromContext(Ctx);
	if (World == nullptr)
	{
		return;
	}

	FVector OriginLocation = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ArcTargeting::ResolveTraceOriginAndDirection(
			Ctx, MakeArrayView(Candidates), Origin, DirectionMode, ExplicitDirection,
			OriginLocation, TraceDirection))
	{
		return;
	}

	const FVector TraceEnd = OriginLocation + TraceDirection * Length;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTargetingSphereTrace), bTraceComplex);
	if (const AActor* SourceActor = Ctx.SourceActor.Get())
	{
		Params.AddIgnoredActor(SourceActor);
	}

	TArray<FHitResult> Hits;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	switch (FilterMode)
	{
		case EArcTargetingTraceFilterMode::Channel:
		{
			World->SweepMultiByChannel(Hits, OriginLocation, TraceEnd, FQuat::Identity, Channel, Shape, Params);
			break;
		}
		case EArcTargetingTraceFilterMode::ObjectType:
		{
			if (ObjectTypes.Num() == 0)
			{
				return;
			}
			FCollisionObjectQueryParams ObjectParams;
			for (TEnumAsByte<EObjectTypeQuery> ObjectType : ObjectTypes)
			{
				ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
			}
			World->SweepMultiByObjectType(Hits, OriginLocation, TraceEnd, FQuat::Identity, ObjectParams, Shape, Params);
			break;
		}
		case EArcTargetingTraceFilterMode::Profile:
		{
			if (ProfileName == NAME_None)
			{
				return;
			}
			World->SweepMultiByProfile(Hits, OriginLocation, TraceEnd, FQuat::Identity, ProfileName, Shape, Params);
			break;
		}
	}

	Candidates.Reserve(Candidates.Num() + Hits.Num());
	for (const FHitResult& Hit : Hits)
	{
		AppendHit(Candidates, Hit, CandidatePriority);
	}
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingTrace_Sphere::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	// Mirror the Execute resolution exactly so settings like Origin / DirectionMode
	// are reflected in the debug visualisation. CandidatesBefore is the candidate
	// view Execute saw, which is what PreviousCandidate / TowardsFirstCandidate read.
	FVector OriginLocation = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ArcTargeting::ResolveTraceOriginAndDirection(
			Ctx, CandidatesBefore, Origin, DirectionMode, ExplicitDirection,
			OriginLocation, TraceDirection))
	{
		return;
	}

	const FColor Color = (DebugColor == FColor()) ? FColor::Cyan : DebugColor;
	const FVector TraceEnd = OriginLocation + TraceDirection * Length;

	UE::DebugDraw::Sphere(Scene, OriginLocation, Radius, 24, Color);
	UE::DebugDraw::Sphere(Scene, TraceEnd, Radius, 24, Color);
	UE::DebugDraw::Line(Scene, OriginLocation, TraceEnd, Color);

	for (int32 Index = CandidatesBefore.Num(); Index < CandidatesAfter.Num(); ++Index)
	{
		UE::DebugDraw::Sphere(Scene, CandidatesAfter[Index].HitResult.ImpactPoint, 12.f, 12, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
