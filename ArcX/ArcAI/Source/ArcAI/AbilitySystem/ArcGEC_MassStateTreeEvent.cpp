// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGEC_MassStateTreeEvent.h"

#include "AbilitySystemComponent.h"
#include "ArcMassStateTreeFunctionLibrary.h"
#include "GameplayEffect.h"

bool UArcGEC_MassStateTreeEvent::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer
															 , FActiveGameplayEffect& ActiveGE) const
{
	UArcMassStateTreeFunctionLibrary::SendStateTreeEventToActorEntities(ActiveGEContainer.Owner, OnActiveEvent, { ActiveGEContainer.Owner->GetAvatarActor() });
	return true;
}

void UArcGEC_MassStateTreeEvent::OnGameplayEffectExecuted(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec
	, FPredictionKey& PredictionKey) const
{
	UArcMassStateTreeFunctionLibrary::SendStateTreeEventToActorEntities(ActiveGEContainer.Owner, OnExecutedEvent, { ActiveGEContainer.Owner->GetAvatarActor() });
}

void UArcGEC_MassStateTreeEvent::OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec
	, FPredictionKey& PredictionKey) const
{
	UArcMassStateTreeFunctionLibrary::SendStateTreeEventToActorEntities(ActiveGEContainer.Owner, OnAppliedEvent, { ActiveGEContainer.Owner->GetAvatarActor() });
}
