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
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
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

UArcNeedsProcessor::UArcNeedsProcessor()
{
	//ProcessingPhase = EMassProcessingPhase::DuringPhysics;
}

void UArcNeedsProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	
	NeedsQuery.AddRequirement<FArcNeedsFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcNeedsProcessor::Execute(
	FMassEntityManager& EntityManager
  , FMassExecutionContext& Context)
{
	NeedsQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcNeedsFragment> Needs = Ctx.GetMutableFragmentView<FArcNeedsFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();

		const float DeltaTime = Ctx.GetDeltaTimeSeconds();
			
		for(int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
		{
			for(int32 NeedIdx = 0; NeedIdx < Needs[EntityIndex].Needs.Num(); NeedIdx++)
			{
				Needs[EntityIndex].Needs[NeedIdx].CurrentValue =
					Needs[EntityIndex].Needs[NeedIdx].CurrentValue + (Needs[EntityIndex].Needs[NeedIdx].ChangeRate * DeltaTime);


				Needs[EntityIndex].Needs[NeedIdx].CurrentValue = FMath::Clamp(Needs[EntityIndex].Needs[NeedIdx].CurrentValue, 0.f, 100.f);
			}
		}
	});
}


float FArcNeedstConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	float Score = 0.f;

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Actor)
	{
		return Score;
	}

	UMassAgentComponent* AgentComponent = InstanceData.Actor->FindComponentByClass<UMassAgentComponent>();
	if (!AgentComponent)
	{
		return Score;
	}

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FMassEntityHandle Handle = AgentComponent->GetEntityHandle();
	if (!Handle.IsValid())
	{
		return Score;
	}
	
	FArcNeedsFragment* Data = MEM.GetFragmentDataPtr<FArcNeedsFragment>(Handle);

	int32 Idx = Data->Needs.IndexOfByKey(InstanceData.NeedName);
	if (Idx == INDEX_NONE)
	{
		return Score;
	}

	
	const float Normalized = Data->Needs[Idx].CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

float FArcMassNeedsConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FArcNeedsFragment* Data = MEM.GetFragmentDataPtr<FArcNeedsFragment>(MassCtx.GetEntity());
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	float Score = 0.f;
	int32 Idx = Data->Needs.IndexOfByKey(InstanceData.NeedName);
	if (Idx == INDEX_NONE)
	{
		return Score;
	}

	
	const float Normalized = Data->Needs[Idx].CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

float FArcMassHungerNeedConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FArcHungerNeedFragment* Data = MEM.GetFragmentDataPtr<FArcHungerNeedFragment>(MassCtx.GetEntity());
	if (!Data)
	{
		return 1.f;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.NeedValue = Data->CurrentValue;
	
	float Score = 0.f;
	
	const float Normalized = Data->CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

bool FArcMassHungerNeedCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(HungerNeedHandle);
	return true;
}

void FArcMassHungerNeedCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcHungerNeedFragment>();
}

bool FArcMassHungerNeedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcHungerNeedFragment* HungerNeed = MassStateTreeContext.GetExternalDataPtr(HungerNeedHandle);
	if (!HungerNeed)
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcMassModifyHungerNeedTask::EnterState: Hunger Need not present."));
		return false;
	}
	
	switch (Operator)
	{
		case UE::StateTree::EComparisonOperator::Less:
			return HungerNeed->CurrentValue < InstanceData.NeedValue;
		case UE::StateTree::EComparisonOperator::LessOrEqual:
			return HungerNeed->CurrentValue <= InstanceData.NeedValue;
		case UE::StateTree::EComparisonOperator::Equal:
			return FMath::IsNearlyEqual(HungerNeed->CurrentValue, InstanceData.NeedValue, 0.01);
		case UE::StateTree::EComparisonOperator::NotEqual:
			return !FMath::IsNearlyEqual(HungerNeed->CurrentValue, InstanceData.NeedValue, 0.01);
		case UE::StateTree::EComparisonOperator::GreaterOrEqual:
			return HungerNeed->CurrentValue >= InstanceData.NeedValue;
		case UE::StateTree::EComparisonOperator::Greater:
			return HungerNeed->CurrentValue > InstanceData.NeedValue;
	}
	
	return false;
}

float FArcMassThirstNeedConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FArcThirstNeedFragment* Data = MEM.GetFragmentDataPtr<FArcThirstNeedFragment>(MassCtx.GetEntity());
	if (!Data)
	{
		return 1.f;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.NeedValue = Data->CurrentValue;
	
	float Score = 0.f;
	
	const float Normalized = Data->CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

float FArcMassFatigueNeedConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FArcFatigueNeedFragment* Data = MEM.GetFragmentDataPtr<FArcFatigueNeedFragment>(MassCtx.GetEntity());
	if (!Data)
	{
		return 1.f;
	}
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.NeedValue = Data->CurrentValue;
	
	float Score = 0.f;
	
	const float Normalized = Data->CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

FArcMassModifyHungerNeedTask::FArcMassModifyHungerNeedTask()
{
	bShouldCallTick = false;
}

bool FArcMassModifyHungerNeedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(HungerNeedHandle);
	return true;
}

void FArcMassModifyHungerNeedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcHungerNeedFragment>();
}

EStateTreeRunStatus FArcMassModifyHungerNeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcHungerNeedFragment* HungerNeed = MassStateTreeContext.GetExternalDataPtr(HungerNeedHandle);
	if (!HungerNeed)
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcMassModifyHungerNeedTask::EnterState: Hunger Need not present."));
		return EStateTreeRunStatus::Failed;
	}
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	HungerNeed->CurrentValue += InstanceData.AddValue;
	HungerNeed->CurrentValue = FMath::Clamp(HungerNeed->CurrentValue, 0.f, 100.f);
	
	return EStateTreeRunStatus::Succeeded;
}

FArcMassModifyThirstNeedTask::FArcMassModifyThirstNeedTask()
{
	bShouldCallTick = false;
}

bool FArcMassModifyThirstNeedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ThirstNeedHandle);
	return true;
}

void FArcMassModifyThirstNeedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcThirstNeedFragment>();
}

EStateTreeRunStatus FArcMassModifyThirstNeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcThirstNeedFragment* ThirstNeed = MassStateTreeContext.GetExternalDataPtr(ThirstNeedHandle);
	if (!ThirstNeed)
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcMassModifyHungerNeedTask::EnterState: Hunger Need not present."));
		return EStateTreeRunStatus::Failed;
	}
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	ThirstNeed->CurrentValue += InstanceData.AddValue;
	ThirstNeed->CurrentValue = FMath::Clamp(ThirstNeed->CurrentValue, 0.f, 100.f);
	
	return EStateTreeRunStatus::Succeeded;
}

FArcMassModifyFatigueNeedTask::FArcMassModifyFatigueNeedTask()
{
	bShouldCallTick = false;
}

bool FArcMassModifyFatigueNeedTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(FatigueNeedHandle);
	return true;
}

void FArcMassModifyFatigueNeedTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcFatigueNeedFragment>();
}

EStateTreeRunStatus FArcMassModifyFatigueNeedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcFatigueNeedFragment* FatigueNeed = MassStateTreeContext.GetExternalDataPtr(FatigueNeedHandle);
	if (!FatigueNeed)
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcMassModifyHungerNeedTask::EnterState: Hunger Need not present."));
		return EStateTreeRunStatus::Failed;
	}
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FatigueNeed->CurrentValue += InstanceData.AddValue;
	FatigueNeed->CurrentValue = FMath::Clamp(FatigueNeed->CurrentValue, 0.f, 100.f);
	
	return EStateTreeRunStatus::Succeeded;
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
