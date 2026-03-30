// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FArcEntityVisualizationTraitCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
