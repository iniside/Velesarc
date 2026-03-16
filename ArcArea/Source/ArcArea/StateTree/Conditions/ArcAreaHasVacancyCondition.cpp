// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaHasVacancyCondition.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaHasVacancyCondition)

bool FArcAreaHasVacancyCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);

	const FMassEntityView EntityView(MassCtx.GetEntityManager(), MassCtx.GetEntity());
	const FArcAreaFragment* AreaFragment = EntityView.GetFragmentDataPtr<FArcAreaFragment>();
	if (!AreaFragment || !AreaFragment->AreaHandle.IsValid())
	{
		return bInvert;
	}

	const UArcAreaSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return bInvert;
	}

	const bool bHasVacancy = Subsystem->HasVacancy(AreaFragment->AreaHandle);
	return bInvert ? !bHasVacancy : bHasVacancy;
}
