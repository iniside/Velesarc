// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "ArcLinkableGuid.generated.h"

/** Fragment that gives a Mass entity a stable, editor-assigned GUID for cross-entity linking. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcLinkableGuidFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Linking")
	FGuid LinkGuid;
};

/** Trait that opts a Mass entity into the linking system by adding FArcLinkableGuidFragment. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Linkable GUID", Category = "Linking"))
class ARCMASS_API UArcLinkableGuidTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
