// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayInteractionContext.h"

#include "GameplayInteractionContext.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "Engine/World.h"
#include "MassEntityTypes.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"
#include "GameplayInteractionStateTreeSchema.h"
#include "SmartObjectSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGameplayInteractionContext)

namespace UE::ArcGameplayInteraction::Private
{

/**
 * Helper to create the appropriate StateTree execution context.
 * When MassContext is provided, creates FMassStateTreeExecutionContext enabling Mass node support.
 */
struct FExecutionContextHolder
{
	TUniquePtr<FStateTreeExecutionContext> Context;

	static FExecutionContextHolder Create(
		UObject& Owner,
		const UStateTree& StateTree,
		FStateTreeInstanceData& InstanceData,
		FMassExecutionContext* MassContext,
		const FMassEntityHandle& EntityHandle)
	{
		FExecutionContextHolder Holder;
		if (MassContext)
		{
			auto* MassCtx = new FMassStateTreeExecutionContext(Owner, StateTree, InstanceData, *MassContext);
			MassCtx->SetEntity(EntityHandle);
			Holder.Context.Reset(MassCtx);
		}
		else
		{
			Holder.Context.Reset(new FStateTreeExecutionContext(Owner, StateTree, InstanceData));
		}
		return Holder;
	}
};

} // namespace UE::ArcGameplayInteraction::Private

bool FArcGameplayInteractionContext::Activate(const UArcGameplayInteractionSmartObjectBehaviorDefinition& InDefinition, FMassExecutionContext* MassContext)
{
	const FStateTreeReference& StateTreeReference = InDefinition.StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();

	UObject* EffectiveOwner = GetOwner();

	if (Definition)
	{
		if (const UStateTree* PrevTree = Definition->GetStateTree())
		{
			FStateTreeReadOnlyExecutionContext PrevStateTreeContext(EffectiveOwner, PrevTree, StateTreeInstanceData);
			if (PrevStateTreeContext.GetStateTreeRunStatus() == EStateTreeRunStatus::Running && PrevTree != StateTree)
			{
				UE_LOG(LogGameplayInteractions, Error, TEXT("Trying to activate interaction with different root State Tree while the previous tree is still running!"));
				return false;
			}
		}
	}

	Definition = &InDefinition;

	if (!IsValid())
	{
		UE_LOG(LogGameplayInteractions, Error, TEXT("Failed to activate interaction. Context is not properly setup."));
		return false;
	}

	if (StateTree == nullptr)
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Definition %s doesn't point to a valid StateTree asset."),
			*GetNameSafe(EffectiveOwner),
			*Definition.GetFullName());
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
		UE_LOG(LogGameplayInteractions, Error, TEXT("Reentrant call to %hs is not allowed!"), __FUNCTION__);
		return false;
	}

	auto Holder = UE::ArcGameplayInteraction::Private::FExecutionContextHolder::Create(*EffectiveOwner, *StateTree, StateTreeInstanceData, MassContext, ContextEntity);
	FStateTreeExecutionContext& StateTreeContext = *Holder.Context;
	TGuardValue<FStateTreeExecutionContext*> ReentrantExecContextGuard(CurrentlyRunningExecContext, &StateTreeContext);

	if (!StateTreeContext.IsValid())
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Unable to initialize StateTree execution context for StateTree asset: %s."),
			*GetNameSafe(EffectiveOwner),
			*StateTree->GetFullName());
		return false;
	}

	if (!ValidateSchema(StateTreeContext))
	{
		return false;
	}

	if (!SetContextRequirements(StateTreeContext, MassContext))
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Unable to provide all external data views for StateTree asset: %s."),
			*GetNameSafe(EffectiveOwner),
			*StateTree->GetFullName());
		return false;
	}

	// Set slot user actor (only when we have an actor)
	if (ContextActor)
	{
		USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(ContextActor->GetWorld());
		check(SmartObjectSubsystem);
		SmartObjectSubsystem->MutateSlotData(ClaimedHandle.SlotHandle, [this, SmartObjectSubsystem](const FSmartObjectSlotView& SlotView)
		{
			if (FGameplayInteractionSlotUserData* UserData = SlotView.GetMutableStateDataPtr<FGameplayInteractionSlotUserData>())
			{
				UserData->UserActor = ContextActor;
			}
			else
			{
				SmartObjectSubsystem->AddSlotData(ClaimedHandle, FConstStructView::Make(FGameplayInteractionSlotUserData(ContextActor)));
			}
		});
	}

	// Start State Tree — FMassStateTreeExecutionContext::Start injects FMassExecutionExtension with entity handle
	if (MassContext)
	{
		static_cast<FMassStateTreeExecutionContext&>(StateTreeContext).Start(&StateTreeReference.GetParameters());
	}
	else
	{
		StateTreeContext.Start(&StateTreeReference.GetParameters());
	}

	return true;
}

