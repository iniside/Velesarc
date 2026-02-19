// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAIInfluenceGridProcessors.h"

UArcInfluenceMappingProcessor_Small::UArcInfluenceMappingProcessor_Small()
	: Super{}
{
	GridIndex = 0;
	bAutoRegisterWithProcessingPhases = true;
	bAllowMultipleInstances = true;
}

void UArcInfluenceMappingProcessor_Small::AddGridTagRequirement(FMassEntityQuery& Query)
{
	Query.AddTagRequirement<FArcInfluenceGridTag_Small>(EMassFragmentPresence::All);
}
