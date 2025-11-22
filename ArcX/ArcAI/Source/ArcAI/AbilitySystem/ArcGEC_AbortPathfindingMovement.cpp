// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGEC_AbortPathfindingMovement.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

bool UArcGEC_AbortPathfindingMovement::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer
																   , FActiveGameplayEffect& ActiveGE) const
{
	if (APawn* P = Cast<APawn>(ActiveGEContainer.Owner->GetAvatarActor()))
	{
		if (AAIController* AI =P->GetController<AAIController>())
		{
			AI->GetPathFollowingComponent()->AbortMove(*P, FPathFollowingResultFlags::MovementStop);
		}
	}
	return true;
}
