// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AArcTQSPreviewTargetActor;
class AArcTQSTestingActor;
class IPropertyUtilities;
class SWidget;
class UArcTQSQueryDefinition;

class FArcTQSTestingActorDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TSharedRef<SWidget> MakeSourceMenu(FGuid ParameterID) const;
	FText GetSourceText(FGuid ParameterID) const;
	void SetSource(FGuid ParameterID, TWeakObjectPtr<AArcTQSPreviewTargetActor> Target) const;
	void HandleDefinitionCompiled(const UArcTQSQueryDefinition& CompiledDefinition) const;
	void RequestRefresh() const;

	TWeakObjectPtr<AArcTQSTestingActor> TestingActor;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
