// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesModule.h"
#include "Effects/ArcEffectTagSource.h"

#define LOCTEXT_NAMESPACE "FArcMassAbilitiesModule"

void FArcMassAbilitiesModule::StartupModule()
{
	BuiltInTagSources.Add(MakeShared<FArcEffectTagSource_OwnedTags>());
	BuiltInTagSources.Add(MakeShared<FArcEffectTagSource_Definition>());
	BuiltInTagSources.Add(MakeShared<FArcEffectTagSource_Ability>());
	BuiltInTagSources.Add(MakeShared<FArcEffectTagSource_Origin>());

	for (const TSharedRef<const FArcEffectTagSource>& Source : BuiltInTagSources)
	{
		ArcEffectTagSources::Register(Source);
	}
}

void FArcMassAbilitiesModule::ShutdownModule()
{
	for (const TSharedRef<const FArcEffectTagSource>& Source : BuiltInTagSources)
	{
		ArcEffectTagSources::Unregister(Source);
	}
	BuiltInTagSources.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassAbilitiesModule, ArcMassAbilities)
