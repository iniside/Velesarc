// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "InteractionTypes.h"
#include "ArcInteractionWidgetConfiguration.generated.h"

class UMVVMViewModelBase;
class UUserWidget;

USTRUCT(BlueprintType)
struct FArcInteractionTarget_WidgetConfiguration : public FInteractionTargetConfiguration
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftClassPtr<UUserWidget> DisplayWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftClassPtr<UMVVMViewModelBase> ViewModelClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	FName ViewModelName;
};
