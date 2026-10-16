// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/Abilities/ArcAbilityViewModel.h"

void UArcAbilityViewModel::SetHasAbility(bool InHasAbility)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasAbility, InHasAbility);
}

void UArcAbilityViewModel::SetIsActive(bool InIsActive)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsActive, InIsActive);
}

void UArcAbilityViewModel::SetCanActivate(bool InCanActivate)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanActivate, InCanActivate);
}

void UArcAbilityViewModel::SetCooldownRemaining(float InCooldownRemaining)
{
	UE_MVVM_SET_PROPERTY_VALUE(CooldownRemaining, InCooldownRemaining);
}

void UArcAbilityViewModel::SetCooldownDuration(float InCooldownDuration)
{
	UE_MVVM_SET_PROPERTY_VALUE(CooldownDuration, InCooldownDuration);
}

void UArcAbilityViewModel::SetCooldownPercent(float InCooldownPercent)
{
	UE_MVVM_SET_PROPERTY_VALUE(CooldownPercent, InCooldownPercent);
}

void UArcAbilityViewModel::SetInputTag(FGameplayTag InInputTag)
{
	UE_MVVM_SET_PROPERTY_VALUE(InputTag, InInputTag);
}

void UArcAbilityViewModel::SetAbilityTags(FGameplayTagContainer InAbilityTags)
{
	UE_MVVM_SET_PROPERTY_VALUE(AbilityTags, InAbilityTags);
}

void UArcAbilityViewModel::ApplySnapshot(const FArcAbilitySnapshot& Snapshot)
{
	SetHasAbility(Snapshot.bHasAbility);
	SetIsActive(Snapshot.bIsActive);
	SetCanActivate(Snapshot.bCanActivate);
	SetCooldownRemaining(Snapshot.CooldownRemaining);
	SetCooldownDuration(Snapshot.CooldownDuration);
	SetCooldownPercent(Snapshot.CooldownPercent);
	SetInputTag(Snapshot.InputTag);
	SetAbilityTags(Snapshot.AbilityTags);
}
