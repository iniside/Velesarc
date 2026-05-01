// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTemplate.h"
#include "MassEntityTypes.h"
#include "Mass/EntityFragments.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Elements/SMInstance/SMInstanceElementData.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "Components/InstancedSkinnedMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityProxyEditingObject)

namespace ArcPlacedEntityProxyEditingPrivate
{
	const FInstancedStruct* FindTemplateInitialValue(
		TConstArrayView<FInstancedStruct> InitialValues,
		const UScriptStruct* FragmentType)
	{
		for (const FInstancedStruct& InitialValue : InitialValues)
		{
			if (InitialValue.GetScriptStruct() == FragmentType)
			{
				return &InitialValue;
			}
		}
		return nullptr;
	}
}

void UArcPlacedEntityProxyEditingObject::Initialize(const FSMInstanceElementId& InSMInstanceElementId)
{
	ISMComponent = InSMInstanceElementId.ISMComponent;
	ISMInstanceId = InSMInstanceElementId.InstanceId;

	ResolveToPartitionActor(InSMInstanceElementId);
	BuildEditableFragmentList();
	StartSyncTicker();
}

void UArcPlacedEntityProxyEditingObject::InitFromSkMInstance(const FSkMInstanceElementId& InSkMInstanceElementId)
{
	bIsSkMInstance = true;
	SkMComponent = InSkMInstanceElementId.SkMComponent;
	SkMPrimitiveInstanceId = InSkMInstanceElementId.InstanceId;

	ResolveToPartitionActorFromSkM(InSkMInstanceElementId);
	BuildEditableFragmentList();
	StartSyncTicker();
}

void UArcPlacedEntityProxyEditingObject::StartSyncTicker()
{
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("UArcPlacedEntityProxyEditingObject"), 0.1f, [this](float)
	{
		const bool bDidSync = SyncProxyStateFromInstance();
		if (!bDidSync)
		{
			MarkAsGarbage();
		}
		return bDidSync;
	});
	SyncProxyStateFromInstance();
}

void UArcPlacedEntityProxyEditingObject::Shutdown()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	ISMComponent.Reset();
	ISMInstanceId = 0;
	SkMComponent.Reset();
	SkMPrimitiveInstanceId = INDEX_NONE;
	bIsSkMInstance = false;
	PartitionActor.Reset();
	EntryIndex = INDEX_NONE;
	TransformIndex = INDEX_NONE;
	Transform = FTransform::Identity;
	EditableFragments.Empty();
	FragmentDisplayNames.Empty();
}

void UArcPlacedEntityProxyEditingObject::BeginDestroy()
{
	Super::BeginDestroy();
	Shutdown();
}

void UArcPlacedEntityProxyEditingObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UArcPlacedEntityProxyEditingObject, Transform))
		{
			if (bIsSkMInstance)
			{
				ISkMInstanceManager* SkMManager = Cast<ISkMInstanceManager>(PartitionActor.Get());
				if (SkMManager != nullptr && SkMComponent.IsValid())
				{
					const FSkMInstanceId InstanceId{ SkMComponent.Get(), SkMPrimitiveInstanceId };

					if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
					{
						if (!bIsWithinInteractiveTransformEdit)
						{
							SkMManager->NotifySkMInstanceMovementStarted(InstanceId);
						}
						bIsWithinInteractiveTransformEdit = true;
					}

					SkMManager->SetSkMInstanceTransform(InstanceId, Transform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);

					if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
					{
						SkMManager->NotifySkMInstanceMovementOngoing(InstanceId);
					}
					else
					{
						if (bIsWithinInteractiveTransformEdit)
						{
							SkMManager->NotifySkMInstanceMovementEnded(InstanceId);
						}
						bIsWithinInteractiveTransformEdit = false;
					}

					GUnrealEd->UpdatePivotLocationForSelection();
					GUnrealEd->RedrawLevelEditingViewports();
				}
			}
			else if (FSMInstanceManager SMInstance = GetSMInstance())
			{
				if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
				{
					if (!bIsWithinInteractiveTransformEdit)
					{
						SMInstance.NotifySMInstanceMovementStarted();
					}
					bIsWithinInteractiveTransformEdit = true;
				}

				SMInstance.SetSMInstanceTransform(Transform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);

				if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
				{
					SMInstance.NotifySMInstanceMovementOngoing();
				}
				else
				{
					if (bIsWithinInteractiveTransformEdit)
					{
						SMInstance.NotifySMInstanceMovementEnded();
					}
					bIsWithinInteractiveTransformEdit = false;
				}

				GUnrealEd->UpdatePivotLocationForSelection();
				GUnrealEd->RedrawLevelEditingViewports();
			}
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

bool UArcPlacedEntityProxyEditingObject::SyncProxyStateFromInstance()
{
	if (bIsSkMInstance)
	{
		ISkMInstanceManager* SkMManager = Cast<ISkMInstanceManager>(PartitionActor.Get());
		if (SkMManager != nullptr && SkMComponent.IsValid())
		{
			const FSkMInstanceId InstanceId{ SkMComponent.Get(), SkMPrimitiveInstanceId };
			SkMManager->GetSkMInstanceTransform(InstanceId, Transform, /*bWorldSpace*/false);
			return true;
		}

		Transform = FTransform::Identity;
		return false;
	}

	if (FSMInstanceManager SMInstance = GetSMInstance())
	{
		SMInstance.GetSMInstanceTransform(Transform, /*bWorldSpace*/false);
		return true;
	}

	Transform = FTransform::Identity;
	return false;
}

FSMInstanceManager UArcPlacedEntityProxyEditingObject::GetSMInstance() const
{
	const FSMInstanceId SMInstanceId = FSMInstanceElementIdMap::Get().GetSMInstanceIdFromSMInstanceElementId(
		FSMInstanceElementId{ ISMComponent.Get(), ISMInstanceId });
	return FSMInstanceManager(SMInstanceId, SMInstanceElementDataUtil::GetSMInstanceManager(SMInstanceId));
}

bool UArcPlacedEntityProxyEditingObject::ResolveToPartitionActor(const FSMInstanceElementId& InSMInstanceElementId)
{
	UInstancedStaticMeshComponent* Component = InSMInstanceElementId.ISMComponent;
	if (Component == nullptr)
	{
		return false;
	}

	AArcPlacedEntityPartitionActor* Actor = Cast<AArcPlacedEntityPartitionActor>(Component->GetOwner());
	if (Actor == nullptr)
	{
		return false;
	}

	const FSMInstanceId SMInstanceId = FSMInstanceElementIdMap::Get().GetSMInstanceIdFromSMInstanceElementId(InSMInstanceElementId);
	int32 ResolvedEntryIndex = INDEX_NONE;
	int32 ResolvedTransformIndex = INDEX_NONE;
	if (!Actor->ResolveInstanceId(SMInstanceId, ResolvedEntryIndex, ResolvedTransformIndex))
	{
		return false;
	}

	PartitionActor = Actor;
	EntryIndex = ResolvedEntryIndex;
	TransformIndex = ResolvedTransformIndex;
	return true;
}

bool UArcPlacedEntityProxyEditingObject::ResolveToPartitionActorFromSkM(const FSkMInstanceElementId& InSkMInstanceElementId)
{
	UInstancedSkinnedMeshComponent* Component = InSkMInstanceElementId.SkMComponent;
	if (Component == nullptr)
	{
		return false;
	}

	AArcPlacedEntityPartitionActor* Actor = Cast<AArcPlacedEntityPartitionActor>(Component->GetOwner());
	if (Actor == nullptr)
	{
		return false;
	}

	const FSkMInstanceId SkMId{ Component, InSkMInstanceElementId.InstanceId };
	int32 ResolvedEntryIndex = INDEX_NONE;
	int32 ResolvedTransformIndex = INDEX_NONE;
	if (!Actor->ResolveSkMInstanceId(SkMId, ResolvedEntryIndex, ResolvedTransformIndex))
	{
		return false;
	}

	PartitionActor = Actor;
	EntryIndex = ResolvedEntryIndex;
	TransformIndex = ResolvedTransformIndex;
	return true;
}

void UArcPlacedEntityProxyEditingObject::BuildEditableFragmentList()
{
	EditableFragments.Empty();
	FragmentDisplayNames.Empty();

	if (!PartitionActor.IsValid() || EntryIndex == INDEX_NONE)
	{
		return;
	}

	const FArcPlacedEntityEntry& Entry = PartitionActor->Entries[EntryIndex];
	if (Entry.EntityConfig == nullptr)
	{
		return;
	}

	UWorld* World = PartitionActor->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FMassEntityTemplate& Template = Entry.EntityConfig->GetOrCreateEntityTemplate(*World);
	const FMassArchetypeCompositionDescriptor& Composition = Template.GetCompositionDescriptor();

	TArray<const UScriptStruct*> FragmentTypes;
	Composition.DebugGetFragments().ExportTypes(FragmentTypes);

	const FArcPlacedEntityFragmentOverrides* ExistingOverrides = Entry.PerInstanceOverrides.Find(TransformIndex);

	TConstArrayView<FInstancedStruct> InitialValues = Template.GetInitialFragmentValues();

	for (const UScriptStruct* FragmentType : FragmentTypes)
	{
		if (FragmentType == nullptr)
		{
			continue;
		}

		// Skip FTransformFragment — handled by the Transform property
		if (FragmentType == FTransformFragment::StaticStruct())
		{
			continue;
		}

		// Only include fragments with at least one editable property
		bool bHasEditableProperty = false;
		for (TFieldIterator<FProperty> PropIt(FragmentType); PropIt; ++PropIt)
		{
			if (PropIt->HasAnyPropertyFlags(CPF_Edit))
			{
				bHasEditableProperty = true;
				break;
			}
		}

		if (!bHasEditableProperty)
		{
			continue;
		}

		const FInstancedStruct* TemplateValue = ArcPlacedEntityProxyEditingPrivate::FindTemplateInitialValue(InitialValues, FragmentType);

		// Always start from the template initial value (current trait defaults)
		if (TemplateValue != nullptr)
		{
			EditableFragments.Add(*TemplateValue);
		}
		else
		{
			EditableFragments.Emplace(FragmentType);
		}

		// Overlay per-property overrides from stored data
		if (ExistingOverrides != nullptr)
		{
			FInstancedStruct& Base = EditableFragments.Last();

			for (const FInstancedStruct& Override : ExistingOverrides->FragmentValues)
			{
				if (Override.GetScriptStruct() != FragmentType)
				{
					continue;
				}

				const uint8* OverrideMemory = Override.GetMemory();
				const uint8* DefaultMemory = TemplateValue != nullptr ? TemplateValue->GetMemory() : nullptr;
				uint8* DestMemory = Base.GetMutableMemory();

				for (TFieldIterator<FProperty> PropIt(FragmentType); PropIt; ++PropIt)
				{
					const FProperty* Prop = *PropIt;
					const int32 Offset = Prop->GetOffset_ForInternal();

					// If the stored override property differs from template default, it was user-customized — apply it
					bool bOverrideDiffers = true;
					if (DefaultMemory != nullptr)
					{
						bOverrideDiffers = !Prop->Identical(OverrideMemory + Offset, DefaultMemory + Offset, PPF_None);
					}

					if (bOverrideDiffers)
					{
						Prop->CopySingleValue(DestMemory + Offset, OverrideMemory + Offset);
					}
				}
				break;
			}
		}

		FragmentDisplayNames.Add(FragmentType->GetDisplayNameText().ToString());
	}
}

void UArcPlacedEntityProxyEditingObject::WriteOverridesToPartitionActor()
{
	if (!PartitionActor.IsValid() || EntryIndex == INDEX_NONE || TransformIndex == INDEX_NONE)
	{
		return;
	}

	PartitionActor->Modify();

	FArcPlacedEntityEntry& Entry = PartitionActor->Entries[EntryIndex];

	if (EditableFragments.Num() == 0)
	{
		Entry.PerInstanceOverrides.Remove(TransformIndex);
		return;
	}

	UWorld* World = PartitionActor->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FMassEntityTemplate& Template = Entry.EntityConfig->GetOrCreateEntityTemplate(*World);
	TConstArrayView<FInstancedStruct> InitialValues = Template.GetInitialFragmentValues();

	TArray<FInstancedStruct> DirtyFragments;
	for (const FInstancedStruct& EditedFragment : EditableFragments)
	{
		const UScriptStruct* FragmentType = EditedFragment.GetScriptStruct();
		if (FragmentType == nullptr)
		{
			continue;
		}

		const FInstancedStruct* DefaultValue = ArcPlacedEntityProxyEditingPrivate::FindTemplateInitialValue(InitialValues, FragmentType);

		// Per-property comparison — only store if at least one property genuinely differs
		bool bHasDirtyProperty = false;
		if (DefaultValue != nullptr)
		{
			const uint8* EditedMemory = EditedFragment.GetMemory();
			const uint8* DefaultMemory = DefaultValue->GetMemory();

			for (TFieldIterator<FProperty> PropIt(FragmentType); PropIt; ++PropIt)
			{
				const FProperty* Prop = *PropIt;
				const int32 Offset = Prop->GetOffset_ForInternal();
				if (!Prop->Identical(EditedMemory + Offset, DefaultMemory + Offset, PPF_None))
				{
					bHasDirtyProperty = true;
					break;
				}
			}
		}
		else
		{
			bHasDirtyProperty = true;
		}

		if (bHasDirtyProperty)
		{
			DirtyFragments.Add(EditedFragment);
		}
	}

	if (DirtyFragments.Num() == 0)
	{
		Entry.PerInstanceOverrides.Remove(TransformIndex);
	}
	else
	{
		FArcPlacedEntityFragmentOverrides& Overrides = Entry.PerInstanceOverrides.FindOrAdd(TransformIndex);
		Overrides.FragmentValues = MoveTemp(DirtyFragments);
	}
}
