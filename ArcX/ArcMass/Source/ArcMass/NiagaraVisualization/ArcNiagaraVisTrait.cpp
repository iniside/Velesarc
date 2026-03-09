// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNiagaraVisTrait.h"

#include "MassEntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcNiagaraVisTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcNiagaraVisFragment>();
	BuildContext.AddTag<FArcNiagaraVisTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(NiagaraConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
