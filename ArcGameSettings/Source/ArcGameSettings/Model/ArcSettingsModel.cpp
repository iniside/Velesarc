/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Model/ArcSettingsModel.h"

#include "Descriptors/ArcSettingBuilder.h"
#include "Descriptors/ArcSettingDescriptor.h"
#include "Model/ArcSettingsStorage.h"

#include "GameFramework/GameUserSettings.h"
#include "Misc/ScopedSlowTask.h"
#include "Templates/ValueOrError.h"

// NOTE: ViewModel headers will exist after Phase 2 (Tasks 8-10).
#include "Misc/ScopeExit.h"
#include "ViewModels/ArcDiscreteSettingViewModel.h"
#include "ViewModels/ArcScalarSettingViewModel.h"
#include "ViewModels/ArcActionSettingViewModel.h"

namespace ArcSettingsModel_Internal
{
	static constexpr const TCHAR* LocalSlotName  = TEXT("LocalSettings");
	static constexpr const TCHAR* SharedSlotName = TEXT("SharedSettings");
}

// ============================================================
// USubsystem
// ============================================================

void UArcSettingsModel::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	OnModelReady.Broadcast(*this);

	LoadPersistedValues();
	RefreshAllViewModels();
}

void UArcSettingsModel::Deinitialize()
{
	Super::Deinitialize();

	ViewModels.Reset();
	SettingDescriptors.Reset();
	TagToIndex.Reset();
	CategoryOrder.Reset();
	EditConditions.Reset();
	OnChangedCallbacks.Reset();
	DiscreteGetters.Reset();
	DiscreteSetters.Reset();
	ScalarGetters.Reset();
	ScalarSetters.Reset();
	DynamicOptionsProviders.Reset();
}

// ============================================================
// Builder entry points
// ============================================================

FArcDiscreteSettingBuilder UArcSettingsModel::Discrete(FGameplayTag InSettingTag, FGameplayTag InCategoryTag)
{
	return FArcDiscreteSettingBuilder(*this, InSettingTag, InCategoryTag);
}

FArcScalarSettingBuilder UArcSettingsModel::Scalar(FGameplayTag InSettingTag, FGameplayTag InCategoryTag)
{
	return FArcScalarSettingBuilder(*this, InSettingTag, InCategoryTag);
}

FArcActionSettingBuilder UArcSettingsModel::Action(FGameplayTag InSettingTag, FGameplayTag InCategoryTag)
{
	return FArcActionSettingBuilder(*this, InSettingTag, InCategoryTag);
}

// ============================================================
// Registration
// ============================================================

void UArcSettingsModel::RegisterSetting(FInstancedStruct&& InDescriptor)
{
	const FArcSettingDescriptor* Base = InDescriptor.GetPtr<FArcSettingDescriptor>();
	if (!ensureAlwaysMsgf(Base, TEXT("UArcSettingsModel::RegisterSetting: descriptor does not derive from FArcSettingDescriptor")))
	{
		return;
	}

	const FGameplayTag SettingTag  = Base->SettingTag;
	const FGameplayTag CategoryTag = Base->CategoryTag;

	if (TagToIndex.Contains(SettingTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("UArcSettingsModel::RegisterSetting: setting '%s' already registered; ignoring duplicate."), *SettingTag.ToString());
		return;
	}

	const int32 Index = SettingDescriptors.Add(MoveTemp(InDescriptor));
	TagToIndex.Add(SettingTag, Index);

	// Track category order
	CategoryOrder.AddUnique(CategoryTag);

	// Retrieve the stored descriptor (InDescriptor was moved)
	const FInstancedStruct& StoredDescriptor = SettingDescriptors[Index];
	const FArcSettingDescriptor* StoredBase  = StoredDescriptor.GetPtr<FArcSettingDescriptor>();

	// Add property to appropriate PropertyBag for Local/Shared storage
	if (StoredBase->StorageType != EArcSettingStorage::Engine && StoredBase->PropertyName != NAME_None)
	{
		FInstancedPropertyBag& TargetBag = (StoredBase->StorageType == EArcSettingStorage::Local) ? LocalStore : SharedStore;

		// Determine value type from descriptor kind
		if (StoredDescriptor.GetScriptStruct() == FArcDiscreteSettingDescriptor::StaticStruct())
		{
			TargetBag.AddProperty(StoredBase->PropertyName, EPropertyBagPropertyType::Int32);
		}
		else if (StoredDescriptor.GetScriptStruct() == FArcScalarSettingDescriptor::StaticStruct())
		{
			TargetBag.AddProperty(StoredBase->PropertyName, EPropertyBagPropertyType::Double);
		}
		// Action settings do not persist a value
	}

	CreateViewModelForSetting(StoredDescriptor);
}

