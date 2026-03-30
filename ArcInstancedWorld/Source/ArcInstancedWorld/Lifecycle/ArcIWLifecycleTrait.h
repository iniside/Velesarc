// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcIWLifecycleTypes.h"
#include "ArcIWLifecycleTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "ArcIW Lifecycle", Category = "ArcIW|Lifecycle"))
class ARCINSTANCEDWORLD_API UArcIWLifecycleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FArcLifecycleConfigFragment LifecycleConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	bool bAutoAdvance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle|Visuals")
	FArcIWLifecycleVisConfigFragment VisConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle|Persistence")
	bool bPersistLifecycle = true;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
