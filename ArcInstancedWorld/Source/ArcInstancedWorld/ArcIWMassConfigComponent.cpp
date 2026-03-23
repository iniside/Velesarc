// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMassConfigComponent.h"
#include "ArcIWSettings.h"

UArcIWMassConfigComponent::UArcIWMassConfigComponent()
{
	RuntimeGridName = UArcIWSettings::Get()->DefaultGridName;
}
