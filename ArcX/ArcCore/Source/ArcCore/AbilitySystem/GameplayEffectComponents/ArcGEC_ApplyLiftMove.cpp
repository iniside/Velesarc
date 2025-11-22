// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGEC_ApplyLiftMove.h"

#include "ArcLayeredMove_Lift.h"
#include "MassAgentComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ChaosMover/Backends/ChaosMoverBackend.h"
#include "Engine/World.h"

bool UArcGEC_ApplyLiftMove::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const
{
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ActiveGEContainer.Owner);
	if (!ArcASC)
	{
		return false;
	}

	if (ActiveGEContainer.IsNetAuthority())
	{
		ActiveGE.EventSet.OnEffectRemoved.AddUObject(this, &UArcGEC_ApplyLiftMove::OnActiveGameplayEffectRemoved);
	}

	UChaosMoverBackendComponent* Backend = ArcASC->GetAvatarActor()->FindComponentByClass<UChaosMoverBackendComponent>();
	if (!Backend)
	{
		return false;;
	}
	if (Backend)
	{
		UChaosMoverSimulation* Sim = Backend->GetSimulation();
		
		TSharedPtr<FArcLayeredMove_Lift> Move = MakeShared<FArcLayeredMove_Lift>(Settings);
		// convert to MS
		Move->DurationMs = ActiveGE.GetDuration() * 1000.f;
		Move->StartLocation = ArcASC->GetAvatarActor()->GetActorLocation();
		Move->TargetLocation = Move->StartLocation + FVector(0,0,300.0f); // hardcoded for now
		
		Sim->QueueLayeredMove(Move);
	}
	
	return true;
}

void UArcGEC_ApplyLiftMove::OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const
{
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(RemovalInfo.ActiveEffect->Handle.GetOwningAbilitySystemComponent());
	if (!ArcASC)
	{
		return;
	}
}