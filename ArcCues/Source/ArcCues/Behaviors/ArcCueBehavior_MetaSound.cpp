// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueBehavior_MetaSound.h"

#include "AudioDevice.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"
#include "MetasoundSource.h"

#include "ArcCuesModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueBehavior_MetaSound)

void FArcCueMetaSoundParameter::Apply(UAudioComponent& Component) const
{
	if (Name.IsNone())
	{
		return;
	}

	switch (Type)
	{
	case EArcCueMetaSoundParameterType::Float:
		Component.SetFloatParameter(Name, FloatValue);
		break;
	case EArcCueMetaSoundParameterType::Integer:
		Component.SetIntParameter(Name, IntegerValue);
		break;
	case EArcCueMetaSoundParameterType::Boolean:
		Component.SetBoolParameter(Name, BooleanValue);
		break;
	case EArcCueMetaSoundParameterType::String:
		Component.SetStringParameter(Name, StringValue);
		break;
	case EArcCueMetaSoundParameterType::Object:
		Component.SetObjectParameter(Name, ObjectValue.Get());
		break;
	}
}

bool FArcCueMetaSoundReaction::Matches(const FArcCueEvent& CueEvent) const
{
	return Event == CueEvent.Type && (PayloadQuery.IsEmpty() || PayloadQuery.Matches(CueEvent.PayloadTags));
}

namespace ArcCueBehaviorMetaSound
{
	struct FResolvedAttachment
	{
		AActor* Actor = nullptr;
		USceneComponent* Component = nullptr;
	};

	static FResolvedAttachment ResolveAttachment(const FArcCueContext& Context, FName ComponentTag, FName SocketName)
	{
		FResolvedAttachment Result;
		FMassEntityManager* EntityManager = Context.EntityManager;
		if (EntityManager == nullptr || !Context.Parameters.TargetEntity.IsValid()
			|| !EntityManager->IsEntityActive(Context.Parameters.TargetEntity))
		{
			UE_LOGF(LogArcCues, Warning, "MetaSound component placement requires an active target entity.");
			return Result;
		}

		FMassActorFragment* ActorFragment = EntityManager->GetFragmentDataPtr<FMassActorFragment>(Context.Parameters.TargetEntity);
		Result.Actor = ActorFragment != nullptr ? ActorFragment->GetMutable() : nullptr;
		if (!IsValid(Result.Actor))
		{
			UE_LOGF(LogArcCues, Warning, "MetaSound target entity %ls has no active actor.",
				*Context.Parameters.TargetEntity.DebugGetDescription());
			Result.Actor = nullptr;
			return Result;
		}

		if (ComponentTag.IsNone())
		{
			Result.Component = Result.Actor->GetRootComponent();
		}
		else
		{
			const TArray<UActorComponent*> Components = Result.Actor->GetComponentsByTag(USceneComponent::StaticClass(), ComponentTag);
			if (Components.Num() != 1)
			{
				UE_LOGF(LogArcCues, Warning, "MetaSound actor '%ls' has %d scene components matching tag '%ls'; exactly one is required.",
					*GetNameSafe(Result.Actor), Components.Num(), *ComponentTag.ToString());
				Result.Actor = nullptr;
				return Result;
			}
			Result.Component = Cast<USceneComponent>(Components[0]);
		}

		if (!IsValid(Result.Component))
		{
			UE_LOGF(LogArcCues, Warning, "MetaSound actor '%ls' has no scene component matching tag '%ls'.",
				*GetNameSafe(Result.Actor), *ComponentTag.ToString());
			Result.Actor = nullptr;
			Result.Component = nullptr;
			return Result;
		}

		if (!SocketName.IsNone() && !Result.Component->DoesSocketExist(SocketName))
		{
			UE_LOGF(LogArcCues, Warning, "MetaSound socket '%ls' does not exist on scene component '%ls'.",
				*SocketName.ToString(), *GetNameSafe(Result.Component));
			Result.Actor = nullptr;
			Result.Component = nullptr;
		}

		return Result;
	}

