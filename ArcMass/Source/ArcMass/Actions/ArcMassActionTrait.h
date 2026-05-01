// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "MassObserverProcessor.h"
#include "StructUtils/InstancedStruct.h"

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
// Config Fragment (Const Shared) — asset pointers only, no TArray<FInstancedStruct>
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassActionConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FArcMassActionTraitEntry> ActionMap;
};

template<>
struct TMassFragmentTraits<FArcMassActionConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// ---------------------------------------------------------------------------
// Trait
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Actions", Category = "Actions"))
class ARCMASS_API UArcMassActionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actions", meta = (ForceInlineRow))
	TMap<FName, FArcMassActionTraitEntry> ActionMap;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

// ---------------------------------------------------------------------------
// Init Observer
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassActionInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassActionInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
