// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectComp_ChanceToApply.h"
#include "ArcMassAbilitiesLogs.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcGameEventTags.h"
#include "Mass/EntityManager.h"

void FArcEffectComp_ChanceToApply::OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
                                                    FArcEffectPreApplicationResult& Result) const
{
	const float Roll = FMath::FRand();
	const bool bPass = Roll < Chance;
	UWorld* World = Spec.Context.EntityManager ? Spec.Context.EntityManager->GetWorld() : nullptr;
	ARCMA_VLOG(World, Log,
		TEXT("[Effect] ChanceToApply rolled=%.2f threshold=%.2f %s"),
		Roll, Chance, bPass ? TEXT("Pass") : TEXT("Block"));
	if (!bPass)
	{
		Result.Veto(ArcGameEventTags::EffectRejectedChance);
	}
}