	static UAudioComponent* CreateComponent(
		const FArcCueBehavior_MetaSound& Behavior,
		const FArcCueContext& Context,
		bool bAutoDestroy)
	{
		if (Context.World == nullptr || !IsValid(Behavior.MetaSound))
		{
			return nullptr;
		}

		FTransform WorldTransform = Behavior.LocalOffset * Context.Parameters.Location;
		FResolvedAttachment Attachment;
		if (Behavior.Placement == EArcCueMetaSoundPlacement::TargetComponent)
		{
			Attachment = ResolveAttachment(Context, Behavior.ComponentTag, Behavior.SocketName);
			if (Attachment.Component == nullptr)
			{
				return nullptr;
			}
			const FTransform ParentTransform = Behavior.SocketName.IsNone()
				? Attachment.Component->GetComponentTransform()
				: Attachment.Component->GetSocketTransform(Behavior.SocketName);
			WorldTransform = Behavior.LocalOffset * ParentTransform;
		}

		FAudioDevice::FCreateComponentParams CreateParams(Context.World, Attachment.Actor);
		CreateParams.SetLocation(WorldTransform.GetLocation());
		CreateParams.bAutoDestroy = bAutoDestroy;
		CreateParams.bPlay = false;
		CreateParams.bStopWhenOwnerDestroyed = Attachment.Actor != nullptr;

		UAudioComponent* Component = FAudioDevice::CreateComponent(Behavior.MetaSound, CreateParams);
		if (Component == nullptr)
		{
			return nullptr;
		}

		Component->bAutoActivate = false;
		Component->bAutoDestroy = bAutoDestroy;
		Component->bAllowSpatialization = CreateParams.ShouldUseAttenuation();
		if (Attachment.Component != nullptr)
		{
			if (!Component->AttachToComponent(
				Attachment.Component,
				FAttachmentTransformRules::KeepRelativeTransform,
				Behavior.SocketName))
			{
				UE_LOGF(LogArcCues, Warning, "MetaSound failed to attach to component '%ls' at socket '%ls'.",
					*GetNameSafe(Attachment.Component), *Behavior.SocketName.ToString());
				Component->DestroyComponent();
				return nullptr;
			}
			Component->SetRelativeTransform(Behavior.LocalOffset);
		}
		else
		{
			Component->SetWorldTransform(WorldTransform);
		}

		return Component;
	}

	static void ApplyParameters(const TArray<FArcCueMetaSoundParameter>& Parameters, UAudioComponent& Component)
	{
		for (const FArcCueMetaSoundParameter& Parameter : Parameters)
		{
			Parameter.Apply(Component);
		}
	}

	static void ApplyTriggers(const TArray<FName>& Triggers, UAudioComponent& Component)
	{
		for (const FName Trigger : Triggers)
		{
			if (!Trigger.IsNone())
			{
				Component.SetTriggerParameter(Trigger);
			}
		}
	}

	static EArcCueMetaSoundAction ResolveAction(
		const FArcCueBehavior_MetaSound& Behavior,
		const FArcCueEvent& Event,
		const FArcCueMetaSoundReaction*& OutActionReaction)
	{
		OutActionReaction = nullptr;
		EArcCueMetaSoundAction Action = EArcCueMetaSoundAction::None;
		switch (Event.Type)
		{
		case EArcCueEvent::Start:
			Action = EArcCueMetaSoundAction::Play;
			break;
		case EArcCueEvent::Stop:
		case EArcCueEvent::End:
			Action = EArcCueMetaSoundAction::FadeOut;
			break;
		case EArcCueEvent::Update:
			break;
		}

		for (const FArcCueMetaSoundReaction& Reaction : Behavior.Reactions)
		{
			if (Reaction.Matches(Event) && Reaction.Action != EArcCueMetaSoundAction::None)
			{
				Action = Reaction.Action;
				OutActionReaction = &Reaction;
			}
		}
		return Action;
	}

	static void ApplyEventParameters(
		const FArcCueBehavior_MetaSound& Behavior,
		const FArcCueEvent& Event,
		UAudioComponent& Component)
	{
		for (const FArcCueMetaSoundReaction& Reaction : Behavior.Reactions)
		{
			if (Reaction.Matches(Event))
			{
				ApplyParameters(Reaction.Parameters, Component);
			}
		}
	}

