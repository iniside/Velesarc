// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Resources/ArcResourceTrait.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Mass/EntityUtils.h"
#include "MassEntityTemplateRegistry.h"
#include "MassWorldUtils.h"

void UArcResourceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (!Config)
	{
		UE_LOGF(LogArcMassMechanics, Error, "UArcResourceTrait::BuildTemplate: Config is null on %ls; the trait adds nothing.", *GetPathNameSafe(this));
		return;
	}

	BuildContext.AddFragment_GetRef<FArcResourceStateFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	FArcResourceConfigSharedFragment SharedConfig;
	SharedConfig.Config = Config;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(SharedConfig));
}

bool UArcResourceTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	OutTraitRequirements.Add(FArcAggregatorFragment::StaticStruct());
	return Super::ValidateTemplate(BuildContext, World, OutTraitRequirements);
}
