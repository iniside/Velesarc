// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcNiagaraVisFragments.h"
#include "ArcNiagaraVisTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCMASS_API UArcNiagaraVisTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NiagaraVisualization")
	FArcNiagaraVisConfigFragment NiagaraConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
