#pragma once

#include "CoreMinimal.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingTraceCommon.generated.h"

struct FArcTargetingContext;

UENUM()
enum class EArcTargetingTraceFilterMode : uint8
{
	Channel,
	ObjectType,
	Profile,
};

UENUM()
enum class EArcTargetingTraceOrigin : uint8
{
	/** Use Ctx.SourceLocation as the trace start. */
	ContextLocation,
	/** Use Ctx.SourceActor location (or socket location if SourceSocketName is set). */
	ContextSourceActor,
	/** Resolve FTransformFragment from Ctx.SourceEntity via UMassEntitySubsystem. */
	ContextSourceEntity,
	/** Use Candidates[0].HitResult.ImpactPoint as the trace start. */
	PreviousCandidate,
};

UENUM()
enum class EArcTargetingTraceDirection : uint8
{
	/** Use Ctx.SourceDirection. */
	ContextDirection,
	/** Direction from trace origin towards Candidates[0].HitResult.ImpactPoint. */
	TowardsFirstCandidate,
	/** Use the ExplicitDirection vector defined on this node. */
	Explicit,
};

namespace ArcTargeting
{
	/**
	 * Resolves the trace origin and direction the same way Execute does, so DrawDebug
	 * can mirror it without duplicating the switch-case ladders. Returns false when
	 * the configured mode cannot be satisfied (matching Execute's early-out cases),
	 * in which case OutOrigin / OutDirection are left unchanged.
	 */
	ARCTARGETING_API bool ResolveTraceOriginAndDirection(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> Candidates,
		EArcTargetingTraceOrigin OriginMode,
		EArcTargetingTraceDirection DirectionMode,
		const FVector& ExplicitDirection,
		FVector& OutOrigin,
		FVector& OutDirection);
}
