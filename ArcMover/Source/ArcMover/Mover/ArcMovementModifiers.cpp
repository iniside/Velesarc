// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcMovementModifiers.h"

#include "Mover/ArcCharacterMoverComponent.h"
#include "Mover/ArcChaosMovementModifiers.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMovementModifiers)

float ArcMover::CombineSpeedPercents(const TMap<FGuid, float>& PercentBySource)
{
	float CombinedScale = 1.0f;
	for (const TPair<FGuid, float>& Entry : PercentBySource)
	{
		CombinedScale *= Entry.Value / 100.0f;
	}
	return CombinedScale;
}

float ArcMover::GetActiveSpeedScale(const UMoverComponent& Mover)
{
	const FMovementModifierGroup& Modifiers = Mover.GetSyncState().MovementModifiers;

	TMap<FGuid, float> PercentBySource;
	const float NPScale = AccumulateSpeedPercents<FArcMovementModifier_SpeedScale>(Modifiers.GetActiveModifiersIterator(), PercentBySource)
		? CombineSpeedPercents(PercentBySource)
		: 1.0f;
	return NPScale * ComputeSpeedScale(Modifiers);
}

FArcMovementModifier_Root::FArcMovementModifier_Root()
{
	DurationMs = -1.0f;
}

FMovementModifierBase* FArcMovementModifier_Root::Clone() const
{
	return new FArcMovementModifier_Root(*this);
}

void FArcMovementModifier_Root::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << SourceId;
}

UScriptStruct* FArcMovementModifier_Root::GetScriptStruct() const
{
	return StaticStruct();
}

bool FArcMovementModifier_Root::Matches(const FMovementModifierBase* Other) const
{
	if (!Other || Other->GetScriptStruct() != StaticStruct())
	{
		return false;
	}

	const FArcMovementModifier_Root* OtherRoot = static_cast<const FArcMovementModifier_Root*>(Other);
	return SourceId == OtherRoot->SourceId;
}

FArcMovementModifier_SpeedScale::FArcMovementModifier_SpeedScale()
{
	DurationMs = -1.0f;
}

void FArcMovementModifier_SpeedScale::OnActivated(const FMovementModifierActivatedParams& Params)
{
	if (UArcCharacterMoverComponent* MoverComponent = Cast<UArcCharacterMoverComponent>(Params.SimContext.SimulationOwner))
	{
		MoverComponent->AddSpeedScaleSource(SourceId);
	}
}

void FArcMovementModifier_SpeedScale::OnDeactivated(const FMovementModifierDeactivatedParams& Params)
{
	if (UArcCharacterMoverComponent* MoverComponent = Cast<UArcCharacterMoverComponent>(Params.SimContext.SimulationOwner))
	{
		MoverComponent->RemoveSpeedScaleSource(SourceId);
	}
}

void FArcMovementModifier_SpeedScale::OnPreMovement(const FMovementModifierPreMovementParams& Params)
{
	if (UArcCharacterMoverComponent* MoverComponent = Cast<UArcCharacterMoverComponent>(Params.SimContext.SimulationOwner))
	{
		MoverComponent->AddSpeedScaleSubstepContribution(SourceId, SpeedPercent);
	}
}

void FArcMovementModifier_SpeedScale::OnPostMovement(const FMovementModifierPostMovementParams& Params)
{
	if (UArcCharacterMoverComponent* MoverComponent = Cast<UArcCharacterMoverComponent>(Params.SimContext.SimulationOwner))
	{
		MoverComponent->RestoreSpeedScaleAfterSubstep();
	}
}

FMovementModifierBase* FArcMovementModifier_SpeedScale::Clone() const
{
	return new FArcMovementModifier_SpeedScale(*this);
}

void FArcMovementModifier_SpeedScale::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << SourceId;
	Ar << SpeedPercent;
}

UScriptStruct* FArcMovementModifier_SpeedScale::GetScriptStruct() const
{
	return StaticStruct();
}

bool FArcMovementModifier_SpeedScale::Matches(const FMovementModifierBase* Other) const
{
	if (!Other || Other->GetScriptStruct() != StaticStruct())
	{
		return false;
	}

	const FArcMovementModifier_SpeedScale* OtherSpeedScale = static_cast<const FArcMovementModifier_SpeedScale*>(Other);
	return SourceId == OtherSpeedScale->SourceId;
}
