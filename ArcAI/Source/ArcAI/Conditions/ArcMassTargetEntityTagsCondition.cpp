#include "ArcMassTargetEntityTagsCondition.h"

#include "MassStateTreeExecutionContext.h"
#include "StateTreeNodeDescriptionHelpers.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"

bool FArcMassTargetEntityTagsCondition::Link(FStateTreeLinker& Linker)
{	
	return true;
}

void FArcMassTargetEntityTagsCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
}

bool FArcMassTargetEntityTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EM = MassStateTreeContext.GetEntityManager();
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.TargetInput.GetEntityHandle().IsValid() == false)
	{
		return false;
	}
	
	FArcMassGameplayTagContainerFragment* MassTags = EM.GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(InstanceData.TargetInput.GetEntityHandle());
	if (!MassTags)
	{
		return false;
	}

	bool bResult = false;
	switch (MatchType)
	{
		case EGameplayContainerMatchType::Any:
			bResult = bExactMatch ? MassTags->Tags.HasAnyExact(InstanceData.TagContainer) : MassTags->Tags.HasAny(InstanceData.TagContainer);
			break;
		case EGameplayContainerMatchType::All:
			bResult = bExactMatch ? MassTags->Tags.HasAllExact(InstanceData.TagContainer) : MassTags->Tags.HasAll(InstanceData.TagContainer);
			break;
		default:
			ensureMsgf(false, TEXT("Unhandled match type %s."), *UEnum::GetValueAsString(MatchType));
	}

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s'%s' contains '%s %s%s'")
		, *UE::StateTree::DescHelpers::GetInvertText(bInvert, EStateTreeNodeFormatting::Text).ToString()
		, *UE::StateTree::DescHelpers::GetGameplayTagContainerAsText(InstanceData.TagContainer).ToString()
		, *UEnum::GetValueAsString(MatchType)
		, *UE::StateTree::DescHelpers::GetExactMatchText(bExactMatch, EStateTreeNodeFormatting::Text).ToString()
		, *UE::StateTree::DescHelpers::GetGameplayTagContainerAsText(MassTags->Tags).ToString());

	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FArcMassTargetEntityTagsCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "TargetEntityTagsCondDesc", "{0}Target Entity Has {1} Tags: {2}"),
				bInvert ? FText::FromString(TEXT("NOT ")) : FText::GetEmpty(),
				UEnum::GetDisplayValueAsText(MatchType),
				FText::FromString(InstanceData->TagContainer.ToStringSimple()));
		}
	}
	return FText::GetEmpty();
}
#endif