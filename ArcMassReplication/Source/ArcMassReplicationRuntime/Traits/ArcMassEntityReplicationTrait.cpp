// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcMassEntityReplicationTrait.h"
#include "Fragments/ArcMassReplicatedTag.h"
#include "Fragments/ArcMassReplicationConfigFragment.h"
#include "Fragments/ArcMassNetIdFragment.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"

void UArcMassEntityReplicationTrait::BuildTemplate(
	FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	BuildContext.AddTag<FArcMassEntityReplicatedTag>();
	BuildContext.AddFragment<FArcMassEntityNetHandleFragment>();

	FArcMassEntityReplicationConfigFragment ConfigFragment;
	ConfigFragment.ReplicatedFragmentEntries = ReplicatedFragments;
	ConfigFragment.CullDistance = CullDistance;
	ConfigFragment.CellSize = CellSize;
	ConfigFragment.EntityConfigAsset = EntityConfigAsset;

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct& SharedConfig = EntityManager.GetOrCreateConstSharedFragment(ConfigFragment);
	BuildContext.AddConstSharedFragment(SharedConfig);
}
