// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisUAFTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "Fragments/MassUAFComponentFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMobileVisUAFTrait)

void UArcMobileVisUAFTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassUAFComponentWrapperFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct AnimDescShared = EntityManager.GetOrCreateConstSharedFragment(AnimDesc);
	BuildContext.AddConstSharedFragment(AnimDescShared);
}
