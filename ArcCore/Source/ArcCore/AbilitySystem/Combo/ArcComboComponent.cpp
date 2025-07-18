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

#include "AbilitySystem/Combo/ArcComboComponent.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"

#include "Input/ArcCoreInputComponent.h"

DEFINE_LOG_CATEGORY(LogArcCombo);

UArcAT_WaitComboAction* UArcAT_WaitComboAction::WaitComboAction(UGameplayAbility* OwningAbility
																, FName TaskInstanceName
																, const FGameplayTagContainer& InEventTags
																, class UArcComboGraph* InComboGraph)
{
	UArcAT_WaitComboAction* MyObj = NewAbilityTask<UArcAT_WaitComboAction>(OwningAbility
		, TaskInstanceName);
	MyObj->EventTags = InEventTags;
	MyObj->ComboGraphTest = InComboGraph;

	return MyObj;
}

void UArcAT_WaitComboAction::Activate()
{
	Super::Activate();
	if (AbilitySystemComponent->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}
	bComboWindowActive = false;
	EventHandle = AbilitySystemComponent->AddGameplayEventTagContainerDelegate(EventTags
		, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitComboAction::HandleGameplayEvent));
	const APawn* Pawn = Cast<APawn>(GetAvatarActor());

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);
	FModifyContextOptions Context;
	Context.bForceImmediately = true;
	Context.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->AddMappingContext(ComboGraphTest->InputMapping
		, 100
		, Context);

	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
	for (const FGuid Option : ComboGraphTest->Nodes[0].ChildNodes)
	{
		int32 Idx = ComboGraphTest->Nodes.IndexOfByKey(Option);
		if (Idx)
		{
			uint32 Handle = ArcIC->BindAction(ComboGraphTest->Nodes[Idx].ComboInput
				, ETriggerEvent::Started
				, this
				, &UArcAT_WaitComboAction::HandleOnInputPressed).GetHandle();
			BoundHandles.Add(Handle);
		}
	}

	UArcComboComponent* ArcCombo = UArcComboComponent::FindComboComponent(GetOwnerActor());
	ArcCombo->AddOnComboWindowStart(FSimpleMulticastDelegate::FDelegate::CreateUObject(this
		, &UArcAT_WaitComboAction::HandleComboWindowStart));

	ArcCombo->AddOnComboWindowEnd(FSimpleMulticastDelegate::FDelegate::CreateUObject(this
		, &UArcAT_WaitComboAction::HandleComboWindowEnd));
	CurrentComboNode = 0;

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	if (ComboGraphTest->Nodes[0].ComboMontage != nullptr)
	{
		ArcASC->PlayAnimMontage(Ability
			, ComboGraphTest->Nodes[0].ComboMontage
			, 1
			, NAME_None
			, 0
			, true);

		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

		MontageEndedDelegate.BindUObject(this
			, &UArcAT_WaitComboAction::OnMontageEnded);
		AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate
			, ComboGraphTest->Nodes[0].ComboMontage);
	}
	// 1. Play animation (Or timer ?)
	// 2. Bind Delegate for Combo Window
	// 3. Execute first step (root node).
	// 4. Handle queue ? One step ahead, as option.
	// OnNextStep.Broadcast(ComboGraphTest->Nodes[0].ItemName, FGameplayTag());
}

void UArcAT_WaitComboAction::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	if (AbilitySystemComponent->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	AbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags
		, EventHandle);
	const APawn* Pawn = Cast<APawn>(GetAvatarActor());

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}
	check(PC);

	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();

	ArcIC->RemoveBinds(BoundHandles);

	BoundHandles.Empty();

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	FModifyContextOptions Context;
	Context.bForceImmediately = true;
	Context.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->RemoveMappingContext(ComboGraphTest->InputMapping
		, Context);
}

void UArcAT_WaitComboAction::OnMontageEnded(UAnimMontage* Montage
											, bool bInterrupted)
{
	if (bInterrupted == false)
	{
		OnComboFinished.Broadcast(FGameplayTag());
	}
}

