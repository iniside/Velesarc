// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcRPGArmorConfig.h"

#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ArcRPGArmorConfig"

UArcRPGArmorConfig::UArcRPGArmorConfig()
{
	ArmorCurve.InitializeAs<FArcArmorCurve>();
}

const FArcArmorCurve* UArcRPGArmorConfig::GetArmorCurve() const
{
	return ArmorCurve.GetPtr<FArcArmorCurve>();
}

#if WITH_EDITOR
EDataValidationResult UArcRPGArmorConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!GetArmorCurve())
	{
		Context.AddWarning(LOCTEXT("NoCurve", "Armor Curve is not set; executions using this asset apply no armor."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
