// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAbilityStateTreeTypes.h"

#include "AbilitySystemComponent.h"
#include "ArcCoreAbilitySystemComponent.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeReference.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"

UArcGameplayAbilityStateTreeSchema::UArcGameplayAbilityStateTreeSchema()
	: AvatarActorClass(AActor::StaticClass())
	, OwnerActorClass(AActor::StaticClass())
	, OwningAbilityClass(UGameplayAbility::StaticClass())
	, AbilitySystemComponentClass(UAbilitySystemComponent::StaticClass())
	, ContextDataDescs({
		  { Arcx::Abilities::AvatarActor, AActor::StaticClass(), FGuid(0xDFB93B9E, 0xEDBE4906, 0x851C66B2, 0x7585FA21) },
		  { Arcx::Abilities::OwnerActor, AActor::StaticClass(), FGuid(0x870E433F, 0x99314B95, 0x982B78B0, 0x1B63BBD1) },
		  { Arcx::Abilities::Ability, UGameplayAbility::StaticClass(), FGuid(0x13BAB427, 0x26DB4A4A, 0xBD5F937E, 0xDB39F841) },
		  { Arcx::Abilities::AbilitySystem, UAbilitySystemComponent::StaticClass(), FGuid(0x9F8E7D6C, 0x5B4A3928, 0x1706F5E4, 0xD3C2B1A0) }
	  })
{
}

bool UArcGameplayAbilityStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FArcGameplayAbilityConditionBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FArcGameplayAbilityEvaluatorBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FArcGameplayAbilityTaskBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UArcGameplayAbilityStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UArcGameplayAbilityStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(AActor::StaticClass())
		|| InStruct.IsChildOf(UActorComponent::StaticClass())
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}

void UArcGameplayAbilityStateTreeSchema::PostLoad()
{
	Super::PostLoad();
	
	ContextDataDescs[0].Struct = AvatarActorClass.Get();
	ContextDataDescs[1].Struct = OwnerActorClass.Get();
	ContextDataDescs[2].Struct = OwningAbilityClass.Get();
	ContextDataDescs[3].Struct = AbilitySystemComponentClass.Get();
}

#if WITH_EDITOR
void UArcGameplayAbilityStateTreeSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FProperty* Property = PropertyChangedEvent.Property;

	if (Property)
	{
		if (Property->GetOwnerClass() == UArcGameplayAbilityStateTreeSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UArcGameplayAbilityStateTreeSchema, AvatarActorClass))
		{
			ContextDataDescs[0].Struct = AvatarActorClass.Get();
		}
		if (Property->GetOwnerClass() == UArcGameplayAbilityStateTreeSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UArcGameplayAbilityStateTreeSchema, OwnerActorClass))
		{
			ContextDataDescs[1].Struct = OwnerActorClass.Get();
		}
		if (Property->GetOwnerClass() == UArcGameplayAbilityStateTreeSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UArcGameplayAbilityStateTreeSchema, OwningAbilityClass))
		{
			ContextDataDescs[2].Struct = OwningAbilityClass.Get();
		}
		if (Property->GetOwnerClass() == UArcGameplayAbilityStateTreeSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UArcGameplayAbilityStateTreeSchema, AbilitySystemComponentClass))
		{
			ContextDataDescs[3].Struct = AbilitySystemComponentClass.Get();
		}
	}
}
#endif


bool FArcGameplayAbilityStateTreeContext::Activate(const FStateTreeReference& StateTreeRef)
{
	const UStateTree* StateTree = StateTreeRef.GetStateTree();

	if (Definition)
	{
		if (const UStateTree* PrevTree = Definition)
		{
			FStateTreeReadOnlyExecutionContext PrevStateTreeContext(OwnerActor, PrevTree, StateTreeInstanceData);
			if (PrevStateTreeContext.GetStateTreeRunStatus() == EStateTreeRunStatus::Running && PrevTree != StateTree)
			{
				return false;
			}
		}
	}

	Definition = StateTreeRef.GetStateTree();
	
	if (!IsValid())
	{
		
		return false;
	}
	
	if (StateTree == nullptr)
	{
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
		return false;
	}

	FStateTreeExecutionContext StateTreeContext(*OwnerActor, *StateTree, StateTreeInstanceData);
	TGuardValue<FStateTreeExecutionContext*> ReentrantExecContextGuard(CurrentlyRunningExecContext, &StateTreeContext);

	if (!StateTreeContext.IsValid())
	{
		return false;
	}

	if (!ValidateSchema(StateTreeContext))
	{
		return false;
	}
	
	if (!SetContextRequirements(StateTreeContext))
	{
		return false;
	}

	// Start State Tree
	StateTreeContext.Start(StateTreeRef.GetGlobalParameters());
	
	return true;
}

