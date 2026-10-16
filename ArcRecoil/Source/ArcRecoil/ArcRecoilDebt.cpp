// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilDebt.h"

namespace ArcRecoilDebtPrivate
{
	float CreditAxis(const float Debt, const float PlayerDelta)
	{
		if (Debt == 0.0f || PlayerDelta == 0.0f || (Debt > 0.0f) == (PlayerDelta > 0.0f))
		{
			return Debt;
		}

		const float Credit = FMath::Min(FMath::Abs(PlayerDelta), FMath::Abs(Debt));

		return Debt > 0.0f ? Debt - Credit : Debt + Credit;
	}
}

// Only player input opposing the debt credits it, and never past zero.
void ArcRecoil::CreditPlayerCompensation(FVector2f& Debt, const FVector2f& PlayerDeltaDeg)
{
	Debt.X = ArcRecoilDebtPrivate::CreditAxis(Debt.X, PlayerDeltaDeg.X);
	Debt.Y = ArcRecoilDebtPrivate::CreditAxis(Debt.Y, PlayerDeltaDeg.Y);
}

FVector2f ArcRecoil::StepDebtRecovery(FVector2f& Debt, const float RecoveryRate, const float Dt)
{
	if (RecoveryRate <= 0.0f || Dt <= 0.0f)
	{
		return FVector2f::ZeroVector;
	}

	const float Alpha = 1.0f - FMath::Exp(-RecoveryRate * Dt);
	const FVector2f Step = Debt * Alpha;
	Debt -= Step;

	return Step;
}
