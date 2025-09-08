#include "ArcIsMassActorAvailableCondition.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcIsMassActorAvailableCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcIsMassActorAvailableCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

bool FArcIsMassActorAvailableCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassStateTreeContext.GetExternalDataPtr(MassActorHandle);
	if (!MassActor)
	{
		return false;
	}

	if (!MassActor->Get())
	{
		return false;
	}

	return true;
}