bool FArcGameplayInteractionContext::Tick(const float DeltaTime, FMassExecutionContext* MassContext)
{
	if (Definition == nullptr || !IsValid())
	{
		return false;
	}

	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	if (StateTree == nullptr)
	{
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
		UE_LOG(LogGameplayInteractions, Error, TEXT("Reentrant call to %hs is not allowed!"), __FUNCTION__);
		return false;
	}

	UObject* EffectiveOwner = GetOwner();
	auto Holder = UE::ArcGameplayInteraction::Private::FExecutionContextHolder::Create(*EffectiveOwner, *StateTree, StateTreeInstanceData, MassContext, ContextEntity);
	FStateTreeExecutionContext& StateTreeContext = *Holder.Context;
	TGuardValue<FStateTreeExecutionContext*> ReentrantExecContextGuard(CurrentlyRunningExecContext, &StateTreeContext);

	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;
	if (SetContextRequirements(StateTreeContext, MassContext))
	{
		RunStatus = StateTreeContext.Tick(DeltaTime);
	}

	LastRunStatus = RunStatus;

	return RunStatus == EStateTreeRunStatus::Running;
}

void FArcGameplayInteractionContext::Deactivate(FMassExecutionContext* MassContext)
{
	auto StopTree = [this](FStateTreeExecutionContext& Context)
	{
		Context.Stop();
	};

	UObject* EffectiveOwner = GetOwner();
	if (Definition == nullptr || EffectiveOwner == nullptr)
	{
		return;
	}

	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	if (StateTree == nullptr)
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to deactivate interaction for %s. Definition %s doesn't point to a valid StateTree asset."),
			*GetNameSafe(EffectiveOwner),
			*Definition.GetFullName());

		return;
	}

	if (CurrentlyRunningExecContext)
	{
		// Tree Stop will be delayed to the end of the frame in a reentrant call, but we need to use the existing execution context
		StopTree(*CurrentlyRunningExecContext);
	}
	else
	{
		auto Holder = UE::ArcGameplayInteraction::Private::FExecutionContextHolder::Create(*EffectiveOwner, *StateTree, StateTreeInstanceData, MassContext, ContextEntity);
		FStateTreeExecutionContext& StateTreeContext = *Holder.Context;
		if (SetContextRequirements(StateTreeContext, MassContext))
		{
			StopTree(StateTreeContext);
		}
	}

	// Remove user (only when we have an actor)
	if (ContextActor)
	{
		const USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(ContextActor->GetWorld());
		check(SmartObjectSubsystem);
		SmartObjectSubsystem->MutateSlotData(ClaimedHandle.SlotHandle, [](const FSmartObjectSlotView& SlotView)
		{
			if (FGameplayInteractionSlotUserData* UserData = SlotView.GetMutableStateDataPtr<FGameplayInteractionSlotUserData>())
			{
				UserData->UserActor = nullptr;
			}
		});
	}
}

void FArcGameplayInteractionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	if (Definition == nullptr || !IsValid())
	{
		return;
	}

	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	if (!StateTree)
	{
		return;
	}
	FStateTreeMinimalExecutionContext StateTreeContext(GetOwner(), StateTree, StateTreeInstanceData);
	StateTreeContext.SendEvent(Tag, Payload, Origin);
}

