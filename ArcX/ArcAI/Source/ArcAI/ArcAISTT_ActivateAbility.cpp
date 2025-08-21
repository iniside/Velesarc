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



#include "ArcAISTT_ActivateAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyRef.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

EStateTreeRunStatus FArcAISTT_ActivateAbility::EnterState(FStateTreeExecutionContext& Context
														  , const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	AAIController* C = Cast<AAIController>(InstanceData.QueryOwner);
	if (!C)
	{
		return EStateTreeRunStatus::Failed;
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(C->GetPawn());
	if (!ASI)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.AbilityHandle.IsValid())
	{
		FGameplayAbilitySpec* Spec =  ASI->GetAbilitySystemComponent()->FindAbilitySpecFromClass(InstanceData.AbilityClass);
		if (Spec)
		{
			InstanceData.AbilityHandle = Spec->Handle;
			InstanceData.SpecPtr = Spec;
		}
		else
		{
			return EStateTreeRunStatus::Failed;
		}
	}
	
	
	ASI->GetAbilitySystemComponent()->TryActivateAbility(InstanceData.AbilityHandle);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAISTT_ActivateAbility::Tick(FStateTreeExecutionContext& Context
	, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.SpecPtr->GetPrimaryInstance()->IsActive())
	{
		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Succeeded;
}

void FArcAISTT_ActivateAbility::ExitState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	
}


EStateTreeRunStatus FArcAIStateTreeRunEnvQueryTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.QueryTemplate)
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* QueryOwner = InstanceData.QueryOwner;
	if (AAIController* AICon = Cast<AAIController>(QueryOwner))
	{
		QueryOwner = AICon->GetPawn();
	}
	
	FEnvQueryRequest Request(InstanceData.QueryTemplate, QueryOwner);

	for (FAIDynamicParam& DynamicParam : InstanceData.QueryConfig)
	{
		Request.SetDynamicParam(DynamicParam, nullptr);
	}
	
	InstanceData.RequestId = Request.Execute(InstanceData.RunMode,
		FQueryFinishedSignature::CreateLambda([InstanceDataRef = Context.GetInstanceDataStructRef(*this)](TSharedPtr<FEnvQueryResult> QueryResult) mutable
			{
				if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
				{
					InstanceData->QueryResult = QueryResult;
					InstanceData->RequestId = INDEX_NONE;
				}
			}));
	
	return InstanceData.RequestId != INDEX_NONE ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FArcAIStateTreeRunEnvQueryTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.QueryResult)
	{
		if (InstanceData.QueryResult->IsSuccessful())
		{
			auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor] = InstanceData.Result.GetMutablePtrTuple<FVector, AActor*, TArray<FVector>, TArray<AActor*>>(Context);
			if (VectorPtr)
			{
				*VectorPtr = InstanceData.QueryResult->GetItemAsLocation(0);
			}
			else if (ActorPtr)
			{
				*ActorPtr = InstanceData.QueryResult->GetItemAsActor(0);
			}
			else if (ArrayOfVector)
			{
				InstanceData.QueryResult->GetAllAsLocations(*ArrayOfVector);
			}
			else if (ArrayOfActor)
			{
				InstanceData.QueryResult->GetAllAsActors(*ArrayOfActor);
			}

			InstanceData.RequestId = INDEX_NONE;
			InstanceData.QueryResult.Reset();

			AActor* QueryOwner = InstanceData.QueryOwner;
			if (AAIController* AICon = Cast<AAIController>(QueryOwner))
			{
				QueryOwner = AICon->GetPawn();
			}
			FEnvQueryRequest Request(InstanceData.QueryTemplate, QueryOwner);

			for (FAIDynamicParam& DynamicParam : InstanceData.QueryConfig)
			{
				Request.SetDynamicParam(DynamicParam, nullptr);
			}
			
			InstanceData.RequestId = Request.Execute(InstanceData.RunMode,
				FQueryFinishedSignature::CreateLambda([InstanceDataRef = Context.GetInstanceDataStructRef(*this)](TSharedPtr<FEnvQueryResult> QueryResult) mutable
				{
					if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
					{
						InstanceData->QueryResult = QueryResult;
						InstanceData->RequestId = INDEX_NONE;
					}
				}));
			return EStateTreeRunStatus::Running;
		}
		else
		{
			return EStateTreeRunStatus::Failed;
		}
	}
	return EStateTreeRunStatus::Running;
}

void FArcAIStateTreeRunEnvQueryTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	//FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	//if (InstanceData.RequestId != INDEX_NONE)
	//{
	//	if (UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(Context.GetOwner()))
	//	{
	//		QueryManager->AbortQuery(InstanceData.RequestId);
	//	}
	//	InstanceData.RequestId = INDEX_NONE;
	//}
	//InstanceData.QueryResult.Reset();
}
