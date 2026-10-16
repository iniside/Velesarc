// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class AArcEcotopeSamplerVolume;

class FArcEcotopeSamplerVolumeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	AArcEcotopeSamplerVolume* GetVolume() const;

	FReply BakeSite();
	FReply SampleForest();
	FReply RebuildInstances();
	FReply ClearState();

	FText GetLastErrorText() const;
	void SetLastError(const FText& Error) const;
	void ClearLastError() const;

	TWeakObjectPtr<AArcEcotopeSamplerVolume> SelectedVolume;
};
