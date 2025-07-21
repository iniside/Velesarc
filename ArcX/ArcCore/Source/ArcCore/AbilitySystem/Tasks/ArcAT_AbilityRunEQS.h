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

#include "Abilities/Tasks/AbilityTask.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Tasks/AITask.h"

#include "ArcAT_AbilityRunEQS.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcEQSResultReady
	, const TArray<FVector>&, Locations
	, const TArray<AActor*>&, Actors);

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_AbilityRunEQS : public UAbilityTask
{
	GENERATED_BODY()
	TWeakObjectPtr<AActor> QueryOwner;

	UPROPERTY(BlueprintAssignable)
	FArcEQSResultReady ResultReady;

public:
	UArcAT_AbilityRunEQS(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * @brief Performs EQS query and return either actors or location sorted by highest score.
	 * @param OwningAbility Hidden
	 * @param QueryTemplate Query to perform.
	 * @param Owner Owner of EQS Query. Optional
	 * @return 
	 */
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_AbilityRunEQS* AbilityRunEQS(UGameplayAbility* OwningAbility
											   , UEnvQuery* QueryTemplate
											   , AActor* Owner = nullptr);

protected:
	virtual void Activate() override;

	void OnEQSRequestFinished(TSharedPtr<FEnvQueryResult> Result);

	FEQSParametrizedQueryExecutionRequest EQSRequest;
	FQueryFinishedSignature EQSFinishedDelegate;
	TSharedPtr<FEnvQueryResult> QueryResult;
};
