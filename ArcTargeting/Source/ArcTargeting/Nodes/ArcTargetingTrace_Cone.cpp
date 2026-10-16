#include "Nodes/ArcTargetingTrace_Cone.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ArcMass/ArcMassEntityLibrary.h"

namespace ArcTargetingTraceConePrivate
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

	void BuildBasis(const FVector& Axis, FVector& OutRight, FVector& OutUp)
	{
		const FVector Seed = (FMath::Abs(Axis.Z) < 0.9f) ? FVector::UpVector : FVector::ForwardVector;
		OutRight = FVector::CrossProduct(Axis, Seed).GetSafeNormal();
		OutUp = FVector::CrossProduct(OutRight, Axis).GetSafeNormal();
	}
}

void FArcTargetingTrace_Cone::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingTraceConePrivate;

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

	// Cone parameters
	const float HalfAngleRad = FMath::DegreesToRadians(HalfAngleDegrees);
	const float CosHalfAngle = FMath::Cos(HalfAngleRad);
	const float BaseRadius = Length * FMath::Tan(HalfAngleRad);

	// Sphere shape sweep along the cone axis; sphere radius = base radius of the cone.
	// The swept capsule fully contains the cone, and we filter results to the cone in code.
	const FVector TraceEnd = OriginLocation + TraceDirection * Length;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTargetingConeTrace), bTraceComplex);
	if (const AActor* SourceActor = Ctx.SourceActor.Get())
	{
		Params.AddIgnoredActor(SourceActor);
	}

	TArray<FHitResult> Hits;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(BaseRadius);
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

	// Cone filter: keep hits whose impact point lies inside the cone defined by
	// (apex = OriginLocation, axis = TraceDirection, length = Length, half-angle = HalfAngleDegrees).
	Candidates.Reserve(Candidates.Num() + Hits.Num());
	const float LengthSq = Length * Length;
	for (const FHitResult& Hit : Hits)
	{
		const FVector ImpactRel = Hit.ImpactPoint - OriginLocation;
		const float DistSq = ImpactRel.SizeSquared();
		if (DistSq > LengthSq)
		{
			continue;
		}
		if (DistSq < KINDA_SMALL_NUMBER)
		{
			// Apex contact — include.
			AppendHit(Candidates, Hit, CandidatePriority);
			continue;
		}
		const float Dist = FMath::Sqrt(DistSq);
		const float Dot = FVector::DotProduct(ImpactRel / Dist, TraceDirection);
		if (Dot < CosHalfAngle)
		{
			continue;
		}
		AppendHit(Candidates, Hit, CandidatePriority);
	}
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingTrace_Cone::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	using namespace ArcTargetingTraceConePrivate;

	FVector OriginLocation = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ArcTargeting::ResolveTraceOriginAndDirection(
			Ctx, CandidatesBefore, Origin, DirectionMode, ExplicitDirection,
			OriginLocation, TraceDirection))
	{
		return;
	}

	const FColor Color = (DebugColor == FColor()) ? FColor::Cyan : DebugColor;

	const float HalfAngleRad = FMath::DegreesToRadians(HalfAngleDegrees);
	const float BaseRadius = Length * FMath::Tan(HalfAngleRad);
	const FVector BaseCenter = OriginLocation + TraceDirection * Length;

	// Axis
	UE::DebugDraw::Line(Scene, OriginLocation, BaseCenter, Color);

	// Base ring + rim spokes
	FVector Right;
	FVector Up;
	BuildBasis(TraceDirection, Right, Up);

	constexpr int32 Segments = 24;
	FVector PrevRim = BaseCenter + Right * BaseRadius;
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / static_cast<float>(Segments)) * 2.f * PI;
		const FVector Rim = BaseCenter + (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * BaseRadius;
		UE::DebugDraw::Line(Scene, PrevRim, Rim, Color);
		PrevRim = Rim;
	}

	// Four spokes from apex to compass points on the rim
	UE::DebugDraw::Line(Scene, OriginLocation, BaseCenter + Right * BaseRadius, Color);
	UE::DebugDraw::Line(Scene, OriginLocation, BaseCenter - Right * BaseRadius, Color);
	UE::DebugDraw::Line(Scene, OriginLocation, BaseCenter + Up * BaseRadius, Color);
	UE::DebugDraw::Line(Scene, OriginLocation, BaseCenter - Up * BaseRadius, Color);

	for (int32 Index = CandidatesBefore.Num(); Index < CandidatesAfter.Num(); ++Index)
	{
		UE::DebugDraw::Sphere(Scene, CandidatesAfter[Index].HitResult.ImpactPoint, 12.f, 12, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
