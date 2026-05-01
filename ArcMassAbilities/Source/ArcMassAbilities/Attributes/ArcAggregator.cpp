// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAggregator.h"

#include <atomic>

#include "GameplayEffectTypes.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

namespace ArcModifiers
{

namespace Private
{
	std::atomic<uint64> GHandleCounter{0};
}

FArcModifierHandle GenerateHandle()
{
	FArcModifierHandle Handle;
	Handle.Id = ++Private::GHandleCounter;
	return Handle;
}

} // namespace ArcModifiers

bool FArcAggregatorMod::Qualifies(const FGameplayTagContainer& SourceTags,
                                     const FGameplayTagContainer& TargetTags) const
{
	const bool bSourceMet = (!SourceTagReqs || SourceTagReqs->IsEmpty() || SourceTagReqs->RequirementsMet(SourceTags));
	const bool bTargetMet = (!TargetTagReqs || TargetTagReqs->IsEmpty() || TargetTagReqs->RequirementsMet(TargetTags));
	return bSourceMet && bTargetMet;
}

void FArcAggregatorMod::UpdateQualifies(const FGameplayTagContainer& SourceTags,
                                         const FGameplayTagContainer& TargetTags) const
{
	bQualified = Qualifies(SourceTags, TargetTags);
}

FArcModifierHandle FArcAggregator::AddMod(EArcModifierOp Op, float Magnitude,
                                           FMassEntityHandle Source,
                                           const FGameplayTagRequirements* SourceTagReqs,
                                           const FGameplayTagRequirements* TargetTagReqs,
                                           uint8 Channel)
{
	FArcModifierHandle Handle = ArcModifiers::GenerateHandle();

	const uint8 OpIndex = static_cast<uint8>(Op);
	check(OpIndex < static_cast<uint8>(EArcModifierOp::Max));

	FArcAggregatorMod& NewMod = Mods[OpIndex].AddDefaulted_GetRef();
	NewMod.Magnitude = Magnitude;
	NewMod.Source = Source;
	NewMod.SourceTagReqs = SourceTagReqs;
	NewMod.TargetTagReqs = TargetTagReqs;
	NewMod.Handle = Handle;
	NewMod.Channel = Channel;

	return Handle;
}

void FArcAggregator::AddMod(EArcModifierOp Op, float Magnitude,
                             FMassEntityHandle Source,
                             const FGameplayTagRequirements* SourceTagReqs,
                             const FGameplayTagRequirements* TargetTagReqs,
                             FArcModifierHandle ExistingHandle,
                             uint8 Channel)
{
	const uint8 OpIndex = static_cast<uint8>(Op);
	check(OpIndex < static_cast<uint8>(EArcModifierOp::Max));

	FArcAggregatorMod& NewMod = Mods[OpIndex].AddDefaulted_GetRef();
	NewMod.Magnitude = Magnitude;
	NewMod.Source = Source;
	NewMod.SourceTagReqs = SourceTagReqs;
	NewMod.TargetTagReqs = TargetTagReqs;
	NewMod.Handle = ExistingHandle;
	NewMod.Channel = Channel;
}

void FArcAggregator::RemoveMod(FArcModifierHandle Handle)
{
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		int32 FoundIndex = Mods[OpIndex].IndexOfByPredicate(
			[Handle](const FArcAggregatorMod& Mod) { return Mod.Handle == Handle; });

		if (FoundIndex != INDEX_NONE)
		{
			Mods[OpIndex].RemoveAtSwap(FoundIndex);
			return;
		}
	}
}

bool FArcAggregator::IsEmpty() const
{
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		if (Mods[OpIndex].Num() > 0)
		{
			return false;
		}
	}
	return true;
}

float FArcAggregationPolicy::Aggregate(
	const FArcAggregationContext& Context,
	const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
) const
{
	return EvaluateDefault(Context.BaseValue, Mods);
}

