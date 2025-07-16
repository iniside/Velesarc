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

#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "ArcAbilityTask.generated.h"

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAbilityTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/*
	 * When InstanceName IsValid and it is not none, the task will be instance once
	 * for the duration of calling ability.
	 * Is single ability is using the same task more than once and wants to pool it, it
	 * required different InstanceNames.
	 */
	template <class T>
	static T* ArcNewAbilityTask(UGameplayAbility* ThisAbility
								, FName InstanceName = FName())
	{
		check(ThisAbility);
		UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ThisAbility->GetAbilitySystemComponentFromActorInfo());
		if (InstanceName != NAME_None)
		{
			UAbilityTask** Task = ASC->PooledTasks.FindOrAdd(ThisAbility).Find(InstanceName);
			if (Task)
			{
				return Cast<T>(*Task);
			}
		}
		
		T* MyObj = NewObject<T>();
		MyObj->InitTask(*ThisAbility, ThisAbility->GetGameplayTaskDefaultPriority());
		MyObj->InstanceName = InstanceName;

		if (InstanceName != NAME_None)
		{
			ASC->PooledTasks.FindOrAdd(ThisAbility).FindOrAdd(InstanceName) = MyObj;
		}

		return MyObj;
	}
};
