// Copyright Lukasz Baran. All Rights Reserved.

#include "Interaction/ArcCoreInteractionSourceComponent.h"

#include "ArcCoreAsyncMessageTypes.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageBindingComponent.h"
#include "AsyncMessageBindingEndpoint.h"
#include "AsyncMessageWorldSubsystem.h"
#include "ArcTargetingSubsystem.h"
#include "ArcTargetingContext.h"
#include "ArcTargetingPreset.h"
#include "ArcTargetingPipelineTypes.h"
#include "Commands/ArcInteractCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayInteractionsTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/PrimitiveComponent.h"
#include "InteractableTargetInterface.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Interaction/ArcInteractionDisplayData.h"
#include "Interaction/ArcInteractionEntityOption.h"
#include "MassEntitySubsystem.h"
#include "SmartObjectDefinition.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "Core/ArcMassGameFrameworkTags.h"
#include "GameFramework/ArcActorFeatureUtils.h"
#include "Player/ArcMassPlayerStateExtensionComponent.h"

UArcCoreInteractionSourceComponent::UArcCoreInteractionSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcCoreInteractionSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InitStateHandle = ArcActorFeatureUtils::RegisterFeatureInitStateDelegate<UArcMassPlayerStateExtensionComponent>(
		GetOwner(),
		ArcActorFeatureUtils::TFeatureDelegate<UArcMassPlayerStateExtensionComponent>::CreateUObject(
			this, &UArcCoreInteractionSourceComponent::OnPlayerStateReady),
		FArcMassGameFrameworkTags::Get().InitState_DataInitialized,
		/*bCallImmediately*/ true,
		/*bCallOnce*/ true);
}

void UArcCoreInteractionSourceComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	TeardownInteractionInput();

	ArcActorFeatureUtils::UnregisterFeatureInitStateDelegate(GetOwner(), InitStateHandle);

	if (TargetingRequestHandle.IsValid())
	{
		UArcTargetingSubsystem* Subsystem = UArcTargetingSubsystem::Get(this);
		if (Subsystem != nullptr)
		{
			Subsystem->Cancel(TargetingRequestHandle);
			Subsystem->Release(TargetingRequestHandle);
		}
		TargetingRequestHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UArcCoreInteractionSourceComponent::OnPlayerStateReady(UArcMassPlayerStateExtensionComponent* PSExt)
{
	APlayerState* PS = GetOwner<APlayerState>();
	APawn* Pawn = PS != nullptr ? PS->GetPawn() : nullptr;
	StartTargetingLoop(Pawn);
}

void UArcCoreInteractionSourceComponent::StartTargetingLoop(APawn* Pawn)
{
	UArcTargetingSubsystem* Subsystem = UArcTargetingSubsystem::Get(this);
	if (Subsystem == nullptr || InteractionTargetingPreset == nullptr)
	{
		return;
	}

	FArcTargetingContext Seed;
	Seed.SourceActor = Pawn;
	Seed.InstigatorActor = GetOwner();
	Seed.SourceObject = this;

	TargetingRequestHandle = Subsystem->MakeRequest(InteractionTargetingPreset, Seed);

	Subsystem->StartAsync(
		TargetingRequestHandle,
		FArcTargetingCompletedDelegate::CreateUObject(this, &UArcCoreInteractionSourceComponent::HandleTargetingCompleted),
		/*bRequeueOnCompletion=*/true);
}

namespace ArcCoreInteractionSourceComponent_Endpoint
{
	/** Resolves the owning local player's async-message binding endpoint so interaction messages are
	 *  delivered only to that player's listeners. Returns null (→ world-wide broadcast) if the controller
	 *  or its UAsyncMessageBindingComponent isn't available. */
	TSharedPtr<FAsyncMessageBindingEndpoint> ResolvePlayerEndpoint(const UArcCoreInteractionSourceComponent* Component)
	{
		const APlayerState* PS = Component != nullptr ? Component->GetOwner<APlayerState>() : nullptr;
		const APlayerController* PC = PS != nullptr ? PS->GetPlayerController() : nullptr;
		if (PC == nullptr)
		{
			return nullptr;
		}

		UActorComponent* BindingComponent = PC->FindComponentByInterface(UAsyncMessageBindingEndpointInterface::StaticClass());
		const IAsyncMessageBindingEndpointInterface* EndpointInterface = Cast<IAsyncMessageBindingEndpointInterface>(BindingComponent);
		return EndpointInterface != nullptr ? EndpointInterface->GetEndpoint() : nullptr;
	}
}

void UArcCoreInteractionSourceComponent::HandleTargetingCompleted(FArcTargetingHandle InHandle)
{
	UArcTargetingSubsystem* Subsystem = UArcTargetingSubsystem::Get(this);
	if (Subsystem == nullptr)
	{
		return;
	}

	const FArcTargetingContext* Ctx = Subsystem->GetContext(InHandle);
	if (Ctx == nullptr)
	{
		return;
	}

	CachedCandidates = Ctx->Candidates;

	// Extract the best target (first candidate after filtering + sorting)
	TScriptInterface<IInteractionTarget> NewTarget;
	FMassEntityHandle NewEntityTarget;

	if (Ctx->Candidates.Num() > 0)
	{
		const FArcTargetCandidate& BestCandidate = Ctx->Candidates[0];

		if (AActor* HitActor = BestCandidate.HitResult.GetActor())
		{
			// Try actor first
			NewTarget = TScriptInterface<IInteractionTarget>(HitActor);
			if (!NewTarget)
			{
				// Try the hit component
				NewTarget = TScriptInterface<IInteractionTarget>(BestCandidate.HitResult.GetComponent());
			}
			if (!NewTarget)
			{
				// Search actor's components for any IInteractionTarget implementor
				for (UActorComponent* Comp : HitActor->GetComponents())
				{
					TScriptInterface<IInteractionTarget> CompTarget(Comp);
					if (CompTarget)
					{
						NewTarget = CompTarget;
						break;
					}
				}
			}
		}

		// Fallback: entity target carried by the candidate
		if (!NewTarget && BestCandidate.Entity.IsValid())
		{
			NewEntityTarget = BestCandidate.Entity;
		}
	}

	// No change — early out
	if (NewTarget == CurrentTarget && NewEntityTarget == CurrentEntityTarget)
	{
		return;
	}

	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());

	// Scope these messages to the owning local player's endpoint so other local players (split-screen) don't
	// receive this player's interaction prompts. Listeners must bind to the same per-controller endpoint.
	// Fail safe: if the endpoint can't be resolved we skip the broadcast entirely rather than fall back to the
	// world-wide default endpoint (which would leak this player's prompts to every listener).
	const TSharedPtr<FAsyncMessageBindingEndpoint> PlayerEndpoint = ArcCoreInteractionSourceComponent_Endpoint::ResolvePlayerEndpoint(this);
	const bool bCanBroadcast = MessageSystem.IsValid() && PlayerEndpoint.IsValid();
	if (MessageSystem.IsValid() && !PlayerEndpoint.IsValid())
	{
		UE_LOG(LogGameplayInteractions, Warning,
			TEXT("%hs: no per-controller async-message endpoint resolved for %s; suppressing interaction broadcast to avoid a world-wide leak."),
			__FUNCTION__, *GetNameSafe(GetOwner()));
	}

	// Invalidate old target
	if (CurrentTarget || CurrentEntityTarget.IsValid())
	{
		TeardownInteractionInput();

		PreviousQueryResults = MoveTemp(CurrentQueryResults);

		if (bCanBroadcast)
		{
			FArcInteractionInvalidatedMessage InvalidatedMsg;
			InvalidatedMsg.PreviousTarget = CurrentTarget;
			InvalidatedMsg.PreviousQueryResults = PreviousQueryResults;

			FAsyncMessageId MessageId(Arcx::InteractionMessages::Interaction_Invalidated);
			MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(InvalidatedMsg), PlayerEndpoint);
		}
	}

	// Set new target
	CurrentTarget = NewTarget;
	CurrentEntityTarget = NewEntityTarget;
	CurrentQueryResults.Reset();

	// Acquire new actor target
	if (CurrentTarget)
	{
		FInteractionContext Context;
		Context.Target = CurrentTarget;
		CurrentTarget->AppendTargetConfiguration(Context, CurrentQueryResults);

		if (bCanBroadcast)
		{
			FArcInteractionAcquiredMessage AcquiredMsg;
			AcquiredMsg.Target = CurrentTarget;
			AcquiredMsg.QueryResults = CurrentQueryResults;

			FAsyncMessageId MessageId(Arcx::InteractionMessages::Interaction_Acquired);
			MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(AcquiredMsg), PlayerEndpoint);
		}
	}
	// Acquire new entity target — build query results from SO definition
	else if (CurrentEntityTarget.IsValid())
	{
		TObjectPtr<UInputMappingContext> AcquiredMappingContext = nullptr;

		UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		if (EntitySubsystem)
		{
			const FMassEntityManager& EM = EntitySubsystem->GetEntityManager();
			const FArcSmartObjectDefinitionSharedFragment* SODefFragment = EM.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(CurrentEntityTarget);
			if (SODefFragment && SODefFragment->SmartObjectDefinition)
			{
				AcquiredMappingContext = SODefFragment->InteractionMappingContext;

				// SlotIndex must track every definition slot (not just those with display data) so it
				// aligns with the server-side GetAllSlots() index used to claim the slot.
				int32 SlotIndex = 0;
				for (const FSmartObjectSlotDefinition& Slot : SODefFragment->SmartObjectDefinition->GetSlots())
				{
					const FArcInteractionDisplayData* DisplayData = Slot.GetDefinitionDataPtr<FArcInteractionDisplayData>();
					if (DisplayData != nullptr)
					{
						if (DisplayData->Options.Num() > 0)
						{
							for (const FArcInteractionOption& Option : DisplayData->Options)
							{
								FArcInteractionEntityOption EntityOption;
								EntityOption.DisplayName = Option.DisplayName;
								EntityOption.OptionTag = Option.OptionTag;
								EntityOption.InputAction = Option.InputAction;
								EntityOption.SlotIndex = SlotIndex;
								CurrentQueryResults.AvailableInteractions.Add(FInstancedStruct::Make(EntityOption));
							}
						}
						else
						{
							// Legacy single-option slot — synthesize one option from the display data.
							FArcInteractionEntityOption EntityOption;
							EntityOption.DisplayName = DisplayData->DisplayName;
							EntityOption.OptionTag = DisplayData->InteractionTag;
							EntityOption.SlotIndex = SlotIndex;
							CurrentQueryResults.AvailableInteractions.Add(FInstancedStruct::Make(EntityOption));
						}
					}

					++SlotIndex;
				}
			}
		}

		if (bCanBroadcast)
		{
			const FArcTargetCandidate& BestCandidate = CachedCandidates[0];

			FArcInteractionAcquiredMessage AcquiredMsg;
			AcquiredMsg.QueryResults = CurrentQueryResults;
			AcquiredMsg.EntityHandle = CurrentEntityTarget;
			AcquiredMsg.Location = ArcMassPhysicsEntityLink::ResolveHitToBodyTransform(BestCandidate.HitResult).GetLocation();
			AcquiredMsg.MappingContext = AcquiredMappingContext;

			FAsyncMessageId MessageId(Arcx::InteractionMessages::Interaction_Acquired);
			MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(AcquiredMsg), PlayerEndpoint);
		}

		SetupInteractionInput(AcquiredMappingContext);
	}

	OnInteractionChanged();
	NativeOnInteractionChanged();
}

