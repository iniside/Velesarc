// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Decision/ArcTacticsConfig.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

#include "SArcTacticsBindings.generated.h"

class FArcTacticsEditorViewModel;

UCLASS(Transient)
class UArcTacticsBindingsViewOwner final : public UObject, public IPropertyBindingBindingCollectionOwner
{
	GENERATED_BODY()

public:
	void Initialize(UArcTacticsConfig* InConfig);
	void Rebuild();

	virtual bool GetBindingDataView(
		const FPropertyBindingBinding& InBinding,
		EBindingSide InSide,
		FPropertyBindingDataView& OutDataView) override;
	virtual void GetBindableStructs(
		FGuid InTargetStructID,
		TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const override;
	virtual bool GetBindableStructByID(
		FGuid InStructID,
		TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const override;
	virtual bool GetBindingDataViewByID(
		FGuid InStructID,
		FPropertyBindingDataView& OutDataView) const override;
	virtual FPropertyBindingBindingCollection* GetEditorPropertyBindings() override;
	virtual const FPropertyBindingBindingCollection* GetEditorPropertyBindings() const override;
	virtual bool CanCreateParameter(FGuid InStructID) const override;
	virtual void CreateParametersForStruct(
		FGuid InStructID,
		TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) override;

private:
	TWeakObjectPtr<UArcTacticsConfig> Config;
	FArcTacticsBindingCollection ProjectedBindings;
};

class SArcTacticsBindings final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_TwoParams(
		FOnBindingPathClicked,
		bool,
		const FPropertyBindingPath&);

	SLATE_BEGIN_ARGS(SArcTacticsBindings) {}
		SLATE_EVENT(FOnBindingPathClicked, OnBindingPathClicked)
	SLATE_END_ARGS()

	virtual ~SArcTacticsBindings() override;
	void Construct(
		const FArguments& InArgs,
		TSharedRef<FArcTacticsEditorViewModel> InViewModel);

private:
	void HandleModelChanged(EArcTacticsAuthoringChange Change);
	void HandleBindingPathClicked(bool bIsStruct, const FPropertyBindingPath& BindingPath) const;

	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	TStrongObjectPtr<UArcTacticsBindingsViewOwner> ViewOwner;
	FOnBindingPathClicked OnBindingPathClicked;
};
