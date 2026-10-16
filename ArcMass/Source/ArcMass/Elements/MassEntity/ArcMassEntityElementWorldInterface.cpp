// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementWorldInterface.h"

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityElementWorldInterface)

bool UArcMassEntityElementWorldInterface::CanEditElement(const FTypedElementHandle& InElementHandle)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.CanEditMassEntityInstance();
}

bool UArcMassEntityElementWorldInterface::IsTemplateElement(const FTypedElementHandle& InElementHandle)
{
	// No template concept for Mass entity instances
	return false;
}

ULevel* UArcMassEntityElementWorldInterface::GetOwnerLevel(const FTypedElementHandle& InElementHandle)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity)
	{
		if (AActor* Actor = MassEntity.GetInstanceId().PartitionActor.Get())
		{
			return Actor->GetLevel();
		}
	}
	return nullptr;
}

UWorld* UArcMassEntityElementWorldInterface::GetOwnerWorld(const FTypedElementHandle& InElementHandle)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity ? (MassEntity.GetInstanceId().PartitionActor.IsValid() ? MassEntity.GetInstanceId().PartitionActor->GetWorld() : nullptr) : nullptr;
}

bool UArcMassEntityElementWorldInterface::GetBounds(const FTypedElementHandle& InElementHandle, FBoxSphereBounds& OutBounds)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (!MassEntity)
	{
		return false;
	}

#if WITH_EDITOR
	// Ask the manager for mesh-accurate bounds — works for both AArcPlacedEntityPartitionActor and AArcAlwaysLoadedEntityActor.
	if (MassEntity.GetMassEntityInstanceMeshBounds(OutBounds))
	{
		return true;
	}
#endif // WITH_EDITOR

	// Fallback: use the instance world transform with a minimal sphere.
	FTransform WorldXform;
	if (!MassEntity.GetMassEntityInstanceTransform(WorldXform, /*bWorldSpace*/true))
	{
		return false;
	}
	const FBoxSphereBounds FallbackBounds(FVector::ZeroVector, FVector(50.0), 50.0);
	OutBounds = FallbackBounds.TransformBy(WorldXform);
	return true;
}

bool UArcMassEntityElementWorldInterface::CanMoveElement(const FTypedElementHandle& InElementHandle, const ETypedElementWorldType InWorldType)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.CanMoveMassEntityInstance(InWorldType);
}

bool UArcMassEntityElementWorldInterface::GetWorldTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.GetMassEntityInstanceTransform(OutTransform, /*bWorldSpace*/true);
}

bool UArcMassEntityElementWorldInterface::SetWorldTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.SetMassEntityInstanceTransform(InTransform, /*bWorldSpace*/true, /*bMarkRenderStateDirty*/true);
}

bool UArcMassEntityElementWorldInterface::GetRelativeTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform)
{
	const FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.GetMassEntityInstanceTransform(OutTransform, /*bWorldSpace*/false);
}

bool UArcMassEntityElementWorldInterface::SetRelativeTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	return MassEntity && MassEntity.SetMassEntityInstanceTransform(InTransform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);
}

void UArcMassEntityElementWorldInterface::NotifyMovementStarted(const FTypedElementHandle& InElementHandle)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity)
	{
		MassEntity.NotifyMassEntityInstanceMovementStarted();
	}
}

void UArcMassEntityElementWorldInterface::NotifyMovementOngoing(const FTypedElementHandle& InElementHandle)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity)
	{
		MassEntity.NotifyMassEntityInstanceMovementOngoing();
	}
}

void UArcMassEntityElementWorldInterface::NotifyMovementEnded(const FTypedElementHandle& InElementHandle)
{
	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle);
	if (MassEntity)
	{
		MassEntity.NotifyMassEntityInstanceMovementEnded();
	}
}
