// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ArcInteractionViewModel.generated.h"

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcInteractionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetHasTarget, Getter = GetHasTarget)
	bool bHasTarget = false;

	void SetHasTarget(bool bInHasTarget);
	bool GetHasTarget() const { return bHasTarget; }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetAnchorX, Getter = GetAnchorX)
	float AnchorX = 0.f;

	void SetAnchorX(float InAnchorX);
	float GetAnchorX() const { return AnchorX; }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetAnchorY, Getter = GetAnchorY)
	float AnchorY = 0.f;

	void SetAnchorY(float InAnchorY);
	float GetAnchorY() const { return AnchorY; }

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetAnchorZ, Getter = GetAnchorZ)
	float AnchorZ = 0.f;

	void SetAnchorZ(float InAnchorZ);
	float GetAnchorZ() const { return AnchorZ; }
};
