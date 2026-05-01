// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassDamageTrait.h"

#include "ArcMassDamageResistanceHandler.h"
#include "ArcMassDamageToHealthHandler.h"
#include "ArcMassDamageResistanceMappingAsset.h"
#include "ArcMassDamageTypes.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "MassEntityTemplateRegistry.h"
#include "StructUtils/InstancedStruct.h"

void UArcMassDamageTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcMassDamageModifiersFragment>();
	BuildContext.AddFragment<FArcMassDamageStatsFragment>();
}
