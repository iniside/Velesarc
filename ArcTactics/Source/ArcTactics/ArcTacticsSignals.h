// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::ArcTactics::Signals
{
	const FName DecisionDue = FName(TEXT("ArcTacticsDecisionDue"));
	const FName TaskWakeup = FName(TEXT("ArcTacticsTaskWakeup"));
	const FName ActionWakeup = FName(TEXT("ArcTacticsActionWakeup"));
	const FName AbilityWakeup = FName(TEXT("ArcTacticsAbilityWakeup"));
}