float FArcAggregationPolicy::EvaluateDefault(
	float BaseValue,
	const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
)
{
	// Early-out: last qualifying override wins
	const TArray<FArcAggregatorMod>& OverrideMods = Mods[static_cast<uint8>(EArcModifierOp::Override)];
	for (int32 i = OverrideMods.Num() - 1; i >= 0; --i)
	{
		if (OverrideMods[i].bQualified)
		{
			return OverrideMods[i].Magnitude;
		}
	}

	float SumAdd = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::Add)])
	{
		if (Mod.bQualified)
		{
			SumAdd += Mod.Magnitude;
		}
	}

	float SumMultiplyAdditive = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)])
	{
		if (Mod.bQualified)
		{
			SumMultiplyAdditive += Mod.Magnitude;
		}
	}

	float SumDivideAdditive = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::DivideAdditive)])
	{
		if (Mod.bQualified)
		{
			SumDivideAdditive += Mod.Magnitude;
		}
	}

	float ProductMultiplyCompound = 1.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)])
	{
		if (Mod.bQualified)
		{
			ProductMultiplyCompound *= Mod.Magnitude;
		}
	}

	float SumAddFinal = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::AddFinal)])
	{
		if (Mod.bQualified)
		{
			SumAddFinal += Mod.Magnitude;
		}
	}

	float Divisor = 1.f + SumDivideAdditive;
	float Result = (BaseValue + SumAdd) * (1.f + SumMultiplyAdditive);
	if (Divisor != 0.f)
	{
		Result /= Divisor;
	}
	Result *= ProductMultiplyCompound;
	Result += SumAddFinal;

	return Result;
}

void FArcAggregator::UpdateQualifications(const FGameplayTagContainer& TargetTags,
                                           FMassEntityManager& EntityManager) const
{
	static const FGameplayTagContainer EmptyTags;

	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		for (const FArcAggregatorMod& Mod : Mods[OpIndex])
		{
			const FGameplayTagContainer* SourceTags = &EmptyTags;

			if (Mod.Source.IsValid() && EntityManager.IsEntityValid(Mod.Source))
			{
				FMassEntityView SourceView(EntityManager, Mod.Source);
				const FArcOwnedTagsFragment* SourceTagsFrag = SourceView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
				if (SourceTagsFrag)
				{
					SourceTags = &SourceTagsFrag->Tags.GetTagContainer();
				}
			}

			Mod.UpdateQualifies(*SourceTags, TargetTags);
		}
	}
}

float FArcAggregator::Evaluate(const FArcAggregationContext& Context) const
{
	UpdateQualifications(Context.OwnerTags, Context.EntityManager);

	if (AggregationPolicy.IsValid())
	{
		const FArcAggregationPolicy* Policy = AggregationPolicy.GetPtr<FArcAggregationPolicy>();
		if (Policy)
		{
			return Policy->Aggregate(Context, Mods);
		}
	}

	return FArcAggregationPolicy::EvaluateDefault(Context.BaseValue, Mods);
}

float FArcAggregator::EvaluateWithTags(float BaseValue,
                                        const FGameplayTagContainer& SourceTags,
                                        const FGameplayTagContainer& TargetTags) const
{
	// Early-out: last qualifying override wins
	const TArray<FArcAggregatorMod>& OverrideMods = Mods[static_cast<uint8>(EArcModifierOp::Override)];
	for (int32 i = OverrideMods.Num() - 1; i >= 0; --i)
	{
		if (OverrideMods[i].Qualifies(SourceTags, TargetTags))
		{
			return OverrideMods[i].Magnitude;
		}
	}

	float SumAdd = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::Add)])
	{
		if (Mod.Qualifies(SourceTags, TargetTags))
		{
			SumAdd += Mod.Magnitude;
		}
	}

	float SumMultiplyAdditive = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)])
	{
		if (Mod.Qualifies(SourceTags, TargetTags))
		{
			SumMultiplyAdditive += Mod.Magnitude;
		}
	}

	float SumDivideAdditive = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::DivideAdditive)])
	{
		if (Mod.Qualifies(SourceTags, TargetTags))
		{
			SumDivideAdditive += Mod.Magnitude;
		}
	}

	float ProductMultiplyCompound = 1.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)])
	{
		if (Mod.Qualifies(SourceTags, TargetTags))
		{
			ProductMultiplyCompound *= Mod.Magnitude;
		}
	}

	float SumAddFinal = 0.f;
	for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::AddFinal)])
	{
		if (Mod.Qualifies(SourceTags, TargetTags))
		{
			SumAddFinal += Mod.Magnitude;
		}
	}

	float Divisor = 1.f + SumDivideAdditive;
	float Result = (BaseValue + SumAdd) * (1.f + SumMultiplyAdditive);
	if (Divisor != 0.f)
	{
		Result /= Divisor;
	}
	Result *= ProductMultiplyCompound;
	Result += SumAddFinal;

	return Result;
}
