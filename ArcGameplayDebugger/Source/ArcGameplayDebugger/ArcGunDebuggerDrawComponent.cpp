// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGunDebuggerDrawComponent.h"

void UArcGunDebuggerDrawComponent::UpdateGunDebug(const FArcGunDebugDrawData& InData)
{
	Data = InData;
	SetDrawLabels(false);
	RefreshShapes();
}

void UArcGunDebuggerDrawComponent::CollectShapes()
{
	if (Data.bDrawTargeting)
	{
		StoredLines.Emplace(Data.TraceStart, Data.TraceEnd, Data.TargetingColor, Data.TargetingSize);
	}
	if (Data.bDrawCameraDirection)
	{
		FVector EndPoint = Data.EyeLocation + Data.EyeDirection * Data.CameraDirectionDrawDistance;
		StoredLines.Emplace(Data.EyeLocation, EndPoint, Data.CameraDirectionColor, Data.CameraDirectionDrawSize);
	}
	if (Data.bDrawCameraPoint)
	{
		StoredPoints.Emplace(Data.CameraPointLocation, Data.CameraPointDrawSize, Data.CameraPointColor);
	}
}
