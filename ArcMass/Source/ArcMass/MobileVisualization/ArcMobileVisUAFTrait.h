// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "ArcMobileVisUAFTrait.generated.h"

USTRUCT()
struct ARCMASS_API FArcMobileVisUAFAnimDesc : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "UAF")
	FName AnimSequenceVarName = TEXT("AnimSequence");

	UPROPERTY(EditAnywhere, Category = "UAF")
	FName AnimSequenceIndexVarName = TEXT("AnimSequenceIndex");

	UPROPERTY(EditAnywhere, Category = "UAF")
	FName PositionVarName = TEXT("Position");

	UPROPERTY(EditAnywhere, Category = "UAF")
	FName JustSwappedVarName = TEXT("JustSwapped");
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mobile Vis UAF", Category = "Visualization"))
class ARCMASS_API UArcMobileVisUAFTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "UAF")
	FArcMobileVisUAFAnimDesc AnimDesc;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
