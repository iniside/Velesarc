// Copyright Lukasz Baran. All Rights Reserved.

#include "Elements/SkMInstance/SkMInstanceElementDetailsInterface.h"

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceManager.h"
#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkMInstanceElementDetailsInterface)

namespace SkMInstanceDetailsInterfacePrivate
{

class FSkMInstanceTypedElementDetailsObject : public ITypedElementDetailsObject
{
public:
	explicit FSkMInstanceTypedElementDetailsObject(const FSkMInstanceElementId& InSkMInstanceElementId)
	{
		UArcPlacedEntityProxyEditingObject* ProxyPtr = NewObject<UArcPlacedEntityProxyEditingObject>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);

		ProxyPtr->InitFromSkMInstance(InSkMInstanceElementId);
		ProxyObject = ProxyPtr;
	}

	virtual ~FSkMInstanceTypedElementDetailsObject() override
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

} // namespace SkMInstanceDetailsInterfacePrivate

TUniquePtr<ITypedElementDetailsObject> USkMInstanceElementDetailsInterface::GetDetailsObject(const FTypedElementHandle& InElementHandle)
{
	const FSkMInstanceElementData* SkMInstanceElement = InElementHandle.GetData<FSkMInstanceElementData>();
	if (SkMInstanceElement == nullptr)
	{
		return nullptr;
	}

	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandleChecked(InElementHandle);
	if (!SkMInstance)
	{
		return nullptr;
	}

	return MakeUnique<SkMInstanceDetailsInterfacePrivate::FSkMInstanceTypedElementDetailsObject>(
		SkMInstanceElement->InstanceElementId);
}