bool FArcGameplayInteractionContext::ValidateSchema(const FStateTreeExecutionContext& StateTreeContext) const
{
	// Ensure that the actor and smart object match the schema.
	const UGameplayInteractionStateTreeSchema* Schema = Cast<UGameplayInteractionStateTreeSchema>(StateTreeContext.GetStateTree()->GetSchema());
	if (Schema == nullptr)
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Expecting %s schema for StateTree asset: %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UGameplayInteractionStateTreeSchema::StaticClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));

		return false;
	}

	// Validate actor types only when actors are provided
	if (ContextActor && Schema->GetContextActorClass() && !ContextActor->IsA(Schema->GetContextActorClass()))
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Expecting Actor to be of type %s (found %s) for StateTree asset: %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Schema->GetContextActorClass()),
			*GetNameSafe(ContextActor->GetClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));

		return false;
	}
	if (SmartObjectActor && Schema->GetSmartObjectActorClass() && !SmartObjectActor->IsA(Schema->GetSmartObjectActorClass()))
	{
		UE_LOG(LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. SmartObject Actor to be of type %s (found %s) for StateTree asset: %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Schema->GetSmartObjectActorClass()),
			*GetNameSafe(SmartObjectActor->GetClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));

		return false;
	}

	return true;
}

