/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcAbilityAction_ApplyMoverModifier.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "MoverComponent.h"

void FArcAbilityAction_ApplyMoverModifier::Execute(FArcAbilityActionContext& Context)
{
	AActor* AvatarActor = Context.Ability->GetAvatarActorFromActorInfo();
	if (!AvatarActor) { return; }

	UMoverComponent* MoverComp = AvatarActor->FindComponentByClass<UMoverComponent>();
	if (!MoverComp) { return; }

	TSharedPtr<FArcMovementModifier_MaxVelocity> Modifier = MakeShared<FArcMovementModifier_MaxVelocity>();
	Modifier->MaxVelocity = MaxVelocity;
	Modifier->Gait = Gait;

	ModifierHandle = MoverComp->QueueMovementModifier(Modifier);
}

void FArcAbilityAction_ApplyMoverModifier::CancelLatent(FArcAbilityActionContext& Context)
{
	AActor* AvatarActor = Context.Ability->GetAvatarActorFromActorInfo();
	if (!AvatarActor) { return; }

	UMoverComponent* MoverComp = AvatarActor->FindComponentByClass<UMoverComponent>();
	if (!MoverComp) { return; }

	MoverComp->CancelModifierFromHandle(ModifierHandle);
}
