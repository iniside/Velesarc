// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementHierarchyInterface.h"

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "Elements/Framework/EngineElementsLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityElementHierarchyInterface)

FTypedElementHandle UArcMassEntityElementHierarchyInterface::GetParentElement(const FTypedElementHandle& InElementHandle, const bool bAllowCreate)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity)
	{
#if WITH_EDITOR
		if (AActor* Actor = MassEntity.GetInstanceId().PartitionActor.Get())
		{
			return UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor, bAllowCreate);
		}
#endif // WITH_EDITOR
	}
	return FTypedElementHandle();
}
