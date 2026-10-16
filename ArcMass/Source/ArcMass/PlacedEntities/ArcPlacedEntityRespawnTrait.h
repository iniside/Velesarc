// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcPlacedEntityRespawnTrait.generated.h"

/** Const-shared config carrying the respawn duration authored on the trait. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityRespawnConfigFragment : public FMassConstSharedFragment
{
    GENERATED_BODY()

    /** Seconds until a removed instance respawns. 0 = no auto-respawn (slot stays permanently removed). */
    UPROPERTY(EditAnywhere, Category = "Respawn", meta = (ClampMin = "0"))
    int32 RespawnTimeSeconds = 0;
};

class AArcPlacedEntityPartitionActor;

/** Per-entity fragment linking the entity back to the partition actor slot that spawned it. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityOwnerFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AArcPlacedEntityPartitionActor> OwnerPartitionActor;

    UPROPERTY()
    int32 EntryIndex = INDEX_NONE;

    UPROPERTY()
    int32 TransformIndex = INDEX_NONE;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories,
    meta = (DisplayName = "Placed Entity Respawn", Category = "PlacedEntities"))
class ARCMASS_API UArcPlacedEntityRespawnTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Respawn", meta = (ClampMin = "0"))
    int32 RespawnTimeSeconds = 60;

    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

template<>
struct TMassFragmentTraits<FArcPlacedEntityOwnerFragment> final
{
    enum
    {
        AuthorAcceptsItsNotTriviallyCopyable = true
    };
};
