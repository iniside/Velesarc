#include "ArcSmartObjectPlannerProcessor.h"

#include "ArcSmartObjectPlannerSubsystem.h"
#include "ArcSmartObjectPlanResponse.h"
#include "GameplayBehavior.h"
#include "GameplayBehaviorConfig.h"
#include "GameplayBehaviorSmartObjectBehaviorDefinition.h"
#include "GameplayBehaviorSubsystem.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "MassActorSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassNavMeshNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectBehaviorDefinition.h"
#include "MassSmartObjectHandler.h"
#include "MassSmartObjectSettings.h"
#include "MassStateTreeExecutionContext.h"
#include "NavCorridor.h"
#include "NavigationSystem.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "InstancedActors/ArcCoreInstancedActorsSubsystem.h"
#include "NeedsSystem/ArcNeedsFragment.h"

void UArcMassGoalPlanInfoTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	//BuildContext.AddFragment<FArcSmartObjectOwnerFragment>();

	const FConstSharedStruct SmartObjectDefFragment = EntityManager.GetOrCreateConstSharedFragment(GoalPlanInfo);
	BuildContext.AddConstSharedFragment(SmartObjectDefFragment);
};




UArcSmartObjectPlannerProcessor::UArcSmartObjectPlannerProcessor()
{
	//ProcessingPhase = EMassProcessingPhase::DuringPhysics;
}

void UArcSmartObjectPlannerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.Initialize(EntityManager);
	
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FArcSmartObjectOwnerFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddConstSharedRequirement<FArcMassGoalPlanInfoSharedFragment>();
	
	ProcessorRequirements.AddSubsystemRequirement<UArcSmartObjectPlannerSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	
	Query.RegisterWithProcessor(*this);
}

void UArcSmartObjectPlannerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSmartObjectPlannerSubsystem* MassEQSSubsystem = Context.GetMutableSubsystem<UArcSmartObjectPlannerSubsystem>();
	if (MassEQSSubsystem->Requests.IsEmpty())
	{
		return;
	}
	
	FArcSmartObjectPlanRequest Request = MassEQSSubsystem->Requests.Pop();
	if (!Request.IsValid())
	{
		return;
	}

	USmartObjectSubsystem* SOSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();

	FArcSmartObjectPlanResponse Response;
	Response.Handle = Request.Handle;
	Response.AccumulatedTags.AppendTags(Request.InitialTags);
	
	Response.Plans.Reserve(32);
	
	TArray<FArcPotentialEntity> PotentialEntities;
	PotentialEntities.Reserve(64);

	TRACE_CPUPROFILER_EVENT_SCOPE(UArcEntityGoalPlannerProcessor::Execute);
	Query.ForEachEntityChunk(Context, [this, &Request, &PotentialEntities, SOSubsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FTransformFragment> TransformFragmentList = Ctx.GetFragmentView<FTransformFragment>();
		TConstArrayView<FArcSmartObjectOwnerFragment> SmartObjectOwnerFragments = Ctx.GetFragmentView<FArcSmartObjectOwnerFragment>();
		const FArcMassGoalPlanInfoSharedFragment& GoalPlanInfoSharedFragment = Ctx.GetConstSharedFragment<FArcMassGoalPlanInfoSharedFragment>();
			
		float SearchRadiusSqr = FMath::Square(Request.SearchRadius);
			
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);
			const FTransformFragment& TransformFragment = TransformFragmentList[EntityIt];
			const FArcSmartObjectOwnerFragment& SmartObjectOwnerFragment = SmartObjectOwnerFragments[EntityIt];
			const FVector& EntityPosition = TransformFragment.GetTransform().GetTranslation();
		
			const FVector::FReal ContextDistanceToEntitySqr = FVector::DistSquared(EntityPosition, Request.SearchOrigin);
			
			
			if (ContextDistanceToEntitySqr <= SearchRadiusSqr)
			{
				FArcPotentialEntity PotentialEntity;
				PotentialEntity.RequestingEntity = Request.RequestingEntity;
				PotentialEntity.EntityHandle = EntityHandle;
				PotentialEntity.Location = EntityPosition;
				PotentialEntity.Provides = GoalPlanInfoSharedFragment.Provides;
				PotentialEntity.Requires = GoalPlanInfoSharedFragment.Requires;

				for (const auto& Cond : GoalPlanInfoSharedFragment.CustomConditions)
				{
					PotentialEntity.CustomConditions.Add(FConstStructView(Cond.GetScriptStruct(), Cond.GetMemory()));	
				}
				
				PotentialEntity.bUsedInPlan = false;

				TArray<FSmartObjectSlotHandle> OutSlots;
				SOSubsystem->FindSlots(SmartObjectOwnerFragment.SmartObjectHandle, FSmartObjectRequestFilter(), /*out*/ OutSlots, {});

				for (int32 SlotIndex = 0; SlotIndex < OutSlots.Num(); SlotIndex++)
				{
					if (SlotIndex >= 4)
					{
						PotentialEntity.FoundCandidateSlots.NumSlots = 4;
						break;
					}
					
					const FSmartObjectSlotHandle& SlotHandle = OutSlots[SlotIndex];
					FSmartObjectRequestResult Result;
					
					FSmartObjectCandidateSlot CandidateSlot;
					CandidateSlot.Result.SmartObjectHandle = SmartObjectOwnerFragment.SmartObjectHandle;
					CandidateSlot.Result.SlotHandle = SlotHandle;
					PotentialEntity.FoundCandidateSlots.Slots[SlotIndex] = CandidateSlot;
					PotentialEntity.FoundCandidateSlots.NumSlots++;
				}
				
				PotentialEntities.Add(PotentialEntity);
			}
		}
	});

	//UE::Tasks::FPipe

	//  Run this as task ?
	MassEQSSubsystem->BuildAllPlans(Request, PotentialEntities);
}

