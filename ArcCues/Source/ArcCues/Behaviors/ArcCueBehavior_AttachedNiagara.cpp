// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueBehavior_AttachedNiagara.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"

#include "ArcMass/Attachment/ArcMassAttachmentFragment.h"
#include "ArcMass/NiagaraVisualization/ArcNiagaraRenderStateHelper.h"

#include "ArcCueEntityFragment.h"
#include "ArcCueSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueBehavior_AttachedNiagara)

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_AttachedNiagara::CreateState(
	const FArcCueContext& Context,
	const FArcCueEvent* Event) const
{
	FMassEntityManager* EntityManager = Context.EntityManager;
	if (EntityManager == nullptr || !IsValid(NiagaraConfig.NiagaraSystem))
	{
		return {};
	}

	FTransform SpawnTransform = NiagaraConfig.ComponentTransform * Context.Parameters.Location;
	if (Context.Parameters.TargetEntity.IsValid() && EntityManager->IsEntityActive(Context.Parameters.TargetEntity))
	{
		const FTransformFragment* TargetTransform = EntityManager->GetFragmentDataPtr<FTransformFragment>(Context.Parameters.TargetEntity);
		if (TargetTransform != nullptr)
		{
			SpawnTransform = NiagaraConfig.ComponentTransform * TargetTransform->GetTransform();
		}
	}

	FArcCueBehaviorState_AttachedNiagara StateValue;
	Control.ApplyInitial(StateValue.Parameters);
	if (Event != nullptr)
	{
		Control.ApplyEvent(*Event, StateValue.Parameters);
	}
	StateValue.PendingAction = EArcCueNiagaraAction::Activate;
	StateValue.bActionPending = true;

	FArcCueEntityFragment CueEntityFragment;
	CueEntityFragment.OwningCue = Context.Handle;
	CueEntityFragment.BehaviorIndex = Context.BehaviorIndex;

	FArcMassAttachmentFragment AttachmentFragment;
	AttachmentFragment.Target = Context.Parameters.TargetEntity;
	AttachmentFragment.LocalOffset = NiagaraConfig.ComponentTransform;

	FArcNiagaraVisConfigFragment VisConfig;
	VisConfig.NiagaraSystem = NiagaraConfig.NiagaraSystem;
	VisConfig.bCastShadow = NiagaraConfig.bCastShadow;
	VisConfig.bAutoActivate = false;
	VisConfig.bFixedBounds = NiagaraConfig.bFixedBounds;
	VisConfig.FixedBounds = NiagaraConfig.FixedBounds;
	VisConfig.ComponentTransform = NiagaraConfig.ComponentTransform;

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcCueEntityFragment>(CueEntityFragment);
	Builder.Add<FArcMassAttachmentFragment>(AttachmentFragment);
	Builder.Add<FTransformFragment>(SpawnTransform);
	Builder.Add(FArcNiagaraVisFragment::StaticStruct());
	Builder.Add<FArcNiagaraVisTag>();
	Builder.Add<FArcNiagaraVisConfigFragment>(VisConfig);
	Builder.Add<FArcNiagaraVisParamsFragment>(StateValue.Parameters);

	StateValue.SpawnedEntity = Builder.Commit();
	if (!StateValue.SpawnedEntity.IsValid())
	{
		return {};
	}

	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_AttachedNiagara>(StateValue);
}

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_AttachedNiagara::OnApplication(const FArcCueContext& Context) const
{
	return Control.bStartOnApplication ? CreateState(Context, nullptr) : TInstancedStruct<FArcCueBehaviorState>();
}

void FArcCueBehavior_AttachedNiagara::OnEvent(
	const FArcCueContext& Context,
	const FArcCueEvent& Event,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	const EArcCueNiagaraAction Action = Control.ResolveAction(Event);
	FArcCueBehaviorState_AttachedNiagara* TypedState = State.GetMutablePtr<FArcCueBehaviorState_AttachedNiagara>();
	if (TypedState == nullptr)
	{
		if (Action == EArcCueNiagaraAction::Activate)
		{
			State = CreateState(Context, &Event);
		}
		return;
	}

	Control.ApplyEvent(Event, TypedState->Parameters);
	TypedState->bParametersPending = true;
	if (Action != EArcCueNiagaraAction::None)
	{
		TypedState->PendingAction = Action;
		TypedState->bActionPending = true;
	}
}

void FArcCueBehavior_AttachedNiagara::OnTick(const FArcCueContext& Context, FArcCueBehaviorState& State, float) const
{
	FArcCueBehaviorState_AttachedNiagara* TypedState = static_cast<FArcCueBehaviorState_AttachedNiagara*>(&State);
	FMassEntityManager* EntityManager = Context.EntityManager;
	if (TypedState == nullptr || EntityManager == nullptr || !TypedState->SpawnedEntity.IsValid()
		|| !EntityManager->CanMutateSynchronously() || !EntityManager->IsEntityActive(TypedState->SpawnedEntity))
	{
		return;
	}

	FArcNiagaraVisFragment* NiagaraFragment = EntityManager->GetFragmentDataPtr<FArcNiagaraVisFragment>(TypedState->SpawnedEntity);
	FArcNiagaraVisParamsFragment* Parameters = EntityManager->GetFragmentDataPtr<FArcNiagaraVisParamsFragment>(TypedState->SpawnedEntity);
	if (NiagaraFragment == nullptr || Parameters == nullptr || NiagaraFragment->RenderStateHelper == nullptr)
	{
		return;
	}

	FArcNiagaraRenderStateHelper& RenderState = *NiagaraFragment->RenderStateHelper;
	if (TypedState->bParametersPending)
	{
		*Parameters = TypedState->Parameters;
		RenderState.ApplyParameterOverrides(*Parameters);
		Parameters->bDirty = false;
		TypedState->bParametersPending = false;
	}

	if (!TypedState->bActionPending)
	{
		return;
	}

	const FNiagaraSystemInstanceControllerPtr Controller = RenderState.GetController();
	if (!Controller.IsValid())
	{
		return;
	}

	switch (TypedState->PendingAction)
	{
	case EArcCueNiagaraAction::Activate:
		Controller->Activate(FNiagaraSystemInstance::EResetMode::ResetAll);
		break;
	case EArcCueNiagaraAction::Deactivate:
		Controller->Deactivate(false);
		break;
	case EArcCueNiagaraAction::DeactivateImmediate:
		Controller->Deactivate(true);
		break;
	case EArcCueNiagaraAction::None:
		break;
	}

	TypedState->PendingAction = EArcCueNiagaraAction::None;
	TypedState->bActionPending = false;
}

void FArcCueBehavior_AttachedNiagara::OnAttachmentLost(
	const FArcCueContext&,
	FMassEntityHandle AttachmentEntity,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	FArcCueBehaviorState_AttachedNiagara* TypedState = State.GetMutablePtr<FArcCueBehaviorState_AttachedNiagara>();
	if (TypedState == nullptr || TypedState->SpawnedEntity != AttachmentEntity)
	{
		return;
	}

	TypedState->SpawnedEntity.Reset();
	State.Reset();
}

void FArcCueBehavior_AttachedNiagara::OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const
{
	FArcCueBehaviorState_AttachedNiagara* TypedState = static_cast<FArcCueBehaviorState_AttachedNiagara*>(&State);
	if (TypedState == nullptr || !TypedState->SpawnedEntity.IsValid())
	{
		return;
	}

	UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(Context.World);
	if (Subsystem != nullptr)
	{
		Subsystem->QueueCueEntityDestroy(TypedState->SpawnedEntity);
		TypedState->SpawnedEntity.Reset();
	}
}
