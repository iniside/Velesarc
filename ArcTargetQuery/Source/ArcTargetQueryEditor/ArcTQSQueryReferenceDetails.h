// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class UArcTQSQueryDefinition;
struct FArcTQSQueryReference;

class FArcTQSQueryReferenceDetails final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static TSharedRef<IPropertyTypeCustomization> MakeFixedDefinitionInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		FDetailWidgetRow& InHeaderRow,
		IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		IDetailChildrenBuilder& InChildrenBuilder,
		IPropertyTypeCustomizationUtils& InCustomizationUtils) override;

	static bool RequiresParametersSync(const FArcTQSQueryReference& Reference);

private:
	void HandleDefinitionCompiled(const UArcTQSQueryDefinition& Definition) const;
	void SyncParameters(const UArcTQSQueryDefinition* DefinitionToSync = nullptr) const;

	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> ParametersHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	bool bFixedDefinition = false;
};
