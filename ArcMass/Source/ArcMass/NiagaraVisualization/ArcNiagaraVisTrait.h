// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcNiagaraVisFragments.h"
#include "ArcNiagaraVisTrait.generated.h"

/** Trait that adds Niagara particle visualization to a Mass entity.
 *  Configures a Niagara system to be spawned and driven by the entity's transform. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Niagara Visualization", Category = "Visualization"))
class ARCMASS_API UArcNiagaraVisTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization")
	FArcNiagaraVisConfigFragment NiagaraConfig;

	const FTransform& GetComponentTransform() const { return NiagaraConfig.ComponentTransform; }
	void SetComponentTransform(const FTransform& InTransform) { NiagaraConfig.ComponentTransform = InTransform; }
	const FArcNiagaraVisConfigFragment& GetNiagaraConfig() const { return NiagaraConfig; }
	bool IsValid() const { return !NiagaraConfig.NiagaraSystem.IsNull(); }

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
