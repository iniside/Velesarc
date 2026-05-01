// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class UArcPlacedEntityProxyEditingObject;

/** Detail customization for UArcPlacedEntityProxyEditingObject.
 *  Displays editable fragment overrides as expandable categories. */
class FArcPlacedEntityProxyDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface

private:
	FArcPlacedEntityProxyDetails() = default;

	TWeakObjectPtr<UArcPlacedEntityProxyEditingObject> ProxyObject;
};
