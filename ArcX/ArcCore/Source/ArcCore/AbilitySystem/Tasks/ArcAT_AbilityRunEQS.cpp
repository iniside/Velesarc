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

#include "AbilitySystem/Tasks/ArcAT_AbilityRunEQS.h"

#include "AIController.h"

UArcAT_AbilityRunEQS::UArcAT_AbilityRunEQS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = false;
	EQSFinishedDelegate = FQueryFinishedSignature::CreateUObject(this
		, &UArcAT_AbilityRunEQS::OnEQSRequestFinished);
}

UArcAT_AbilityRunEQS* UArcAT_AbilityRunEQS::AbilityRunEQS(UGameplayAbility* OwningAbility
														  , UEnvQuery* QueryTemplate
														  , AActor* Owner)
{
	if (QueryTemplate == nullptr || Owner == nullptr)
	{
		return nullptr;
	}

	UArcAT_AbilityRunEQS* MyTask = NewAbilityTask<UArcAT_AbilityRunEQS>(OwningAbility);
	if (MyTask)
	{
		MyTask->EQSRequest.QueryTemplate = QueryTemplate;
		MyTask->QueryOwner = Owner != nullptr ? Owner : OwningAbility->GetCurrentActorInfo()->OwnerActor.Get();
	}

	return MyTask;
}

void UArcAT_AbilityRunEQS::Activate()
{
	if (EQSRequest.QueryTemplate && QueryOwner.IsValid())
	{
		Super::Activate();
		if (AAIController* AI = Cast<AAIController>(QueryOwner.Get()))
		{
			EQSRequest.Execute(*AI->GetPawn()
				, AI->GetBlackboardComponent()
				, EQSFinishedDelegate);
		}
		else
		{
			EQSRequest.Execute(*QueryOwner.Get()
				, nullptr
				, EQSFinishedDelegate);
		}
	}
}

void UArcAT_AbilityRunEQS::OnEQSRequestFinished(TSharedPtr<FEnvQueryResult> Result)
{
	if (IsFinished() == false)
	{
		Algo::Sort(Result->Items);
		TArray<FVector> OutLocations;
		TArray<AActor*> OutActors;

		Result->GetAllAsLocations(OutLocations);
		Result->GetAllAsActors(OutActors);
		ResultReady.Broadcast(OutLocations
			, OutActors);

		EndTask();
	}
}