void UArcCoreInteractionSourceComponent::NotifyTriggerInteraction()
{
	NativeOnTriggerInteraction();
	OnTriggerInteraction();
}

namespace ArcCoreInteractionSourceComponent_Local
{
	/** Priority for the per-interaction IMC. Below ability-task IMCs (999) so interaction keys never shadow abilities. */
	constexpr int32 InteractionInputPriority = 100;
}

void UArcCoreInteractionSourceComponent::SetupInteractionInput(UInputMappingContext* MappingContext)
{
	// Opt-in guard: no IMC authored → legacy single-trigger path only, nothing to bind here.
	if (MappingContext == nullptr)
	{
		return;
	}

	APlayerState* PS = GetOwner<APlayerState>();
	APawn* Pawn = PS != nullptr ? PS->GetPawn() : nullptr;
	APlayerController* PC = PS != nullptr ? PS->GetPlayerController() : nullptr;
	if (Pawn == nullptr || PC == nullptr)
	{
		return;
	}

	UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (InputComponent == nullptr)
	{
		return;
	}

	// Bind each acquired option that carries an input action; capture its OptionTag + SlotIndex as bound args.
	bool bBoundAny = false;
	for (const FInstancedStruct& Entry : CurrentQueryResults.AvailableInteractions)
	{
		const FArcInteractionEntityOption* Option = Entry.GetPtr<FArcInteractionEntityOption>();
		if (Option == nullptr || Option->InputAction == nullptr)
		{
			continue;
		}

		const FEnhancedInputActionEventBinding& Binding = InputComponent->BindAction(
			Option->InputAction,
			ETriggerEvent::Triggered,
			this,
			&UArcCoreInteractionSourceComponent::HandleInteractionOptionTriggered,
			Option->OptionTag,
			Option->SlotIndex);

		InteractionBindingHandles.Add(Binding.GetHandle());
		bBoundAny = true;
	}

	if (!bBoundAny)
	{
		return;
	}

	BoundInputComponent = InputComponent;

	// Push the IMC on the LocalPlayer subsystem so it survives pawn possession changes.
	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer != nullptr
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (InputSubsystem != nullptr)
	{
		InputSubsystem->AddMappingContext(MappingContext, ArcCoreInteractionSourceComponent_Local::InteractionInputPriority);
		ActiveMappingContext = MappingContext;
	}
}

