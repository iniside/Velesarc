#include "ArcMassEntityTagsCondition.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "StateTreeNodeDescriptionHelpers.h"

bool FArcMassEntityTagsCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassGameplayTagsHandle);
	
	return true;
}

void FArcMassEntityTagsCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcMassGameplayTagContainerFragment>();
}

bool FArcMassEntityTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassGameplayTagContainerFragment* MassTags = MassStateTreeContext.GetExternalDataPtr(MassGameplayTagsHandle);
	if (!MassTags)
	{
		return false;
	}

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
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