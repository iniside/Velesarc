// Copyright Lukasz Baran. All Rights Reserved.

#include "Elements/MassEntity/ArcMassEntityElementDetailsInterface.h"

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityElementDetailsInterface)

namespace ArcMassEntityDetailsInterfacePrivate
{

class FArcMassEntityTypedElementDetailsObject : public ITypedElementDetailsObject
{
public:
	explicit FArcMassEntityTypedElementDetailsObject(const FArcMassEntityElementId& InElementId)
	{
		UArcPlacedEntityProxyEditingObject* ProxyPtr = NewObject<UArcPlacedEntityProxyEditingObject>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);

		ProxyPtr->InitFromMassEntityElement(InElementId);
		ProxyObject = ProxyPtr;
	}

	virtual ~FArcMassEntityTypedElementDetailsObject() override
	{
		if (UArcPlacedEntityProxyEditingObject* ProxyPtr = ProxyObject.Get())
		{
			ProxyPtr->Shutdown();
		}
	}

	virtual UObject* GetObject() override
	{
		return ProxyObject.Get();
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		if (UArcPlacedEntityProxyEditingObject* ProxyPtr = ProxyObject.Get())
		{
			Collector.AddReferencedObject(ProxyObject);
			ProxyObject = ProxyPtr;
		}
	}

private:
	TWeakObjectPtr<UArcPlacedEntityProxyEditingObject> ProxyObject;
};

} // namespace ArcMassEntityDetailsInterfacePrivate

TUniquePtr<ITypedElementDetailsObject> UArcMassEntityElementDetailsInterface::GetDetailsObject(const FTypedElementHandle& InElementHandle)
{
	const FArcMassEntityElementData* MassEntityElement = InElementHandle.GetData<FArcMassEntityElementData>();
	if (MassEntityElement == nullptr)
	{
		return nullptr;
	}

	FArcMassEntityManager MassEntity = ArcMassEntityElementUtils::GetArcMassEntityFromHandle(InElementHandle, /*bSilent*/true);
	if (!MassEntity)
	{
		return nullptr;
	}

	return MakeUnique<ArcMassEntityDetailsInterfacePrivate::FArcMassEntityTypedElementDetailsObject>(
		MassEntityElement->InstanceElementId);
}
