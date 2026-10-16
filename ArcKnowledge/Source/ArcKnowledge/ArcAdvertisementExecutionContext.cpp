// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAdvertisementExecutionContext.h"
#include "ArcAdvertisementInstruction_StateTree.h"
#include "ArcAdvertisementStateTreeSchema.h"
#include "Engine/World.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAdvertisementExecutionContext)

DEFINE_LOG_CATEGORY(LogArcAdvertisement);

namespace UE::ArcKnowledge::Private
{

/**
 * Helper to create and configure the appropriate StateTree execution context.
 * When MassContext is provided, creates FMassStateTreeExecutionContext enabling Mass node support.
 * Returns a unique_ptr that holds the context on the heap (since FMassStateTreeExecutionContext
 * and FStateTreeExecutionContext are different sizes, we can't stack-allocate polymorphically).
 */
struct FExecutionContextHolder
{
	TUniquePtr<FStateTreeExecutionContext> Context;

	static FExecutionContextHolder Create(
		UObject& Owner,
		const UStateTree& StateTree,
		FStateTreeInstanceData& InstanceData,
		FMassExecutionContext* MassContext,
		const FMassEntityHandle& ExecutingEntity)
	{
		FExecutionContextHolder Holder;
		if (MassContext)
		{
			auto* MassCtx = new FMassStateTreeExecutionContext(Owner, StateTree, InstanceData, *MassContext);
			MassCtx->SetEntity(ExecutingEntity);
			Holder.Context.Reset(MassCtx);
		}
		else
		{
			Holder.Context.Reset(new FStateTreeExecutionContext(Owner, StateTree, InstanceData));
		}
		return Holder;
	}
};

} // namespace UE::ArcKnowledge::Private

bool FArcAdvertisementExecutionContext::Activate(const FArcAdvertisementInstruction_StateTree& Instruction, FMassExecutionContext* MassContext)
{
	const FStateTreeReference& StateTreeReference = Instruction.StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();

	if (!IsValid())
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to activate advertisement behavior. Context is not properly setup (no Owner).");
		return false;
	}

	if (StateTree == nullptr)
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to activate advertisement behavior for %ls. Instruction doesn't point to a valid StateTree asset.",
			*GetNameSafe(Owner));
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
		UE_LOGF(LogArcAdvertisement, Error,"Reentrant call to %s is not allowed!", __FUNCTION__);
		return false;
	}

	auto Holder = UE::ArcKnowledge::Private::FExecutionContextHolder::Create(*Owner, *StateTree, StateTreeInstanceData, MassContext, ExecutingEntity);
	FStateTreeExecutionContext& StateTreeContext = *Holder.Context;
	TGuardValue<FStateTreeExecutionContext*> ReentrantGuard(CurrentlyRunningExecContext, &StateTreeContext);

	if (!StateTreeContext.IsValid())
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to activate advertisement behavior for %ls. Unable to initialize StateTree execution context for: %ls.",
			*GetNameSafe(Owner), *StateTree->GetFullName());
		return false;
	}

	// Validate schema type
	const UArcAdvertisementStateTreeSchema* Schema = Cast<UArcAdvertisementStateTreeSchema>(StateTreeContext.GetStateTree()->GetSchema());
	if (Schema == nullptr)
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to activate advertisement behavior for %ls. Expecting %ls schema for StateTree asset: %ls.",
			*GetNameSafe(Owner),
			*GetNameSafe(UArcAdvertisementStateTreeSchema::StaticClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));
		return false;
	}

	// Validate actor class if an actor is provided and the schema specifies a class
	if (ContextActor && Schema->GetContextActorClass() && !ContextActor->IsA(Schema->GetContextActorClass()))
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to activate advertisement behavior. Expecting Actor of type %ls (found %ls) for StateTree asset: %ls.",
			*GetNameSafe(Schema->GetContextActorClass()),
			*GetNameSafe(ContextActor->GetClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));
		return false;
	}

	if (!SetContextRequirements(StateTreeContext, MassContext))
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to activate advertisement behavior for %ls. Unable to provide all external data views for StateTree asset: %ls.",
			*GetNameSafe(Owner), *StateTree->GetFullName());
		return false;
	}

	// Cache the reference for Tick/Deactivate
	ActiveStateTreeReference = &StateTreeReference;

	// Start the tree — FMassStateTreeExecutionContext::Start injects FMassExecutionExtension with entity handle
	if (MassContext)
	{
		static_cast<FMassStateTreeExecutionContext&>(StateTreeContext).Start(&StateTreeReference.GetParameters());
	}
	else
	{
		StateTreeContext.Start(StateTreeReference.GetParameters().GetValue());
	}

	return true;
}

