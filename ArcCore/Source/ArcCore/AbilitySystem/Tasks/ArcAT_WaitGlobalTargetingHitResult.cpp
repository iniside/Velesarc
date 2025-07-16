/**
 * This file is part of ArcX.
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



#include "ArcAT_WaitGlobalTargetingHitResult.h"

#include "AbilitySystemComponent.h"
#include "ArcWorldDelegates.h"
#include "Engine/World.h"

UArcAT_WaitGlobalTargetingHitResult* UArcAT_WaitGlobalTargetingHitResult::WaitGlobalTargetingHitResult(UGameplayAbility* OwningAbility
																									   , FName TaskInstanceName
																									   , FGameplayTag TargetingTag)
{
	UArcAT_WaitGlobalTargetingHitResult* MyObj = NewAbilityTask<UArcAT_WaitGlobalTargetingHitResult>(OwningAbility, TaskInstanceName);
	MyObj->TargetingTag = TargetingTag;

	return MyObj;
}

void UArcAT_WaitGlobalTargetingHitResult::Activate()
{
	Super::Activate();

	UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(AbilitySystemComponent->GetWorld());

	TargetingDelegateHandle = WorldDelegates->AddOnGlobalHitResultChangedMap(TargetingTag, FArcAnyGlobalHitResultChangedDelegate::FDelegate::CreateUObject(
		this, &UArcAT_WaitGlobalTargetingHitResult::HandleOnHitResultChanged));
}

void UArcAT_WaitGlobalTargetingHitResult::OnDestroy(bool bInOwnerFinished)
{
	if (TargetingDelegateHandle.IsValid())
	{
		UArcWorldDelegates* WorldDelegates = UArcWorldDelegates::Get(AbilitySystemComponent->GetWorld());
		WorldDelegates->RemoveOnGlobalHitResultChangedMap(TargetingTag, TargetingDelegateHandle);
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitGlobalTargetingHitResult::HandleOnHitResultChanged(const FHitResult& HitResult)
{
	OnHitResultChanged.Broadcast(HitResult);
}
