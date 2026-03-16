// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPlayerPersistenceTrait.h"

#include "ArcMassPlayerPersistence.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcMassPlayerPersistenceTrait::BuildTemplate(
	FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	BuildContext.AddTag<FArcMassPlayerPersistenceTag>();

	FMassEntityManager& EntityManager =
		UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment =
		EntityManager.GetOrCreateConstSharedFragment(PersistenceConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
