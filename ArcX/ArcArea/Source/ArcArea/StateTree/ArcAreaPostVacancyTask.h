// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcAreaPostVacancyTask.generated.h"

USTRUCT()
struct FArcAreaPostVacancyTaskInstanceData
{
	GENERATED_BODY()

	/** Slot index to post vacancy for. */
	UPROPERTY(EditAnywhere, Category = Input)
	int32 SlotIndex = INDEX_NONE;

	/** Output: the knowledge handle of the posted vacancy. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle VacancyHandle;

	/** Output: whether posting succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSucceeded = false;
};

/**
 * Posts a vacancy advertisement for a specific slot to ArcKnowledge.
 * Uses the slot's VacancyConfig for tags and relevance.
 */
USTRUCT(meta = (DisplayName = "Arc Post Area Vacancy", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaPostVacancyTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaPostVacancyTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};