bool FArcGameplayInteractionContext::SetContextRequirements(FStateTreeExecutionContext& StateTreeContext, FMassExecutionContext* MassContext)
{
	if (!StateTreeContext.IsValid())
	{
		return false;
	}

	if (!::IsValid(Definition))
	{
		return false;
	}

	if (ContextActor)
	{
		StateTreeContext.SetContextDataByName(UE::GameplayInteraction::Names::ContextActor, FStateTreeDataView(ContextActor));
	}
	if (SmartObjectActor)
	{
		StateTreeContext.SetContextDataByName(UE::GameplayInteraction::Names::SmartObjectActor, FStateTreeDataView(SmartObjectActor));
	}
	StateTreeContext.SetContextDataByName(UE::GameplayInteraction::Names::SmartObjectClaimedHandle, FStateTreeDataView(FStructView::Make(ClaimedHandle)));
	StateTreeContext.SetContextDataByName(UE::GameplayInteraction::Names::SlotEntranceHandle, FStateTreeDataView(FStructView::Make(SlotEntranceHandle)));
	StateTreeContext.SetContextDataByName(UE::GameplayInteraction::Names::AbortContext, FStateTreeDataView(FStructView::Make(AbortContext)));
	StateTreeContext.SetContextDataByName(TEXT("ContextEntityHandle"), FStateTreeDataView(FStructView::Make(ContextEntity)));
	StateTreeContext.SetContextDataByName(TEXT("SmartObjectEntityHandle"), FStateTreeDataView(FStructView::Make(SmartObjectEntity)));
	StateTreeContext.SetContextDataByName(TEXT("SlotTransform"), FStateTreeDataView(FStructView::Make(SlotTransform)));

	UObject* EffectiveOwner = GetOwner();
	checkf(EffectiveOwner != nullptr, TEXT("Should never reach this point with an invalid Owner since IsValid() is checked before."));
	const UWorld* World = EffectiveOwner->GetWorld();

	FMassEntityManager* EntityManagerPtr = MassContext ? &MassContext->GetEntityManagerChecked() : nullptr;
	const FMassEntityHandle CapturedEntity = ContextEntity;

	StateTreeContext.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateLambda(
		[World, ContextActor = ContextActor.Get(), EntityManagerPtr, CapturedEntity]
		(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDescs, TArrayView<FStateTreeDataView> OutDataViews)
		{
			check(ExternalDescs.Num() == OutDataViews.Num());

			const bool bHasEntityView = EntityManagerPtr != nullptr && CapturedEntity.IsValid();

			for (int32 Index = 0; Index < ExternalDescs.Num(); Index++)
			{
				const FStateTreeExternalDataDesc& Desc = ExternalDescs[Index];
				if (Desc.Struct == nullptr)
				{
					continue;
				}

				// Mass fragments — resolve from entity
				if (bHasEntityView && UE::Mass::IsA<FMassFragment>(Desc.Struct))
				{
					const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Desc.Struct);
					const FMassEntityView EntityView(*EntityManagerPtr, CapturedEntity);
					FStructView Fragment = EntityView.GetFragmentDataStruct(ScriptStruct);
					if (Fragment.IsValid())
					{
						OutDataViews[Index] = FStateTreeDataView(Fragment);
					}
					else if (Desc.Requirement == EStateTreeExternalDataRequirement::Required)
					{
						UE_LOG(LogGameplayInteractions, Error, TEXT("Missing Fragment: %s"), *GetNameSafe(ScriptStruct));
					}
				}
				else if (bHasEntityView && UE::Mass::IsA<FMassSharedFragment>(Desc.Struct))
				{
					const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Desc.Struct);
					const FMassEntityView EntityView(*EntityManagerPtr, CapturedEntity);
					FStructView Fragment = EntityView.GetSharedFragmentDataStruct(ScriptStruct);
					if (Fragment.IsValid())
					{
						OutDataViews[Index] = FStateTreeDataView(Fragment.GetScriptStruct(), Fragment.GetMemory());
					}
					else if (Desc.Requirement == EStateTreeExternalDataRequirement::Required)
					{
						UE_LOG(LogGameplayInteractions, Error, TEXT("Missing Shared Fragment: %s"), *GetNameSafe(ScriptStruct));
					}
				}
				else if (bHasEntityView && UE::Mass::IsA<FMassConstSharedFragment>(Desc.Struct))
				{
					const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(Desc.Struct);
					const FMassEntityView EntityView(*EntityManagerPtr, CapturedEntity);
					FConstStructView Fragment = EntityView.GetConstSharedFragmentDataStruct(ScriptStruct);
					if (Fragment.IsValid())
					{
						OutDataViews[Index] = FStateTreeDataView(Fragment.GetScriptStruct(), const_cast<uint8*>(Fragment.GetMemory()));
					}
					else if (Desc.Requirement == EStateTreeExternalDataRequirement::Required)
					{
						UE_LOG(LogGameplayInteractions, Error, TEXT("Missing Const Shared Fragment: %s"), *GetNameSafe(ScriptStruct));
					}
				}
				// World subsystems
				else if (World != nullptr && Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
				{
					UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(ToRawPtr(Desc.Struct))));
					if (Subsystem)
					{
						OutDataViews[Index] = FStateTreeDataView(Subsystem);
					}
					else if (Desc.Requirement == EStateTreeExternalDataRequirement::Required)
					{
						UE_LOG(LogGameplayInteractions, Error, TEXT("Missing Subsystem: %s"), *GetNameSafe(Desc.Struct));
					}
				}
				// Actors
				else if (ContextActor != nullptr && Desc.Struct->IsChildOf(AActor::StaticClass()))
				{
					OutDataViews[Index] = FStateTreeDataView(ContextActor);
				}
				// Actor components
				else if (ContextActor != nullptr && Desc.Struct->IsChildOf(UActorComponent::StaticClass()))
				{
					if (UActorComponent* Component = ContextActor->FindComponentByClass(Cast<UClass>(const_cast<UStruct*>(ToRawPtr(Desc.Struct)))))
					{
						OutDataViews[Index] = FStateTreeDataView(Component);
					}
					else if (Desc.Requirement == EStateTreeExternalDataRequirement::Required)
					{
						UE_LOG(LogGameplayInteractions, Error, TEXT("Missing ActorComponent: %s"), *GetNameSafe(Desc.Struct));
					}
				}
			}

			return true;
		})
	);

	return StateTreeContext.AreContextDataViewsValid();
}

