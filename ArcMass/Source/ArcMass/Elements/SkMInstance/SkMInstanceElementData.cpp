// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "GameFramework/Actor.h"

UE_DEFINE_TYPED_ELEMENT_DATA_RTTI(FSkMInstanceElementData);

const FName NAME_SkMInstance = "SkMInstance";

namespace SkMInstanceElementDataUtil
{

ISkMInstanceManager* GetSkMInstanceManager(const FSkMInstanceId& InstanceId)
{
	if (!InstanceId)
	{
		return nullptr;
	}

	AActor* OwnerActor = InstanceId.SkMComponent->GetOwner();
	if (OwnerActor == nullptr)
	{
		return nullptr;
	}

	// If the owner actor is a manager provider, ask it for the specific manager.
	ISkMInstanceManagerProvider* ManagerProvider = Cast<ISkMInstanceManagerProvider>(OwnerActor);
	if (ManagerProvider != nullptr)
	{
		return ManagerProvider->GetSkMInstanceManager(InstanceId);
	}

	// If the owner actor is itself a manager, use it directly.
	ISkMInstanceManager* InstanceManager = Cast<ISkMInstanceManager>(OwnerActor);
	if (InstanceManager != nullptr)
	{
		return InstanceManager;
	}

	return nullptr;
}

FSkMInstanceManager GetSkMInstanceFromHandle(const FTypedElementHandle& InHandle, const bool bSilent)
{
	const FSkMInstanceElementData* SkMInstanceElement = InHandle.GetData<FSkMInstanceElementData>(bSilent);
	if (SkMInstanceElement == nullptr)
	{
		return FSkMInstanceManager();
	}

	const FSkMInstanceId SkMInstanceId = ElementIdToSkMInstanceId(SkMInstanceElement->InstanceElementId);
	return FSkMInstanceManager(SkMInstanceId, GetSkMInstanceManager(SkMInstanceId));
}

FSkMInstanceManager GetSkMInstanceFromHandleChecked(const FTypedElementHandle& InHandle)
{
	const FSkMInstanceElementData& SkMInstanceElement = InHandle.GetDataChecked<FSkMInstanceElementData>();
	const FSkMInstanceId SkMInstanceId = ElementIdToSkMInstanceId(SkMInstanceElement.InstanceElementId);
	return FSkMInstanceManager(SkMInstanceId, GetSkMInstanceManager(SkMInstanceId));
}

} // namespace SkMInstanceElementDataUtil
