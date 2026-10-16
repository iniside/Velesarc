// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Misc/Optional.h"
#include "Effects/ArcEffectContext.h"

class UArcEffectDefinition;

struct ARCMASSABILITIES_API FArcEffectApplyRequest
{
	FArcEffectContext Context;

	UArcEffectDefinition* Definition = nullptr;

	TOptional<float> DurationOverride;

	TMap<FGameplayTag, float> SetByCallerMagnitudes;
};

struct ARCMASSABILITIES_API FArcEffectPreApplicationResult
{
	FGameplayTag RejectReason;

	bool bVeto = false;

	void Veto(FGameplayTag Reason)
	{
		bVeto = true;
		RejectReason = Reason;
	}
};
