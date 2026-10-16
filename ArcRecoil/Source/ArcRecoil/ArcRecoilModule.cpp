// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilModule.h"

#include "Debug/ArcRecoilDebugScreen.h"

DEFINE_LOG_CATEGORY(LogArcRecoil);

TAutoConsoleVariable<int32> CVarArcRecoilLogEntity(
	TEXT("arc.Recoil.LogEntity"),
	-1,
	TEXT("Entity index to log ArcRecoil per-frame state for. -1 disables."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarArcRecoilDebug(
	TEXT("arc.Recoil.Debug"),
	0,
	TEXT("1 = draw all ArcRecoil debug: world-space aim/spread geometry plus the screen-space impact overlay. 0 = off."),
	ECVF_Default);

void FArcRecoilModule::StartupModule()
{
	if (ArcRecoil::DebugScreen::Register())
	{
		UE_LOGF(LogArcRecoil, Log,"DebugScreen: registered with UDebugDrawService on show flag '%ls'; enable with arc.Recoil.Debug 1.",
			ArcRecoil::DebugScreen::ShowFlagName());
	}
	else
	{
		UE_LOGF(LogArcRecoil, Warning,"DebugScreen: UDebugDrawService::Register('%ls') returned an invalid handle - the show flag did not resolve, so arc.Recoil.Debug can never draw the overlay.",
			ArcRecoil::DebugScreen::ShowFlagName());
	}
}

void FArcRecoilModule::ShutdownModule()
{
	ArcRecoil::DebugScreen::Unregister();
}

IMPLEMENT_MODULE(FArcRecoilModule, ArcRecoil)