	static void ApplyEventTriggers(
		const FArcCueBehavior_MetaSound& Behavior,
		const FArcCueEvent& Event,
		UAudioComponent& Component)
	{
		for (const FArcCueMetaSoundReaction& Reaction : Behavior.Reactions)
		{
			if (Reaction.Matches(Event))
			{
				ApplyTriggers(Reaction.Triggers, Component);
			}
		}
	}
}

void FArcCueBehavior_MetaSound::OnExecute(const FArcCueContext& Context) const
{
	UAudioComponent* Component = ArcCueBehaviorMetaSound::CreateComponent(*this, Context, true);
	if (Component == nullptr)
	{
		return;
	}

	ArcCueBehaviorMetaSound::ApplyParameters(InitialParameters, *Component);
	Component->Play();
	ArcCueBehaviorMetaSound::ApplyTriggers(InitialTriggers, *Component);
}

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_MetaSound::OnApplication(const FArcCueContext& Context) const
{
	if (!bStartOnApplication)
	{
		return {};
	}

	UAudioComponent* Component = ArcCueBehaviorMetaSound::CreateComponent(*this, Context, false);
	if (Component == nullptr)
	{
		return {};
	}

	ArcCueBehaviorMetaSound::ApplyParameters(InitialParameters, *Component);
	Component->Play();
	ArcCueBehaviorMetaSound::ApplyTriggers(InitialTriggers, *Component);

	FArcCueBehaviorState_MetaSound State;
	State.Component = Component;
	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_MetaSound>(State);
}

void FArcCueBehavior_MetaSound::OnEvent(
	const FArcCueContext& Context,
	const FArcCueEvent& Event,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	const FArcCueMetaSoundReaction* ActionReaction = nullptr;
	const EArcCueMetaSoundAction Action = ArcCueBehaviorMetaSound::ResolveAction(*this, Event, ActionReaction);
	FArcCueBehaviorState_MetaSound* TypedState = State.GetMutablePtr<FArcCueBehaviorState_MetaSound>();
	UAudioComponent* Component = TypedState != nullptr ? TypedState->Component.Get() : nullptr;
	bool bCreated = false;
	if (!IsValid(Component))
	{
		if (Action != EArcCueMetaSoundAction::Play)
		{
			return;
		}

		Component = ArcCueBehaviorMetaSound::CreateComponent(*this, Context, false);
		if (Component == nullptr)
		{
			return;
		}

		ArcCueBehaviorMetaSound::ApplyParameters(InitialParameters, *Component);
		FArcCueBehaviorState_MetaSound NewState;
		NewState.Component = Component;
		State = TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_MetaSound>(NewState);
		bCreated = true;
	}

	ArcCueBehaviorMetaSound::ApplyEventParameters(*this, Event, *Component);
	if (Action == EArcCueMetaSoundAction::Play)
	{
		Component->Play();
		if (bCreated)
		{
			ArcCueBehaviorMetaSound::ApplyTriggers(InitialTriggers, *Component);
		}
		ArcCueBehaviorMetaSound::ApplyEventTriggers(*this, Event, *Component);
		return;
	}

	ArcCueBehaviorMetaSound::ApplyEventTriggers(*this, Event, *Component);
	if (Action == EArcCueMetaSoundAction::Stop)
	{
		Component->Stop();
	}
	else if (Action == EArcCueMetaSoundAction::FadeOut)
	{
		const float Duration = ActionReaction != nullptr ? ActionReaction->FadeOutDuration : DefaultFadeOutDuration;
		const float Volume = ActionReaction != nullptr ? ActionReaction->FadeVolumeLevel : DefaultFadeVolumeLevel;
		const EAudioFaderCurve Curve = ActionReaction != nullptr ? ActionReaction->FadeCurve : DefaultFadeCurve;
		if (Duration > 0.0f && Component->IsPlaying())
		{
			Component->FadeOut(Duration, Volume, Curve);
		}
		else
		{
			Component->Stop();
		}
	}
}

void FArcCueBehavior_MetaSound::OnRemoved(const FArcCueContext&, FArcCueBehaviorState& State) const
{
	FArcCueBehaviorState_MetaSound* TypedState = static_cast<FArcCueBehaviorState_MetaSound*>(&State);
	if (TypedState == nullptr || !IsValid(TypedState->Component))
	{
		return;
	}

	TypedState->Component->Stop();
	TypedState->Component->DestroyComponent();
	TypedState->Component = nullptr;
}
