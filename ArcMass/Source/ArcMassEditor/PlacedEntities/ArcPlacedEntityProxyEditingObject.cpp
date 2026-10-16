// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityManager.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTemplate.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityFragments.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

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

	bIsMassEntity = false;
	PartitionActor.Reset();
	OverrideHostObject.Reset();
	OverrideHost = nullptr;
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
			if (bIsMassEntity)
			{
				IArcMassEntityManager* Manager = GetMassEntityManager();
				if (Manager != nullptr)
				{
					if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
					{
						if (!bIsWithinInteractiveTransformEdit)
						{
							Manager->NotifyMassEntityInstanceMovementStarted(GetMassEntityId());
						}
						bIsWithinInteractiveTransformEdit = true;
					}

					Manager->SetMassEntityInstanceTransform(GetMassEntityId(), Transform, /*bWorldSpace*/false, /*bMarkRenderStateDirty*/true);

					if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
					{
						Manager->NotifyMassEntityInstanceMovementOngoing(GetMassEntityId());
					}
					else
					{
						if (bIsWithinInteractiveTransformEdit)
						{
							Manager->NotifyMassEntityInstanceMovementEnded(GetMassEntityId());
						}
						bIsWithinInteractiveTransformEdit = false;
					}

					GUnrealEd->UpdatePivotLocationForSelection();
					GUnrealEd->RedrawLevelEditingViewports();
				}
			}
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

bool UArcPlacedEntityProxyEditingObject::SyncProxyStateFromInstance()
{
	if (bIsMassEntity)
	{
		IArcMassEntityManager* Manager = GetMassEntityManager();
		if (Manager != nullptr && EntryIndex != INDEX_NONE && TransformIndex != INDEX_NONE)
		{
			Manager->GetMassEntityInstanceTransform(GetMassEntityId(), Transform, /*bWorldSpace*/false);
			return true;
		}
		Transform = FTransform::Identity;
		return false;
	}

	Transform = FTransform::Identity;
	return false;
}

void UArcPlacedEntityProxyEditingObject::InitFromMassEntityElement(const FArcMassEntityElementId& InId)
{
	if (ResolveToPartitionActorFromMassEntity(InId))
	{
		bIsMassEntity = true;
	}
	BuildEditableFragmentList();
	StartSyncTicker();
}

bool UArcPlacedEntityProxyEditingObject::ResolveToPartitionActorFromMassEntity(const FArcMassEntityElementId& InId)
{
	if (!InId)
	{
		return false;
	}

	// Any owner implementing IArcPlacedEntityOverrideHost is supported
	// (AArcPlacedEntityPartitionActor and AArcAlwaysLoadedEntityActor).
	UObject* Owner = InId.PartitionActor.Get();
	IArcPlacedEntityOverrideHost* Host = Cast<IArcPlacedEntityOverrideHost>(Owner);
	if (Host == nullptr)
	{
		return false;
	}

	OverrideHostObject = Owner;
	OverrideHost = Host;
	// Keep the typed PartitionActor pointer too when the host is the partition actor (back-compat for GetPartitionActor()).
	PartitionActor = Cast<AArcPlacedEntityPartitionActor>(Owner);
	EntryIndex = InId.EntryIndex;
	TransformIndex = InId.TransformIndex;
	return true;
}

TArray<FArcPlacedEntityEntry>* UArcPlacedEntityProxyEditingObject::GetActiveEntries()
{
	if (OverrideHost != nullptr)
	{
		return &OverrideHost->GetPlacedEntityEntries();
	}
	if (PartitionActor.IsValid())
	{
		return &PartitionActor->Entries;
	}
	return nullptr;
}

const TArray<FArcPlacedEntityEntry>* UArcPlacedEntityProxyEditingObject::GetActiveEntries() const
{
	if (OverrideHost != nullptr)
	{
		return &OverrideHost->GetPlacedEntityEntries();
	}
	if (PartitionActor.IsValid())
	{
		return &PartitionActor->Entries;
	}
	return nullptr;
}

UWorld* UArcPlacedEntityProxyEditingObject::GetActiveOwnerWorld() const
{
	if (AActor* OwnerActor = Cast<AActor>(OverrideHostObject.Get()))
	{
		return OwnerActor->GetWorld();
	}
	if (PartitionActor.IsValid())
	{
		return PartitionActor->GetWorld();
	}
	return nullptr;
}

const FArcPlacedEntityEntry* UArcPlacedEntityProxyEditingObject::GetActiveEntry() const
{
	const TArray<FArcPlacedEntityEntry>* Entries = GetActiveEntries();
	if (Entries == nullptr || !Entries->IsValidIndex(EntryIndex))
	{
		return nullptr;
	}
	return &(*Entries)[EntryIndex];
}

void UArcPlacedEntityProxyEditingObject::ModifyActiveOwner()
{
	if (UObject* OwnerObject = OverrideHostObject.Get())
	{
		OwnerObject->Modify();
	}
	else if (PartitionActor.IsValid())
	{
		PartitionActor->Modify();
	}
}

void UArcPlacedEntityProxyEditingObject::BuildEditableFragmentList()
{
	EditableFragments.Empty();
	FragmentDisplayNames.Empty();

	const TArray<FArcPlacedEntityEntry>* Entries = GetActiveEntries();
	if (Entries == nullptr || !Entries->IsValidIndex(EntryIndex))
	{
		return;
	}

	const FArcPlacedEntityEntry& Entry = (*Entries)[EntryIndex];
	if (Entry.EntityConfig == nullptr)
	{
		return;
	}

	UWorld* World = GetActiveOwnerWorld();
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

	UpdateDisplayName();
}

void UArcPlacedEntityProxyEditingObject::UpdateDisplayName()
{
	const TArray<FArcPlacedEntityEntry>* Entries = GetActiveEntries();
	if (Entries == nullptr || !Entries->IsValidIndex(EntryIndex))
	{
		EntityDisplayName = FString();
		return;
	}

	const FArcPlacedEntityEntry& Entry = (*Entries)[EntryIndex];
	EntityDisplayName = FString::Printf(TEXT("%s [%d]"), *GetNameSafe(Entry.EntityConfig), TransformIndex);
}

void UArcPlacedEntityProxyEditingObject::WriteOverridesToPartitionActor()
{
	TArray<FArcPlacedEntityEntry>* Entries = GetActiveEntries();
	if (Entries == nullptr || !Entries->IsValidIndex(EntryIndex) || TransformIndex == INDEX_NONE)
	{
		return;
	}

	ModifyActiveOwner();

	FArcPlacedEntityEntry& Entry = (*Entries)[EntryIndex];

	if (EditableFragments.Num() == 0)
	{
		Entry.PerInstanceOverrides.Remove(TransformIndex);
		return;
	}

	UWorld* World = GetActiveOwnerWorld();
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

FArcMassEntityElementId UArcPlacedEntityProxyEditingObject::GetMassEntityId() const
{
	FArcMassEntityElementId Id;
	Id.PartitionActor = Cast<AActor>(OverrideHostObject.Get());
	Id.EntryIndex = EntryIndex;
	Id.TransformIndex = TransformIndex;
	return Id;
}

IArcMassEntityManager* UArcPlacedEntityProxyEditingObject::GetMassEntityManager() const
{
	// Both AArcPlacedEntityPartitionActor and AArcAlwaysLoadedEntityActor implement
	// IArcMassEntityManager directly, so the direct cast resolves the same manager
	// that the element id expects.
	return Cast<IArcMassEntityManager>(OverrideHostObject.Get());
}
