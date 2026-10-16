// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcEffectTask_Mover.h"

#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "DefaultMovementSet/LayeredMoves/MontageStateProvider.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "GameFramework/Actor.h"
#include "InstantMovementEffect.h"
#include "LayeredMove.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mover/ArcCharacterMoverComponent.h"
#include "Mover/ArcChaosCharacterMoverComponent.h"
#include "Mover/ArcChaosLayeredMove_Knockback.h"
#include "Mover/ArcChaosMovementModifiers.h"
#include "Mover/ArcLayeredMove_Knockback.h"
#include "Mover/ArcLayeredMove_Root.h"
#include "Mover/ArcMovementModifiers.h"
#include "Mover/ArcMoverTypes.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcEffectTaskMover, Log, All);

namespace ArcEffectTaskMover
{
	const UArcEffectDefinition* GetDefinition(const FArcActiveEffect& OwningEffect)
	{
		const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
		return Spec ? Spec->Definition.Get() : nullptr;
	}

	bool HasPersistentLifetime(const FArcActiveEffect& OwningEffect)
	{
		const UArcEffectDefinition* Definition = GetDefinition(OwningEffect);
		return Definition && Definition->StackingPolicy.DurationType != EArcEffectDuration::Instant;
	}

	UMoverComponent* ResolveAuthorizedMover(FArcActiveEffect& OwningEffect, const TCHAR* TaskName)
	{
		UWorld* World = OwningEffect.OwnerWorld;
		if (!World || !OwningEffect.OwnerEntity.IsValid())
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "%ls requires a valid effect world and owner entity.", TaskName);
			return nullptr;
		}

		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!EntitySubsystem)
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "%ls could not resolve the Mass entity subsystem.", TaskName);
			return nullptr;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		if (!EntityManager.IsEntityValid(OwningEffect.OwnerEntity))
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "%ls received an invalid owner entity.", TaskName);
			return nullptr;
		}

		FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(OwningEffect.OwnerEntity);
		AActor* Actor = ActorFragment ? ActorFragment->GetMutable() : nullptr;
		if (!Actor)
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "%ls requires an owner actor through FMassActorFragment.", TaskName);
			return nullptr;
		}

		if (World->GetNetMode() != NM_Standalone && !Actor->HasAuthority())
		{
			return nullptr;
		}

		UMoverComponent* MoverComponent = Actor->FindComponentByClass<UMoverComponent>();
		if (!MoverComponent)
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "%ls requires a Mover component on actor %ls.", TaskName, *GetNameSafe(Actor));
		}
		return MoverComponent;
	}

	bool IsChaosMover(const UMoverComponent& MoverComponent)
	{
		return MoverComponent.IsA<UArcChaosCharacterMoverComponent>();
	}

	bool IsSupportedArcMover(const UMoverComponent& MoverComponent)
	{
		return MoverComponent.IsA<UArcCharacterMoverComponent>() || IsChaosMover(MoverComponent);
	}

	FMovementModifierHandle QueueRootModifier(UMoverComponent& MoverComponent, const FGuid& SourceId)
	{
		if (UArcCharacterMoverComponent* NPMover = Cast<UArcCharacterMoverComponent>(&MoverComponent))
		{
			return NPMover->QueueRootMovementModifier(SourceId);
		}

		if (!IsChaosMover(MoverComponent) || !SourceId.IsValid())
		{
			return FMovementModifierHandle();
		}

		TSharedPtr<FArcMovementModifier_Root> Marker = MakeShared<FArcMovementModifier_Root>();
		Marker->SourceId = SourceId;
		return ArcMover::QueueRoot(MoverComponent, Marker);
	}

	FMovementModifierHandle QueueSpeedScaleModifier(UMoverComponent& MoverComponent, const FGuid& SourceId, const float SpeedPercent)
	{
		if (UArcCharacterMoverComponent* NPMover = Cast<UArcCharacterMoverComponent>(&MoverComponent))
		{
			return NPMover->QueueSpeedScaleMovementModifier(SourceId, SpeedPercent);
		}

		if (UArcChaosCharacterMoverComponent* ChaosMover = Cast<UArcChaosCharacterMoverComponent>(&MoverComponent))
		{
			return ChaosMover->QueueSpeedScaleMovementModifier(SourceId, SpeedPercent);
		}

		return FMovementModifierHandle();
	}

	void LogInvalidPayload(const TCHAR* TaskName)
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "%ls requires a supported native Mover payload.", TaskName);
	}

	bool CanCloneModifier(const FInstancedStruct& Modifier)
	{
		const UScriptStruct* ScriptStruct = Modifier.GetScriptStruct();
		return ScriptStruct && ScriptStruct != FMovementModifierBase::StaticStruct();
	}

	bool CanCloneLayeredMove(const FInstancedStruct& LayeredMove)
	{
		const UScriptStruct* ScriptStruct = LayeredMove.GetScriptStruct();
		return ScriptStruct && ScriptStruct != FLayeredMoveBase::StaticStruct()
			&& ScriptStruct != FLayeredMove_MontageStateProvider::StaticStruct();
	}

	bool CanCloneInstantEffect(const FInstancedStruct& InstantEffect)
	{
		const UScriptStruct* ScriptStruct = InstantEffect.GetScriptStruct();
		return ScriptStruct && ScriptStruct != FInstantMovementEffect::StaticStruct();
	}
}

