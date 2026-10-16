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

#include "Pawn/ArcMassPawnExtensionComponent.h"

#include "ArcMassGameplayFramework.h"
#include "Core/ArcMassGameFrameworkTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"
#include "MassProcessingPhaseManager.h"
#include "MassSimulationSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "StructUtils/InstancedStruct.h"

#include "Core/CameraRigAsset.h"
#include "GameFramework/GameplayCameraComponent.h"

#include "Pawn/ArcMassFrameworkAgentComponent.h"
#include "Pawn/ArcMassPawn.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnDataFragments.h"
#include "Pawn/ArcMassPlayerInputComponent.h"

DEFINE_LOG_CATEGORY(LogArcMassPawnExtensionComponent);

namespace ArcMassPawnExtension
{
	/**
	 * Loads PawnData->DefaultCameraRig (if set) and activates it on the pawn's
	 * UGameplayCameraComponent via ActivatePersistentBaseCameraRig. Silent no-op if the rig is
	 * unset or the pawn has no camera component (legacy actor, headless server, etc.).
	 */
	static void ActivateDefaultCameraRig(APawn* Pawn, const UArcMassPawnData* PawnData)
	{
		if (Pawn == nullptr || PawnData == nullptr)
		{
			return;
		}

		if (PawnData->DefaultCameraRig.IsNull())
		{
			return;
		}

		UGameplayCameraComponent* CameraComp = nullptr;
		if (AArcMassPlayerPawn* PlayerPawn = Cast<AArcMassPlayerPawn>(Pawn))
		{
			// Player pawns guarantee a camera component via three-tier resolution
			// (BP subobject → PawnData → runtime fallback).
			CameraComp = PlayerPawn->GetGameplayCameraComponent();
		}
		else
		{
			CameraComp = Pawn->FindComponentByClass<UGameplayCameraComponent>();
		}

		if (CameraComp == nullptr)
		{
			UE_LOGF(LogArcMassPawnExtensionComponent, Verbose
				,"ActivateDefaultCameraRig: pawn [%ls] has no UGameplayCameraComponent; skipping rig [%ls].", *GetNameSafe(Pawn)
				, *PawnData->DefaultCameraRig.ToString());
			return;
		}

		UCameraRigAsset* Rig = PawnData->DefaultCameraRig.LoadSynchronous();
		if (Rig == nullptr)
		{
			UE_LOGF(LogArcMassPawnExtensionComponent, Warning
				,"ActivateDefaultCameraRig: failed to load DefaultCameraRig [%ls] for pawn [%ls].", *PawnData->DefaultCameraRig.ToString()
				, *GetNameSafe(Pawn));
			return;
		}

		CameraComp->ActivatePersistentBaseCameraRig(Rig);
	}
}

const FName UArcMassPawnExtensionComponent::NAME_ActorFeatureName(TEXT("PawnExtension"));
const FName UArcMassPawnExtensionComponent::NAME_BindInputsNow(TEXT("BindInputsNow"));

UArcMassPawnExtensionComponent::UArcMassPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UArcMassPawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = REPNOTIFY_Always;
	SharedParams.Condition = COND_None;

	DOREPLIFETIME_WITH_PARAMS_FAST(UArcMassPawnExtensionComponent, PawnData, SharedParams);
}

void UArcMassPawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"OnRegister: Component=%p Owner=[%ls] OwnerClass=[%ls] PawnCast=%p NetMode=%d HasAuthority=%d", this
		, *GetNameSafe(GetOwner())
		, GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("<null>")
		, Pawn
		, GetWorld() ? (int32)GetWorld()->GetNetMode() : -1
		, GetOwner() ? (int32)GetOwner()->HasAuthority() : -1);

	ensureAlwaysMsgf((Pawn != nullptr)
		, TEXT("UArcMassPawnExtensionComponent on [%s] can only be added to Pawn actors.")
		, *GetNameSafe(GetOwner()));

	TArray<UActorComponent*> PawnExtensionComponents;
	if (Pawn)
	{
		Pawn->GetComponents(UArcMassPawnExtensionComponent::StaticClass(), PawnExtensionComponents);
	}
	ensureAlwaysMsgf((PawnExtensionComponents.Num() <= 1)
		, TEXT("Only one UArcMassPawnExtensionComponent should exist on [%s].")
		, *GetNameSafe(GetOwner()));

	// Register with the init state system (no-op outside game worlds).
	RegisterInitStateFeature();

	// Register the owning pawn as a GameFramework component receiver so GameFeatureActions
	// (e.g. PushWidgetToLayer / AddWidget) that AddExtensionHandler on APawn get invoked.
	if (AActor* Owner = GetOwner())
	{
		UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(Owner);
	}
}

void UArcMassPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"BeginPlay: Owner=[%ls] PawnData=[%ls] HasAuthority=%d", *GetNameSafe(GetOwner())
		, *GetNameSafe(PawnData)
		, GetOwner() ? (int32)GetOwner()->HasAuthority() : -1);

	// Listen for changes to all features on this actor.
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	// Auto-add the bridge component if missing so this extension works on any pawn.
	// The bridge forwards entity-handle assignment back to CheckDefaultInitialization
	// directly via UArcMassFrameworkAgentComponent::SetEntityHandle override.
	if (AActor* Owner = GetOwner())
	{
		if (Owner->FindComponentByClass<UArcMassFrameworkAgentComponent>() == nullptr)
		{
			UArcMassFrameworkAgentComponent* AgentComp = NewObject<UArcMassFrameworkAgentComponent>(
				Owner, UArcMassFrameworkAgentComponent::StaticClass(), NAME_None, RF_Transient);
			if (AgentComp)
			{
				AgentComp->RegisterComponent();
				UE_LOGF(LogArcMassPawnExtensionComponent, Display
					,"BeginPlay: created+registered UArcMassFrameworkAgentComponent on [%ls]", *GetNameSafe(Owner));
			}
		}
		else
		{
			UE_LOGF(LogArcMassPawnExtensionComponent, Display
				,"BeginPlay: UArcMassFrameworkAgentComponent already present on [%ls]", *GetNameSafe(Owner));
		}
	}

	// Notify state manager that we have spawned, then try to advance the rest of the chain.
	const bool bEnteredSpawned = TryToChangeInitState(FArcMassGameFrameworkTags::Get().InitState_Spawned);
	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"BeginPlay: TryToChangeInitState(Spawned) returned %d on [%ls]", (int32)bEnteredSpawned, *GetNameSafe(GetOwner()));
	ensureMsgf(bEnteredSpawned
		, TEXT("UArcMassPawnExtensionComponent::BeginPlay failed to enter InitState.Spawned on [%s]")
		, *GetNameSafe(GetOwner()));
	CheckDefaultInitialization();
}

void UArcMassPawnExtensionComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (PrePhysicsEndHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UMassSimulationSubsystem* SimSubsystem = World->GetSubsystem<UMassSimulationSubsystem>())
			{
				SimSubsystem->GetMutablePhaseManager()
					.GetOnPhaseEnd(EMassProcessingPhase::PrePhysics)
					.Remove(PrePhysicsEndHandle);
			}
		}
		PrePhysicsEndHandle.Reset();
	}

	for (TSharedPtr<FStreamableHandle>& H : PendingInitLoadHandles)
	{
		if (H.IsValid() && H->IsActive())
		{
			H->CancelHandle();
		}
	}
	PendingInitLoadHandles.Reset();

	if (AActor* Owner = GetOwner())
	{
		UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(Owner);
	}

	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

void UArcMassPawnExtensionComponent::TrackInitLoadHandle(TSharedPtr<FStreamableHandle> Handle)
{
	if (Handle.IsValid())
	{
		PendingInitLoadHandles.Add(Handle);
	}
}

