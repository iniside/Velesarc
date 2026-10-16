// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IPropertyHandle;
class IPropertyUtilities;
class SWidget;
class UArcTQSQueryDefinition;

class FArcTQSPreviewSetupDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TSharedRef<SWidget> MakeSourceMenu(FGuid ParameterID) const;
	FText GetSourceText(FGuid ParameterID) const;
	void SetSource(FGuid ParameterID, FName MockName) const;
	void HandleDefinitionCompiled(const UArcTQSQueryDefinition& CompiledDefinition) const;

	TWeakObjectPtr<UArcTQSQueryDefinition> Definition;
	TSharedPtr<IPropertyHandle> SeedHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