void FArcEffectTask_PersistentMoverModifier::OnApplication(FArcActiveEffect& OwningEffect)
{
	CancelOwnedModifier();
	SourceId = FGuid::NewGuid();

	if (!ArcEffectTaskMover::HasPersistentLifetime(OwningEffect))
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Persistent Root / Speed Modifier rejects an instant effect lifetime.");
		SourceId.Invalidate();
		return;
	}

	QueueOwnedModifier(OwningEffect);
}

void FArcEffectTask_PersistentMoverModifier::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	CancelOwnedModifier();
	SourceId.Invalidate();
}

void FArcEffectTask_PersistentMoverModifier::OnExecute(FArcActiveEffect& OwningEffect)
{
	const UArcEffectDefinition* Definition = ArcEffectTaskMover::GetDefinition(OwningEffect);
	if (Definition && Definition->StackingPolicy.DurationType == EArcEffectDuration::Instant)
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Persistent Root / Speed Modifier rejects an instant effect lifetime.");
	}
}

void FArcEffectTask_PersistentMoverModifier::OnInhibitionChanged(FArcActiveEffect& OwningEffect, const bool bInhibited)
{
	if (bInhibited)
	{
		CancelOwnedModifier();
	}
	else if (SourceId.IsValid())
	{
		QueueOwnedModifier(OwningEffect);
	}
}

void FArcEffectTask_PersistentMoverModifier::QueueOwnedModifier(FArcActiveEffect& OwningEffect)
{
	CancelOwnedModifier();

	if (!SourceId.IsValid() || !ArcEffectTaskMover::HasPersistentLifetime(OwningEffect))
	{
		return;
	}

	UMoverComponent* ResolvedMover = ArcEffectTaskMover::ResolveAuthorizedMover(OwningEffect, TEXT("Persistent Root / Speed Modifier"));
	if (!ResolvedMover)
	{
		return;
	}

	if (!ArcEffectTaskMover::IsSupportedArcMover(*ResolvedMover))
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Persistent Root / Speed Modifier requires UArcCharacterMoverComponent or UArcChaosCharacterMoverComponent.");
		return;
	}

	FMovementModifierHandle QueuedHandle;
	if (Modifier.GetPtr<FArcMovementModifier_Root>())
	{
		QueuedHandle = ArcEffectTaskMover::QueueRootModifier(*ResolvedMover, SourceId);
	}
	else if (const FArcMovementModifier_SpeedScale* SpeedScale = Modifier.GetPtr<FArcMovementModifier_SpeedScale>())
	{
		if (!FMath::IsFinite(SpeedScale->SpeedPercent) || SpeedScale->SpeedPercent < 0.0f)
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "Persistent Root / Speed Modifier requires a finite, non-negative SpeedPercent.");
			return;
		}
		QueuedHandle = ArcEffectTaskMover::QueueSpeedScaleModifier(*ResolvedMover, SourceId, SpeedScale->SpeedPercent);
	}
	else
	{
		ArcEffectTaskMover::LogInvalidPayload(TEXT("Persistent Root / Speed Modifier"));
		return;
	}

	if (!QueuedHandle.IsValid())
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Persistent Root / Speed Modifier failed to queue its native modifier.");
		return;
	}

	MoverComponent = ResolvedMover;
	ModifierHandle = QueuedHandle;
}

