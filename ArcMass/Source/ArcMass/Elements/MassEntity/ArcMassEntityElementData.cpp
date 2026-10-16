// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "GameFramework/Actor.h"

UE_DEFINE_TYPED_ELEMENT_DATA_RTTI(FArcMassEntityElementData);

const FName NAME_ArcMassEntity = "ArcMassEntity";

namespace ArcMassEntityElementUtils
{

IArcMassEntityManager* GetArcMassEntityManager(const FArcMassEntityElementId& InId)
{
	AActor* Owner = InId.PartitionActor.Get();
	if (!Owner)
	{
		return nullptr;
	}
	if (IArcMassEntityManagerProvider* Provider = Cast<IArcMassEntityManagerProvider>(Owner))
	{
		return Provider->GetMassEntityManager(InId);
	}
	return Cast<IArcMassEntityManager>(Owner);
}

} // namespace ArcMassEntityElementUtils
