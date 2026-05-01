// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAttributesTrait.h"
#include "Traits/ArcDefaultEffectsObserver.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcAttributesTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcEffectStackFragment>();
	BuildContext.AddFragment<FArcAggregatorFragment>();
	BuildContext.AddFragment<FArcOwnedTagsFragment>();
	BuildContext.AddFragment_GetRef<FArcPendingAttributeOpsFragment>();

	for (const FInstancedStruct& AttrFragment : AttributeFragments)
	{
		if (AttrFragment.IsValid())
		{
			BuildContext.AddFragment(AttrFragment);
		}
	}

	if (DefaultEffects.Num() > 0)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

		FArcDefaultEffectsSharedFragment DefaultEffectsConfig;
		DefaultEffectsConfig.DefaultEffects = DefaultEffects;
		const FConstSharedStruct SharedFragment = EntityManager.GetOrCreateConstSharedFragment(DefaultEffectsConfig);
		BuildContext.AddConstSharedFragment(SharedFragment);
	}
}
