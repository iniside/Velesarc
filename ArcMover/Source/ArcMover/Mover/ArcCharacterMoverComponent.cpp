// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcCharacterMoverComponent.h"

#include "Mover/ArcLayeredMove_Root.h"
#include "Mover/ArcMovementModifiers.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCharacterMoverComponent)

FMovementModifierHandle UArcCharacterMoverComponent::QueueRootMovementModifier(const FGuid& SourceId)
{
	if (!SourceId.IsValid())
	{
		return FMovementModifierHandle();
	}

	TSharedPtr<FArcMovementModifier_Root> Marker = MakeShared<FArcMovementModifier_Root>();
	Marker->SourceId = SourceId;
	return ArcMover::QueueRoot(*this, Marker);
}

FMovementModifierHandle UArcCharacterMoverComponent::QueueSpeedScaleMovementModifier(const FGuid& SourceId, float SpeedPercent)
{
	if (!SourceId.IsValid() || !FMath::IsFinite(SpeedPercent) || SpeedPercent < 0.0f)
	{
		return FMovementModifierHandle();
	}

	TSharedPtr<FArcMovementModifier_SpeedScale> Modifier = MakeShared<FArcMovementModifier_SpeedScale>();
	Modifier->SourceId = SourceId;
	Modifier->SpeedPercent = SpeedPercent;
	return QueueModifier(FQueueModifierParams(Modifier));
}

void UArcCharacterMoverComponent::AddSpeedScaleSource(const FGuid& SourceId)
{
	if (SourceId.IsValid())
	{
		ActiveSpeedScaleSources.Add(SourceId);
	}
}

void UArcCharacterMoverComponent::RemoveSpeedScaleSource(const FGuid& SourceId)
{
	ActiveSpeedScaleSources.Remove(SourceId);
	SpeedScaleSubstepContributions.Remove(SourceId);
}

void UArcCharacterMoverComponent::AddSpeedScaleSubstepContribution(const FGuid& SourceId, float SpeedPercent)
{
	if (!ActiveSpeedScaleSources.Contains(SourceId) || !FMath::IsFinite(SpeedPercent) || SpeedPercent < 0.0f)
	{
		return;
	}

	SpeedScaleSubstepContributions.Add(SourceId, SpeedPercent);
	if (bSpeedScaleApplied || SpeedScaleSubstepContributions.Num() != ActiveSpeedScaleSources.Num())
	{
		return;
	}

	UCommonLegacyMovementSettings* MovementSettings = FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	if (!MovementSettings)
	{
		return;
	}

	float CombinedScale = 1.0f;
	for (const TPair<FGuid, float>& Contribution : SpeedScaleSubstepContributions)
	{
		CombinedScale *= Contribution.Value / 100.0f;
	}

	ScaledMovementSettings = MovementSettings;
	UnscaledMaxSpeed = MovementSettings->MaxSpeed;
	MovementSettings->MaxSpeed = UnscaledMaxSpeed * CombinedScale;
	bSpeedScaleApplied = true;
}

void UArcCharacterMoverComponent::RestoreSpeedScaleAfterSubstep()
{
	if (bSpeedScaleApplied)
	{
		if (UCommonLegacyMovementSettings* MovementSettings = ScaledMovementSettings.Get())
		{
			MovementSettings->MaxSpeed = UnscaledMaxSpeed;
		}
	}

	ScaledMovementSettings.Reset();
	SpeedScaleSubstepContributions.Reset();
	UnscaledMaxSpeed = 0.0f;
	bSpeedScaleApplied = false;
}
