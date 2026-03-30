// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Persistence/ArcMassPersistenceTrait.h"

#include "ArcMassPersistence.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassCommonFragments.h"

void UArcMassPersistenceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcMassPersistenceFragment>();
	BuildContext.AddTag<FArcMassPersistenceTag>();

	FArcMassPersistenceConfigFragment ConfigCopy = PersistenceConfig;

	if (const UMassEntityConfigAsset* ConfigAsset = GetTypedOuter<UMassEntityConfigAsset>())
	{
		ConfigCopy.ConfigGuid = ConfigAsset->GetConfig().GetGuid();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(ConfigCopy);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
