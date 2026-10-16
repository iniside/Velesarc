// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingFilter_Interactable.generated.h"

/**
 * Keeps only candidates that can be interacted with:
 *   1. The hit actor, the hit component, or any component on the actor implements IInteractionTarget; OR
 *   2. The candidate's Mass entity (Candidate.Entity, or — fallback — UArcMassEntityLibrary::ResolveHitToEntity)
 *      carries a FArcSmartObjectDefinitionSharedFragment with a valid SmartObjectDefinition.
 *
 * Everything else is dropped.
 */
USTRUCT(DisplayName = "Filter: Interactable")
struct ARCGAMEPLAYINTERACTION_API FArcTargetingFilter_Interactable : public FArcTargetingNode
{
	GENERATED_BODY()

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Filter; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
