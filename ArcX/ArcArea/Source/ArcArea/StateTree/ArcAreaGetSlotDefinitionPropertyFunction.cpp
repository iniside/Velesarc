// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaGetSlotDefinitionPropertyFunction.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaGetSlotDefinitionPropertyFunction)

void FArcAreaGetSlotDefinitionPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RoleTag = FGameplayTag();
	InstanceData.bAutoPostVacancy = false;
	InstanceData.VacancyRelevance = 0.0f;
	InstanceData.SmartObjectSlotIndex = 0;

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
	if (!AreaData || !AreaData->SlotDefinitions.IsValidIndex(InstanceData.SlotIndex))
	{
		return;
	}

	const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[InstanceData.SlotIndex];
	InstanceData.RoleTag = SlotDef.RoleTag;
	InstanceData.bAutoPostVacancy = SlotDef.VacancyConfig.bAutoPostVacancy;
	InstanceData.VacancyRelevance = SlotDef.VacancyConfig.VacancyRelevance;
	InstanceData.SmartObjectSlotIndex = SlotDef.SmartObjectSlotIndex;
}
