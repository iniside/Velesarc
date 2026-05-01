// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Faction/ArcFactionTypes.h"
#include "ArcFactionTrait.generated.h"

/**
 * Trait that adds faction fragments to a Mass entity template.
 * Configure faction name, archetype, and initial diplomacy in the entity config.
 */
UCLASS(meta = (DisplayName = "Arc Faction"))
class ARCECONOMY_API UArcFactionTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Faction")
    FName FactionName;

    UPROPERTY(EditAnywhere, Category = "Faction")
    FGameplayTag FactionArchetypeTag;

    UPROPERTY(EditAnywhere, Category = "Faction")
    EArcFactionArchetype Archetype = EArcFactionArchetype::Economic;

protected:
    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
