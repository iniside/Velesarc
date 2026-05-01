// Copyright Lukasz Baran. All Rights Reserved.

#include "Faction/ArcFactionTrait.h"
#include "Faction/ArcFactionFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassSpawnerTypes.h"

void UArcFactionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    FArcFactionFragment& FactionFrag = BuildContext.AddFragment_GetRef<FArcFactionFragment>();
    FactionFrag.FactionName = FactionName;
    FactionFrag.FactionArchetypeTag = FactionArchetypeTag;
    FactionFrag.Archetype = Archetype;

    BuildContext.AddFragment_GetRef<FArcFactionDiplomacyFragment>();
    BuildContext.AddFragment_GetRef<FArcFactionSettlementsFragment>();
}
