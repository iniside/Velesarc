// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityWaitInputTask.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/ArcCoreInputComponent.h"

FArcGameplayAbilityWaitInputTask::FArcGameplayAbilityWaitInputTask()
{
}

EStateTreeRunStatus FArcGameplayAbilityWaitInputTask::EnterState(FStateTreeExecutionContext& Context
																 , const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(InstanceData.AbilitySystemComponent);
	const APawn* Pawn = Cast<APawn>(ArcASC->GetAvatarActor());

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);

	if (InstanceData.InputMappingContext)
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		check(Subsystem);

		FModifyContextOptions IMVContext;
		IMVContext.bForceImmediately = true;
		IMVContext.bIgnoreAllPressedKeysUntilRelease = false;
		Subsystem->AddMappingContext(InstanceData.InputMappingContext
			, 999
			, IMVContext);
	}


	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();

	InstanceData.InputHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
	, ETriggerEvent::Triggered
	, [WeakContext = Context.MakeWeakExecutionContext()](const FInputActionValue& Val)
	{
		FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
		FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();

		StrongContext.BroadcastDelegate(InstanceData->OnInputTriggered);

	}).GetHandle();

	return EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityWaitInputTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(InstanceData.AbilitySystemComponent);
	if (!ArcASC)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(ArcASC->GetAvatarActor());
	if (!Pawn)
	{
		return;
	}

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	if (!PC)
	{
		return;
	}

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();

	ArcIC->RemoveBindingByHandle(InstanceData.InputHandle);
	ArcIC->RemoveBindingByHandle(InstanceData.ReleasedHandle);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	FModifyContextOptions CInputontext;
	CInputontext.bForceImmediately = true;
	CInputontext.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->RemoveMappingContext(InstanceData.InputMappingContext, CInputontext);
}

#if WITH_EDITOR
FText FArcGameplayAbilityWaitInputTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->InputAction)
		{
			return FText::Format(NSLOCTEXT("ArcCore", "WaitInputDesc", "Wait For Input: {0}"), FText::FromString(GetNameSafe(InstanceData->InputAction)));
		}
	}
	return NSLOCTEXT("ArcCore", "WaitInputDescDefault", "Wait For Input");
}
#endif
