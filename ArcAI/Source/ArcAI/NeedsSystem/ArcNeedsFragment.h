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

#include "Fragments/ArcNeedFragment.h"
#include "MassStateTreeSchema.h"
#include "MassStateTreeTypes.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreePropertyRef.h"
#include "StateTreeTaskBase.h"
#include "Considerations/StateTreeCommonConsiderations.h"
#include "ArcNeedsFragment.generated.h"

USTRUCT()
struct FArcMassModifyNeedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	EArcNeedOperation Operation = EArcNeedOperation::Add;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EArcNeedFillType FillType = EArcNeedFillType::Fixed;

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float ModifyValue = 0.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	uint8 NumberOfTicks = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float ModifyPeriod = 0.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	UE::StateTree::EComparisonOperator TargetCompareOp = UE::StateTree::EComparisonOperator::Less;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float ThresholdCompareValue = 0.f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (MetaStruct = "/Script/ArcNeeds.ArcNeedFragment"))
	TObjectPtr<UScriptStruct> NeedType;

	UPROPERTY()
	uint8 CurrentTicks = 0;

	UPROPERTY()
	float CurrentPeriodTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Arc Modify Need", Category = "Arc|Needs"))
struct FArcMassModifyNeedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassModifyNeedTaskInstanceData;

public:
	FArcMassModifyNeedTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FArcMassNeedConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Parameter")
	float NeedValue = 0.f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (MetaStruct = "/Script/ArcNeeds.ArcNeedFragment"))
	TObjectPtr<UScriptStruct> NeedType;
};

/**
 * Consideration using a Float as input to the response curve.
 */
USTRUCT(DisplayName = "Arc Mass Thirst Need Consideration")
struct FArcMassNeedConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassNeedConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

protected:
	//~ Begin FStateTreeConsiderationBase Interface
	virtual float GetScore(FStateTreeExecutionContext& Context) const override;
	//~ End FStateTreeConsiderationBase Interface

public:
	UPROPERTY(EditAnywhere, Category = "Default")
	FStateTreeConsiderationResponseCurve ResponseCurve;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
