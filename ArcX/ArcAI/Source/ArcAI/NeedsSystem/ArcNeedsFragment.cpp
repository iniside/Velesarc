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
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeTypes.h"
#include "MassSignalSubsystem.h"

#include "MassActors/Public/MassAgentComponent.h"

#include "StateTree.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

UArcHungerNeedProcessor::UArcHungerNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UArcHungerNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	
	NeedsQuery.AddRequirement<FArcHungerNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcHungerNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	NeedsQuery.ForEachEntityChunk(Context, [this, DeltaTime](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcHungerNeedFragment> Needs = Ctx.GetMutableFragmentView<FArcHungerNeedFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcHungerNeedFragment& Need = Needs[EntityIt];
			Need.CurrentValue = Need.CurrentValue + (Need.ChangeRate * DeltaTime);
			Need.CurrentValue = FMath::Clamp(Need.CurrentValue, 0.f, 100.f);
		}
	});
}

UArcThirstNeedProcessor::UArcThirstNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UArcThirstNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	
	NeedsQuery.AddRequirement<FArcThirstNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcThirstNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	NeedsQuery.ForEachEntityChunk(Context, [this, DeltaTime](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcThirstNeedFragment> Needs = Ctx.GetMutableFragmentView<FArcThirstNeedFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcThirstNeedFragment& Need = Needs[EntityIt];
			Need.CurrentValue = Need.CurrentValue + (Need.ChangeRate * DeltaTime);
			Need.CurrentValue = FMath::Clamp(Need.CurrentValue, 0.f, 100.f);
		}
	});
}

UArcFatigueNeedProcessor::UArcFatigueNeedProcessor()
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcFatigueNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	
	NeedsQuery.AddRequirement<FArcFatigueNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcFatigueNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	NeedsQuery.ForEachEntityChunk(Context, [this, DeltaTime](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcFatigueNeedFragment> Needs = Ctx.GetMutableFragmentView<FArcFatigueNeedFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcFatigueNeedFragment& Need = Needs[EntityIt];
			Need.CurrentValue = Need.CurrentValue + (Need.ChangeRate * DeltaTime);
			Need.CurrentValue = FMath::Clamp(Need.CurrentValue, 0.f, 100.f);
		}
	});
}

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
	
		float RealModifyValue = InstanceData.ModifyValue;
		if (InstanceData.Operation == EArcNeedOperation::Add && InstanceData.TargetEntity.IsValid() 
			&& MEM.IsEntityValid(InstanceData.TargetEntity) && InstanceData.ResourceType)
		{
			FStructView ResourceView = MEM.GetFragmentDataStruct(InstanceData.TargetEntity, InstanceData.ResourceType);
			if (ResourceView.IsValid())
			{
				FArcResourceFragment* ResourceFragment = ResourceView.GetPtr<FArcResourceFragment>();
				if (InstanceData.ModifyValue < 0 && ResourceFragment->CurrentResourceAmount >= FMath::Abs(InstanceData.ModifyValue))
				{
					ResourceFragment->CurrentResourceAmount -= InstanceData.ModifyValue;
					ResourceFragment->CurrentResourceAmount = FMath::Max(ResourceFragment->CurrentResourceAmount, 0.f);
				}
				else
				{
					RealModifyValue = ResourceFragment->CurrentResourceAmount;
					ResourceFragment->CurrentResourceAmount = 0.f;
				}
			}
		}
	
		FArcNeedFragment* NeedFragment = View.GetPtr<FArcNeedFragment>();

		switch (InstanceData.Operation)
		{
			case EArcNeedOperation::Add:
				NeedFragment->CurrentValue += RealModifyValue;
				NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
				break;
			case EArcNeedOperation::Subtract:
				NeedFragment->CurrentValue -= RealModifyValue;
				NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
				break;
			case EArcNeedOperation::Override:
				NeedFragment->CurrentValue = RealModifyValue;
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
					float RealModifyValue = InstanceData->ModifyValue;
					if (InstanceData->Operation == EArcNeedOperation::Add && InstanceData->TargetEntity.IsValid() 
						&& MEM.IsEntityValid(InstanceData->TargetEntity) && InstanceData->ResourceType)
					{
						FStructView ResourceView = MEM.GetFragmentDataStruct(InstanceData->TargetEntity, InstanceData->ResourceType);
						if (ResourceView.IsValid())
						{
							FArcResourceFragment* ResourceFragment = ResourceView.GetPtr<FArcResourceFragment>();
							if (ResourceFragment->CurrentResourceAmount <= 0.f)
							{
								StrongCtx.FinishTask(EStateTreeFinishTaskType::Succeeded);
								MassSignalSubsystem->SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, Entity);
								return false; // Stop the ticker if there's no resources left
							}
							switch (InstanceData->TargetOperation)
							{
								case EArcNeedOperation::Add:
									ResourceFragment->CurrentResourceAmount += InstanceData->ModifyValue;
									ResourceFragment->CurrentResourceAmount = FMath::Max(ResourceFragment->CurrentResourceAmount, 0.f);
									break;
								case EArcNeedOperation::Subtract:
									
									if (ResourceFragment->CurrentResourceAmount >= FMath::Abs(InstanceData->ModifyValue))
									{
										ResourceFragment->CurrentResourceAmount -= InstanceData->ModifyValue;
										ResourceFragment->CurrentResourceAmount = FMath::Max(ResourceFragment->CurrentResourceAmount, 0.f);
									}
									else
									{
										RealModifyValue = ResourceFragment->CurrentResourceAmount;
										ResourceFragment->CurrentResourceAmount = 0.f;
									}
									break;
								case EArcNeedOperation::Override:
									ResourceFragment->CurrentResourceAmount = InstanceData->ModifyValue;
									ResourceFragment->CurrentResourceAmount = FMath::Max(ResourceFragment->CurrentResourceAmount, 0.f);
									break;
							}
						}
					}
					FArcNeedFragment* NeedFragment = View.GetPtr<FArcNeedFragment>();

					switch (InstanceData->Operation)
					{
						case EArcNeedOperation::Add:
							NeedFragment->CurrentValue += RealModifyValue;
							NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
							break;
						case EArcNeedOperation::Subtract:
							NeedFragment->CurrentValue -= RealModifyValue;
							NeedFragment->CurrentValue = FMath::Clamp(NeedFragment->CurrentValue, 0.f, 100.f);
							break;
						case EArcNeedOperation::Override:
							NeedFragment->CurrentValue = RealModifyValue;
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
				NSLOCTEXT("ArcAI", "ModifyNeedTaskDescription", "Modify Need {0} by {1} From {2}"), 
				FText::FromString(GetNameSafe(InstanceData->NeedType)), FText::AsNumber(InstanceData->ModifyValue), 
				FText::FromString(GetNameSafe(InstanceData->ResourceType)));
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

