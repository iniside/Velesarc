#include "Nodes/ArcTargetingTraceCommon.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityFragments.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"

namespace ArcTargeting
{
	bool ResolveTraceOriginAndDirection(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> Candidates,
		EArcTargetingTraceOrigin OriginMode,
		EArcTargetingTraceDirection DirectionMode,
		const FVector& ExplicitDirection,
		FVector& OutOrigin,
		FVector& OutDirection)
	{
		FVector ResolvedOrigin = FVector::ZeroVector;
		switch (OriginMode)
		{
			case EArcTargetingTraceOrigin::ContextLocation:
			{
				ResolvedOrigin = Ctx.SourceLocation;
				break;
			}
			case EArcTargetingTraceOrigin::ContextSourceActor:
			{
				const AActor* Source = Ctx.SourceActor.Get();
				if (Source == nullptr)
				{
					return false;
				}
				if (Ctx.SourceSocketName != NAME_None)
				{
					const USceneComponent* Root = Source->GetRootComponent();
					if (Root != nullptr)
					{
						ResolvedOrigin = Root->GetSocketLocation(Ctx.SourceSocketName);
						break;
					}
				}
				ResolvedOrigin = Source->GetActorLocation();
				break;
			}
			case EArcTargetingTraceOrigin::ContextSourceEntity:
			{
				if (!Ctx.SourceEntity.IsValid())
				{
					return false;
				}
				const AActor* WorldOwner = Ctx.SourceActor.Get();
				const UObject* WorldObject = (WorldOwner != nullptr) ? static_cast<const UObject*>(WorldOwner) : Ctx.SourceObject.Get();
				const UWorld* World = (WorldObject != nullptr) ? WorldObject->GetWorld() : nullptr;
				if (World == nullptr)
				{
					return false;
				}
				const UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
				if (EntitySubsystem == nullptr)
				{
					return false;
				}
				const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
				const FMassEntityView EntityView(EntityManager, Ctx.SourceEntity);
				const FTransformFragment* TransformFrag = EntityView.GetFragmentDataPtr<FTransformFragment>();
				if (TransformFrag == nullptr)
				{
					return false;
				}
				ResolvedOrigin = TransformFrag->GetTransform().GetLocation();
				break;
			}
			case EArcTargetingTraceOrigin::PreviousCandidate:
			{
				if (Candidates.Num() == 0)
				{
					return false;
				}
				ResolvedOrigin = Candidates[0].HitResult.ImpactPoint;
				break;
			}
		}

		FVector ResolvedDirection = FVector::ForwardVector;
		switch (DirectionMode)
		{
			case EArcTargetingTraceDirection::ContextDirection:
			{
				ResolvedDirection = Ctx.SourceDirection;
				break;
			}
			case EArcTargetingTraceDirection::TowardsFirstCandidate:
			{
				if (Candidates.Num() == 0)
				{
					return false;
				}
				ResolvedDirection = (Candidates[0].HitResult.ImpactPoint - ResolvedOrigin).GetSafeNormal();
				if (ResolvedDirection.IsNearlyZero())
				{
					return false;
				}
				break;
			}
			case EArcTargetingTraceDirection::Explicit:
			{
				ResolvedDirection = ExplicitDirection;
				break;
			}
		}

		ResolvedDirection = ResolvedDirection.GetSafeNormal();
		if (ResolvedDirection.IsNearlyZero())
		{
			return false;
		}

		OutOrigin = ResolvedOrigin;
		OutDirection = ResolvedDirection;
		return true;
	}
}
