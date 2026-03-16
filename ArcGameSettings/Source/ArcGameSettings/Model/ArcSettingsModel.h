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

#pragma once

#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "Model/ArcSettingsTypes.h"

#include "ArcSettingsModel.generated.h"

struct FArcSettingOption;

class FArcDiscreteSettingBuilder;
class FArcScalarSettingBuilder;
class FArcActionSettingBuilder;
class UArcDiscreteSettingViewModel;
class UArcScalarSettingViewModel;
class UArcActionSettingViewModel;
class UMVVMViewModelBase;

// ---- Delegates ----

DECLARE_MULTICAST_DELEGATE_OneParam(FOnArcSettingsModelReady, UArcSettingsModel&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnArcSettingChanged, FGameplayTag);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnArcSettingEditConditionChanged, FGameplayTag);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnArcSettingAction, FGameplayTag, FGameplayTag);

/**
 * Game instance subsystem that owns all game setting descriptors, values, and viewmodels.
 * Settings are registered via the fluent builder API (Discrete/Scalar/Action).
 */
UCLASS()
class ARCGAMESETTINGS_API UArcSettingsModel : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ~Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End USubsystem interface

	// ---- Builder entry points ----

	/** Begin building a discrete (index-based) setting. */
	FArcDiscreteSettingBuilder Discrete(FGameplayTag InSettingTag, FGameplayTag InCategoryTag);

	/** Begin building a scalar (double-range) setting. */
	FArcScalarSettingBuilder Scalar(FGameplayTag InSettingTag, FGameplayTag InCategoryTag);

	/** Begin building an action (button) setting. */
	FArcActionSettingBuilder Action(FGameplayTag InSettingTag, FGameplayTag InCategoryTag);

	// ---- Registration (called by builders) ----

	void RegisterSetting(FInstancedStruct&& InDescriptor);

	void RegisterEditCondition(FGameplayTag InTag, TFunction<EArcSettingVisibility(const UArcSettingsModel&)> InCondition);

	void RegisterOnChanged(FGameplayTag InTag, TFunction<void(UArcSettingsModel&, FGameplayTag)> InCallback);
	void UnregisterOnChanged(FGameplayTag InTag);

	void UnregisterSetting(FGameplayTag InTag);

	void RegisterDiscreteGetter(FGameplayTag InTag, TFunction<int32()> InGetter);
	void RegisterDiscreteSetter(FGameplayTag InTag, TFunction<void(int32)> InSetter);
	void RegisterScalarGetter(FGameplayTag InTag, TFunction<double()> InGetter);
	void RegisterScalarSetter(FGameplayTag InTag, TFunction<void(double)> InSetter);
	void RegisterDynamicOptionsProvider(FGameplayTag InTag, TFunction<TArray<FArcSettingOption>()> InProvider);

	// ---- Value access ----

	int32 GetDiscreteIndex(FGameplayTag InTag) const;
	void SetDiscreteIndex(FGameplayTag InTag, int32 InIndex);

	double GetScalarValue(FGameplayTag InTag) const;
	void SetScalarValue(FGameplayTag InTag, double InValue);

	// ---- Queries ----

	const TArray<FGameplayTag>& GetCategoryTags() const { return CategoryOrder; }
	TArray<FGameplayTag> GetSettingsForCategory(FGameplayTag InCategoryTag) const;
	const FInstancedStruct* GetDescriptor(FGameplayTag InTag) const;
	UMVVMViewModelBase* GetViewModel(FGameplayTag InTag) const;
	EArcSettingVisibility GetVisibility(FGameplayTag InTag) const;

	// ---- Dirty tracking / persistence ----

	void SnapshotCurrentState();
	void ApplyChanges();
	void RestoreToInitial();
	bool HasPendingChanges() const;

	void UpdateOptions(FGameplayTag InTag, TArray<FArcSettingOption> InOptions);
	TArray<FArcSettingOption> GetOptionsForSetting(FGameplayTag InTag) const;

	// ---- Events ----

	FOnArcSettingsModelReady OnModelReady;
	FOnArcSettingChanged OnSettingChanged;
	FOnArcSettingEditConditionChanged OnSettingEditConditionChanged;
	FOnArcSettingAction OnActionExecuted;

private:
	void CreateViewModelForSetting(const FInstancedStruct& InDescriptor);
	void RefreshViewModel(FGameplayTag InTag);
	void RefreshAllViewModels();
	void EvaluateEditConditions(FGameplayTag InTag);
	void LoadPersistedValues();
	void SavePersistedValues();

	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UMVVMViewModelBase>> ViewModels;

	TArray<FInstancedStruct> SettingDescriptors;
	TMap<FGameplayTag, int32> TagToIndex;

	/** Unique ordered list of categories as settings were registered. */
	TArray<FGameplayTag> CategoryOrder;

	/** Per-tag edit condition callbacks (not UPROPERTY — raw TFunction). */
	TMap<FGameplayTag, TFunction<EArcSettingVisibility(const UArcSettingsModel&)>> EditConditions;

	/** Per-tag on-changed callbacks registered by external code. */
	TMap<FGameplayTag, TFunction<void(UArcSettingsModel&, FGameplayTag)>> OnChangedCallbacks;

	TMap<FGameplayTag, TFunction<int32()>> DiscreteGetters;
	TMap<FGameplayTag, TFunction<void(int32)>> DiscreteSetters;
	TMap<FGameplayTag, TFunction<double()>> ScalarGetters;
	TMap<FGameplayTag, TFunction<void(double)>> ScalarSetters;
	TMap<FGameplayTag, TFunction<TArray<FArcSettingOption>()>> DynamicOptionsProviders;

	FInstancedPropertyBag LocalStore;
	FInstancedPropertyBag SharedStore;
	FInstancedPropertyBag LocalSnapshot;
	FInstancedPropertyBag SharedSnapshot;

	int32 ChangeDepth = 0;
	static constexpr int32 MaxChangeDepth = 2;
};
