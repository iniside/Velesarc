// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityComboStepTask.h"

#include "AbilitySystemComponent.h"
#include "ArcAN_SendGameplayEvent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimMontage.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/ArcCoreInputComponent.h"

class UArcAnimNotify_MarkGameplayEvent;

UArcAnimNotify_MarkComboWindow::UArcAnimNotify_MarkComboWindow(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{}

FArcGameplayAbilityComboStepTask::FArcGameplayAbilityComboStepTask()
{
}

EStateTreeRunStatus FArcGameplayAbilityComboStepTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime = 0;
	InstanceData.bIsComboWindowActive = false;

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

		FModifyContextOptions IMCContext;
		IMCContext.bForceImmediately = true;
		IMCContext.bIgnoreAllPressedKeysUntilRelease = false;
		Subsystem->AddMappingContext(InstanceData.InputMappingContext
			, 999
			, IMCContext);
	}


	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();

	InstanceData.InputHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Triggered
		, [WeakContext = Context.MakeWeakExecutionContext()](const FInputActionValue& Val)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (InstanceData && InstanceData->bIsComboWindowActive)
			{
				FGameplayEventData Payload;

				InstanceData->AbilitySystemComponent->HandleGameplayEvent(InstanceData->InputPressedTag, &Payload);

				StrongContext.BroadcastDelegate(InstanceData->OnComboTriggered);
			}
		}).GetHandle();

	FAnimNotifyContext AnimContext;
	InstanceData.MontageToPlay->GetAnimNotifies(0, 30, AnimContext);

	InstanceData.NotifyEvents.Empty();

	int8 ComboWindowsNum = 0;
	for (const FAnimNotifyEventReference& AN : AnimContext.ActiveNotifies)
	{
		const FAnimNotifyEvent* Event = AN.GetNotify();
		if (!Event)
		{
			continue;
		}

		if (UArcAnimNotify_MarkComboWindow* SPR = Cast<UArcAnimNotify_MarkComboWindow>(Event->Notify))
		{
			if (ComboWindowsNum == 0)
			{
				InstanceData.StartComboTime = AN.GetNotify()->GetTriggerTime();
			}
			if (ComboWindowsNum == 1)
			{
				InstanceData.EndComboTime = AN.GetNotify()->GetTriggerTime();
			}
			ComboWindowsNum++;
		}

		UArcAnimNotify_MarkGameplayEvent* MGE = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
		if (MGE)
		{
			FArcNotifyEvent NewNotify;
			NewNotify.Tag = MGE->GetEventTag();
			float Time = Event->GetTime();
			NewNotify.OriginalTime = Event->GetTriggerTime();
			NewNotify.Time = Event->GetTriggerTime() / 1;
			InstanceData.NotifyEvents.Add(NewNotify);
		}
	}

	if (AnimContext.ActiveNotifies.Num() == 2)
	{
		InstanceData.StartComboTime = AnimContext.ActiveNotifies[0].GetNotify()->GetTriggerTime();
		InstanceData.EndComboTime = AnimContext.ActiveNotifies[1].GetNotify()->GetTriggerTime();
	}

	return EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityComboStepTask::HandleOnInputTriggered(FStateTreeWeakExecutionContext WeakContext)
{

}

EStateTreeRunStatus FArcGameplayAbilityComboStepTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime += DeltaTime;

	if (InstanceData.CurrentTime >= InstanceData.StartComboTime &&
		InstanceData.CurrentTime <= InstanceData.EndComboTime)
	{
		InstanceData.bIsComboWindowActive = true;
	}
	else
	{
		InstanceData.bIsComboWindowActive = false;
	}

	if (InstanceData.CurrentTime >= InstanceData.EndComboTime && !InstanceData.bIsComboWindowActive)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	for (int32 Idx = InstanceData.NotifyEvents.Num() - 1; Idx >= 0; --Idx)
	{
		if (InstanceData.CurrentTime >= InstanceData.NotifyEvents[Idx].Time)
		{
			FGameplayEventData Payload;
			//UE_LOG(LogArcCore, Log, TEXT("Notify %s Time %.2f, CurrentTime %.2f"), *Notifies[Idx].Tag.ToString(), Notifies[Idx].Time, CurrentNotifyTime);
			InstanceData.AbilitySystemComponent->HandleGameplayEvent(InstanceData.NotifyEvents[Idx].Tag, &Payload);

			InstanceData.NotifyEvents.RemoveAt(Idx);
		}
	}

	return EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityComboStepTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
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
FText FArcGameplayAbilityComboStepTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->MontageToPlay)
		{
			return FText::Format(NSLOCTEXT("ArcCore", "ComboStepDesc", "Combo Step: {0}"), FText::FromString(GetNameSafe(InstanceData->MontageToPlay)));
		}
	}
	return NSLOCTEXT("ArcCore", "ComboStepDescDefault", "Combo Step");
}
#endif