void UArcSettingsModel::RegisterEditCondition(FGameplayTag InTag, TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition)
{
	EditConditions.Add(InTag, MoveTemp(InCondition));
}

void UArcSettingsModel::RegisterOnChanged(FGameplayTag InTag, TFunction<void(UArcSettingsModel&, FGameplayTag)> InCallback)
{
	OnChangedCallbacks.Add(InTag, MoveTemp(InCallback));
}

void UArcSettingsModel::UnregisterOnChanged(FGameplayTag InTag)
{
	OnChangedCallbacks.Remove(InTag);
}

void UArcSettingsModel::UnregisterSetting(FGameplayTag InTag)
{
	const int32* IndexPtr = TagToIndex.Find(InTag);
	if (!IndexPtr)
	{
		return;
	}

	const int32 Index = *IndexPtr;
	SettingDescriptors.RemoveAt(Index);
	TagToIndex.Remove(InTag);
	ViewModels.Remove(InTag);
	EditConditions.Remove(InTag);
	OnChangedCallbacks.Remove(InTag);

	// Rebuild TagToIndex for entries after the removed one
	for (auto& Pair : TagToIndex)
	{
		if (Pair.Value > Index)
		{
			Pair.Value -= 1;
		}
	}
}

void UArcSettingsModel::RegisterDiscreteGetter(FGameplayTag InTag, TFunction<int32()> InGetter)
{
	DiscreteGetters.Add(InTag, MoveTemp(InGetter));
}

void UArcSettingsModel::RegisterDiscreteSetter(FGameplayTag InTag, TFunction<void(int32)> InSetter)
{
	DiscreteSetters.Add(InTag, MoveTemp(InSetter));
}

void UArcSettingsModel::RegisterScalarGetter(FGameplayTag InTag, TFunction<double()> InGetter)
{
	ScalarGetters.Add(InTag, MoveTemp(InGetter));
}

void UArcSettingsModel::RegisterScalarSetter(FGameplayTag InTag, TFunction<void(double)> InSetter)
{
	ScalarSetters.Add(InTag, MoveTemp(InSetter));
}

void UArcSettingsModel::RegisterDynamicOptionsProvider(FGameplayTag InTag, TFunction<TArray<FArcSettingOption>()> InProvider)
{
	DynamicOptionsProviders.Add(InTag, MoveTemp(InProvider));
}

// ============================================================
// Value access
// ============================================================

int32 UArcSettingsModel::GetDiscreteIndex(FGameplayTag InTag) const
{
	// Lambda getter takes priority over PropertyBag
	if (const TFunction<int32()>* Getter = DiscreteGetters.Find(InTag))
	{
		return (*Getter)();
	}

	const int32* IndexPtr = TagToIndex.Find(InTag);
	if (!IndexPtr)
	{
		return 0;
	}

	const FInstancedStruct& Descriptor = SettingDescriptors[*IndexPtr];
	const FArcSettingDescriptor* Base   = Descriptor.GetPtr<FArcSettingDescriptor>();
	if (!Base)
	{
		return 0;
	}

	if (Base->StorageType == EArcSettingStorage::Local)
	{
		auto Result = LocalStore.GetValueInt32(Base->PropertyName);
		return Result.HasValue() ? Result.GetValue() : 0;
	}
	else if (Base->StorageType == EArcSettingStorage::Shared)
	{
		auto Result = SharedStore.GetValueInt32(Base->PropertyName);
		return Result.HasValue() ? Result.GetValue() : 0;
	}
	else
	{
		// Engine storage — not directly supported for discrete index via UGameUserSettings
		// Callers should use property path access instead
		return 0;
	}
}

