// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcTransportSignalIdleTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcEconomyTypes.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcTransportSignalIdleTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransporterFragmentHandle);
	Linker.LinkExternalData(NPCFragmentHandle);
	return true;
}

EStateTreeRunStatus FArcTransportSignalIdleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	FArcTransporterFragment& Transporter = Context.GetExternalData(TransporterFragmentHandle);
	Transporter.TaskState = EArcTransporterTaskState::Idle;
	Transporter.SourceBuildingHandle = FMassEntityHandle();
	Transporter.DestinationBuildingHandle = FMassEntityHandle();
	Transporter.TargetBuildingHandle = FMassEntityHandle();
	Transporter.TargetItemDefinition = nullptr;
	Transporter.TargetQuantity = 0;

	const FArcEconomyNPCFragment& NPC = Context.GetExternalData(NPCFragmentHandle);
	if (NPC.SettlementHandle.IsValid())
	{
		UMassSignalSubsystem* SignalSubsystem = MassContext.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntity(ArcEconomy::Signals::TransporterIdle, NPC.SettlementHandle);
		}
	}

	return EStateTreeRunStatus::Succeeded;
}