bool FArcGameplayAbilityStateTreeContext::Tick(const float DeltaTime)
{
	if (Definition == nullptr || !IsValid())
	{
		return false;
	}
	
	const UStateTree* StateTree = Definition;
	if (StateTree == nullptr)
	{
		return false;
	}

	if (CurrentlyRunningExecContext)
	{
		return false;
	}

	FStateTreeExecutionContext StateTreeContext(*OwnerActor, *StateTree, StateTreeInstanceData);
	TGuardValue<FStateTreeExecutionContext*> ReentrantExecContextGuard(CurrentlyRunningExecContext, &StateTreeContext);

	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;
	if (SetContextRequirements(StateTreeContext))
	{
		RunStatus = StateTreeContext.Tick(DeltaTime);
	}

	LastRunStatus = RunStatus;

	return RunStatus == EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityStateTreeContext::Deactivate()
{
	auto StopTree = [this](FStateTreeExecutionContext& Context)
	{
		Context.Stop();
	};

	if (Definition == nullptr || OwnerActor == nullptr)
	{
		return;
	}

	const UStateTree* StateTree = Definition;
	if (StateTree == nullptr)
	{
		return;
	}

	if (CurrentlyRunningExecContext)
	{
		// Tree Stop will be delayed to the end of the frame in a reentrant call, but we need to use the existing execution context
		StopTree(*CurrentlyRunningExecContext);
	}
	else
	{
		FStateTreeExecutionContext StateTreeContext(OwnerActor, StateTree, StateTreeInstanceData);
		if (SetContextRequirements(StateTreeContext))
		{
			StopTree(StateTreeContext);
		}
	}

}

void FArcGameplayAbilityStateTreeContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	if (Definition == nullptr || !IsValid())
	{
		return;
	}

	const UStateTree* StateTree = Definition;
	if (!StateTree)
	{
		return;
	}
	FStateTreeMinimalExecutionContext StateTreeContext(OwnerActor, StateTree, StateTreeInstanceData);
	StateTreeContext.SendEvent(Tag, Payload, Origin);
}

bool FArcGameplayAbilityStateTreeContext::ValidateSchema(const FStateTreeExecutionContext& StateTreeContext) const
{
	// Ensure that the actor and smart object match the schema.
	//const UGameplayInteractionStateTreeSchema* Schema = Cast<UGameplayInteractionStateTreeSchema>(StateTreeContext.GetStateTree()->GetSchema());
	//if (Schema == nullptr)
	//{
	//	UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
	//		TEXT("Failed to activate interaction for %s. Expecting %s schema for StateTree asset: %s."),
	//		*GetNameSafe(ContextActor),
	//		*GetNameSafe(UGameplayInteractionStateTreeSchema::StaticClass()),
	//		*GetFullNameSafe(StateTreeContext.GetStateTree()));
	//
	//	return false;
	//}
	//if (!ContextActor || !ContextActor->IsA(Schema->GetContextActorClass()))
	//{
	//	UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
	//		TEXT("Failed to activate interaction for %s. Expecting Actor to be of type %s (found %s) for StateTree asset: %s."),
	//		*GetNameSafe(ContextActor),
	//		*GetNameSafe(Schema->GetContextActorClass()),
	//		*GetNameSafe(ContextActor ? ContextActor->GetClass() : nullptr),
	//		*GetFullNameSafe(StateTreeContext.GetStateTree()));
	//
	//	return false;
	//}
	//if (!SmartObjectActor || !SmartObjectActor->IsA(Schema->GetSmartObjectActorClass()))
	//{
	//	UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
	//		TEXT("Failed to activate interaction for %s. SmartObject Actor to be of type %s (found %s) for StateTree asset: %s."),
	//		*GetNameSafe(ContextActor),
	//		*GetNameSafe(Schema->GetSmartObjectActorClass()),
	//		*GetNameSafe(SmartObjectActor ? SmartObjectActor->GetClass() : nullptr),
	//		*GetFullNameSafe(StateTreeContext.GetStateTree()));
	//
	//	return false;
	//}

	return true;
}

bool FArcGameplayAbilityStateTreeContext::SetContextRequirements(FStateTreeExecutionContext& StateTreeContext)
{
	if (!StateTreeContext.IsValid())
	{
		return false;
	}

	if (!::IsValid(Definition))
	{
		return false;
	}
	
	StateTreeContext.SetContextDataByName(Arcx::Abilities::OwnerActor, FStateTreeDataView(OwnerActor));
	StateTreeContext.SetContextDataByName(Arcx::Abilities::AvatarActor, FStateTreeDataView(AvatarActor));
	StateTreeContext.SetContextDataByName(Arcx::Abilities::Ability, FStateTreeDataView(OwningAbility));
    StateTreeContext.SetContextDataByName(Arcx::Abilities::AbilitySystem, FStateTreeDataView(AbilitySystemComponent));

	checkf(OwnerActor != nullptr, TEXT("Should never reach this point with an invalid ContextActor since it is required to get a valid StateTreeContext."));
	const UWorld* World = OwnerActor->GetWorld();
	
	StateTreeContext.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateLambda(
		[World, Owner = OwnerActor, Avatar = AvatarActor, AbilitySystem = AbilitySystemComponent, Ability = OwningAbility]
		(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDescs, TArrayView<FStateTreeDataView> OutDataViews)
		{
			check(ExternalDescs.Num() == OutDataViews.Num());
			OutDataViews[0] = FStateTreeDataView(Owner);
			OutDataViews[1] = FStateTreeDataView(Avatar);
			OutDataViews[2] = FStateTreeDataView(Ability);
			OutDataViews[3] = FStateTreeDataView(AbilitySystem);
			return true;
		})
	);

	return StateTreeContext.AreContextDataViewsValid();
}