void UArcMassPawnExtensionComponent::RegisterMassReadyCallback()
{
	if (bMassEntityFullyInitialized || PrePhysicsEndHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassSimulationSubsystem* SimSubsystem = World->GetSubsystem<UMassSimulationSubsystem>();
	if (SimSubsystem == nullptr)
	{
		// Headless / cooked-server paths without simulation: degrade to the (legacy) immediate
		// path so we don't deadlock the pawn at DataAvailable.
		UE_LOGF(LogArcMassPawnExtensionComponent, Warning
			,"RegisterMassReadyCallback: no UMassSimulationSubsystem on [%ls]; falling back to immediate init.", *GetNameSafe(GetOwner()));
		bMassEntityFullyInitialized = true;
		CheckDefaultInitialization();
		return;
	}

	TWeakObjectPtr<UArcMassPawnExtensionComponent> WeakSelf(this);
	PrePhysicsEndHandle = SimSubsystem->GetMutablePhaseManager()
		.GetOnPhaseEnd(EMassProcessingPhase::PrePhysics)
		.AddLambda([WeakSelf](float DeltaSeconds)
		{
			if (UArcMassPawnExtensionComponent* Self = WeakSelf.Get())
			{
				Self->OnMassPrePhysicsEnded(DeltaSeconds);
			}
		});

	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"RegisterMassReadyCallback: bound one-shot OnPhaseEnd(PrePhysics) on [%ls]", *GetNameSafe(GetOwner()));
}

void UArcMassPawnExtensionComponent::OnMassPrePhysicsEnded(float /*DeltaSeconds*/)
{
	if (PrePhysicsEndHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UMassSimulationSubsystem* SimSubsystem = World->GetSubsystem<UMassSimulationSubsystem>())
			{
				SimSubsystem->GetMutablePhaseManager()
					.GetOnPhaseEnd(EMassProcessingPhase::PrePhysics)
					.Remove(PrePhysicsEndHandle);
			}
		}
		PrePhysicsEndHandle.Reset();
	}

	bMassEntityFullyInitialized = true;

	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"OnMassPrePhysicsEnded: gate flipped on [%ls]; advancing init chain", *GetNameSafe(GetOwner()));

	CheckDefaultInitialization();
}

void UArcMassPawnExtensionComponent::NotifyEntityReadyImmediately()
{
	// Checkpoint E: log entry showing current bMassEntityFullyInitialized (idempotency-guard check).
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] UArcMassPawnExtensionComponent::NotifyEntityReadyImmediately: bMassEntityFullyInitialized=%d Owner=%ls",
		(int32)bMassEntityFullyInitialized,
		GetOwner() ? *GetOwner()->GetPathName() : TEXT("<null>"));

	if (bMassEntityFullyInitialized)
	{
		return;
	}

	bMassEntityFullyInitialized = true;

	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"NotifyEntityReadyImmediately: spawner-driven path on [%ls]; advancing init chain without PrePhysics fence", *GetNameSafe(GetOwner()));

	CheckDefaultInitialization();
}

void UArcMassPawnExtensionComponent::SetPawnData(const UArcMassPawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOGF(LogArcMassPawnExtensionComponent, Error
			,"Trying to set PawnData [%ls] on pawn [%ls] that already has valid PawnData [%ls].", *GetNameSafe(InPawnData)
			, *GetNameSafe(Pawn)
			, *GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PawnData, this);

	Pawn->ForceNetUpdate();

	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"SetPawnData: assigned PawnData=[%ls] on pawn [%ls] (auth)", *GetNameSafe(PawnData), *GetNameSafe(Pawn));

	CheckDefaultInitialization();
}

void UArcMassPawnExtensionComponent::OnRep_PawnData()
{
	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"OnRep_PawnData: PawnData=[%ls] on [%ls]", *GetNameSafe(PawnData), *GetNameSafe(GetOwner()));
	CheckDefaultInitialization();
}

