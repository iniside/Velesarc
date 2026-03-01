// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaTrait.h"

#include "ArcMAss/ArcMassSmartObjectFragments.h"
#include "MassEntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcAreaTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAreaFragment>();
	BuildContext.AddTag<FArcAreaTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	FArcAreaConfigFragment Config;
	Config.AreaDefinition = AreaDefinition;
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	// Add smart object ownership fragments when a definition is provided.
	// The area observer will create the SmartObject at the entity's transform.
	if (SmartObjectDefinition)
	{
		BuildContext.AddFragment<FArcSmartObjectOwnerFragment>();

		FArcSmartObjectDefinitionSharedFragment SODefFragment;
		SODefFragment.SmartObjectDefinition = SmartObjectDefinition;
		const FConstSharedStruct SODefSharedFragment = EntityManager.GetOrCreateConstSharedFragment(SODefFragment);
		BuildContext.AddConstSharedFragment(SODefSharedFragment);
	}
}
