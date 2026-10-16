// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassGameFrameworkTestTypes.h"

#include "MassEntityTemplateRegistry.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"

void UArcMassActorBridgeTestTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassActorFragment>();
	BuildContext.AddFragment<FTransformFragment>();
}

int32 FArcMassTestPawnInitFragment::InitCount = 0;
int32 FArcMassTestPawnInitFragment::LastInitOrderIndex = -1;
int32 FArcMassTestPawnInitFragment::NextOrderCounter = 0;

int32 FArcMassTestAddPawnComponent::GiveCount = 0;
int32 FArcMassTestAddPawnComponent::LastGiveOrderIndex = -1;
