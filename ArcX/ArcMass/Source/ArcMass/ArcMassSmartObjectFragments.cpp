// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSmartObjectFragments.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcMassSmartObjectOwnerTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FArcSmartObjectOwnerFragment>();

	const FConstSharedStruct SmartObjectDefFragment = EntityManager.GetOrCreateConstSharedFragment(SmartObjectDefinition);
	BuildContext.AddConstSharedFragment(SmartObjectDefFragment);
}