void UArcAT_WaitComboAction::HandleGameplayEvent(FGameplayTag EventTag
												 , const FGameplayEventData* Payload)
{
	OnComboEvent.Broadcast(EventTag);
}

void UArcAT_WaitComboAction::HandleComboEvent(FGameplayTag EventTag)
{
	OnComboEvent.Broadcast(EventTag);
}

void UArcAT_WaitComboAction::HandleComboWindowStart()
{
	UE_LOG(LogArcCombo
		, Log
		, TEXT("WINDOW START [NewMontage %s]")
		, *ComboGraphTest->Nodes[CurrentComboNode].ComboMontage->GetName());
	bComboWindowActive = true;
	FGameplayTag StepName = CurrentComboNode != INDEX_NONE
							? ComboGraphTest->Nodes[CurrentComboNode].WindowStartTag
							: FGameplayTag();
	OnComboWindowStart.Broadcast(StepName);
}

void UArcAT_WaitComboAction::HandleComboWindowEnd()
{
	UE_LOG(LogArcCombo
		, Log
		, TEXT("WINDOW END [NewMontage %s]")
		, *ComboGraphTest->Nodes[CurrentComboNode].ComboMontage->GetName());
	bComboWindowActive = false;
	FGameplayTag StepName = CurrentComboNode != INDEX_NONE
							? ComboGraphTest->Nodes[CurrentComboNode].WindowEndTag
							: FGameplayTag();
	OnComboWindowEnd.Broadcast(StepName);

	// we should probabaly do some cleanup here ?
}

void UArcAT_WaitComboAction::HandleOnInputPressed(const FInputActionInstance& InputActionInstance)
{
	if (bComboWindowActive == false)
	{
		UE_LOG(LogArcCombo
			, Log
			, TEXT( "bComboWindowActive == false [CurrentComboNode %d] [CurrentMontage %s] [Incoming Input %s]")
			, CurrentComboNode
			, *ComboGraphTest->Nodes[CurrentComboNode].ComboMontage->GetName()
			, *InputActionInstance.GetSourceAction()->GetName());
		return;
	}
	bComboWindowActive = false;
	const APawn* Pawn = Cast<APawn>(GetAvatarActor());
	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();

	ArcIC->RemoveBinds(BoundHandles);

	BoundHandles.Empty();
	int32 PreviousNode = CurrentComboNode;

	CurrentComboNode = ComboGraphTest->Nodes.IndexOfByKey(InputActionInstance.GetSourceAction());
	UE_LOG(LogArcCombo
		, Log
		, TEXT("SELECTED NODE [NewComboNode %d] [NewMontage %s] [Incoming Input %s]")
		, CurrentComboNode
		, *ComboGraphTest->Nodes[CurrentComboNode].ComboMontage->GetName()
		, *InputActionInstance.GetSourceAction()->GetName());
	if (CurrentComboNode != INDEX_NONE)
	{
		OnNextStep.Broadcast(ComboGraphTest->Nodes[CurrentComboNode].StepTag);
		if (ComboGraphTest->Nodes[CurrentComboNode].ComboMontage != nullptr)
		{
			UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
			ArcASC->PlayAnimMontage(Ability
				, ComboGraphTest->Nodes[CurrentComboNode].ComboMontage
				, 1
				, NAME_None
				, 0
				, true);

			const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
			UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

			MontageEndedDelegate.Unbind();
			if (PreviousNode != INDEX_NONE)
			{
				FOnMontageEnded MontageEnded;
				AnimInstance->Montage_SetEndDelegate(MontageEnded
					, ComboGraphTest->Nodes[PreviousNode].ComboMontage);
			}
			MontageEndedDelegate.BindUObject(this
				, &UArcAT_WaitComboAction::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate
				, ComboGraphTest->Nodes[CurrentComboNode].ComboMontage);

			ACharacter* C = Cast<ACharacter>(GetAvatarActor());
			C->GetMesh()->GetAnimInstance()->Montage_Play(ComboGraphTest->Nodes[CurrentComboNode].ComboMontage);
		}

		if (ComboGraphTest->Nodes[CurrentComboNode].ChildNodes.Num() > 0)
		{
			for (const FGuid Option : ComboGraphTest->Nodes[CurrentComboNode].ChildNodes)
			{
				int32 Idx = ComboGraphTest->Nodes.IndexOfByKey(Option);
				if (Idx)
				{
					uint32 Handle = ArcIC->BindAction(ComboGraphTest->Nodes[Idx].ComboInput
						, ETriggerEvent::Started
						, this
						, &UArcAT_WaitComboAction::HandleOnInputPressed).GetHandle();
					BoundHandles.Add(Handle);
				}
			}
		}
		else
		{
			const APlayerController* PC = Pawn->GetController<APlayerController>();
			if (PC == nullptr)
			{
				return;
			}
			check(PC);

			CurrentComboNode = 0;

			UE_LOG(LogArcCombo
				, Log
				, TEXT( "BACK TO ROOT [NewComboNode %d] [NewMontage %s] [Incoming Input %s]")
				, CurrentComboNode
				, *ComboGraphTest->Nodes[CurrentComboNode].ComboMontage->GetName()
				, *InputActionInstance.GetSourceAction()->GetName());

			uint32 Handle = ArcIC->BindAction(ComboGraphTest->Nodes[CurrentComboNode].ComboInput
				, ETriggerEvent::Started
				, this
				, &UArcAT_WaitComboAction::HandleOnInputPressed).GetHandle();
			BoundHandles.Add(Handle);
		}
	}
	else
	{
		// if we can loop..
		{
			const APlayerController* PC = Pawn->GetController<APlayerController>();
			if (PC == nullptr)
			{
				return;
			}
			check(PC);

			ArcIC->RemoveBinds(BoundHandles);

			int32 Idx = 0;
			if (Idx)
			{
				uint32 Handle = ArcIC->BindAction(ComboGraphTest->Nodes[Idx].ComboInput
					, ETriggerEvent::Started
					, this
					, &UArcAT_WaitComboAction::HandleOnInputPressed).GetHandle();
				BoundHandles.Add(Handle);
			}
		}
	}
}

