// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConsiderationBase.h"
#include "Considerations/StateTreeCommonConsiderations.h"
#include "ArcTimeOfDayConsideration.generated.h"

USTRUCT()
struct FArcTimeOfDayConsiderationInstanceData
{
	GENERATED_BODY()

	/** Current time of day in hours (0-24), auto-populated from DaySequence. */
	UPROPERTY(VisibleAnywhere, Category = "Parameter")
	float TimeOfDay = 0.f;
};

/**
 * StateTree utility consideration: scores based on current time of day.
 * Reads ADaySequenceActor::GetTimeOfDay() and maps raw hours (0-24) through a response curve.
 * Designers configure curves using intuitive hour values (e.g., peak at 12.0 for noon).
 */
USTRUCT(meta = (DisplayName = "Arc Time of Day", Category = "Arc|AI"))
struct ARCAI_API FArcTimeOfDayConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTimeOfDayConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;

public:
	/** Maps normalized time-of-day [0,1] to a [0,1] score. X: 0.0=midnight, 0.25=6:00, 0.5=noon, 0.75=18:00, 1.0=midnight. */
	UPROPERTY(EditAnywhere, Category = "Default")
	FStateTreeConsiderationResponseCurve ResponseCurve;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
