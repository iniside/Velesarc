// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaModule.h"

DEFINE_LOG_CATEGORY(LogArcAreaDiag);

#define LOCTEXT_NAMESPACE "FArcAreaModule"

void FArcAreaModule::StartupModule()
{
}

void FArcAreaModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcAreaModule, ArcArea)
