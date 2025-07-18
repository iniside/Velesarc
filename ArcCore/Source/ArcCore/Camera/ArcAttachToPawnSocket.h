#pragma once

#include "Core/CameraNode.h"

#include "ArcAttachToPawnSocket.generated.h"

class UCameraValueInterpolator;

UCLASS(meta=(CameraNodeCategories="Attachment"))
class ARCCORE_API UArcAttachToPawnSocket : public UCameraNode
{
	GENERATED_BODY()

protected:

	// UCameraNode interface.
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;

public:
	UPROPERTY(EditAnywhere)
	FName SocketName;

	UPROPERTY(EditAnywhere)
	FName ComponentTag = NAME_None;
	
	UPROPERTY(EditAnywhere)
	bool bUsePawnBaseEyeHeight = false;

	UPROPERTY(EditAnywhere, Category="Collision")
	TObjectPtr<UCameraValueInterpolator> OffsetInterpolator;

	UPROPERTY(EditAnywhere, Category="Collision")
	TObjectPtr<UCameraValueInterpolator> OffsetInterpolatorZ;
};