void UArcCoreInteractionSourceComponent::TeardownInteractionInput()
{
	if (UEnhancedInputComponent* InputComponent = BoundInputComponent.Get())
	{
		for (const uint32 Handle : InteractionBindingHandles)
		{
			InputComponent->RemoveBindingByHandle(Handle);
		}
	}
	InteractionBindingHandles.Reset();
	BoundInputComponent.Reset();

	if (ActiveMappingContext != nullptr)
	{
		APlayerState* PS = GetOwner<APlayerState>();
		APlayerController* PC = PS != nullptr ? PS->GetPlayerController() : nullptr;
		ULocalPlayer* LocalPlayer = PC != nullptr ? PC->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer != nullptr
			? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
			: nullptr;
		if (InputSubsystem != nullptr)
		{
			InputSubsystem->RemoveMappingContext(ActiveMappingContext);
		}
		ActiveMappingContext = nullptr;
	}
}

void UArcCoreInteractionSourceComponent::HandleInteractionOptionTriggered(const FInputActionInstance& /*Instance*/, FGameplayTag OptionTag, int32 SlotIndex)
{
	if (!CurrentEntityTarget.IsValid())
	{
		return;
	}

	APlayerState* PS = GetOwner<APlayerState>();
	APawn* Pawn = PS != nullptr ? PS->GetPawn() : nullptr;
	APlayerController* PC = PS != nullptr ? PS->GetPlayerController() : nullptr;
	if (Pawn == nullptr || PC == nullptr)
	{
		return;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	const bool bSent = Arcx::SendServerCommand<FArcInteractCommand>(PC
		, Pawn
		, /*TargetObject*/ nullptr
		, CurrentEntityTarget
		, EyeLocation
		, EyeRotation.Vector()
		, OptionTag
		, SlotIndex);

	if (bSent)
	{
		NotifyTriggerInteraction();
	}
}
