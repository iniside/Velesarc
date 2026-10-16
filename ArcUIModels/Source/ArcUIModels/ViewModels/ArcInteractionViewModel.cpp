// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcInteractionViewModel.h"

void UArcInteractionViewModel::SetHasTarget(bool bInHasTarget)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasTarget, bInHasTarget);
}

void UArcInteractionViewModel::SetAnchorX(float InAnchorX)
{
	UE_MVVM_SET_PROPERTY_VALUE(AnchorX, InAnchorX);
}

void UArcInteractionViewModel::SetAnchorY(float InAnchorY)
{
	UE_MVVM_SET_PROPERTY_VALUE(AnchorY, InAnchorY);
}

void UArcInteractionViewModel::SetAnchorZ(float InAnchorZ)
{
	UE_MVVM_SET_PROPERTY_VALUE(AnchorZ, InAnchorZ);
}
