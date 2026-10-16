// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcRecoil
{
	ARCRECOIL_API void CreditPlayerCompensation(FVector2f& Debt, const FVector2f& PlayerDeltaDeg);

	ARCRECOIL_API FVector2f StepDebtRecovery(FVector2f& Debt, float RecoveryRate, float Dt);
}
