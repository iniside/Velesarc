// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementSelectionInterface.h"

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityElementSelectionInterface)

bool UArcMassEntityElementSelectionInterface::SelectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity && ITypedElementSelectionInterface::SelectElement(InElementHandle, InSelectionSet, InSelectionOptions))
	{
		MassEntity.NotifyMassEntityInstanceSelectionChanged(/*bIsSelected*/true);
		return true;
	}
	return false;
}

bool UArcMassEntityElementSelectionInterface::DeselectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity && ITypedElementSelectionInterface::DeselectElement(InElementHandle, InSelectionSet, InSelectionOptions))
	{
		MassEntity.NotifyMassEntityInstanceSelectionChanged(/*bIsSelected*/false);
		return true;
	}
	return false;
}
