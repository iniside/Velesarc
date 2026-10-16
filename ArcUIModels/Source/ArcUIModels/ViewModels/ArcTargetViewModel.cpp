// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcTargetViewModel.h"

void UArcTargetViewModel::SetHasTarget(bool bInHasTarget)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasTarget, bInHasTarget);
}

void UArcTargetViewModel::SetMarkerX(float InMarkerX)
{
	UE_MVVM_SET_PROPERTY_VALUE(MarkerX, InMarkerX);
}

void UArcTargetViewModel::SetMarkerY(float InMarkerY)
{
	UE_MVVM_SET_PROPERTY_VALUE(MarkerY, InMarkerY);
}

void UArcTargetViewModel::SetMarkerZ(float InMarkerZ)
{
	UE_MVVM_SET_PROPERTY_VALUE(MarkerZ, InMarkerZ);
}
