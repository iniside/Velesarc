// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlOpenDocumentTask.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "StateTreeExecutionContext.h"

#if ARC_WITH_RMLUI
#include "Host/ArcRmlViewportSubsystem.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRmlOpenDocumentTask)

DEFINE_LOG_CATEGORY_STATIC(LogArcRmlOpenDocumentTask, Log, All);

namespace ArcRmlOpenDocumentTask
{
#if ARC_WITH_RMLUI
	static ULocalPlayer* ResolveLocalPlayer(const FMassEntityManager& EntityManager, FMassEntityHandle ContextEntity)
	{
		const FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(ContextEntity);
		if (ActorFragment == nullptr)
		{
			return nullptr;
		}

		const AActor* Actor = ActorFragment->Get();
		if (Actor == nullptr)
		{
			return nullptr;
		}

		APlayerController* PC = nullptr;
		if (const APlayerState* PS = Cast<APlayerState>(Actor))
		{
			PC = PS->GetPlayerController();
		}
		else if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			const APlayerState* PawnPS = Pawn->GetPlayerState();
			PC = PawnPS ? PawnPS->GetPlayerController() : nullptr;
		}

		return PC ? Cast<ULocalPlayer>(PC->Player) : nullptr;
	}

	static UArcRmlViewportSubsystem* GetViewportSubsystem(const FArcRmlOpenDocumentTaskInstanceData& InstanceData)
	{
		const ULocalPlayer* LocalPlayer = InstanceData.LocalPlayer.Get();
		return LocalPlayer ? LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>() : nullptr;
	}
#endif
} // namespace ArcRmlOpenDocumentTask

FArcRmlOpenDocumentTask::FArcRmlOpenDocumentTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FArcRmlOpenDocumentTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.LocalPlayer.Reset();
	InstanceData.bTracking = false;
	InstanceData.bOwnsDocument = false;

	if (!InstanceData.Document.IsSet())
	{
		UE_LOGF(LogArcRmlOpenDocumentTask, Error, "EnterState: Document is not set.");
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.ContextEntity.IsValid())
	{
		UE_LOGF(LogArcRmlOpenDocumentTask, Warning, "EnterState: ContextEntity is not valid.");
		return EStateTreeRunStatus::Failed;
	}

#if ARC_WITH_RMLUI
	UWorld* World = Context.GetWorld();
	const UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (EntitySubsystem == nullptr)
	{
		UE_LOGF(LogArcRmlOpenDocumentTask, Warning, "EnterState: No UMassEntitySubsystem found.");
		return EStateTreeRunStatus::Failed;
	}

	ULocalPlayer* LocalPlayer = ArcRmlOpenDocumentTask::ResolveLocalPlayer(EntitySubsystem->GetEntityManager(), InstanceData.ContextEntity);
	if (LocalPlayer == nullptr)
	{
		return EStateTreeRunStatus::Running;
	}

	UArcRmlViewportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>();
	if (Subsystem == nullptr)
	{
		UE_LOGF(LogArcRmlOpenDocumentTask, Warning, "EnterState: the local player has no RML viewport subsystem for '%ls'.", *InstanceData.Document.Path);
		return EStateTreeRunStatus::Failed;
	}

	if (!Subsystem->IsDocumentOpen(InstanceData.Document))
	{
		if (!Subsystem->OpenDocument(InstanceData.Document))
		{
			UE_LOGF(LogArcRmlOpenDocumentTask, Warning, "EnterState: failed to open '%ls'.", *InstanceData.Document.Path);
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.bOwnsDocument = true;
	}

	InstanceData.LocalPlayer = LocalPlayer;
	InstanceData.bTracking = true;
#endif

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcRmlOpenDocumentTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.bTracking)
	{
		return EStateTreeRunStatus::Running;
	}

#if ARC_WITH_RMLUI
	const UArcRmlViewportSubsystem* Subsystem = ArcRmlOpenDocumentTask::GetViewportSubsystem(InstanceData);
	if (Subsystem == nullptr || !Subsystem->IsDocumentOpen(InstanceData.Document))
	{
		return EStateTreeRunStatus::Succeeded;
	}
#endif

	return EStateTreeRunStatus::Running;
}

void FArcRmlOpenDocumentTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

#if ARC_WITH_RMLUI
	if (InstanceData.bOwnsDocument)
	{
		UArcRmlViewportSubsystem* Subsystem = ArcRmlOpenDocumentTask::GetViewportSubsystem(InstanceData);
		if (Subsystem && Subsystem->IsDocumentOpen(InstanceData.Document))
		{
			Subsystem->CloseDocument(InstanceData.Document);
		}
	}
#endif

	InstanceData.LocalPlayer.Reset();
	InstanceData.bTracking = false;
	InstanceData.bOwnsDocument = false;
}
