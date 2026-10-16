// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRiverGeneratorComponent.h"

#include "ArcRiverModule.h"

FArcRiverGenerateDelegate GArcRiverCreateWaterZone;
FArcRiverGenerateDelegate GArcRiverGenerate;
FArcRiverGenerateDelegate GArcRiverClear;
FArcRiverGenerateDelegate GArcRiverClearAll;

const FName ArcRiver::GeneratedTag(TEXT("ArcRiverGenerated"));

namespace ArcRiverGeneratorComponentPrivate
{
	void RunAction(FArcRiverGenerateDelegate& Delegate, UArcRiverGeneratorComponent& Component, const TCHAR* Action)
	{
		if (!Delegate.IsBound())
		{
			UE_LOGF(LogArcRiver, Log,"%ls: no generator bound.", Action);
			return;
		}

		FText Error;
		if (!Delegate.Execute(Component, Error))
		{
			UE_LOGF(LogArcRiver, Error,"%ls failed: %ls", Action, *Error.ToString());
		}
	}
}

void UArcRiverGeneratorComponent::CreateWaterZone()
{
	ArcRiverGeneratorComponentPrivate::RunAction(GArcRiverCreateWaterZone, *this, TEXT("CreateWaterZone"));
}

void UArcRiverGeneratorComponent::GenerateRivers()
{
	ArcRiverGeneratorComponentPrivate::RunAction(GArcRiverGenerate, *this, TEXT("GenerateRivers"));
}

void UArcRiverGeneratorComponent::ClearRivers()
{
	ArcRiverGeneratorComponentPrivate::RunAction(GArcRiverClear, *this, TEXT("ClearRivers"));
}

void UArcRiverGeneratorComponent::ClearAllRivers()
{
	ArcRiverGeneratorComponentPrivate::RunAction(GArcRiverClearAll, *this, TEXT("ClearAllRivers"));
}

FGuid UArcRiverGeneratorComponent::GetOrCreateGeneratorId()
{
	if (!GeneratorId.IsValid())
	{
		Modify();
		GeneratorId = FGuid::NewGuid();
	}
	return GeneratorId;
}