TArray<const FArcComboNode*> FArcComboNode::GetChildNodes(const UArcComboGraph* TreeData) const
{
	TArray<const FArcComboNode*> Nodes;
	for (const FGuid& G : ChildNodes)
	{
		const FArcComboNode* N = TreeData->Nodes.FindByKey(G);
		if (N)
		{
			Nodes.Add(N);
		}
	}

	return Nodes;
}

TArray<const FArcComboNode*> FArcComboNode::GetParentNodes(const UArcComboGraph* TreeData) const
{
	TArray<const FArcComboNode*> Nodes;
	for (const FGuid& G : ParentNodes)
	{
		const FArcComboNode* N = TreeData->Nodes.FindByKey(G);
		if (N)
		{
			Nodes.Add(N);
		}
	}

	return Nodes;
}
#if WITH_EDITORONLY_DATA
void UArcComboGraph::RemoveNode(const FGuid& Guid)
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		Nodes[Idx].ParentNodes.Remove(Guid);
		Nodes[Idx].ChildNodes.Remove(Guid);
	}

	const int32 Idx = Nodes.IndexOfByKey(Guid);
	if (Idx != INDEX_NONE)
	{
		Nodes.RemoveAt(Idx);
	}
}
#endif
TArray<const FArcComboNode*> UArcComboGraph::GetChildNodes(const FGuid& InNodeId) const
{
	const FArcComboNode* For = FindNode(InNodeId);
	return For->GetChildNodes(this);
}

TArray<const FArcComboNode*> UArcComboGraph::GetParentNodes(const FGuid& InNodeId) const
{
	const FArcComboNode* For = FindNode(InNodeId);
	return For->GetParentNodes(this);
}

// Sets default values for this component's properties
UArcComboComponent::UArcComboComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
void UArcComboComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

// Called every frame
void UArcComboComponent::TickComponent(float DeltaTime
									   , ELevelTick TickType
									   , FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime
		, TickType
		, ThisTickFunction);

	// ...
}
