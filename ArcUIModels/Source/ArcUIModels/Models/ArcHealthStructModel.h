// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "Events/ArcMassSignalListener.h"
#include "Mass/EntityHandle.h"
#include "ArcHealthStructModel.generated.h"

struct FArcAttributeChangedFragment;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcHealthStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName HealthViewModelName = TEXT("Health");

	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

private:
	void OnAttributeChanged(
		FMassEntityHandle Entity,
		const FArcAttributeChangedFragment& Fragment,
		UArcFrontendMVVMModelComponent& Component);

	FArcMassSignalListener AttributeListener;
};
