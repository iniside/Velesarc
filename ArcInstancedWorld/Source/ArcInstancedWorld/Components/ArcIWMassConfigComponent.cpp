// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMassConfigComponent.h"
#include "ArcInstancedWorld/ArcIWSettings.h"

UArcIWMassConfigComponent::UArcIWMassConfigComponent()
{
	RuntimeGridName = UArcIWSettings::Get()->DefaultGridName;
}
