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



#pragma once
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"

#include "ArcActiveGameplayEffectForRPC.generated.h"

struct FActiveGameplayEffect;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcActiveGameplayEffectForRPC
{
	GENERATED_BODY()

	FArcActiveGameplayEffectForRPC() = default;
	FArcActiveGameplayEffectForRPC(const FActiveGameplayEffect& Other);

	float GetTimeRemaining(float WorldTime) const
	{
		float Duration = GetDuration();		
		return (Duration == FGameplayEffectConstants::INFINITE_DURATION ? -1.f : Duration - (WorldTime - StartWorldTime));
	}
	
	float GetDuration() const
	{
		return Duration;
	}

	float GetPeriod() const
	{
		return Period;
	}

	float GetEndTime() const
	{
		float Duration = GetDuration();		
		return (Duration == FGameplayEffectConstants::INFINITE_DURATION ? -1.f : Duration + StartWorldTime);
	}
	// ---------------------------------------------------------------------------------------------------------------------------------

	/** Globally unique ID for identify this active gameplay effect. Can be used to look up owner. Not networked. */
	UPROPERTY()
	FActiveGameplayEffectHandle Handle;

	UPROPERTY()
	float Duration = -1;

	UPROPERTY()
	float Period = -1;

	/** Server time this started */
	UPROPERTY()
	float StartServerWorldTime = 0.0f;
	
	UPROPERTY()
	FGameplayEffectSpecForRPC Spec;

	/** Used for handling duration modifications being replicated */
	UPROPERTY(NotReplicated)
	float CachedStartServerWorldTime = 0.0f;

	UPROPERTY(NotReplicated)
	float StartWorldTime = 0.0f;
};