FArcProvideNextStateTreeTask::FArcProvideNextStateTreeTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcProvideNextStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	if (InstanceData.TargetInput.GetEntityHandle().IsValid())
	{
		FMassEntityView TargetView(MassCtx.GetEntityManager(), InstanceData.TargetInput.GetEntityHandle());
		FArcNeedStateTreeFragment* PossibleActions = TargetView.GetFragmentDataPtr<FArcNeedStateTreeFragment>();
		if (PossibleActions)
		{
			
		}
		
	}
	
	UStateTree** StateTreePtr = InstanceData.StateTree.GetMutablePtr<UStateTree*>(Context);

	UStateTree* NextStateTree = nullptr;
	if (StateTreePtr)
	{
		NextStateTree = *StateTreePtr;
	}
	
	if (InstanceData.QueuedStateTrees.IsEmpty() && NextStateTree)
	{
		Context.BroadcastDelegate(InstanceData.OnQueueFinished);
		StateTreePtr = nullptr;
		return EStateTreeRunStatus::Succeeded;
	}
	
	if (InstanceData.QueuedStateTrees.IsEmpty() == false)
	{
		*StateTreePtr = InstanceData.QueuedStateTrees.Pop();
		return EStateTreeRunStatus::Succeeded;
	}
	
	if (InstanceData.StateTrees.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.QueuedStateTrees = InstanceData.StateTrees;

	*StateTreePtr = InstanceData.QueuedStateTrees.Pop();
	return EStateTreeRunStatus::Succeeded;
	//Context.BroadcastDelegate(InstanceData.OnQueueFinished);
	
}

FArcMassInjectStateTreeTask::FArcMassInjectStateTreeTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcMassInjectStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.StateTree)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityView View(MassCtx.GetEntityManager(), MassCtx.GetEntity());
	FMassStateTreeInstanceFragment* Frag = View.GetFragmentDataPtr<FMassStateTreeInstanceFragment>();
	if (!Frag)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UMassStateTreeSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UMassStateTreeSubsystem>();
	UMassSignalSubsystem* Signal = MassCtx.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	FStateTreeReferenceOverrides* Overrides = Subsystem->GetLinkedStateTreeOverrides(Frag->InstanceHandle);
	InstanceData.StateTreeRef.SetStateTree(InstanceData.StateTree.Get());

	InstanceData.LinkedStateTreeOverrides.Reset();
	
	Overrides->AddOverride(InstanceData.StateTag, InstanceData.StateTreeRef);

	Signal->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
	return EStateTreeRunStatus::Succeeded;
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

FText FArcMassInjectStateTreeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "InjectStateTreeDesc", "Inject StateTree: {0}"), FText::FromString(GetNameSafe(InstanceData->StateTree)));
		}
	}
	return FText::GetEmpty();
}

FText FArcProvideNextStateTreeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "ProvideNextSTDesc", "Provide Next StateTree ({0} queued)"), FText::AsNumber(InstanceData->StateTrees.Num()));
		}
	}
	return FText::GetEmpty();
}
#endif
