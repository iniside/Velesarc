// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "MassEntityTraitBase.h"
#include "ArcWorkAreaBoundsTrait.generated.h"

USTRUCT()
struct ARCMASS_API FArcWorkAreaBoundsFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Bounds")
	FVector Extent = FVector(500.0, 500.0, 200.0);
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Work Area Bounds", Category = "Spatial"))
class ARCMASS_API UArcWorkAreaBoundsTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	const FVector& GetExtent() const { return Bounds.Extent; }
	bool IsValid() const { return Bounds.Extent.GetMin() > 0; }

protected:
	UPROPERTY(EditAnywhere, Category = "Bounds", meta = (ShowOnlyInnerProperties))
	FArcWorkAreaBoundsFragment Bounds;
};
