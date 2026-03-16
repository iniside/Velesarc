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

#include "ArcNeedsFragment.h"

#include "ArcAILogs.h"
#include "Fragments/ArcNeedFragment.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassStateTreeExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTypes.h"
#include "VisualLogger/VisualLogger.h"

FArcMassModifyNeedTask::FArcMassModifyNeedTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassModifyNeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& MEM = MassStateTreeContext.GetEntityManager();
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* MassSignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	if (InstanceData.FillType == EArcNeedFillType::Fixed)
	{
		FStructView View = MEM.GetFragmentDataStruct(MassStateTreeContext.GetEntity(), InstanceData.NeedType);
		if (!View.IsValid())
		{
			UE_LOG(LogStateTree, Warning, TEXT("FArcMassModifyNeedTask::EnterState: Need not present."));
			return EStateTreeRunStatus::Failed;
		}

		FArcNeedFragment* NeedFragment = View.GetPtr<FArcNeedFragment>();

		switch (InstanceData.Operation)
		{
			case EArcNeedOperation::Add:
				NeedFragment->CurrentValue += InstanceData.ModifyValue;
				NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
				break;
			case EArcNeedOperation::Subtract:
				NeedFragment->CurrentValue -= InstanceData.ModifyValue;
				NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
				break;
			case EArcNeedOperation::Override:
				NeedFragment->CurrentValue = InstanceData.ModifyValue;
				NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
				break;
		}

		return EStateTreeRunStatus::Succeeded;
	}
	if (InstanceData.FillType != EArcNeedFillType::Fixed)
	{
		if (InstanceData.FillType == EArcNeedFillType::FixedTicks && InstanceData.NumberOfTicks == 0)
		{
			UE_LOG(LogStateTree, Warning, TEXT("FArcMassModifyNeedTask::EnterState: NumberOf Ticks must be greater than 0 for Fixed Ticks fill type."));
			return EStateTreeRunStatus::Failed;
		}

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[this, WeakCtx = Context.MakeWeakExecutionContext(), Entity = MassStateTreeContext.GetEntity(), MES, MassSignalSubsystem]
			(float DeltaTime) -> bool
		{
			auto StrongCtx = WeakCtx.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongCtx.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceData)
			{
				StrongCtx.FinishTask(EStateTreeFinishTaskType::Failed);
				MassSignalSubsystem->SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, Entity);
				return false; // Stop the ticker if we can't access instance data
			}

			InstanceData->CurrentPeriodTime += DeltaTime;

			if (InstanceData->CurrentPeriodTime >= InstanceData->ModifyPeriod)
			{
				FMassEntityManager& MEM = MES->GetMutableEntityManager();
				InstanceData->CurrentPeriodTime = 0.f;
				InstanceData->CurrentTicks++;

				FStructView View = MEM.GetFragmentDataStruct(Entity, InstanceData->NeedType);
				if (View.IsValid())
				{
					FArcNeedFragment* NeedFragment = View.GetPtr<FArcNeedFragment>();

					switch (InstanceData->Operation)
					{
						case EArcNeedOperation::Add:
							NeedFragment->CurrentValue += InstanceData->ModifyValue;
							NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
							break;
						case EArcNeedOperation::Subtract:
							NeedFragment->CurrentValue -= InstanceData->ModifyValue;
							NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
							break;
						case EArcNeedOperation::Override:
							NeedFragment->CurrentValue = InstanceData->ModifyValue;
							NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
							break;
					}

					if (InstanceData->FillType == EArcNeedFillType::UntilValue)
					{
						bool bComparisonResult = false;
						switch (InstanceData->TargetCompareOp)
						{
							case UE::StateTree::EComparisonOperator::Less:
								 bComparisonResult = NeedFragment->CurrentValue < InstanceData->ThresholdCompareValue;
								break;
							case UE::StateTree::EComparisonOperator::LessOrEqual:
								 bComparisonResult = NeedFragment->CurrentValue <= InstanceData->ThresholdCompareValue;
								break;
							case UE::StateTree::EComparisonOperator::Equal:
								 bComparisonResult = FMath::IsNearlyEqual(NeedFragment->CurrentValue, InstanceData->ThresholdCompareValue, 0.01);
								break;
							case UE::StateTree::EComparisonOperator::NotEqual:
								 bComparisonResult = !FMath::IsNearlyEqual(NeedFragment->CurrentValue, InstanceData->ThresholdCompareValue, 0.01);
								break;
							case UE::StateTree::EComparisonOperator::GreaterOrEqual:
								 bComparisonResult = NeedFragment->CurrentValue >= InstanceData->ThresholdCompareValue;
								break;
							case UE::StateTree::EComparisonOperator::Greater:
								 bComparisonResult = NeedFragment->CurrentValue > InstanceData->ThresholdCompareValue;
								break;
						}

						if (bComparisonResult)
						{
							StrongCtx.FinishTask(EStateTreeFinishTaskType::Succeeded);
							MassSignalSubsystem->SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, Entity);
							return false; // Stop the ticker if the need is full
						}

					}
				}

				if (InstanceData->FillType == EArcNeedFillType::FixedTicks && InstanceData->CurrentTicks >= InstanceData->NumberOfTicks)
				{
					StrongCtx.FinishTask(EStateTreeFinishTaskType::Succeeded);
					MassSignalSubsystem->SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, Entity);
					return false; // Stop the ticker
				}
			}

			return true; // Continue ticking
		}));
	}

	return EStateTreeRunStatus::Running;
}

FText FArcMassModifyNeedTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup
	, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(
				NSLOCTEXT("ArcAI", "ModifyNeedTaskDescription", "Modify Need {0} by {1}"),
				FText::FromString(GetNameSafe(InstanceData->NeedType)), FText::AsNumber(InstanceData->ModifyValue));
		}
	}

	return FText::GetEmpty();
}

float FArcMassNeedConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FConstStructView Data = MEM.GetFragmentDataStruct(MassCtx.GetEntity(), InstanceData.NeedType);
	if (!Data.IsValid())
	{
		return 1.f;
	}

	const FArcNeedFragment* DataPtr = Data.GetPtr<FArcNeedFragment>();

	InstanceData.NeedValue = DataPtr->CurrentValue;

	float Score = 0.f;

	const float Normalized = DataPtr->CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

#if WITH_EDITOR
FText FArcMassNeedConsideration::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "MassNeedConsDesc", "Need: {0}"), FText::FromString(GetNameSafe(InstanceData->NeedType)));
		}
	}
	return FText::GetEmpty();
}
#endif