UArcAT_WaitAbilityStateTree::UArcAT_WaitAbilityStateTree(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	bTickingTask = true;
}

UArcAT_WaitAbilityStateTree* UArcAT_WaitAbilityStateTree::WaitAbilityStateTree(UGameplayAbility* OwningAbility
																			  , const FStateTreeReference& StateTreeRef
																			  , FGameplayTag InputEventTag
																			  , FGameplayTag InputReleasedEventTag
																			  , FGameplayTagContainer EventTags)
{
	UArcAT_WaitAbilityStateTree* MyObj = NewAbilityTask<UArcAT_WaitAbilityStateTree>(OwningAbility);
	MyObj->StateTreeRef = StateTreeRef;
	MyObj->EventTags = EventTags;
	MyObj->InputEventTag = InputEventTag;
	MyObj->InputReleasedEventTag = InputReleasedEventTag;
	
	return MyObj;
}

void UArcAT_WaitAbilityStateTree::Activate()
{
	AActor* Avatar = AbilitySystemComponent->GetAvatarActor();
	AActor* Owner = AbilitySystemComponent->GetOwnerActor();
	
	Context.SetAvatarActor(Avatar);
	Context.SetOwnerActor(Owner);
	Context.SetOwningAbility(Ability.Get());
	Context.SetAbilitySystemComponent(AbilitySystemComponent.Get());
	
	const bool bActivated = Context.Activate(StateTreeRef);
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	if (InputEventTag.IsValid())
	{
		InputDelegateHandle = ArcASC->AddOnInputPressedMap(GetAbilitySpecHandle(), FArcAbilityInputDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitAbilityStateTree::HandleOnInputPressed));	
	}
	
	if (InputReleasedEventTag.IsValid())
	{
		InputDelegateHandle = ArcASC->AddOnInputReleasedMap(GetAbilitySpecHandle(), FArcAbilityInputDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitAbilityStateTree::HandleOnInputReleased));	
	}
	
	if (bActivated)
	{
		EventHandle = ArcASC->AddGameplayEventTagContainerDelegate(EventTags
			, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitAbilityStateTree::HandleOnGameplayEvent));
	}
}

void UArcAT_WaitAbilityStateTree::HandleOnInputPressed(FGameplayAbilitySpec& InSpec)
{
	FGameplayEventData TempData = {};
	TempData.EventTag = InputEventTag;
	OnGameplayEvent.Broadcast(TempData, InputEventTag);
	
	FArcGameplayAbilityInputEvent Event;
	Event.InputTag = InputEventTag;
	Context.SendEvent(InputEventTag, FConstStructView::Make(Event));
}

void UArcAT_WaitAbilityStateTree::HandleOnInputReleased(FGameplayAbilitySpec& InSpec)
{
	FGameplayEventData TempData = {};
	TempData.EventTag = InputReleasedEventTag;
	OnGameplayEvent.Broadcast(TempData, InputReleasedEventTag);
	
	FArcGameplayAbilityInputEvent Event;
	Event.InputTag = InputReleasedEventTag;
	Context.SendEvent(InputReleasedEventTag, FConstStructView::Make(Event));
}

void UArcAT_WaitAbilityStateTree::HandleOnGameplayEvent(FGameplayTag EventTag
														, const FGameplayEventData* Payload)
{
	FGameplayEventData TempData = *Payload;
	TempData.EventTag = EventTag;
	OnGameplayEvent.Broadcast(TempData, EventTag);
}

void UArcAT_WaitAbilityStateTree::TickTask(float DeltaTime)
{
	Context.Tick(DeltaTime);
	EStateTreeRunStatus LastRunStatus = Context.GetLastRunStatus();
	
	FGameplayEventData TempData;
	TempData.EventTag = FGameplayTag();
	switch (LastRunStatus)
	{
		case EStateTreeRunStatus::Running:
			break;
		case EStateTreeRunStatus::Stopped:
			OnStateTreeStopped.Broadcast(TempData, FGameplayTag());
			break;
		case EStateTreeRunStatus::Succeeded:
			OnStateTreeSucceeded.Broadcast(TempData, FGameplayTag());
			break;
		case EStateTreeRunStatus::Failed:
			OnStateTreeFailed.Broadcast(TempData, FGameplayTag());
			break;
		case EStateTreeRunStatus::Unset:
			break;
	}
}

void UArcAT_WaitAbilityStateTree::OnDestroy(bool AbilityEnded)
{
	Context.Deactivate();
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	
	ArcASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	
	Super::OnDestroy(AbilityEnded);
}