FArcMassUseSmartObjectTask::FArcMassUseSmartObjectTask()
{
	bShouldCallTick = false;
}

bool FArcMassUseSmartObjectTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	return true;
}

void FArcMassUseSmartObjectTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite(SmartObjectSubsystemHandle);
	Builder.AddReadWrite(MassSignalSubsystemHandle);
	
	Builder.AddReadWrite(SmartObjectUserHandle);
	Builder.AddReadWrite(MoveTargetHandle);
}

EStateTreeRunStatus FArcMassUseSmartObjectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const UWorld* World = Context.GetWorld();
	
	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();

	UMassEntitySubsystem* MassEntitySubsystem = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	FMassActorFragment* ActorFragment = MassCtx.GetEntityManager().GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity());
	AActor* Interactor = const_cast<AActor*>(ActorFragment->Get());

	bool bHaveRunActorBehavior = false;
	if (Interactor)
	{
		const UArcAIGameplayInteractionSmartObjectBehaviorDefinition* GI = SOSubsystem->GetBehaviorDefinition<UArcAIGameplayInteractionSmartObjectBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
		if (GI)
		{
			InstanceData.GameplayInteractionContext.SetContextEntity(MassCtx.GetEntity());
			InstanceData.GameplayInteractionContext.SetSmartObjectEntity(InstanceData.SmartObjectEntityHandle.EntityHandle);
			
			InstanceData.GameplayInteractionContext.SetContextActor(Interactor);
			InstanceData.GameplayInteractionContext.SetSmartObjectActor(Interactor);
			InstanceData.GameplayInteractionContext.SetClaimedHandle(InstanceData.SmartObjectClaimHandle);

			InstanceData.GameplayInteractionContext.Activate(*GI);

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(Interactor, [SignalSubsystem, SOSubsystem, Interactor, WeakContext = Context.MakeWeakExecutionContext(), EntityHandle = MassCtx.GetEntity()](float DeltaTime)
			{
				const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
					if (!DataPtr)
					{
						return false;
					}

					const bool bKeepTicking = DataPtr->GameplayInteractionContext.Tick(DeltaTime);
					if (bKeepTicking == false)
					{
						SOSubsystem->Release(DataPtr->SmartObjectClaimHandle);
						StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {EntityHandle});
						return false;;
					}

					return true;
			}));

			bHaveRunActorBehavior = true;
		}

		const UGameplayBehaviorSmartObjectBehaviorDefinition* BD = SOSubsystem->GetBehaviorDefinition<UGameplayBehaviorSmartObjectBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
		if (BD)
		{
			const UGameplayBehaviorConfig* GameplayBehaviorConfig = BD != nullptr ? BD->GameplayBehaviorConfig.Get() : nullptr;
			InstanceData.GameplayBehavior = GameplayBehaviorConfig != nullptr ? GameplayBehaviorConfig->GetBehavior(*Context.GetWorld()) : nullptr;

			if (!InstanceData.GameplayBehavior)
			{
				return EStateTreeRunStatus::Failed;
			}
		
	
			const bool bBehaviorActive = UGameplayBehaviorSubsystem::TriggerBehavior(*InstanceData.GameplayBehavior
				, *Interactor, GameplayBehaviorConfig, nullptr);

			if (bBehaviorActive)
			{
				InstanceData.GameplayBehavior->GetOnBehaviorFinishedDelegate().AddWeakLambda(MassEntitySubsystem, [SOSubsystem, WeakContext = Context.MakeWeakExecutionContext(), EntityHandle = MassCtx.GetEntity(), SignalSubsystem]
					(UGameplayBehavior& InBehavior, AActor& /*Avatar*/, const bool /*bInterrupted*/)
				{
					const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
					FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
					//if (DataPtr && DataPtr->GameplayBehavior == &InBehavior)
					{
						SOSubsystem->Release(DataPtr->SmartObjectClaimHandle);
						DataPtr->GameplayBehavior = nullptr;
						StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {EntityHandle});
					}
				});

				bHaveRunActorBehavior = true;
			}	
		}
	}

	
	bool bRunMassBehavior = false;
	if (bAlwaysRunMassBehavior || bHaveRunActorBehavior == false)
	{
		bRunMassBehavior = true;
	}

	const USmartObjectMassBehaviorDefinition* MBD = SOSubsystem->GetBehaviorDefinition<USmartObjectMassBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
	if (bRunMassBehavior && MBD)
	{
		const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassStateTreeContext.GetMassEntityExecutionContext(), *SOSubsystem, *SignalSubsystem);

		FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	
		if (SOUser.InteractionHandle.IsValid())
		{
			//MASSBEHAVIOR_LOG(Error, TEXT("Agent is already using smart object slot %s."), *LexToString(SOUser.InteractionHandle));
			return bHaveRunActorBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}
		
		if (!MassSmartObjectHandler.StartUsingSmartObject(MassStateTreeContext.GetEntity(), SOUser, InstanceData.SmartObjectClaimHandle))
		{
			return bHaveRunActorBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}

		FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

		MoveTarget.CreateNewAction(EMassMovementAction::Animate, *World);

		bool bSuccess = UE::MassNavigation::ActivateActionAnimate(Interactor, MassStateTreeContext.GetEntity(), MoveTarget);	
		
		return bSuccess ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}
	
	return bHaveRunActorBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FArcMassUseSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitio) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.GameplayBehavior = nullptr;
	if (InstanceData.GameplayInteractionContext.IsValid())
	{
		InstanceData.GameplayInteractionContext.Deactivate();
	}

	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	if (SOUser.InteractionHandle.IsValid())
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with a valid InteractionHandle: stop using the smart object."));

		const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassStateTreeContext.GetMassEntityExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
		MassSmartObjectHandler.StopUsingSmartObject(MassStateTreeContext.GetEntity(), SOUser, EMassSmartObjectInteractionStatus::Aborted);
	}
	else
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with an invalid ClaimHandle: nothing to do."));
	}
	
	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	SOSubsystem->Release(InstanceData.SmartObjectClaimHandle);
}