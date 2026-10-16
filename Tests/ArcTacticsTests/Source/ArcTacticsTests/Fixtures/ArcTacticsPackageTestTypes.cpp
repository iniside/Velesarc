#include "Fixtures/ArcTacticsPackageTestTypes.h"

#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "StructUtils/StructView.h"

namespace ArcTacticsPackageTestTypesPrivate
{
	void RunOps(
		UArcTacticsInstanceSubsystem* Subsystem,
		const FMassEntityHandle Entity,
		const FArcTacticsInstanceHandle Handle,
		const TConstArrayView<TObjectPtr<const UArcTacticsConfig>> Configs,
		const TConstArrayView<bool> Adds,
		TArray<bool>& OutResults,
		int32& OutPackages,
		int32* OutPending)
	{
		if (!Subsystem)
		{
			return;
		}
		for (int32 OpIndex = 0; OpIndex < Configs.Num() && OpIndex < Adds.Num(); ++OpIndex)
		{
			const UArcTacticsConfig* Config = Configs[OpIndex];
			if (!Config)
			{
				continue;
			}
			OutResults.Add(Adds[OpIndex]
				? Subsystem->AddConfig(Entity, *Config)
				: Subsystem->RemoveConfig(Entity, *Config));
		}
		const FArcTacticsInstanceSlot* Slot = Subsystem->Resolve(Handle);
		OutPackages = Slot ? Slot->Packages.Num() : INDEX_NONE;
		if (OutPending)
		{
			*OutPending = Slot ? Slot->PendingPackageChanges.Num() : INDEX_NONE;
		}
	}
}

EArcTacticsTaskStatus FArcTacticsPackageTask::Enter(FArcTacticsTaskContext&, FStructView InstanceData) const
{
	FArcTacticsPackageTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPackageTaskInstanceData>();
	if (!Data)
	{
		return EArcTacticsTaskStatus::Failed;
	}
	if (Data->Probe)
	{
		++Data->Probe->EnterCount;
	}
	if (Data->Mode == 1)
	{
		return EArcTacticsTaskStatus::Running;
	}
	if (Data->Mode == 2 || (Data->Probe && Data->Probe->bFailEnter))
	{
		return EArcTacticsTaskStatus::Failed;
	}
	return EArcTacticsTaskStatus::Succeeded;
}

EArcTacticsTaskStatus FArcTacticsPackageTask::Tick(FArcTacticsTaskContext&, FStructView InstanceData) const
{
	FArcTacticsPackageTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPackageTaskInstanceData>();
	if (!Data)
	{
		return EArcTacticsTaskStatus::Failed;
	}
	if (Data->Probe)
	{
		++Data->Probe->TickCount;
	}
	return EArcTacticsTaskStatus::Succeeded;
}

void FArcTacticsPackageTask::Cancel(FArcTacticsTaskContext& Context, FStructView InstanceData) const
{
	FArcTacticsPackageTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPackageTaskInstanceData>();
	UArcTacticsPackageProbe* Probe = Data ? Data->Probe.Get() : nullptr;
	if (!Probe)
	{
		return;
	}
	++Probe->CancelCount;
	const TArray<TObjectPtr<const UArcTacticsConfig>> Configs = MoveTemp(Probe->CancelConfigs);
	const TArray<bool> Adds = MoveTemp(Probe->CancelAdds);
	Probe->CancelConfigs.Reset();
	Probe->CancelAdds.Reset();
	ArcTacticsPackageTestTypesPrivate::RunOps(
		Context.InstanceSubsystem.Get(), Context.Entity, Context.InstanceHandle,
		Configs, Adds, Probe->CancelResults, Probe->PackagesDuringCancel, nullptr);
}

void FArcTacticsPackageTask::OnAsyncQueryCompleted(
	FArcTacticsTaskContext& Context,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&) const
{
	FArcTacticsPackageTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPackageTaskInstanceData>();
	UArcTacticsPackageProbe* Probe = Data ? Data->Probe.Get() : nullptr;
	if (!Probe)
	{
		return;
	}
	++Probe->CallbackCount;
	const TArray<TObjectPtr<const UArcTacticsConfig>> Configs = MoveTemp(Probe->CallbackConfigs);
	const TArray<bool> Adds = MoveTemp(Probe->CallbackAdds);
	Probe->CallbackConfigs.Reset();
	Probe->CallbackAdds.Reset();
	ArcTacticsPackageTestTypesPrivate::RunOps(
		Context.InstanceSubsystem.Get(), Context.Entity, Context.InstanceHandle,
		Configs, Adds, Probe->CallbackResults, Probe->PackagesDuringCallback, nullptr);
}

EArcTacticsActionStatus FArcTacticsPackageScriptAction::Enter(
	FArcTacticsActionContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsPackageScriptActionInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsPackageScriptActionInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	if (Data->Probe)
	{
		++Data->Probe->EnterCount;
	}
	if (Data->bAcquireHold)
	{
		if (!Context.PathContext || !Context.HoldState)
		{
			return EArcTacticsActionStatus::Failed;
		}
		FArcTacticsMovementRuntimeData RuntimeData;
		if (!ArcTacticsMovement::AcquireHold(*Context.PathContext, RuntimeData, *Context.HoldState))
		{
			return EArcTacticsActionStatus::Failed;
		}
	}
	return EArcTacticsActionStatus::Running;
}

EArcTacticsActionStatus FArcTacticsPackageScriptAction::Tick(
	FArcTacticsActionContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsPackageScriptActionInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsPackageScriptActionInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	if (UArcTacticsPackageProbe* Probe = Data->Probe)
	{
		++Probe->TickCount;
		ArcTacticsPackageTestTypesPrivate::RunOps(
			Context.RequestToken.InstanceSubsystem.Get(), Context.Entity, Context.RequestToken.InstanceHandle,
			Probe->TickConfigs, Probe->TickAdds, Probe->TickResults,
			Probe->PackagesDuringTick, &Probe->PendingDuringTick);
	}
	return EArcTacticsActionStatus::Running;
}

void FArcTacticsPackageScriptAction::Cancel(
	FArcTacticsActionContext& Context,
	FStructView InstanceData,
	const EArcTacticsActionCancelMode CancelMode) const
{
	FArcTacticsPackageScriptActionInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsPackageScriptActionInstanceData>();
	UArcTacticsPackageProbe* Probe = Data ? Data->Probe.Get() : nullptr;
	if (!Probe)
	{
		return;
	}
	++Probe->CancelCount;
	Probe->LastCancelMode = CancelMode;
	const TArray<TObjectPtr<const UArcTacticsConfig>> Configs = MoveTemp(Probe->CancelConfigs);
	const TArray<bool> Adds = MoveTemp(Probe->CancelAdds);
	Probe->CancelConfigs.Reset();
	Probe->CancelAdds.Reset();
	ArcTacticsPackageTestTypesPrivate::RunOps(
		Context.RequestToken.InstanceSubsystem.Get(), Context.Entity, Context.RequestToken.InstanceHandle,
		Configs, Adds, Probe->CancelResults, Probe->PackagesDuringCancel, nullptr);
}

EArcTacticsActionStatus FArcTacticsPackageStep::Tick(FArcTacticsStepContext&, FStructView InstanceData) const
{
	FArcTacticsPackageStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsPackageStepInstanceData>();
	if (Data)
	{
		++Data->TickCount;
	}
	return EArcTacticsActionStatus::Running;
}
