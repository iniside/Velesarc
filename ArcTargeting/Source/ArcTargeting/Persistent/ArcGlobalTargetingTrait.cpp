#include "Persistent/ArcGlobalTargetingTrait.h"

#include "ArcTargetingPreset.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityUtils.h"
#include "MassEntityTemplateRegistry.h"
#include "MassWorldUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGlobalTargetingTrait, Log, All);

void UArcGlobalTargetingTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FArcGlobalTargetingConfigFragment Config;
	TSet<FGameplayTag> AddedTags;
	for (const FArcGlobalTargetingConfigEntry& Entry : Entries)
	{
		if (!Entry.Tag.IsValid() || Entry.Preset == nullptr || Entry.MinInterval < 0.f)
		{
			UE_LOGF(LogArcGlobalTargetingTrait, Warning, "Ignoring invalid global targeting entry on %ls.", *GetPathName());
			continue;
		}
		if (AddedTags.Contains(Entry.Tag))
		{
			UE_LOGF(LogArcGlobalTargetingTrait, Warning, "Ignoring duplicate global targeting tag %ls on %ls.", *Entry.Tag.ToString(), *GetPathName());
			continue;
		}
		AddedTags.Add(Entry.Tag);
		Config.Entries.Add(Entry);
	}

	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddTag<FArcGlobalTargetingRegistrationTag>();
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