void UArcMassPawnExtensionComponent::OnPawnReadyToInitialize_RegisterAndCall(FArcMassOnPawnReadyToInitialize::FDelegate Delegate)
{
	// No dedupe: previous IsBoundToObject(Delegate.GetUObject()) compared the wrong
	// key (would silently drop a second binding from the same UObject onto a different
	// method). Callers are responsible for not double-binding.
	OnPawnReadyToInitializeDelegate.Add(Delegate);

	if (bPawnReadyToInitialize)
	{
		Delegate.Execute(this);
	}
}

void UArcMassPawnExtensionComponent::CheckDefaultInitialization()
{
	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"CheckDefaultInitialization: Owner=[%ls] CurrentState=[%ls] PawnData=[%ls]", *GetNameSafe(GetOwner())
		, *GetInitState().ToString()
		, *GetNameSafe(PawnData));

	// Before checking our progress, try progressing any other features we might depend on.
	CheckDefaultInitializationForImplementers();

	// Single feature -> no DataInitialized -> GameplayReady step (it would just check
	// "all features reached DataInitialized" against itself). The terminal state is
	// DataInitialized; OnPawnReadyToInitialize fires from there.
	static const TArray<FGameplayTag> StateChain = {
		FArcMassGameFrameworkTags::Get().InitState_Spawned
		, FArcMassGameFrameworkTags::Get().InitState_DataAvailable
		, FArcMassGameFrameworkTags::Get().InitState_DataInitialized
	};

	ContinueInitStateChain(StateChain);

	UE_LOGF(LogArcMassPawnExtensionComponent, Display
		,"CheckDefaultInitialization end: Owner=[%ls] PostState=[%ls]", *GetNameSafe(GetOwner())
		, *GetInitState().ToString());
}