void UArcSettingsModel::SetDiscreteIndex(FGameplayTag InTag, int32 InIndex)
{
	if (ChangeDepth >= MaxChangeDepth)
	{
		return;
	}

	++ChangeDepth;
	ON_SCOPE_EXIT { --ChangeDepth; };

	// Lambda setter takes priority over PropertyBag
	if (const TFunction<void(int32)>* Setter = DiscreteSetters.Find(InTag))
	{
		(*Setter)(InIndex);
	}
	else
	{
		const int32* IndexPtr = TagToIndex.Find(InTag);
		if (!IndexPtr)
		{
			return;
		}

		const FInstancedStruct& Descriptor = SettingDescriptors[*IndexPtr];
		const FArcSettingDescriptor* Base   = Descriptor.GetPtr<FArcSettingDescriptor>();
		if (!Base)
		{
			return;
		}

		if (Base->StorageType == EArcSettingStorage::Local)
		{
			LocalStore.SetValueInt32(Base->PropertyName, InIndex);
		}
		else if (Base->StorageType == EArcSettingStorage::Shared)
		{
			SharedStore.SetValueInt32(Base->PropertyName, InIndex);
		}
	}

	// Fire registered on-changed callback only at top-level depth
	if (ChangeDepth == 1)
	{
		if (auto* Callback = OnChangedCallbacks.Find(InTag))
		{
			(*Callback)(*this, InTag);
		}
	}

	// Re-evaluate edit conditions for settings that depend on this one
	EvaluateEditConditions(InTag);

	RefreshViewModel(InTag);

	OnSettingChanged.Broadcast(InTag);
}

double UArcSettingsModel::GetScalarValue(FGameplayTag InTag) const
{
	// Lambda getter takes priority over PropertyBag
	if (const TFunction<double()>* Getter = ScalarGetters.Find(InTag))
	{
		return (*Getter)();
	}

	const int32* IndexPtr = TagToIndex.Find(InTag);
	if (!IndexPtr)
	{
		return 0.0;
	}

	const FInstancedStruct& Descriptor = SettingDescriptors[*IndexPtr];
	const FArcSettingDescriptor* Base   = Descriptor.GetPtr<FArcSettingDescriptor>();
	if (!Base)
	{
		return 0.0;
	}

	if (Base->StorageType == EArcSettingStorage::Local)
	{
		auto Result = LocalStore.GetValueDouble(Base->PropertyName);
		return Result.HasValue() ? Result.GetValue() : 0.0;
	}
	else if (Base->StorageType == EArcSettingStorage::Shared)
	{
		auto Result = SharedStore.GetValueDouble(Base->PropertyName);
		return Result.HasValue() ? Result.GetValue() : 0.0;
	}

	return 0.0;
}

void UArcSettingsModel::SetScalarValue(FGameplayTag InTag, double InValue)
{
	if (ChangeDepth >= MaxChangeDepth)
	{
		return;
	}

	++ChangeDepth;
	ON_SCOPE_EXIT { --ChangeDepth; };

	// Lambda setter takes priority over PropertyBag
	if (const TFunction<void(double)>* Setter = ScalarSetters.Find(InTag))
	{
		(*Setter)(InValue);
	}
	else
	{
		const int32* IndexPtr = TagToIndex.Find(InTag);
		if (!IndexPtr)
		{
			return;
		}

		const FInstancedStruct& Descriptor = SettingDescriptors[*IndexPtr];
		const FArcSettingDescriptor* Base   = Descriptor.GetPtr<FArcSettingDescriptor>();
		if (!Base)
		{
			return;
		}

		if (Base->StorageType == EArcSettingStorage::Local)
		{
			LocalStore.SetValueDouble(Base->PropertyName, InValue);
		}
		else if (Base->StorageType == EArcSettingStorage::Shared)
		{
			SharedStore.SetValueDouble(Base->PropertyName, InValue);
		}
	}

	// Fire registered on-changed callback only at top-level depth
	if (ChangeDepth == 1)
	{
		if (auto* Callback = OnChangedCallbacks.Find(InTag))
		{
			(*Callback)(*this, InTag);
		}
	}

	EvaluateEditConditions(InTag);

	RefreshViewModel(InTag);

	OnSettingChanged.Broadcast(InTag);
}

