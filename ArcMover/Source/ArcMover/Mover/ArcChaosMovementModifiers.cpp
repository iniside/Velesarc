// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosMovementModifiers.h"

#include "Mover/ArcMovementModifiers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosMovementModifiers)

float ArcMover::ComputeSpeedScale(const FMovementModifierGroup& Modifiers)
{
	// Queued entries count: a modifier queued this tick is only queued in the start state (ChaosMoverStateMachine.cpp:617,746).
	TMap<FGuid, float> PercentBySource;
	if (!AccumulateSpeedPercents<FArcChaosMovementModifier_SpeedScale>(Modifiers.GetActiveModifiersIterator(), PercentBySource)
		|| !AccumulateSpeedPercents<FArcChaosMovementModifier_SpeedScale>(Modifiers.GetQueuedModifiersIterator(), PercentBySource))
	{
		return 1.0f;
	}
	return CombineSpeedPercents(PercentBySource);
}

FArcChaosMovementModifier_SpeedScale::FArcChaosMovementModifier_SpeedScale()
{
	DurationMs = -1.0f;
}

FMovementModifierBase* FArcChaosMovementModifier_SpeedScale::Clone() const
{
	return new FArcChaosMovementModifier_SpeedScale(*this);
}

void FArcChaosMovementModifier_SpeedScale::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << SourceId;
	Ar << SpeedPercent;
}

UScriptStruct* FArcChaosMovementModifier_SpeedScale::GetScriptStruct() const
{
	return StaticStruct();
}

bool FArcChaosMovementModifier_SpeedScale::Matches(const FMovementModifierBase* Other) const
{
	if (!Other || Other->GetScriptStruct() != StaticStruct())
	{
		return false;
	}

	const FArcChaosMovementModifier_SpeedScale* OtherSpeedScale = static_cast<const FArcChaosMovementModifier_SpeedScale*>(Other);
	return SourceId == OtherSpeedScale->SourceId;
}
