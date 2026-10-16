// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcOwnedTagsTrait.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"

void UArcOwnedTagsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// FArcOwnedTagsFragment is a sparse fragment — seeded via BuildSparseFragments, not here.
}

void UArcOwnedTagsTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	FArcOwnedTagsFragment OwnedTags;
	for (const FGameplayTag& Tag : DefaultTags)
	{
		OwnedTags.Tags.AddTag(Tag);
	}
	Out.AddSparseFragment(FConstStructView::Make(OwnedTags));
}
