// Copyright Lukasz Baran. All Rights Reserved.

#include "Interaction/ArcTargetingFilter_Interactable.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "InteractableTargetInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityHandle.h"
#include "SmartObjectDefinition.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"

#if UE_DEBUG_RENDERING_ENABLED
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#endif

namespace ArcTargetingFilterInteractable_Internal
{
	static bool IsInteractableCandidate(const FArcTargetCandidate& Candidate, UWorld* World)
	{
		AActor* Actor = Candidate.HitResult.GetActor();
		if (Actor != nullptr)
		{
			if (Actor->Implements<UInteractionTarget>())
			{
				return true;
			}

			UPrimitiveComponent* HitComp = Cast<UPrimitiveComponent>(Candidate.HitResult.GetComponent());
			if (HitComp != nullptr && HitComp->Implements<UInteractionTarget>())
			{
				return true;
			}

			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (Comp != nullptr && Comp->Implements<UInteractionTarget>())
				{
					return true;
				}
			}
		}

		// Fallback: check if the candidate resolves to a Mass entity with a SmartObject definition.
		FMassEntityHandle EntityHandle = Candidate.Entity;
		if (!EntityHandle.IsValid())
		{
			EArcMassResult MassResult = EArcMassResult::NotValid;
			EntityHandle = UArcMassEntityLibrary::ResolveHitToEntity(Candidate.HitResult, MassResult);
			if (MassResult != EArcMassResult::Valid || !EntityHandle.IsValid())
			{
				return false;
			}
		}

		if (World == nullptr)
		{
			return false;
		}

		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (EntitySubsystem == nullptr)
		{
			return false;
		}

		const FMassEntityManager& EM = EntitySubsystem->GetEntityManager();
		const FArcSmartObjectDefinitionSharedFragment* SODefFragment = EM.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(EntityHandle);
		if (SODefFragment != nullptr && SODefFragment->SmartObjectDefinition != nullptr)
		{
			return true;
		}

		return false;
	}

#if UE_DEBUG_RENDERING_ENABLED
	static bool IsCandidateInArray(const FArcTargetCandidate& Candidate, TConstArrayView<FArcTargetCandidate> Array)
	{
		const AActor* HitActor = Candidate.HitResult.GetActor();
		const FVector ImpactPoint = Candidate.HitResult.ImpactPoint;

		for (const FArcTargetCandidate& Other : Array)
		{
			if (Other.HitResult.GetActor() == HitActor && Other.HitResult.ImpactPoint.Equals(ImpactPoint, 0.1f))
			{
				return true;
			}
		}
		return false;
	}
#endif
} // namespace ArcTargetingFilterInteractable_Internal

void FArcTargetingFilter_Interactable::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	UWorld* World = nullptr;
	if (Ctx.SourceActor.IsValid())
	{
		World = Ctx.SourceActor->GetWorld();
	}
	else if (Ctx.InstigatorActor.IsValid())
	{
		World = Ctx.InstigatorActor->GetWorld();
	}

	Candidates.RemoveAll([World](const FArcTargetCandidate& Candidate)
	{
		return !ArcTargetingFilterInteractable_Internal::IsInteractableCandidate(Candidate, World);
	});
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingFilter_Interactable::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	const FColor Color = (DebugColor == FColor()) ? FColor::Red : DebugColor;

	for (const FArcTargetCandidate& Before : CandidatesBefore)
	{
		if (!ArcTargetingFilterInteractable_Internal::IsCandidateInArray(Before, CandidatesAfter))
		{
			UE::DebugDraw::Box(Scene,
				Before.HitResult.ImpactPoint,
				FVector(15.f, 15.f, 15.f),
				FQuat::Identity,
				Color,
				1.5f);
		}
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
