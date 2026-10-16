#include "ArcMassActorGameplayAttributeCondition.h"

#include "MassStateTreeDependency.h"
#include "StateTreeLinker.h"

bool FArcMassActorGameplayAttributeCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);

	return true;
}

void FArcMassActorGameplayAttributeCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

bool FArcMassActorGameplayAttributeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	// GAS retired (Step 6): neutralized, pending Mass-attribute reimpl
	return true;
}

#if WITH_EDITOR
FText FArcMassActorGameplayAttributeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "GameplayAttributeCondDesc", "Attribute {0} {1}"),
				UEnum::GetDisplayValueAsText(Operator),
				FText::AsNumber(InstanceData->Value));
		}
	}
	return FText::GetEmpty();
}
#endif