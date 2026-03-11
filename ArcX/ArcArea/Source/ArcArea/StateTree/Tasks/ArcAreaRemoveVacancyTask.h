// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcAreaRemoveVacancyTask.generated.h"

USTRUCT()
struct FArcAreaRemoveVacancyTaskInstanceData
{
	GENERATED_BODY()

	/** The knowledge handle to remove (obtained from PostVacancy output). */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcKnowledgeHandle VacancyHandle;

	/** Output: whether removal succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSucceeded = false;
};

/**
 * Removes a vacancy advertisement from ArcKnowledge.
 * Counterpart to FArcAreaPostVacancyTask.
 */
USTRUCT(meta = (DisplayName = "Arc Remove Area Vacancy", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaRemoveVacancyTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaRemoveVacancyTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 80, 60); }
#endif
};