bool FArcAdvertisementExecutionContext::Tick(const float DeltaTime, FMassExecutionContext* MassContext)
{
	if (ActiveStateTreeReference == nullptr || !IsValid())
	{
		return false;
	}

	const UStateTree* StateTree = ActiveStateTreeReference->GetStateTree();
	if (StateTree == nullptr)
	{
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
				UE_LOGF(LogArcAdvertisement, Error,"Reentrant call to %s is not allowed!", __FUNCTION__);
		return false;
	}

	auto Holder = UE::ArcKnowledge::Private::FExecutionContextHolder::Create(*Owner, *StateTree, StateTreeInstanceData, MassContext, ExecutingEntity);
	FStateTreeExecutionContext& StateTreeContext = *Holder.Context;
	TGuardValue<FStateTreeExecutionContext*> ReentrantGuard(CurrentlyRunningExecContext, &StateTreeContext);

	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;
	if (SetContextRequirements(StateTreeContext, MassContext))
	{
		RunStatus = StateTreeContext.Tick(DeltaTime);
	}

	LastRunStatus = RunStatus;
	return RunStatus == EStateTreeRunStatus::Running;
}

void FArcAdvertisementExecutionContext::Deactivate(FMassExecutionContext* MassContext)
{
	auto StopTree = [this](FStateTreeExecutionContext& Context)
	{
		Context.Stop();
	};

	if (ActiveStateTreeReference == nullptr || Owner == nullptr)
	{
		return;
	}

	const UStateTree* StateTree = ActiveStateTreeReference->GetStateTree();
	if (StateTree == nullptr)
	{
		UE_LOGF(LogArcAdvertisement, Error,"Failed to deactivate advertisement behavior for %ls. No valid StateTree asset.",
			*GetNameSafe(Owner));
		return;
	}

	if (CurrentlyRunningExecContext)
	{
		// Reentrant: use the existing context
		StopTree(*CurrentlyRunningExecContext);
	}
	else
	{
		auto Holder = UE::ArcKnowledge::Private::FExecutionContextHolder::Create(*Owner, *StateTree, StateTreeInstanceData, MassContext, ExecutingEntity);
		FStateTreeExecutionContext& StateTreeContext = *Holder.Context;
		if (SetContextRequirements(StateTreeContext, MassContext))
		{
			StopTree(StateTreeContext);
		}
	}

	ActiveStateTreeReference = nullptr;
}

void FArcAdvertisementExecutionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	if (ActiveStateTreeReference == nullptr || !IsValid())
	{
		return;
	}

	const UStateTree* StateTree = ActiveStateTreeReference->GetStateTree();
	if (!StateTree)
	{
		return;
	}

	FStateTreeMinimalExecutionContext StateTreeContext(Owner, StateTree, StateTreeInstanceData);
	StateTreeContext.SendEvent(Tag, Payload, Origin);
}

bool FArcAdvertisementExecutionContext::SetContextRequirements(FStateTreeExecutionContext& StateTreeContext, FMassExecutionContext* MassContext)
{
	if (!StateTreeContext.IsValid())
	{
		return false;
	}

	// ContextActor is optional — only set if available
	if (ContextActor)
	{
		StateTreeContext.SetContextDataByName(UE::ArcKnowledge::Names::ContextActor, FStateTreeDataView(ContextActor));
	}

	StateTreeContext.SetContextDataByName(UE::ArcKnowledge::Names::ExecutingEntityHandle, FStateTreeDataView(FStructView::Make(ExecutingEntity)));
	StateTreeContext.SetContextDataByName(UE::ArcKnowledge::Names::SourceEntityHandle, FStateTreeDataView(FStructView::Make(SourceEntity)));
	StateTreeContext.SetContextDataByName(UE::ArcKnowledge::Names::AdvertisementHandle, FStateTreeDataView(FStructView::Make(AdvertisementHandle)));
	StateTreeContext.SetContextDataByName(UE::ArcKnowledge::Names::KnowledgeLocation, FStateTreeDataView(FStructView::Make(KnowledgeLocation)));
	StateTreeContext.SetContextDataByName(UE::ArcKnowledge::Names::SourceEntityLocation, FStateTreeDataView(FStructView::Make(SourceEntityLocation)));

	checkf(Owner != nullptr, TEXT("Should never reach this point with an invalid Owner since IsValid() is checked before."));
	const UWorld* World = Owner->GetWorld();

	// Capture data for the CollectExternalData callback
	FMassEntityManager* EntityManagerPtr = MassContext ? &MassContext->GetEntityManagerChecked() : nullptr;
	const FMassEntityHandle CapturedEntity = ExecutingEntity;

	StateTreeContext.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateLambda(
		[World, ContextActor = ContextActor.Get(), EntityManagerPtr, CapturedEntity]
		(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDescs, TArrayView<FStateTreeDataView> OutDataViews)
		{
			check(ExternalDescs.Num() == OutDataViews.Num());

			// Build entity view for Mass fragment resolution (if Mass context available)
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
						UE_LOGF(LogArcAdvertisement, Error,"Missing Fragment: %ls", *GetNameSafe(ScriptStruct));
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
						UE_LOGF(LogArcAdvertisement, Error,"Missing Shared Fragment: %ls", *GetNameSafe(ScriptStruct));
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
						UE_LOGF(LogArcAdvertisement, Error,"Missing Const Shared Fragment: %ls", *GetNameSafe(ScriptStruct));
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
						UE_LOGF(LogArcAdvertisement, Error,"Missing Subsystem: %ls", *GetNameSafe(Desc.Struct));
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
						UE_LOGF(LogArcAdvertisement, Error,"Missing ActorComponent: %ls", *GetNameSafe(Desc.Struct));
					}
				}
			}

			return true;
		})
	);

	return StateTreeContext.AreContextDataViewsValid();
}
