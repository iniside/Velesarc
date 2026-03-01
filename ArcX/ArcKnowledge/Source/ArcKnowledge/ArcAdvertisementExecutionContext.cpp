// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAdvertisementExecutionContext.h"
#include "ArcAdvertisementInstruction_StateTree.h"
#include "ArcAdvertisementStateTreeSchema.h"
#include "Engine/World.h"
#include "StateTreeExecutionContext.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAdvertisementExecutionContext)

DEFINE_LOG_CATEGORY(LogArcAdvertisement);

bool FArcAdvertisementExecutionContext::Activate(const FArcAdvertisementInstruction_StateTree& Instruction)
{
	const FStateTreeReference& StateTreeReference = Instruction.StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();

	if (!IsValid())
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to activate advertisement behavior. Context is not properly setup (no Owner)."));
		return false;
	}

	if (StateTree == nullptr)
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to activate advertisement behavior for %s. Instruction doesn't point to a valid StateTree asset."),
			*GetNameSafe(Owner));
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Reentrant call to %hs is not allowed!"), __FUNCTION__);
		return false;
	}

	FStateTreeExecutionContext StateTreeContext(*Owner, *StateTree, StateTreeInstanceData);
	TGuardValue<FStateTreeExecutionContext*> ReentrantGuard(CurrentlyRunningExecContext, &StateTreeContext);

	if (!StateTreeContext.IsValid())
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to activate advertisement behavior for %s. Unable to initialize StateTree execution context for: %s."),
			*GetNameSafe(Owner), *StateTree->GetFullName());
		return false;
	}

	// Validate schema type
	const UArcAdvertisementStateTreeSchema* Schema = Cast<UArcAdvertisementStateTreeSchema>(StateTreeContext.GetStateTree()->GetSchema());
	if (Schema == nullptr)
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to activate advertisement behavior for %s. Expecting %s schema for StateTree asset: %s."),
			*GetNameSafe(Owner),
			*GetNameSafe(UArcAdvertisementStateTreeSchema::StaticClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));
		return false;
	}

	// Validate actor class if an actor is provided and the schema specifies a class
	if (ContextActor && Schema->GetContextActorClass() && !ContextActor->IsA(Schema->GetContextActorClass()))
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to activate advertisement behavior. Expecting Actor of type %s (found %s) for StateTree asset: %s."),
			*GetNameSafe(Schema->GetContextActorClass()),
			*GetNameSafe(ContextActor->GetClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));
		return false;
	}

	if (!SetContextRequirements(StateTreeContext))
	{
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to activate advertisement behavior for %s. Unable to provide all external data views for StateTree asset: %s."),
			*GetNameSafe(Owner), *StateTree->GetFullName());
		return false;
	}

	// Cache the reference for Tick/Deactivate
	ActiveStateTreeReference = &StateTreeReference;

	// Start the tree
	StateTreeContext.Start(&StateTreeReference.GetParameters());

	return true;
}

bool FArcAdvertisementExecutionContext::Tick(const float DeltaTime)
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
		UE_LOG(LogArcAdvertisement, Error, TEXT("Reentrant call to %hs is not allowed!"), __FUNCTION__);
		return false;
	}

	FStateTreeExecutionContext StateTreeContext(*Owner, *StateTree, StateTreeInstanceData);
	TGuardValue<FStateTreeExecutionContext*> ReentrantGuard(CurrentlyRunningExecContext, &StateTreeContext);

	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;
	if (SetContextRequirements(StateTreeContext))
	{
		RunStatus = StateTreeContext.Tick(DeltaTime);
	}

	LastRunStatus = RunStatus;
	return RunStatus == EStateTreeRunStatus::Running;
}

void FArcAdvertisementExecutionContext::Deactivate()
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
		UE_LOG(LogArcAdvertisement, Error, TEXT("Failed to deactivate advertisement behavior for %s. No valid StateTree asset."),
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
		FStateTreeExecutionContext StateTreeContext(*Owner, *StateTree, StateTreeInstanceData);
		if (SetContextRequirements(StateTreeContext))
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

bool FArcAdvertisementExecutionContext::SetContextRequirements(FStateTreeExecutionContext& StateTreeContext)
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

	checkf(Owner != nullptr, TEXT("Should never reach this point with an invalid Owner since IsValid() is checked before."));
	const UWorld* World = Owner->GetWorld();

	StateTreeContext.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateLambda(
		[World, ContextActor = ContextActor.Get()]
		(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDescs, TArrayView<FStateTreeDataView> OutDataViews)
		{
			check(ExternalDescs.Num() == OutDataViews.Num());
			for (int32 Index = 0; Index < ExternalDescs.Num(); Index++)
			{
				const FStateTreeExternalDataDesc& Desc = ExternalDescs[Index];
				if (Desc.Struct != nullptr)
				{
					if (World != nullptr && Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
					{
						UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(ToRawPtr(Desc.Struct))));
						OutDataViews[Index] = FStateTreeDataView(Subsystem);
					}
					else if (ContextActor != nullptr && Desc.Struct->IsChildOf(AActor::StaticClass()))
					{
						OutDataViews[Index] = FStateTreeDataView(ContextActor);
					}
				}
			}

			return true;
		})
	);

	return StateTreeContext.AreContextDataViewsValid();
}