void FArcEffectTask_PersistentMoverModifier::CancelOwnedModifier()
{
	if (UMoverComponent* OwnedMover = MoverComponent.Get())
	{
		if (ModifierHandle.IsValid())
		{
			OwnedMover->CancelModifier(ModifierHandle);
		}
	}

	MoverComponent.Reset();
	ModifierHandle.Invalidate();
}

void FArcEffectTask_QueueMoverModifier::OnExecute(FArcActiveEffect& OwningEffect)
{
	UMoverComponent* MoverComponent = ArcEffectTaskMover::ResolveAuthorizedMover(OwningEffect, TEXT("Queue Mover Modifier"));
	const FMovementModifierBase* AuthoredModifier = Modifier.GetPtr<FMovementModifierBase>();
	if (!MoverComponent || !AuthoredModifier)
	{
		if (!AuthoredModifier)
		{
			ArcEffectTaskMover::LogInvalidPayload(TEXT("Queue Mover Modifier"));
		}
		return;
	}

	if (!ArcEffectTaskMover::CanCloneModifier(Modifier))
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Modifier rejects a payload whose native Clone implementation is unavailable.");
		return;
	}

	TSharedPtr<FMovementModifierBase> ClonedModifier(AuthoredModifier->Clone());
	if (!ClonedModifier.IsValid() || !FMath::IsFinite(ClonedModifier->DurationMs) || ClonedModifier->DurationMs <= 0.0f)
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Modifier requires a native modifier with a finite positive DurationMs.");
		return;
	}

	const UScriptStruct* ModifierStruct = ClonedModifier->GetScriptStruct();
	if (ModifierStruct == FArcMovementModifier_Root::StaticStruct())
	{
		if (!ArcMover::QueueRoot(*MoverComponent, StaticCastSharedPtr<FArcMovementModifier_Root>(ClonedModifier)).IsValid())
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Modifier failed to queue its native modifier.");
		}
		return;
	}

	if (ArcEffectTaskMover::IsChaosMover(*MoverComponent))
	{
		if (ModifierStruct == FArcMovementModifier_MaxVelocity::StaticStruct())
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Modifier rejects FArcMovementModifier_MaxVelocity on a Chaos mover; use a speed scale modifier.");
			return;
		}

		if (ModifierStruct == FArcMovementModifier_SpeedScale::StaticStruct())
		{
			const FArcMovementModifier_SpeedScale* SpeedScale = static_cast<const FArcMovementModifier_SpeedScale*>(ClonedModifier.Get());
			TSharedPtr<FArcChaosMovementModifier_SpeedScale> ChaosSpeedScale = MakeShared<FArcChaosMovementModifier_SpeedScale>();
			ChaosSpeedScale->DurationMs = SpeedScale->DurationMs;
			ChaosSpeedScale->SourceId = SpeedScale->SourceId;
			ChaosSpeedScale->SpeedPercent = SpeedScale->SpeedPercent;
			ClonedModifier = ChaosSpeedScale;
		}
	}

	const FMovementModifierHandle QueuedHandle = MoverComponent->QueueModifier(FQueueModifierParams(ClonedModifier));
	if (!QueuedHandle.IsValid())
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Modifier failed to queue its native modifier.");
	}
}

