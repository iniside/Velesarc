// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAggregator.h"

#include <atomic>

#include "Tags/ArcGameplayTagRequirements.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Effects/ArcMagnitudeCalculation.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

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
	const bool bSourceMet = (SourceTagReqs.IsEmpty() || SourceTagReqs.RequirementsMet(SourceTags));
	const bool bTargetMet = (TargetTagReqs.IsEmpty() || TargetTagReqs.RequirementsMet(TargetTags));
	return bSourceMet && bTargetMet;
}

void FArcAggregatorMod::UpdateQualifies(const FGameplayTagContainer& SourceTags,
                                         const FGameplayTagContainer& TargetTags) const
{
	bQualified = Qualifies(SourceTags, TargetTags);
}

FArcModifierHandle FArcAggregator::AddMod(EArcModifierOp Op, float Magnitude,
                                           FMassEntityHandle Source,
                                           const FArcGameplayTagRequirements* SourceTagReqs,
                                           const FArcGameplayTagRequirements* TargetTagReqs,
                                           uint8 Channel)
{
	FArcModifierHandle Handle = ArcModifiers::GenerateHandle();

	const uint8 OpIndex = static_cast<uint8>(Op);
	check(OpIndex < static_cast<uint8>(EArcModifierOp::Max));

	FArcAggregatorMod& NewMod = Mods[OpIndex].AddDefaulted_GetRef();
	NewMod.Magnitude = Magnitude;
	NewMod.Source = Source;
	// Nullable-pointer = "no reqs" interim API — copy when non-null, leave default-empty otherwise.
	if (SourceTagReqs) { NewMod.SourceTagReqs = *SourceTagReqs; }
	if (TargetTagReqs) { NewMod.TargetTagReqs = *TargetTagReqs; }
	NewMod.Handle = Handle;
	NewMod.Channel = Channel;

	return Handle;
}

void FArcAggregator::AddMod(EArcModifierOp Op, float Magnitude,
                             FMassEntityHandle Source,
                             const FArcGameplayTagRequirements* SourceTagReqs,
                             const FArcGameplayTagRequirements* TargetTagReqs,
                             FArcModifierHandle ExistingHandle,
                             uint8 Channel)
{
	const uint8 OpIndex = static_cast<uint8>(Op);
	check(OpIndex < static_cast<uint8>(EArcModifierOp::Max));

	FArcAggregatorMod& NewMod = Mods[OpIndex].AddDefaulted_GetRef();
	NewMod.Magnitude = Magnitude;
	NewMod.Source = Source;
	// Nullable-pointer = "no reqs" interim API — copy when non-null, leave default-empty otherwise.
	if (SourceTagReqs) { NewMod.SourceTagReqs = *SourceTagReqs; }
	if (TargetTagReqs) { NewMod.TargetTagReqs = *TargetTagReqs; }
	NewMod.Handle = ExistingHandle;
	NewMod.Channel = Channel;
}

FArcModifierHandle FArcAggregator::AddLiveMod(EArcModifierOp Op, const FConstSharedStruct& LiveMagnitude,
                                               int32 StackCount, float InitialMagnitude,
                                               FMassEntityHandle Source,
                                               const FArcGameplayTagRequirements* SourceTagReqs,
                                               const FArcGameplayTagRequirements* TargetTagReqs,
                                               uint8 Channel)
{
	FArcModifierHandle Handle = AddMod(Op, InitialMagnitude, Source, SourceTagReqs, TargetTagReqs, Channel);
	FArcAggregatorMod& NewMod = Mods[static_cast<uint8>(Op)].Last();
	NewMod.LiveMagnitude = LiveMagnitude;
	NewMod.StackCount = StackCount;
	return Handle;
}

bool FArcAggregator::RemoveMod(FArcModifierHandle Handle, FConstSharedStruct* OutLiveMagnitude)
{
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		int32 FoundIndex = Mods[OpIndex].IndexOfByPredicate(
			[Handle](const FArcAggregatorMod& Mod) -> bool { return Mod.Handle == Handle; });

		if (FoundIndex != INDEX_NONE)
		{
			if (OutLiveMagnitude)
			{
				*OutLiveMagnitude = Mods[OpIndex][FoundIndex].LiveMagnitude;
			}
			Mods[OpIndex].RemoveAtSwap(FoundIndex);
			return true;
		}
	}
	return false;
}

