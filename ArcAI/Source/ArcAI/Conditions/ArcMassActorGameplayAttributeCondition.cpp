#include "ArcMassActorGameplayAttributeCondition.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
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

	AActor* Actor = const_cast<AActor*>(MassActor->Get());
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);

	if (!ASI)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();

	if (!ASC)
	{
		return false;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	const float AttributeValue = ASC->GetNumericAttribute(InstanceData.Attribute);
	switch (Operator)
	{
		case EGenericAICheck::Less:
			return AttributeValue < InstanceData.Value;
		case EGenericAICheck::LessOrEqual:
			return AttributeValue <= InstanceData.Value;
		case EGenericAICheck::Equal:
			return FMath::IsNearlyEqual(AttributeValue, InstanceData.Value);
		case EGenericAICheck::NotEqual:
			return !FMath::IsNearlyEqual(AttributeValue, InstanceData.Value);
		case EGenericAICheck::GreaterOrEqual:
			return AttributeValue >= InstanceData.Value;
		case EGenericAICheck::Greater:
			return AttributeValue > InstanceData.Value;
		case EGenericAICheck::IsTrue:
			return true;
		case EGenericAICheck::MAX:
			return false;
	}
	
	return false;
}

#if WITH_EDITOR
FText FArcMassActorGameplayAttributeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "GameplayAttributeCondDesc", "{0} {1} {2}"),
				FText::FromString(InstanceData->Attribute.GetName()),
				UEnum::GetDisplayValueAsText(Operator),
				FText::AsNumber(InstanceData->Value));
		}
	}
	return FText::GetEmpty();
}
#endif