// ============================================================
// Queries
// ============================================================

TArray<FGameplayTag> UArcSettingsModel::GetSettingsForCategory(FGameplayTag InCategoryTag) const
{
	TArray<FGameplayTag> Result;
	for (const FInstancedStruct& Descriptor : SettingDescriptors)
	{
		if (const FArcSettingDescriptor* Base = Descriptor.GetPtr<FArcSettingDescriptor>())
		{
			if (Base->CategoryTag == InCategoryTag)
			{
				Result.Add(Base->SettingTag);
			}
		}
	}
	return Result;
}

const FInstancedStruct* UArcSettingsModel::GetDescriptor(FGameplayTag InTag) const
{
	const int32* IndexPtr = TagToIndex.Find(InTag);
	return IndexPtr ? &SettingDescriptors[*IndexPtr] : nullptr;
}

UMVVMViewModelBase* UArcSettingsModel::GetViewModel(FGameplayTag InTag) const
{
	if (const TObjectPtr<UMVVMViewModelBase>* Found = ViewModels.Find(InTag))
	{
		return Found->Get();
	}
	return nullptr;
}

EArcSettingVisibility UArcSettingsModel::GetVisibility(FGameplayTag InTag) const
{
	if (const TFunction<EArcSettingVisibility(const UArcSettingsModel&)>* Condition = EditConditions.Find(InTag))
	{
		return (*Condition)(*this);
	}
	return EArcSettingVisibility::Visible;
}

// ============================================================
// Dirty tracking / persistence
// ============================================================

void UArcSettingsModel::SnapshotCurrentState()
{
	LocalSnapshot  = LocalStore;
	SharedSnapshot = SharedStore;
}

void UArcSettingsModel::RestoreToInitial()
{
	LocalStore  = LocalSnapshot;
	SharedStore = SharedSnapshot;
	RefreshAllViewModels();
}

void UArcSettingsModel::ApplyChanges()
{
	SavePersistedValues();
}

bool UArcSettingsModel::HasPendingChanges() const
{
	// Compare current stores against snapshots via serialization round-trip
	// Simple heuristic: always return false until snapshot has been taken.
	// Full comparison can be added in a later task.
	return false;
}

void UArcSettingsModel::UpdateOptions(FGameplayTag InTag, TArray<FArcSettingOption> InOptions)
{
	const int32* IndexPtr = TagToIndex.Find(InTag);
	if (!IndexPtr)
	{
		return;
	}

	FInstancedStruct& StoredDescriptor = SettingDescriptors[*IndexPtr];
	FArcDiscreteSettingDescriptor* Desc = StoredDescriptor.GetMutablePtr<FArcDiscreteSettingDescriptor>();
	if (!Desc)
	{
		return;
	}

	Desc->Options = MoveTemp(InOptions);
	RefreshViewModel(InTag);
}

TArray<FArcSettingOption> UArcSettingsModel::GetOptionsForSetting(FGameplayTag InTag) const
{
	// Dynamic provider takes priority over static descriptor options
	if (const TFunction<TArray<FArcSettingOption>()>* Provider = DynamicOptionsProviders.Find(InTag))
	{
		return (*Provider)();
	}

	const int32* IndexPtr = TagToIndex.Find(InTag);
	if (!IndexPtr)
	{
		return {};
	}

	const FInstancedStruct& Descriptor = SettingDescriptors[*IndexPtr];
	const FArcDiscreteSettingDescriptor* Desc = Descriptor.GetPtr<FArcDiscreteSettingDescriptor>();
	if (!Desc)
	{
		return {};
	}

	return Desc->Options;
}

// ============================================================
// Private helpers
// ============================================================

