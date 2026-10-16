// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueBehavior_SocketNiagara.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "ArcCuesModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueBehavior_SocketNiagara)

namespace ArcCueBehaviorSocketNiagara
{
	static USkeletalMeshComponent* ResolveMesh(const FArcCueContext& Context, FName MeshComponentTag, FName SocketName)
	{
		FMassEntityManager* EntityManager = Context.EntityManager;
		if (EntityManager == nullptr || !Context.Parameters.TargetEntity.IsValid()
			|| !EntityManager->IsEntityActive(Context.Parameters.TargetEntity))
		{
			UE_LOGF(LogArcCues, Warning, "Socket Niagara requires an active target entity for socket '%ls'.", *SocketName.ToString());
			return nullptr;
		}

		FMassActorFragment* ActorFragment = EntityManager->GetFragmentDataPtr<FMassActorFragment>(Context.Parameters.TargetEntity);
		AActor* Actor = ActorFragment != nullptr ? ActorFragment->GetMutable() : nullptr;
		if (!IsValid(Actor))
		{
			UE_LOGF(LogArcCues, Warning, "Socket Niagara target entity %ls has no active actor for socket '%ls'.",
				*Context.Parameters.TargetEntity.DebugGetDescription(), *SocketName.ToString());
			return nullptr;
		}

		USkeletalMeshComponent* Mesh = nullptr;
		if (!MeshComponentTag.IsNone())
		{
			const TArray<UActorComponent*> Components = Actor->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), MeshComponentTag);
			Mesh = Components.IsEmpty() ? nullptr : Cast<USkeletalMeshComponent>(Components[0]);
		}
		else
		{
			Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
		}

		if (!IsValid(Mesh))
		{
			UE_LOGF(LogArcCues, Warning, "Socket Niagara actor '%ls' has no skeletal mesh matching component tag '%ls'.",
				*GetNameSafe(Actor), *MeshComponentTag.ToString());
			return nullptr;
		}

		if (SocketName.IsNone() || !Mesh->DoesSocketExist(SocketName))
		{
			UE_LOGF(LogArcCues, Warning, "Socket Niagara socket '%ls' does not exist on skeletal mesh component '%ls'.",
				*SocketName.ToString(), *GetNameSafe(Mesh));
			return nullptr;
		}

		return Mesh;
	}

	static UNiagaraComponent* Spawn(
		const FArcCueContext& Context,
		UNiagaraSystem* NiagaraSystem,
		FName MeshComponentTag,
		FName SocketName,
		const FTransform& LocalOffset,
		bool bAutoDestroy)
	{
		if (!IsValid(NiagaraSystem))
		{
			return nullptr;
		}

		USkeletalMeshComponent* Mesh = ResolveMesh(Context, MeshComponentTag, SocketName);
		if (Mesh == nullptr)
		{
			return nullptr;
		}

		return UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraSystem,
			Mesh,
			SocketName,
			LocalOffset.GetLocation(),
			LocalOffset.Rotator(),
			LocalOffset.GetScale3D(),
			EAttachLocation::KeepRelativeOffset,
			bAutoDestroy,
			bAutoDestroy ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None,
			false,
			false);
	}

	static void ApplyAction(UNiagaraComponent& Component, EArcCueNiagaraAction Action)
	{
		switch (Action)
		{
		case EArcCueNiagaraAction::Activate:
			Component.Activate(true);
			break;
		case EArcCueNiagaraAction::Deactivate:
			Component.Deactivate();
			break;
		case EArcCueNiagaraAction::DeactivateImmediate:
			Component.DeactivateImmediate();
			break;
		case EArcCueNiagaraAction::None:
			break;
		}
	}
}

void FArcCueBehavior_SocketNiagara::OnExecute(const FArcCueContext& Context) const
{
	UNiagaraComponent* Component = ArcCueBehaviorSocketNiagara::Spawn(
		Context, NiagaraSystem, MeshComponentTag, SocketName, LocalOffset, true);
	if (Component == nullptr)
	{
		return;
	}

	Control.ApplyInitial(*Component);
	Component->Activate(true);
}

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_SocketNiagara::OnApplication(const FArcCueContext& Context) const
{
	if (!Control.bStartOnApplication)
	{
		return {};
	}

	UNiagaraComponent* Component = ArcCueBehaviorSocketNiagara::Spawn(
		Context, NiagaraSystem, MeshComponentTag, SocketName, LocalOffset, false);
	if (Component == nullptr)
	{
		return {};
	}

	Control.ApplyInitial(*Component);
	Component->Activate(true);

	FArcCueBehaviorState_SocketNiagara State;
	State.Component = Component;
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] Socket Niagara created: Handle=%u Component=%ls Socket=%ls", Context.Handle.GetId(), *GetPathNameSafe(Component), *SocketName.ToString());
	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_SocketNiagara>(State);
}

void FArcCueBehavior_SocketNiagara::OnEvent(
	const FArcCueContext& Context,
	const FArcCueEvent& Event,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	const EArcCueNiagaraAction Action = Control.ResolveAction(Event);
	FArcCueBehaviorState_SocketNiagara* TypedState = State.GetMutablePtr<FArcCueBehaviorState_SocketNiagara>();
	UNiagaraComponent* Component = TypedState != nullptr ? TypedState->Component.Get() : nullptr;
	if (!IsValid(Component))
	{
		if (Action != EArcCueNiagaraAction::Activate)
		{
			return;
		}

		Component = ArcCueBehaviorSocketNiagara::Spawn(
			Context, NiagaraSystem, MeshComponentTag, SocketName, LocalOffset, false);
		if (Component == nullptr)
		{
			return;
		}

		Control.ApplyInitial(*Component);
		FArcCueBehaviorState_SocketNiagara NewState;
		NewState.Component = Component;
		State = TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_SocketNiagara>(NewState);
	}

	Control.ApplyEvent(Event, *Component);
	ArcCueBehaviorSocketNiagara::ApplyAction(*Component, Action);
}

void FArcCueBehavior_SocketNiagara::OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const
{
	FArcCueBehaviorState_SocketNiagara* TypedState = static_cast<FArcCueBehaviorState_SocketNiagara*>(&State);
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] Socket Niagara removal: Handle=%u", Context.Handle.GetId());
	if (TypedState == nullptr || !IsValid(TypedState->Component))
	{
		return;
	}

	TypedState->Component->DeactivateImmediate();
	TypedState->Component->DestroyComponent();
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] Socket Niagara destroyed: Handle=%u", Context.Handle.GetId());
	TypedState->Component = nullptr;
}
