// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTargetingPreview.generated.h"

USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcPreviewShapeBase
{
	GENERATED_BODY()
	virtual ~FArcPreviewShapeBase() = default;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Preview Shape (Sphere)"))
struct ARCABILITYKIT_API FArcPreviewShape_Sphere : public FArcPreviewShapeBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Shape")
	float Radius = 100.f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UArcTargetingPreview : public UInterface
{
	GENERATED_BODY()
};

class ARCABILITYKIT_API IArcTargetingPreview
{
	GENERATED_BODY()

public:
	/** Called once after spawn. Shape is an FInstancedStruct wrapping a FArcPreviewShapeBase. */
	UFUNCTION(BlueprintNativeEvent, Category = "Arc|Targeting|Preview")
	void SetShape(const FInstancedStruct& Shape);
	virtual void SetShape_Implementation(const FInstancedStruct& /*Shape*/) {}

	/** Toggle visibility. Called with false at spawn, true on first valid update. */
	UFUNCTION(BlueprintNativeEvent, Category = "Arc|Targeting|Preview")
	void SetActive(bool bActive);
	virtual void SetActive_Implementation(bool /*bActive*/) {}
};
