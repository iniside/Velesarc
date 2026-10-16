// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "Mass/EntityElementTypes.h"

#include "ArcMassActionTrait.generated.h"

class UArcMassStatelessActionAsset;
class UArcMassStatefulActionAsset;

// ---------------------------------------------------------------------------
// Trait Entry — per-signal pair of asset references
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassActionTraitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Actions")
	TObjectPtr<UArcMassStatelessActionAsset> StatelessActions;

	UPROPERTY(EditAnywhere, Category = "Actions")
	TObjectPtr<UArcMassStatefulActionAsset> StatefulActions;
};

// ---------------------------------------------------------------------------
// Tag
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassActionTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Trait
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Actions", Category = "Actions"))
class ARCMASS_API UArcMassActionTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actions", meta = (ForceInlineRow))
	TMap<FName, FArcMassActionTraitEntry> ActionMap;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};

