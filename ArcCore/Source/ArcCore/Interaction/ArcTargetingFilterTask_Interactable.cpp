// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingFilterTask_Interactable.h"

#include "InteractableTargetInterface.h"
#include "Components/PrimitiveComponent.h"

bool UArcTargetingFilterTask_Interactable::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const
{
	if (AActor* Actor = TargetData.HitResult.GetActor())
	{
		if (Actor->Implements<UInteractionTarget>())
		{
			return false;
		}

		if (UPrimitiveComponent* HitComp = TargetData.HitResult.GetComponent())
		{
			if (HitComp->Implements<UInteractionTarget>())
			{
				return false;
			}
		}

		// Search actor's components for any IInteractionTarget implementor
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->Implements<UInteractionTarget>())
			{
				return false;
			}
		}
	}

	return true;
}
