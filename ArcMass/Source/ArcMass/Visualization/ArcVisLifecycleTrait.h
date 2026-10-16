// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcMass/Visualization/ArcVisLifecycle.h"
#include "ArcVisLifecycleTrait.generated.h"

/** Trait that adds visualization lifecycle configuration to a Mass entity.
 *  Registers the canonical FArcLifecycleFragment + FArcLifecycleConfigFragment
 *  plus the new FArcLifecyclePhaseVisualsFragment so UArcVisLifecycleVisSwitchProcessor
 *  can swap meshes/materials at phase boundaries. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Vis Lifecycle", Category = "Visualization"))
class ARCMASS_API UArcVisLifecycleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Phase timing and terminal-phase configuration (canonical lifecycle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FArcLifecycleConfigFragment LifecycleConfig;

	/** Per-phase visual overrides (mesh / material swaps at each phase boundary). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle|Visuals")
	TArray<FArcVisLifecyclePhaseVisuals> PhaseVisuals;

	/** When true, the canonical UArcLifecycleTickProcessor advances phases automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	bool bAutoAdvance = true;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