bool UArcMassPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager
														, FGameplayTag CurrentState
														, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();

	const FGameplayTag InitSpawned = FArcMassGameFrameworkTags::Get().InitState_Spawned;
	const FGameplayTag InitDataAvailable = FArcMassGameFrameworkTags::Get().InitState_DataAvailable;
	const FGameplayTag InitDataInitialized = FArcMassGameFrameworkTags::Get().InitState_DataInitialized;

	if (!CurrentState.IsValid() && DesiredState == InitSpawned)
	{
		// As long as we are on a valid pawn, we count as spawned.
		if (Pawn)
		{
			return true;
		}
	}
	else if (CurrentState == InitSpawned && DesiredState == InitDataAvailable)
	{
		// 1) PawnData required (server: SetPawnData; client: OnRep_PawnData).
		if (!PawnData)
		{
			UE_LOGF(LogArcMassPawnExtensionComponent, Display
				,"CanChangeInitState[Spawned->DataAvailable]: gate(1) PawnData=null on [%ls] -> false", *GetNameSafe(GetOwner()));
			return false;
		}

		// 2) Controller required on authority or locally-controlled, with PlayerState pairing.
		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		if (bHasAuthority || bIsLocallyControlled)
		{
			AController* Controller = GetController<AController>();
			if (Controller == nullptr)
			{
				UE_LOGF(LogArcMassPawnExtensionComponent, Display
					,"CanChangeInitState[Spawned->DataAvailable]: gate(2) Controller=null (auth=%d localCtrl=%d) on [%ls] -> false", (int32)bHasAuthority, (int32)bIsLocallyControlled, *GetNameSafe(GetOwner()));
				return false;
			}

			// For player-controlled pawns: owning PlayerState must be valid and bound to the controller.
			APlayerController* PC = Cast<APlayerController>(Controller);
			if (PC != nullptr)
			{
				const bool bHasControllerPairedWithPS = (Controller->PlayerState != nullptr)
													  && (Controller->PlayerState->GetOwner() == Controller);
				if (!bHasControllerPairedWithPS)
				{
					UE_LOGF(LogArcMassPawnExtensionComponent, Display
						,"CanChangeInitState[Spawned->DataAvailable]: gate(2b) PlayerState pairing failed (PS=[%ls] PSOwner=[%ls] PC=[%ls]) on [%ls] -> false", *GetNameSafe(Controller->PlayerState)
						, Controller->PlayerState ? *GetNameSafe(Controller->PlayerState->GetOwner()) : TEXT("<null>")
						, *GetNameSafe(Controller)
						, *GetNameSafe(GetOwner()));
					return false;
				}
			}
		}

		// 3) Locally-controlled non-bots additionally need InputComponent + LocalPlayer.
		const bool bIsBot = Pawn->IsBotControlled();
		if (bIsLocallyControlled && !bIsBot)
		{
			APlayerController* PC = GetController<APlayerController>();
			if (!Pawn->InputComponent || !PC || !PC->GetLocalPlayer())
			{
				UE_LOGF(LogArcMassPawnExtensionComponent, Display
					,"CanChangeInitState[Spawned->DataAvailable]: gate(3) Input/LocalPlayer missing (Input=%p PC=%p LP=%p) on [%ls] -> false", Pawn->InputComponent.Get(), PC, PC ? PC->GetLocalPlayer() : nullptr
					, *GetNameSafe(GetOwner()));
				return false;
			}
		}

		// 4) Mass entity handle must be valid.
		const UArcMassFrameworkAgentComponent* AgentComp = GetOwner() ? GetOwner()->FindComponentByClass<UArcMassFrameworkAgentComponent>() : nullptr;
		if (AgentComp == nullptr || !AgentComp->GetEntityHandle().IsValid())
		{
			UE_LOGF(LogArcMassPawnExtensionComponent, Display
				,"CanChangeInitState[Spawned->DataAvailable]: gate(4) AgentComp=%p EntityHandleValid=%d on [%ls] -> false", AgentComp, AgentComp ? (int32)AgentComp->GetEntityHandle().IsValid() : -1
				, *GetNameSafe(GetOwner()));
			return false;
		}

		UE_LOGF(LogArcMassPawnExtensionComponent, Display
			,"CanChangeInitState[Spawned->DataAvailable]: all gates passed on [%ls] -> true", *GetNameSafe(GetOwner()));
		return true;
	}
	else if (CurrentState == InitDataAvailable && DesiredState == InitDataInitialized)
	{
		// All other features on this actor must have reached DataAvailable.
		const bool bAllReady = Manager->HaveAllFeaturesReachedInitState(Pawn, InitDataAvailable);

		// Mass entity must be fully initialized (creation context closed, observer-deferred
		// sparse fragments flushed). Flipped from OnMassPrePhysicsEnded; bypassed by tests
		// that drive HandleChangeInitState directly without going through SetEntityHandle.
		if (!bMassEntityFullyInitialized)
		{
			UE_LOGF(LogArcMassPawnExtensionComponent, Display
				,"CanChangeInitState[DataAvailable->DataInitialized]: HaveAllFeaturesReached=%d but Mass entity not yet fully initialized on [%ls] -> false", (int32)bAllReady, *GetNameSafe(GetOwner()));
			return false;
		}

		UE_LOGF(LogArcMassPawnExtensionComponent, Display
			,"CanChangeInitState[DataAvailable->DataInitialized]: HaveAllFeaturesReached=%d on [%ls]", (int32)bAllReady, *GetNameSafe(GetOwner()));
		return bAllReady;
	}

	return false;
}

void UArcMassPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager
														   , FGameplayTag CurrentState
														   , FGameplayTag DesiredState)
{
	// Checkpoint E: log every From -> To transition.
	UE_LOGF(LogArcMassSpawnDiag, Display,"[ArcMassSpawnDiag] UArcMassPawnExtensionComponent::HandleChangeInitState: %ls -> %ls Owner=%ls",
		*CurrentState.ToString(), *DesiredState.ToString(),
		GetOwner() ? *GetOwner()->GetPathName() : TEXT("<null>"));

	const FGameplayTag InitDataAvailable = FArcMassGameFrameworkTags::Get().InitState_DataAvailable;
	const FGameplayTag InitDataInitialized = FArcMassGameFrameworkTags::Get().InitState_DataInitialized;

	if (CurrentState == InitDataAvailable && DesiredState == InitDataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		if (Pawn == nullptr || PawnData == nullptr)
		{
			return;
		}

		AController* Controller = GetController<AController>();
		APlayerController* PlayerController = Cast<APlayerController>(Controller);
		APlayerState* PlayerState = Pawn->GetPlayerState();

		// Resolve EntityManager + entity handle from the bridge component.
		FMassEntityManager* EntityManager = nullptr;
		FMassEntityHandle EntityHandle;
		if (UWorld* World = GetWorld())
		{
			if (UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>())
			{
				EntityManager = &MassSubsystem->GetMutableEntityManager();
			}
		}
		if (UArcMassFrameworkAgentComponent* AgentComp = Pawn->FindComponentByClass<UArcMassFrameworkAgentComponent>())
		{
			EntityHandle = AgentComp->GetEntityHandle();
		}

		// 1) Add components from PawnData->Components.
		// CLAUDE.md: const FInstancedStruct array on a UDataAsset -> use GetPtr<T>().
		for (const FInstancedStruct& Entry : PawnData->Components)
		{
			const FArcMassAddPawnComponent* AddComp = Entry.GetPtr<FArcMassAddPawnComponent>();
			if (AddComp)
			{
				AddComp->GiveComponent(Pawn);
			}
		}

		// 2) Run init fragments.
		for (const FInstancedStruct& Entry : PawnData->InitFragments)
		{
			const FArcMassPawnInitFragment* InitFrag = Entry.GetPtr<FArcMassPawnInitFragment>();
			if (InitFrag)
			{
				InitFrag->Initialize(Pawn, PlayerState, PlayerController, EntityManager, EntityHandle, this);
			}
		}

		// 3) Activate the default camera rig (if any) on the pawn's UGameplayCameraComponent.
		ArcMassPawnExtension::ActivateDefaultCameraRig(Pawn, PawnData);

		// 4) Player-only: spawn the player input component on demand and wire Enhanced Input
		// from PawnData->InputConfig. Gate is already validated in CanChangeInitState;
		// re-check here so server / simulated proxies skip cleanly without paying for the component.
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const bool bIsBot = Pawn->IsBotControlled();
		if (bIsLocallyControlled && !bIsBot && PlayerController != nullptr && Pawn->InputComponent != nullptr)
		{
			UArcMassPlayerInputComponent* InputComp = Pawn->FindComponentByClass<UArcMassPlayerInputComponent>();
			if (InputComp == nullptr)
			{
				InputComp = NewObject<UArcMassPlayerInputComponent>(
					Pawn, UArcMassPlayerInputComponent::StaticClass(), NAME_None, RF_Transient);
				if (InputComp)
				{
					InputComp->RegisterComponent();
				}
			}
			if (InputComp)
			{
				InputComp->InitializePlayerInput(Pawn->InputComponent);
			}
		}

		bPawnReadyToInitialize = true;
		OnPawnReadyToInitializeDelegate.Broadcast(this);

		// Signal the GameFramework extension graph that this pawn is fully initialized so
		// GameFeatureActions watching APawn (PushWidgetToLayer / AddWidget) fire.
		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
			Pawn, UGameFrameworkComponentManager::NAME_GameActorReady);
	}
}

void UArcMassPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	// If another feature is now in DataAvailable, see if we should transition to DataInitialized.
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		if (Params.FeatureState == FArcMassGameFrameworkTags::Get().InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}

