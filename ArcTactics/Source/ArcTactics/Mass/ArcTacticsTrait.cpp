// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsTrait.h"

#include "ArcTacticsModule.h"
#include "Decision/ArcTacticsConfig.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/EntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "MassWorldUtils.h"

void UArcTacticsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcTacticsTag>();
	BuildContext.AddFragment<FArcTacticsInstanceFragment>();
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	FArcTacticsConfigFragment ConfigFragment;
	ConfigFragment.Configs.Reserve(Configs.Num());
	for (const TObjectPtr<UArcTacticsConfig>& Config : Configs)
	{
		ConfigFragment.Configs.Add(Config);
	}
	ConfigFragment.Settings = Settings;
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(ConfigFragment));
}

bool UArcTacticsTrait::ValidateTemplate(
	const FMassEntityTemplateBuildContext&,
	const UWorld&,
	FAdditionalTraitRequirements&) const
{
	if (!Settings.IsValid())
	{
		UE_LOGF(LogArcTactics, Error, "UArcTacticsTrait on '%ls': invalid decision Settings.", *GetPathNameSafe(GetOuter()));
		return false;
	}
	for (const TObjectPtr<UArcTacticsConfig>& Config : Configs)
	{
		if (!Config || !Config->CompiledData.IsExecutable())
		{
			return false;
		}
	}
	return true;
}
