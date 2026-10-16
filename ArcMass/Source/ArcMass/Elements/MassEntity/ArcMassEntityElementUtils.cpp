// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementOwnerStore.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "Mesh/ArcRenderMeshFragments.h"

#if WITH_EDITOR
namespace
{
	TTypedElementOwnerStore<FArcMassEntityElementData, FArcMassEntityElementId> GArcMassEntityElementOwnerStore;
} // anonymous namespace
#endif // WITH_EDITOR

namespace ArcMassEntityElementUtils
{

static TTypedElementOwner<FArcMassEntityElementData> CreateArcMassEntityElementImpl(const FArcMassEntityElementId& InId)
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	TTypedElementOwner<FArcMassEntityElementData> MassEntityElement;
	if (ensureAlways(Registry) && ArcMassEntityElementUtils::GetArcMassEntityManager(InId))
	{
		MassEntityElement = Registry->CreateElement<FArcMassEntityElementData>(NAME_ArcMassEntity);
		if (MassEntityElement)
		{
			MassEntityElement.GetDataChecked().InstanceElementId = InId;
		}
	}

	return MassEntityElement;
}

FArcMassEntityManager GetArcMassEntityFromHandle(const FTypedElementHandle& InHandle, const bool bSilent)
{
	const FArcMassEntityElementData* Data = InHandle.GetData<FArcMassEntityElementData>(bSilent);
	if (!Data)
	{
		return FArcMassEntityManager();
	}
	return FArcMassEntityManager(Data->InstanceElementId, GetArcMassEntityManager(Data->InstanceElementId));
}

FArcMassEntityManager GetArcMassEntityFromHandleChecked(const FTypedElementHandle& InHandle)
{
	const FArcMassEntityElementData& Data = InHandle.GetDataChecked<FArcMassEntityElementData>();
	return FArcMassEntityManager(Data.InstanceElementId, GetArcMassEntityManager(Data.InstanceElementId));
}

FTypedElementHandle AcquireEditorArcMassEntityElementHandle(const FArcMassEntityElementId& InId, const bool bAllowCreate)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		TTypedElementOwnerScopedAccess<FArcMassEntityElementData> MassEntityElement = bAllowCreate
			? GArcMassEntityElementOwnerStore.FindOrRegisterElementOwner(
				InId,
				[&InId]()
				{
					return CreateArcMassEntityElementImpl(InId);
				})
			: GArcMassEntityElementOwnerStore.FindElementOwner(InId);

		if (MassEntityElement)
		{
			return MassEntityElement->AcquireHandle();
		}
	}
#endif // WITH_EDITOR

	return FTypedElementHandle();
}

void DestroyEditorArcMassEntityElement(const FArcMassEntityElementId& InId)
{
#if WITH_EDITOR
	if (TTypedElementOwner<FArcMassEntityElementData> MassEntityElement =
			GArcMassEntityElementOwnerStore.UnregisterElementOwner(InId))
	{
		UTypedElementRegistry::GetInstance()->DestroyElement(MassEntityElement);
	}
#endif
}

#if WITH_EDITOR
void DrainEditorArcMassEntityElementOwnerStore()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	GArcMassEntityElementOwnerStore.UnregisterElementOwners(
		[](const TTypedElementOwner<FArcMassEntityElementData>&) { return true; },
		[Registry](TTypedElementOwner<FArcMassEntityElementData>&& Element)
		{
			if (Registry) { Registry->DestroyElement(Element); }
			else          { Element.Private_DestroyNoRef(); }
		});

	// DestroyElement only *defers* destruction; at pre-exit no further frame/GC tick runs
	// to flush it before statics destruct. Flush explicitly so the registry's deferred
	// queue holds no dangling slots. (ProcessDeferredElementsToDestroy is public + doc'd
	// as manually callable — TypedElementRegistry.h:243.)
	if (Registry)
	{
		Registry->ProcessDeferredElementsToDestroy();
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
FTypedElementHandle ResolveRenderInstanceId(const FArcTypeElementInstanceId& InInstanceId)
{
	FArcMassEntityElementId Id;
	Id.PartitionActor = Cast<AActor>(InInstanceId.Owner.Get());
	Id.EntryIndex = InInstanceId.EntryIndex;
	Id.TransformIndex = InInstanceId.TransformIndex;
	return AcquireEditorArcMassEntityElementHandle(Id, /*bAllowCreate*/false);
}
#endif // WITH_EDITOR

} // namespace ArcMassEntityElementUtils
