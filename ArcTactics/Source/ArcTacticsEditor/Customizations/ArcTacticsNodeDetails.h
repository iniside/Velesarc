// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTacticsEditorClipboard.h"
#include "Decision/ArcTacticsNode.h"
#include "IPropertyTypeCustomization.h"

class FArcTacticsEditorViewModel;
class IDetailCustomNodeBuilder;
class IPropertyHandle;
class IPropertyUtilities;
struct FSlateBrush;
struct FSlateColor;

namespace ArcTacticsNodeDetails
{
	ARCTACTICSEDITOR_API TSharedRef<IDetailCustomNodeBuilder> MakeParameterBagDetails(
		TSharedPtr<IPropertyHandle> BagHandle,
		TSharedPtr<IPropertyUtilities> PropertyUtilities,
		const FGuid& ID);
	ARCTACTICSEDITOR_API void StampBindingScope(
		const TSharedPtr<IPropertyHandle>& Handle,
		const FGuid& ID,
		EArcTacticsPropertyRole Role = EArcTacticsPropertyRole::Invalid);
	ARCTACTICSEDITOR_API void StampBindingScopeChildren(
		const TSharedPtr<IPropertyHandle>& Handle,
		const FGuid& ID,
		EArcTacticsPropertyRole Role = EArcTacticsPropertyRole::Invalid);
}

class FArcTacticsNodeDetails final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(
		TWeakPtr<FArcTacticsEditorViewModel> InViewModel);
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	explicit FArcTacticsNodeDetails(TWeakPtr<FArcTacticsEditorViewModel> InViewModel);
	FText GetTypeText() const;
	FText GetDescription() const;
	FText GetTooltip() const;
	const FSlateBrush* GetIcon() const;
	FSlateColor GetIconColor() const;

	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	FArcTacticsEditorSelection Item;
};
