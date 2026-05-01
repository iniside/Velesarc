/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "ArcTQSStep.h"
#include "ArcTQSStep_WorldCondition.generated.h"

class UArcWorldConditionDefinition;

/**
 * TQS step that evaluates a WorldCondition definition against each target.
 * Fills the schema context data with querier (source) and candidate (target) info,
 * then calls IsTrue() on the condition query.
 *
 * Requires a UArcWorldConditionDefinition asset configured with a schema that
 * provides the context data needed by its conditions (e.g., UArcTQSWorldConditionSchema).
 *
 * Targets without entity handles or actors are skipped (pass through).
 */
USTRUCT(DisplayName = "World Condition")
struct ARCTARGETQUERY_API FArcTQSStep_WorldCondition : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_WorldCondition()
	{
		StepType = EArcTQSStepType::Filter;
	}

	UPROPERTY(EditAnywhere, Category = "Step")
	TObjectPtr<UArcWorldConditionDefinition> ConditionDefinition = nullptr;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
