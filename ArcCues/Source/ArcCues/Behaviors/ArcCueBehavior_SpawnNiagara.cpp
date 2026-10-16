// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueBehavior_SpawnNiagara.h"

#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueBehavior_SpawnNiagara)

namespace ArcCueBehaviorSpawnNiagara
{
	static UNiagaraComponent* Spawn(
		const FArcCueContext& Context,
		UNiagaraSystem* NiagaraSystem,
		const FTransform& LocalOffset,
		bool bAutoDestroy)
	{
		if (Context.World == nullptr || !IsValid(NiagaraSystem))
		{
			return nullptr;
		}

		const FTransform SpawnTransform = LocalOffset * Context.Parameters.Location;
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Context.World,
			NiagaraSystem,
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			SpawnTransform.GetScale3D(),
			bAutoDestroy,
			false,
			bAutoDestroy ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None);
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

void FArcCueBehavior_SpawnNiagara::OnExecute(const FArcCueContext& Context) const
{
	UNiagaraComponent* Component = ArcCueBehaviorSpawnNiagara::Spawn(Context, NiagaraSystem, LocalOffset, true);
	if (Component == nullptr)
	{
		return;
	}

	Control.ApplyInitial(*Component);
	Component->Activate(true);
}

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_SpawnNiagara::OnApplication(const FArcCueContext& Context) const
{
	if (!Control.bStartOnApplication)
	{
		return {};
	}

	UNiagaraComponent* Component = ArcCueBehaviorSpawnNiagara::Spawn(Context, NiagaraSystem, LocalOffset, false);
	if (Component == nullptr)
	{
		return {};
	}

	Control.ApplyInitial(*Component);
	Component->Activate(true);

	FArcCueBehaviorState_SpawnNiagara State;
	State.Component = Component;
	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_SpawnNiagara>(State);
}

void FArcCueBehavior_SpawnNiagara::OnEvent(
	const FArcCueContext& Context,
	const FArcCueEvent& Event,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	const EArcCueNiagaraAction Action = Control.ResolveAction(Event);
	FArcCueBehaviorState_SpawnNiagara* TypedState = State.GetMutablePtr<FArcCueBehaviorState_SpawnNiagara>();
	UNiagaraComponent* Component = TypedState != nullptr ? TypedState->Component.Get() : nullptr;

	if (!IsValid(Component))
	{
		if (Action != EArcCueNiagaraAction::Activate)
		{
			return;
		}

		Component = ArcCueBehaviorSpawnNiagara::Spawn(Context, NiagaraSystem, LocalOffset, false);
		if (Component == nullptr)
		{
			return;
		}

		Control.ApplyInitial(*Component);
		FArcCueBehaviorState_SpawnNiagara NewState;
		NewState.Component = Component;
		State = TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_SpawnNiagara>(NewState);
	}

	Control.ApplyEvent(Event, *Component);
	ArcCueBehaviorSpawnNiagara::ApplyAction(*Component, Action);
}

void FArcCueBehavior_SpawnNiagara::OnRemoved(const FArcCueContext&, FArcCueBehaviorState& State) const
{
	FArcCueBehaviorState_SpawnNiagara* TypedState = static_cast<FArcCueBehaviorState_SpawnNiagara*>(&State);
	if (TypedState == nullptr || !IsValid(TypedState->Component))
	{
		return;
	}

	TypedState->Component->DeactivateImmediate();
	TypedState->Component->DestroyComponent();
	TypedState->Component = nullptr;
}
