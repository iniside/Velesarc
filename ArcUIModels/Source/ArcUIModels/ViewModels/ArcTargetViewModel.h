// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ArcTargetViewModel.generated.h"

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcTargetViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetHasTarget, Getter = GetHasTarget)
	bool bHasTarget = false;

	void SetHasTarget(bool bInHasTarget);
	bool GetHasTarget() const { return bHasTarget; }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetMarkerX, Getter = GetMarkerX)
	float MarkerX = 0.f;

	void SetMarkerX(float InMarkerX);
	float GetMarkerX() const { return MarkerX; }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetMarkerY, Getter = GetMarkerY)
	float MarkerY = 0.f;

	void SetMarkerY(float InMarkerY);
	float GetMarkerY() const { return MarkerY; }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetMarkerZ, Getter = GetMarkerZ)
	float MarkerZ = 0.f;

	void SetMarkerZ(float InMarkerZ);
	float GetMarkerZ() const { return MarkerZ; }
};
