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


#include "GameplayAbilitySpec.h"
#include "GameplayAbilitySpecHandle.h"
#include "StateTreePropertyRef.h"
#include "StateTreeTaskBase.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "UObject/Object.h"
#include "ArcAISTT_ActivateAbility.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcAISTT_ActivateAbilityInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> QueryOwner = nullptr;
	
	// The query template to run
	UPROPERTY(EditAnywhere, Category = Parameter)
	TSubclassOf<UGameplayAbility> AbilityClass;

	FGameplayAbilitySpecHandle AbilityHandle;

	FGameplayAbilitySpec* SpecPtr = nullptr;
};

/**
 * 
 */
USTRUCT()
struct ARCAI_API FArcAISTT_ActivateAbility : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAISTT_ActivateAbilityInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

class AActor;
class UEnvQuery;

USTRUCT()
struct FArcAIStateTreeRunEnvQueryInstanceData
{
	GENERATED_BODY()

	// Result of the query. If an array is binded, it will output all the created values otherwise it will output the best one.
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/CoreUObject.Vector, /Script/Engine.Actor", CanRefToArray))
	FStateTreePropertyRef Result;

	// The query will be run with this actor has the owner object.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> QueryOwner = nullptr;

	// The query template to run
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UEnvQuery> QueryTemplate;

	// Query config associated with the query template.
	UPROPERTY(EditAnywhere, EditFixedSize, Category = Parameter)
	TArray<FAIDynamicParam> QueryConfig;

	/** determines which item will be stored (All = only first matching) */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode = EEnvQueryRunMode::SingleResult;

	TSharedPtr<FEnvQueryResult> QueryResult = nullptr;

	int32 RequestId = INDEX_NONE;

	bool bShouldRestart = false;
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Arc Run Env Query Continuous", Category = "Common"))
struct FArcAIStateTreeRunEnvQueryTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAIStateTreeRunEnvQueryInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
