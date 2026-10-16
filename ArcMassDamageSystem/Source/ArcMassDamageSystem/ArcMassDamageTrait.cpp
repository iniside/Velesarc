// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassDamageTrait.h"

#include "ArcAttributeSet_Damage.h"
#include "ArcAttributeSet_DamageModifiers.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "MassEntityTemplateRegistry.h"
#include "StructUtils/InstancedStruct.h"

void UArcMassDamageTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
}

void UArcMassDamageTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcAttributeSet_DamageModifiers>();
	Out.AddSparseFragment<FArcAttributeSet_Damage>();
}