void FArcAggregator::RefreshLiveMagnitudes(const FArcAggregationContext& Context) const
{
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		const EArcModifierOp Op = static_cast<EArcModifierOp>(OpIndex);
		for (const FArcAggregatorMod& Mod : Mods[OpIndex])
		{
			const FArcLiveMagnitude* Live = Mod.LiveMagnitude.GetPtr<FArcLiveMagnitude>();
			if (Live)
			{
				Mod.Magnitude = ArcAggregatorEval::ScaleByStack(Op, Live->EvaluateLive(Context), Mod.StackCount);
			}
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

namespace ArcAggregatorEval
{

float ScaleByStack(EArcModifierOp Op, float Magnitude, int32 StackCount)
{
	switch (Op)
	{
	case EArcModifierOp::Add:
	case EArcModifierOp::AddFinal:
	case EArcModifierOp::MultiplyAdditive:
	case EArcModifierOp::DivideAdditive:
		return Magnitude * StackCount;
	case EArcModifierOp::MultiplyCompound:
		return FMath::Pow(Magnitude, static_cast<float>(StackCount));
	default:
		return Magnitude;
	}
}

/**
 * Channel-layered modifier evaluation.
 *
 * Channels evaluate in ascending order; each channel's full formula output (including
 * its AddFinal) becomes the next channel's base value — GAS-style layering
 * (FAggregatorModChannelContainer::EvaluateWithBase).
 *
 * Per channel:
 *  - Override: the LAST qualifying override on the channel wins outright; the channel's
 *    remaining ops are skipped and evaluation proceeds to the next channel with the
 *    override magnitude as the new base. NOTE: Arc keeps last-qualifying-wins —
 *    a deliberate divergence from GAS (which takes the first); tests pin this.
 *  - Otherwise: Value = (Value + SumAdd) * (1 + SumMulAdd) / (1 + SumDivAdd)
 *                        * ProdCompound + SumAddFinal,
 *    with a nearly-zero divisor treated as 1.
 *
 * All-channel-0 input reproduces the pre-channel formula exactly.
 * Complexity: O(usedChannels × mods); the common single-channel case matches the old cost.
 */
float EvaluateChannels(
	float BaseValue,
	const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)],
	TFunctionRef<bool(const FArcAggregatorMod&)> Qualifies)
{
	TArray<uint8, TInlineAllocator<4>> UsedChannels;
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		for (const FArcAggregatorMod& Mod : Mods[OpIndex])
		{
			UsedChannels.AddUnique(Mod.Channel);
		}
	}

	if (UsedChannels.IsEmpty())
	{
		return BaseValue;
	}

	UsedChannels.Sort();

	float Value = BaseValue;

	for (const uint8 Channel : UsedChannels)
	{
		// Override: last qualifying mod on this channel wins the whole channel.
		const TArray<FArcAggregatorMod>& OverrideMods = Mods[static_cast<uint8>(EArcModifierOp::Override)];
		bool bOverridden = false;
		for (int32 Index = OverrideMods.Num() - 1; Index >= 0; --Index)
		{
			const FArcAggregatorMod& Mod = OverrideMods[Index];
			if (Mod.Channel == Channel && Qualifies(Mod))
			{
				Value = Mod.Magnitude;
				bOverridden = true;
				break;
			}
		}
		if (bOverridden)
		{
			continue;
		}

		float SumAdd = 0.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::Add)])
		{
			if (Mod.Channel == Channel && Qualifies(Mod))
			{
				SumAdd += Mod.Magnitude;
			}
		}

		float SumMultiplyAdditive = 0.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)])
		{
			if (Mod.Channel == Channel && Qualifies(Mod))
			{
				SumMultiplyAdditive += Mod.Magnitude;
			}
		}

		float SumDivideAdditive = 0.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::DivideAdditive)])
		{
			if (Mod.Channel == Channel && Qualifies(Mod))
			{
				SumDivideAdditive += Mod.Magnitude;
			}
		}

		float ProductMultiplyCompound = 1.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)])
		{
			if (Mod.Channel == Channel && Qualifies(Mod))
			{
				ProductMultiplyCompound *= Mod.Magnitude;
			}
		}

		float SumAddFinal = 0.f;
		for (const FArcAggregatorMod& Mod : Mods[static_cast<uint8>(EArcModifierOp::AddFinal)])
		{
			if (Mod.Channel == Channel && Qualifies(Mod))
			{
				SumAddFinal += Mod.Magnitude;
			}
		}

		float Divisor = 1.f + SumDivideAdditive;
		if (FMath::IsNearlyZero(Divisor))
		{
			Divisor = 1.f;
		}

		Value = (Value + SumAdd) * (1.f + SumMultiplyAdditive) / Divisor * ProductMultiplyCompound + SumAddFinal;
	}

	return Value;
}

} // namespace ArcAggregatorEval

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
	return ArcAggregatorEval::EvaluateChannels(BaseValue, Mods,
		[](const FArcAggregatorMod& Mod) { return Mod.bQualified; });
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
				const FArcOwnedTagsFragment* SourceTagsFrag = SourceView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
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
	RefreshLiveMagnitudes(Context);
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

float FArcAggregator::EvaluateWithTags(const FArcAggregationContext& Context,
                                        const FGameplayTagContainer& SourceTags,
                                        const FGameplayTagContainer& TargetTags) const
{
	RefreshLiveMagnitudes(Context);

	const FArcAggregationPolicy* Policy = AggregationPolicy.IsValid() ? AggregationPolicy.GetPtr<FArcAggregationPolicy>() : nullptr;
	if (!Policy)
	{
		return ArcAggregatorEval::EvaluateChannels(Context.BaseValue, Mods,
			[&SourceTags, &TargetTags](const FArcAggregatorMod& Mod) -> bool { return Mod.Qualifies(SourceTags, TargetTags); });
	}

	TArray<FArcAggregatorMod> QualifiedMods[static_cast<uint8>(EArcModifierOp::Max)];
	for (uint8 OpIndex = 0; OpIndex < static_cast<uint8>(EArcModifierOp::Max); ++OpIndex)
	{
		for (const FArcAggregatorMod& Mod : Mods[OpIndex])
		{
			if (Mod.Qualifies(SourceTags, TargetTags))
			{
				FArcAggregatorMod& Qualified = QualifiedMods[OpIndex].Add_GetRef(Mod);
				Qualified.bQualified = true;
			}
		}
	}

	return Policy->Aggregate(Context, QualifiedMods);
}
