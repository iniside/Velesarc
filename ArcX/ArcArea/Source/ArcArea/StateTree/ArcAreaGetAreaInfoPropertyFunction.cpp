// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaGetAreaInfoPropertyFunction.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaGetAreaInfoPropertyFunction)

void FArcAreaGetAreaInfoPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData = {};

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	const FMassEntityView EntityView(EntityManager, MassCtx.GetEntity());
	const FArcAreaFragment* AreaFragment = EntityView.GetFragmentDataPtr<FArcAreaFragment>();
	if (!AreaFragment || !AreaFragment->AreaHandle.IsValid())
	{
		return;
	}

	const UWorld* World = MassCtx.GetWorld();
	const UArcAreaSubsystem* Subsystem = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	const FArcAreaData* AreaData = Subsystem->GetAreaData(AreaFragment->AreaHandle);
	if (!AreaData)
	{
		return;
	}

	InstanceData.AreaHandle = AreaData->Handle;
	InstanceData.AreaTags = AreaData->AreaTags;
	InstanceData.Location = AreaData->Location;
	InstanceData.SmartObjectHandle = AreaData->SmartObjectHandle;
	InstanceData.DisplayName = AreaData->DisplayName;
}
