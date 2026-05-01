// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementWorldInterface.h"

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkMInstanceElementWorldInterface)

bool USkMInstanceElementWorldInterface::CanEditElement(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.CanEditSkMInstance();
}

bool USkMInstanceElementWorldInterface::IsTemplateElement(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.GetSkMComponent()->IsTemplate();
}

ULevel* USkMInstanceElementWorldInterface::GetOwnerLevel(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance)
	{
		const AActor* ComponentOwner = SkMInstance.GetSkMComponent()->GetOwner();
		if (ComponentOwner)
		{
			return ComponentOwner->GetLevel();
		}
	}
	return nullptr;
}

UWorld* USkMInstanceElementWorldInterface::GetOwnerWorld(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance ? SkMInstance.GetSkMComponent()->GetWorld() : nullptr;
}

bool USkMInstanceElementWorldInterface::GetBounds(const FTypedElementHandle& InElementHandle, FBoxSphereBounds& OutBounds)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance)
	{
		UInstancedSkinnedMeshComponent* SkMComponent = SkMInstance.GetSkMComponent();

		// Use the skinned asset's local bounds, not the component's aggregate bounds
		FBoxSphereBounds LocalBounds(ForceInitToZero);
		if (USkinnedAsset* SkinnedAsset = SkMComponent->GetSkinnedAsset())
		{
			LocalBounds = SkinnedAsset->GetBounds();
		}

		FTransform InstanceTransform;
		SkMInstance.GetSkMInstanceTransform(InstanceTransform, /*bWorldSpace*/true);

		OutBounds = LocalBounds.TransformBy(InstanceTransform);
		return true;
	}
	return false;
}

bool USkMInstanceElementWorldInterface::CanMoveElement(const FTypedElementHandle& InElementHandle, const ETypedElementWorldType InWorldType)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.CanMoveSkMInstance(InWorldType);
}

bool USkMInstanceElementWorldInterface::GetWorldTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.GetSkMInstanceTransform(OutTransform, /*bWorldSpace*/true);
}

bool USkMInstanceElementWorldInterface::SetWorldTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.SetSkMInstanceTransform(InTransform, /*bWorldSpace*/true, /*bMarkRenderStateDirty*/true);
}

bool USkMInstanceElementWorldInterface::GetRelativeTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.GetSkMInstanceTransform(OutTransform, /*bWorldSpace*/false);
}

bool USkMInstanceElementWorldInterface::SetRelativeTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	return SkMInstance && SkMInstance.SetSkMInstanceTransform(InTransform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);
}

void USkMInstanceElementWorldInterface::NotifyMovementStarted(const FTypedElementHandle& InElementHandle)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance)
	{
		SkMInstance.NotifySkMInstanceMovementStarted();
	}
}

void USkMInstanceElementWorldInterface::NotifyMovementOngoing(const FTypedElementHandle& InElementHandle)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance)
	{
		SkMInstance.NotifySkMInstanceMovementOngoing();
	}
}

void USkMInstanceElementWorldInterface::NotifyMovementEnded(const FTypedElementHandle& InElementHandle)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance)
	{
		SkMInstance.NotifySkMInstanceMovementEnded();
	}
}
