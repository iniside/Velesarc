// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTimeOfDayConsideration.h"
#include "ArcAILogs.h"
#include "DaySequenceSubsystem.h"
#include "DaySequenceActor.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"

#if ENABLE_VISUAL_LOG
#include "VisualLogger/VisualLogger.h"
#endif
float FArcTimeOfDayConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FArcTimeOfDayConsiderationInstanceData& InstanceData = Context.GetInstanceData(*this);

	const UWorld* World = Context.GetWorld();
	if (!World)
	{
		return 0.f;
	}

	const UDaySequenceSubsystem* DaySubsystem = World->GetSubsystem<UDaySequenceSubsystem>();
	if (!DaySubsystem)
	{
		return 0.f;
	}

	const ADaySequenceActor* DayActor = DaySubsystem->GetDaySequenceActor();
	if (!DayActor)
	{
		return 0.f;
	}

	const float TimeOfDay = DayActor->GetTimeOfDay();
	InstanceData.TimeOfDay = TimeOfDay;

	// Normalize hours to [0,1] for the response curve (matches FArcMassNeedConsideration pattern).
	// Curve X axis: 0.0 = midnight, 0.25 = 6:00, 0.5 = noon, 0.75 = 18:00, 1.0 = midnight.
	const float Score = ResponseCurve.Evaluate(TimeOfDay / 24.0f);

#if ENABLE_VISUAL_LOG
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose,
		TEXT("State %s TimeOfDay: %.1fh Score: %.3f"), *Context.GetActiveStateName(), TimeOfDay, Score);
#endif
	
	return Score;
}

#if WITH_EDITOR
FText FArcTimeOfDayConsideration::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "TimeOfDayConsiderationDesc", "Time of Day");
}
#endif
