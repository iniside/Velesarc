// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Resources/ArcResourceTypes.h"

#include "Modifiers/ArcModifierFunctions.h"
#include "Tags/ArcTagCountContainer.h"

bool FArcResourceContext::ReadAttribute(const FArcAttributeRef& Attribute, float& OutValue) const
{
	const FArcAttribute* Resolved = ArcAttributes::ResolveAttribute(EntityManager, Entity, Attribute);
	if (!Resolved)
	{
		return false;
	}

	OutValue = Resolved->GetCurrentValue();
	return true;
}

bool FArcResourceContext::ReadUpperBound(float& OutValue) const
{
	if (!Resource.MaxAttribute.IsValid())
	{
		OutValue = Resource.Max;
		return true;
	}

	return ReadAttribute(Resource.MaxAttribute, OutValue);
}

float FArcResourceTarget::GetTarget(const FArcResourceContext& Context) const
{
	return Context.CurrentValue;
}

float FArcResourceTarget_Max::GetTarget(const FArcResourceContext& Context) const
{
	float UpperBound = 0.f;
	return Context.ReadUpperBound(UpperBound) ? UpperBound : Context.CurrentValue;
}

float FArcResourceTarget_Min::GetTarget(const FArcResourceContext& Context) const
{
	return Context.Resource.Min;
}

int32 ArcResourcePips::NetPips(float Regen, float Degen)
{
	return FMath::Clamp(FMath::FloorToInt32(Regen) - FMath::FloorToInt32(Degen), -Cap, Cap);
}

float FArcResourceRate::GetRate(const FArcResourceContext& Context) const
{
	return 0.f;
}

float FArcResourceRate_Constant::GetRate(const FArcResourceContext& Context) const
{
	return UnitsPerSecond;
}

float FArcResourceRate_Pips::GetRate(const FArcResourceContext& Context) const
{
	float Regen = 0.f;
	float PerPipValue = 0.f;
	if (!RegenPips.IsValid() || !PerPip.IsValid()
		|| !Context.ReadAttribute(RegenPips, Regen) || !Context.ReadAttribute(PerPip, PerPipValue))
	{
		return 0.f;
	}

	float Degen = 0.f;
	if (DegenPips.IsValid())
	{
		Context.ReadAttribute(DegenPips, Degen);
	}

	const int32 Pips = ArcResourcePips::NetPips(Regen, Degen);
	return static_cast<float>(Pips) * FMath::Max(0.f, PerPipValue);
}

float FArcResourceDef::GetRateMultiplier(const FArcResourceContext& Context) const
{
	float Multiplier = 1.f;
	if (!Context.OwnedTags)
	{
		return Multiplier;
	}

	for (const FArcResourceRateMultiplier& Entry : RateMultipliers)
	{
		if (Context.OwnedTags->HasTag(Entry.Condition))
		{
			Multiplier *= Entry.Multiplier;
		}
	}
	return Multiplier;
}

float FArcResourceDef::ComputeStep(const FArcResourceContext& Context, float StepSeconds) const
{
	float UpperBound = 0.f;
	if (!Context.ReadUpperBound(UpperBound))
	{
		return 0.f;
	}

	if (Context.CurrentValue > UpperBound)
	{
		return UpperBound - Context.CurrentValue;
	}

	const FArcResourceRate* RateSlot = Rate.GetPtr<FArcResourceRate>();
	const FArcResourceTarget* TargetSlot = Target.GetPtr<FArcResourceTarget>();
	if (!RateSlot || !TargetSlot)
	{
		return 0.f;
	}

	const float SignedRate = RateSlot->GetRate(Context) * GetRateMultiplier(Context);
	if (SignedRate == 0.f)
	{
		return 0.f;
	}

	const float TargetValue = FMath::Clamp(TargetSlot->GetTarget(Context), Min, UpperBound);
	const float MaxStep = FMath::Abs(SignedRate) * StepSeconds;

	if (SignedRate > 0.f)
	{
		if (Context.OwnedTags && Context.OwnedTags->HasAny(BlockedBy))
		{
			return 0.f;
		}
		return FMath::Clamp(TargetValue - Context.CurrentValue, -MaxStep, MaxStep);
	}

	const bool bMoveUp = Context.CurrentValue > TargetValue
		|| (Context.CurrentValue == TargetValue && TargetValue <= Min);
	if (bMoveUp)
	{
		return FMath::Clamp(UpperBound - Context.CurrentValue, 0.f, MaxStep);
	}
	return -FMath::Clamp(Context.CurrentValue - Min, 0.f, MaxStep);
}
