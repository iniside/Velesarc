// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Persistence/ArcMassPersistenceTrait.h"

#include "ArcMassPersistence.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassCommonFragments.h"

void UArcMassPersistenceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcMassPersistenceFragment>();
	BuildContext.AddTag<FArcMassPersistenceTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(PersistenceConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
