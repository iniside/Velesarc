// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Aggregation/ArcPolicy_ConditionalHighestWins.h"

namespace ArcPolicyConditionalHighestWinsPrivate
{
	bool IsBonus(EArcModifierOp Op, float Magnitude)
	{
		switch (Op)
		{
		case EArcModifierOp::MultiplyCompound:
			return Magnitude > 1.f;
		case EArcModifierOp::DivideAdditive:
			return Magnitude < 0.f;
		default:
			return Magnitude > 0.f;
		}
	}

	float GetBonusStrength(EArcModifierOp Op, float Magnitude)
	{
		return Op == EArcModifierOp::DivideAdditive ? -Magnitude : Magnitude;
	}
}

float FArcPolicy_ConditionalHighestWins::Aggregate(
	const FArcAggregationContext& Context,
	const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
) const
{
	if (ConditionalChannels.IsEmpty())
	{
		return EvaluateDefault(Context.BaseValue, Mods);
	}

	TArray<const FArcAggregatorMod*, TInlineAllocator<8>> Excluded;
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		const EArcModifierOp Op = static_cast<EArcModifierOp>(OpIndex);
		if (Op == EArcModifierOp::Override)
		{
			continue;
		}

		for (const uint8 Channel : ConditionalChannels)
		{
			const FArcAggregatorMod* Best = nullptr;
			for (const FArcAggregatorMod& Mod : Mods[OpIndex])
			{
				if (Mod.Channel != Channel || !Mod.bQualified || !ArcPolicyConditionalHighestWinsPrivate::IsBonus(Op, Mod.Magnitude))
				{
					continue;
				}

				if (!Best)
				{
					Best = &Mod;
				}
				else if (ArcPolicyConditionalHighestWinsPrivate::GetBonusStrength(Op, Mod.Magnitude)
					> ArcPolicyConditionalHighestWinsPrivate::GetBonusStrength(Op, Best->Magnitude))
				{
					Excluded.Add(Best);
					Best = &Mod;
				}
				else
				{
					Excluded.Add(&Mod);
				}
			}
		}
	}

	return ArcAggregatorEval::EvaluateChannels(Context.BaseValue, Mods,
		[&Excluded](const FArcAggregatorMod& Mod) { return Mod.bQualified && !Excluded.Contains(&Mod); });
}