void FArcEffectTask_QueueMoverLayeredMove::OnExecute(FArcActiveEffect& OwningEffect)
{
	UMoverComponent* MoverComponent = ArcEffectTaskMover::ResolveAuthorizedMover(OwningEffect, TEXT("Queue Mover Layered Move"));
	const FLayeredMoveBase* AuthoredMove = LayeredMove.GetPtr<FLayeredMoveBase>();
	if (!MoverComponent || !AuthoredMove)
	{
		if (!AuthoredMove)
		{
			ArcEffectTaskMover::LogInvalidPayload(TEXT("Queue Mover Layered Move"));
		}
		return;
	}

	if (!ArcEffectTaskMover::CanCloneLayeredMove(LayeredMove))
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Layered Move rejects a payload whose native Clone implementation is unavailable.");
		return;
	}

	TSharedPtr<FLayeredMoveBase> ClonedMove(AuthoredMove->Clone());
	if (!ClonedMove.IsValid())
	{
		ArcEffectTaskMover::LogInvalidPayload(TEXT("Queue Mover Layered Move"));
		return;
	}

	const bool bChaosMover = ArcEffectTaskMover::IsChaosMover(*MoverComponent);
	if (ClonedMove->GetScriptStruct()->IsChildOf(FArcLayeredMove_Knockback::StaticStruct()))
	{
		const FArcLayeredMove_Knockback* Knockback = static_cast<const FArcLayeredMove_Knockback*>(ClonedMove.Get());
		const FVector UpDirection = MoverComponent->GetUpDirection();
		if (!Knockback->HasValidParameters(UpDirection))
		{
			UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Layered Move received invalid knockback parameters.");
			return;
		}

		if (bChaosMover)
		{
			TSharedPtr<FArcChaosLayeredMove_Knockback> ChaosKnockback = MakeShared<FArcChaosLayeredMove_Knockback>();
			ChaosKnockback->Priority = Knockback->Priority;
			ChaosKnockback->DurationMs = Knockback->DurationMs;
			ChaosKnockback->Direction = Knockback->Direction;
			ChaosKnockback->DistanceCm = Knockback->DistanceCm;
			ChaosKnockback->DurationSeconds = Knockback->DurationSeconds;
			ChaosKnockback->UpDirection = UpDirection;
			ClonedMove = ChaosKnockback;
		}
	}
	else if (bChaosMover && !ClonedMove->SupportsAsync())
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Layered Move rejects %ls on a Chaos mover because it does not support async simulation.",
			*GetNameSafe(ClonedMove->GetScriptStruct()));
		return;
	}

	if (!FMath::IsFinite(ClonedMove->DurationMs) || ClonedMove->DurationMs <= 0.0f)
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Layered Move requires a native move with a finite positive DurationMs.");
		return;
	}

	MoverComponent->QueueLayeredMove(FQueueLayeredMoveParams(ClonedMove));
}

void FArcEffectTask_QueueMoverInstantEffect::OnExecute(FArcActiveEffect& OwningEffect)
{
	UMoverComponent* MoverComponent = ArcEffectTaskMover::ResolveAuthorizedMover(OwningEffect, TEXT("Queue Mover Instant Effect"));
	const FInstantMovementEffect* AuthoredEffect = InstantEffect.GetPtr<FInstantMovementEffect>();
	if (!MoverComponent || !AuthoredEffect)
	{
		if (!AuthoredEffect)
		{
			ArcEffectTaskMover::LogInvalidPayload(TEXT("Queue Mover Instant Effect"));
		}
		return;
	}

	if (!ArcEffectTaskMover::CanCloneInstantEffect(InstantEffect))
	{
		UE_LOGF(LogArcEffectTaskMover, Warning, "Queue Mover Instant Effect rejects a payload whose native Clone implementation is unavailable.");
		return;
	}

	TSharedPtr<FInstantMovementEffect> ClonedEffect(AuthoredEffect->Clone());
	if (!ClonedEffect.IsValid())
	{
		ArcEffectTaskMover::LogInvalidPayload(TEXT("Queue Mover Instant Effect"));
		return;
	}

	MoverComponent->QueueInstantMovementEffect(FQueueInstantMovementEffectParams(ClonedEffect));
}
