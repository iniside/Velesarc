#include "Nodes/ArcTargetingTrace_Line.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace ArcTargetingTraceLinePrivate
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

void FArcTargetingTrace_Line::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingTraceLinePrivate;

	UWorld* World = WorldFromContext(Ctx);
	if (World == nullptr)
	{
		return;
	}

	const FVector Start = Ctx.SourceLocation;
	const FVector End = Start + Ctx.SourceDirection.GetSafeNormal() * MaxDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTargetingLineTrace), bTraceComplex);
	if (const AActor* SourceActor = Ctx.SourceActor.Get())
	{
		Params.AddIgnoredActor(SourceActor);
	}

	FCollisionObjectQueryParams ObjectParams;
	if (FilterMode == EArcTargetingTraceFilterMode::ObjectType)
	{
		if (ObjectTypes.Num() == 0)
		{
			return;
		}
		for (TEnumAsByte<EObjectTypeQuery> ObjectType : ObjectTypes)
		{
			ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
		}
	}
	else if (FilterMode == EArcTargetingTraceFilterMode::Profile)
	{
		if (ProfileName == NAME_None)
		{
			return;
		}
	}

	if (bMultiHit)
	{
		TArray<FHitResult> Hits;
		bool bAnyHit = false;
		switch (FilterMode)
		{
			case EArcTargetingTraceFilterMode::Channel:
				bAnyHit = World->LineTraceMultiByChannel(Hits, Start, End, Channel, Params);
				break;
			case EArcTargetingTraceFilterMode::ObjectType:
				bAnyHit = World->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, Params);
				break;
			case EArcTargetingTraceFilterMode::Profile:
				bAnyHit = World->LineTraceMultiByProfile(Hits, Start, End, ProfileName, Params);
				break;
		}
		if (bAnyHit)
		{
			Candidates.Reserve(Candidates.Num() + Hits.Num());
			for (const FHitResult& Hit : Hits)
			{
				AppendHit(Candidates, Hit, CandidatePriority);
			}
		}
	}
	else
	{
		FHitResult Hit;
		bool bHit = false;
		switch (FilterMode)
		{
			case EArcTargetingTraceFilterMode::Channel:
				bHit = World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params);
				break;
			case EArcTargetingTraceFilterMode::ObjectType:
				bHit = World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params);
				break;
			case EArcTargetingTraceFilterMode::Profile:
				bHit = World->LineTraceSingleByProfile(Hit, Start, End, ProfileName, Params);
				break;
		}
		if (bHit)
		{
			AppendHit(Candidates, Hit, CandidatePriority);
		}
	}
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingTrace_Line::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	const FVector End = Ctx.SourceLocation + Ctx.SourceDirection.GetSafeNormal() * MaxDistance;

	const int32 NewCount = CandidatesAfter.Num() - CandidatesBefore.Num();
	const bool bHit = NewCount > 0;
	const FColor Color = (DebugColor == FColor()) ? (bHit ? FColor::Green : FColor::Red) : DebugColor;

	UE::DebugDraw::Arrow(Scene, Ctx.SourceLocation, End, 12.f, Color, 1.f);

	for (int32 Index = CandidatesBefore.Num(); Index < CandidatesAfter.Num(); ++Index)
	{
		UE::DebugDraw::Sphere(Scene, CandidatesAfter[Index].HitResult.ImpactPoint, 8.f, 12, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
