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

#include "ArcAI/TargetQuery/Steps/ArcTQSStep_WorldCondition.h"

#include "MassEntitySubsystem.h"
#include "ArcCore/Conditions/ArcWorldConditionDefinition.h"
#include "ArcCore/Conditions/ArcWorldConditionSchema.h"
#include "WorldConditionContext.h"
#include "Engine/World.h"
#include "TargetQuery/ArcTQSTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSStep_WorldCondition)

float FArcTQSStep_WorldCondition::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (!ConditionDefinition || !ConditionDefinition->IsValid())
	{
		return 1.0f; // No condition = pass
	}

	const FWorldConditionQueryDefinition& QueryDef = ConditionDefinition->GetQueryDefinition();
	const TSubclassOf<UWorldConditionSchema> SchemaClass = QueryDef.GetSchemaClass();
	if (!SchemaClass)
	{
		return 1.0f;
	}

	const UWorldConditionSchema* Schema = GetDefault<UWorldConditionSchema>(SchemaClass);
	if (!Schema)
	{
		return 1.0f;
	}

	// Fill context data by name — works with any schema in the hierarchy.
	// Slots that don't exist in the schema are silently skipped.
	FWorldConditionContextData ContextData(*Schema);

	// Source (querier) context
	if (QueryContext.QuerierEntity.IsValid())
	{
		ContextData.SetContextData(TEXT("SourceEntity"), &QueryContext.QuerierEntity);
	}
	if (QueryContext.QuerierActor.IsValid())
	{
		ContextData.SetContextData(TEXT("SourceActor"), QueryContext.QuerierActor.Get());
	}

	// Target context
	if (Item.EntityHandle.IsValid())
	{
		ContextData.SetContextData(TEXT("TargetEntity"), &Item.EntityHandle);
	}

	AActor* TargetActor = const_cast<FArcTQSTargetItem&>(Item).GetActor(QueryContext.EntityManager);
	if (TargetActor)
	{
		ContextData.SetContextData(TEXT("TargetActor"), TargetActor);
	}

	// Persistent context
	if (QueryContext.EntityManager)
	{
		ContextData.SetContextData(TEXT("MassEntitySubsystem"), QueryContext.World->GetSubsystem<UMassEntitySubsystem>());
	}

	// Evaluate: create ephemeral state, activate, check, deactivate
	FWorldConditionQueryState QueryState;
	QueryState.Initialize(*ConditionDefinition, QueryDef);

	FWorldConditionContext Context(QueryState, ContextData);
	if (!Context.Activate())
	{
		return 1.0f; // Activation failed = skip (pass through)
	}

	const bool bResult = Context.IsTrue();
	Context.Deactivate();

	return bResult ? 1.0f : 0.0f;
}