UArcGameplayInteractionStateTreeSchema::UArcGameplayInteractionStateTreeSchema()
	: Super()
{
	ContextDataDescs.Add({ TEXT("ContextEntityHandle"), FMassEntityHandle::StaticStruct(), FGuid("1c514052-47d0-4c8b-8308-056175ddaa5e")});
	ContextDataDescs.Add({ TEXT("SmartObjectEntityHandle"), FMassEntityHandle::StaticStruct(), FGuid("f640404a-c5b8-4c45-a6e4-a2707254fa5a")});
	ContextDataDescs.Add({ TEXT("SlotTransform"), TBaseStructure<FTransform>::Get(), FGuid("a3e17c84-5f92-4d1b-b8a3-6c4d2e9f0a15")});
	
	for (FStateTreeExternalDataDesc& DataDesc : ContextDataDescs)
	{
		if (DataDesc.Name == UE::GameplayInteraction::Names::SmartObjectActor)
		{
			DataDesc.Requirement = EStateTreeExternalDataRequirement::Optional;	
		}
		
	}
}

bool UArcGameplayInteractionStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	// Allow everything the parent GameplayInteraction schema allows, plus Mass node types
	return Super::IsStructAllowed(InScriptStruct)
		|| InScriptStruct->IsChildOf(FMassStateTreeEvaluatorBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FMassStateTreeTaskBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FMassStateTreeConditionBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionBase::StaticStruct());
}

bool UArcGameplayInteractionStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// Allow everything the parent allows, plus Mass fragments
	return Super::IsExternalItemAllowed(InStruct)
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass())
		|| UE::Mass::IsA<FMassFragment>(&InStruct)
		|| UE::Mass::IsA<FMassSharedFragment>(&InStruct)
		|| UE::Mass::IsA<FMassConstSharedFragment>(&InStruct);
}

bool UArcGameplayInteractionStateTreeSchema::Link(FStateTreeLinker& Linker)
{
	if (!Super::Link(Linker))
	{
		return false;
	}

	// Collect Mass dependencies from all nodes
	Dependencies.Empty();
	UE::MassBehavior::FStateTreeDependencyBuilder Builder(Dependencies);

	auto BuildDependencies = [&Builder](const UStateTree* StateTree)
	{
		for (FConstStructView Node : StateTree->GetNodes())
		{
			if (const FMassStateTreeEvaluatorBase* Evaluator = Node.GetPtr<const FMassStateTreeEvaluatorBase>())
			{
				Evaluator->GetDependencies(Builder);
			}
			else if (const FMassStateTreeTaskBase* Task = Node.GetPtr<const FMassStateTreeTaskBase>())
			{
				Task->GetDependencies(Builder);
			}
			else if (const FMassStateTreeConditionBase* Condition = Node.GetPtr<const FMassStateTreeConditionBase>())
			{
				Condition->GetDependencies(Builder);
			}
			else if (const FMassStateTreePropertyFunctionBase* PropertyFunction = Node.GetPtr<const FMassStateTreePropertyFunctionBase>())
			{
				PropertyFunction->GetDependencies(Builder);
			}
		}
	};

	TArray<const UStateTree*, TInlineAllocator<4>> StateTrees;
	StateTrees.Add(CastChecked<const UStateTree>(GetOuter()));
	for (int32 Index = 0; Index < StateTrees.Num(); ++Index)
	{
		const UStateTree* StateTree = StateTrees[Index];
		BuildDependencies(StateTree);

		for (const FCompactStateTreeState& State : StateTree->GetStates())
		{
			if (State.LinkedAsset && State.Type == EStateTreeStateType::LinkedAsset)
			{
				StateTrees.AddUnique(State.LinkedAsset);
			}
		}
	}

	return true;
}

#if WITH_EDITOR
void UArcGameplayInteractionSmartObjectBehaviorDefinition::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	
	if (UStateTree* StateTree = StateTreeReference.GetMutableStateTree())
	{
		OutDeps.Add(StateTree);
	}
}
#endif