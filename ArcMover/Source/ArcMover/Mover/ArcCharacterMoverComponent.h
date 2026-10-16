// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"

#include "ArcCharacterMoverComponent.generated.h"

class UCommonLegacyMovementSettings;
struct FArcMovementModifier_SpeedScale;

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class ARCMOVER_API UArcCharacterMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	FMovementModifierHandle QueueRootMovementModifier(const FGuid& SourceId);
	FMovementModifierHandle QueueSpeedScaleMovementModifier(const FGuid& SourceId, float SpeedPercent);

private:
	friend struct FArcMovementModifier_SpeedScale;

	void AddSpeedScaleSource(const FGuid& SourceId);
	void RemoveSpeedScaleSource(const FGuid& SourceId);
	void AddSpeedScaleSubstepContribution(const FGuid& SourceId, float SpeedPercent);
	void RestoreSpeedScaleAfterSubstep();

	TSet<FGuid> ActiveSpeedScaleSources;
	TMap<FGuid, float> SpeedScaleSubstepContributions;
	TWeakObjectPtr<UCommonLegacyMovementSettings> ScaledMovementSettings;
	float UnscaledMaxSpeed = 0.0f;
	bool bSpeedScaleApplied = false;
};
