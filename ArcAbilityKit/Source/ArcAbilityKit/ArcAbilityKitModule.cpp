// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAbilityKitModule.h"
#include "Effects/ArcEffectTagSource.h"
#include "Sources/ArcEffectTagSource_SourceItem.h"

void FArcAbilityKitModule::StartupModule()
{
	const TSharedRef<const FArcEffectTagSource> Source = MakeShared<FArcEffectTagSource_SourceItem>();
	ArcEffectTagSources::Register(Source);
	SourceItemTagSource = Source;
}

void FArcAbilityKitModule::ShutdownModule()
{
	if (SourceItemTagSource.IsValid())
	{
		ArcEffectTagSources::Unregister(SourceItemTagSource.ToSharedRef());
		SourceItemTagSource.Reset();
	}
}

IMPLEMENT_MODULE(FArcAbilityKitModule, ArcAbilityKit)
