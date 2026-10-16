// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FArcTacticsEditorViewModel;
class IPropertyUtilities;
enum class EArcTacticsAuthoringChange : uint8;
struct FArcTacticsEditorSelection;

class FArcTacticsConfigDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(
		TWeakPtr<FArcTacticsEditorViewModel> InViewModel);
	virtual ~FArcTacticsConfigDetails() override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	explicit FArcTacticsConfigDetails(TWeakPtr<FArcTacticsEditorViewModel> InViewModel);
	void HandleSelectionChanged(const FArcTacticsEditorSelection& Selection);
	void HandleModelChanged(EArcTacticsAuthoringChange Change);
	void RequestRefresh() const;

	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle ModelChangedHandle;
};
