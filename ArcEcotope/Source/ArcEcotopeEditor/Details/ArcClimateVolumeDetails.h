// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class AArcClimateVolume;
class IPropertyUtilities;

class FArcClimateVolumeDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	AArcClimateVolume* GetVolume() const;
	FReply BakeClimate();
	FReply ClearClimate();
	FReply ValidateClimate();
	void ReportFailure(const FText& Action, const FText& Error) const;
	void ReportSuccess(const FText& Message) const;
	void RefreshDetails() const;

	TWeakObjectPtr<AArcClimateVolume> SelectedVolume;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