void UArcSettingsModel::CreateViewModelForSetting(const FInstancedStruct& InDescriptor)
{
	const FArcSettingDescriptor* Base = InDescriptor.GetPtr<FArcSettingDescriptor>();
	if (!Base)
	{
		return;
	}

	UMVVMViewModelBase* NewVM = nullptr;

	if (InDescriptor.GetScriptStruct() == FArcDiscreteSettingDescriptor::StaticStruct())
	{
		UArcDiscreteSettingViewModel* VM = NewObject<UArcDiscreteSettingViewModel>(this);
		VM->InitializeWithTag(Base->SettingTag);
		NewVM = VM;
	}
	else if (InDescriptor.GetScriptStruct() == FArcScalarSettingDescriptor::StaticStruct())
	{
		UArcScalarSettingViewModel* VM = NewObject<UArcScalarSettingViewModel>(this);
		VM->InitializeWithTag(Base->SettingTag);
		NewVM = VM;
	}
	else if (InDescriptor.GetScriptStruct() == FArcActionSettingDescriptor::StaticStruct())
	{
		UArcActionSettingViewModel* VM = NewObject<UArcActionSettingViewModel>(this);
		VM->InitializeWithTag(Base->SettingTag);
		NewVM = VM;
	}

	if (NewVM)
	{
		ViewModels.Add(Base->SettingTag, NewVM);
	}
}

void UArcSettingsModel::RefreshViewModel(FGameplayTag InTag)
{
	UMVVMViewModelBase* VMBase = GetViewModel(InTag);
	if (!VMBase)
	{
		return;
	}

	const FInstancedStruct* Descriptor = GetDescriptor(InTag);
	if (!Descriptor)
	{
		return;
	}

	if (Descriptor->GetScriptStruct() == FArcDiscreteSettingDescriptor::StaticStruct())
	{
		if (UArcDiscreteSettingViewModel* VM = Cast<UArcDiscreteSettingViewModel>(VMBase))
		{
			VM->Refresh(*this);
		}
	}
	else if (Descriptor->GetScriptStruct() == FArcScalarSettingDescriptor::StaticStruct())
	{
		if (UArcScalarSettingViewModel* VM = Cast<UArcScalarSettingViewModel>(VMBase))
		{
			VM->Refresh(*this);
		}
	}
	else if (Descriptor->GetScriptStruct() == FArcActionSettingDescriptor::StaticStruct())
	{
		if (UArcActionSettingViewModel* VM = Cast<UArcActionSettingViewModel>(VMBase))
		{
			VM->Refresh(*this);
		}
	}
}

void UArcSettingsModel::RefreshAllViewModels()
{
	for (const FInstancedStruct& Descriptor : SettingDescriptors)
	{
		if (const FArcSettingDescriptor* Base = Descriptor.GetPtr<FArcSettingDescriptor>())
		{
			RefreshViewModel(Base->SettingTag);
		}
	}
}

void UArcSettingsModel::EvaluateEditConditions(FGameplayTag InChangedTag)
{
	// Re-evaluate conditions on all settings that declare a dependency on InChangedTag
	for (const FInstancedStruct& Descriptor : SettingDescriptors)
	{
		const FArcSettingDescriptor* Base = Descriptor.GetPtr<FArcSettingDescriptor>();
		if (!Base)
		{
			continue;
		}

		if (Base->DependsOn.Contains(InChangedTag))
		{
			OnSettingEditConditionChanged.Broadcast(Base->SettingTag);
		}
	}
}

void UArcSettingsModel::LoadPersistedValues()
{
	ArcSettingsStorage::LoadLocalStore(LocalStore, ArcSettingsModel_Internal::LocalSlotName);
	ArcSettingsStorage::LoadLocalStore(SharedStore, ArcSettingsModel_Internal::SharedSlotName);
}

void UArcSettingsModel::SavePersistedValues()
{
	ArcSettingsStorage::SaveLocalStore(LocalStore, ArcSettingsModel_Internal::LocalSlotName);
	ArcSettingsStorage::SaveLocalStore(SharedStore, ArcSettingsModel_Internal::SharedSlotName);

	if (UGameUserSettings* GameUserSettings = UGameUserSettings::GetGameUserSettings())
	{
		GameUserSettings->SaveSettings();
	}
}
