// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectExecution.h"
#include "ArcDamageExecutionBase.generated.h"

USTRUCT(meta = (Hidden, ToolTip = "Base of executions that deal one hit of the effect's damage type; the damage type validator checks effects carrying one and reports its data warnings."))
struct ARCMASSMECHANICS_API FArcDamageExecutionBase : public FArcEffectExecution
{
	GENERATED_BODY()

#if WITH_EDITOR
	virtual void GatherDataWarnings(TArray<FText>& OutWarnings) const {}
#endif
};
