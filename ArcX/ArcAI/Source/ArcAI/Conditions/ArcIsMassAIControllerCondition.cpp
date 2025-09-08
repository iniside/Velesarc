#include "ArcIsMassAIControllerCondition.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcIsMassAIControllerCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcIsMassAIControllerCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

bool FArcIsMassAIControllerCondition::TestCondition(FStateTreeExecutionContext& Context) const
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

	const APawn* P = Cast<APawn>(MassActor->Get());
	if (!P)
	{
		return false;
	}

	if (!P->GetController())
	{
		return false;
	}

	return true;
}