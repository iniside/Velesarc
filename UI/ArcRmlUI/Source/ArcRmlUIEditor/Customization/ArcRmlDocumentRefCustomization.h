// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class FDragDropOperation;
class IPropertyHandle;
class SSearchableComboBox;
class SWidget;
struct FContentBrowserItem;

class FArcRmlDocumentRefCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	void RefreshOptions();
	FText GetCurrentText() const;
	TSharedRef<SWidget> MakeOptionWidget(TSharedPtr<FString> Option) const;
	void HandleSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	bool IsDropRecognized(TSharedPtr<FDragDropOperation> Operation) const;
	FReply HandleDropped(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent);
	void HandleBrowseClicked() const;
	void HandleClearClicked();
	bool IsEditable() const;
	bool CanBrowse() const;
	bool FindDraggedDocument(TSharedPtr<FDragDropOperation> Operation, FString& OutPackagePath) const;
	FContentBrowserItem FindCurrentItem() const;
	void SetPath(const FString& PackagePath);

	TSharedPtr<IPropertyHandle> PathHandle;
	TSharedPtr<SSearchableComboBox> ComboBox;
	TArray<TSharedPtr<FString>> Options;
};
