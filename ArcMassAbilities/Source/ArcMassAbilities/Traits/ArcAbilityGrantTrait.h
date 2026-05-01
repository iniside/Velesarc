// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcAbilityGrantTrait.generated.h"

class UArcMassAbilitySet;

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityGrantSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UArcMassAbilitySet>> AbilitySets;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Abilities", Category = "Abilities"))
class ARCMASSABILITIES_API UArcAbilityGrantTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TObjectPtr<UArcMassAbilitySet>> AbilitySets;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
