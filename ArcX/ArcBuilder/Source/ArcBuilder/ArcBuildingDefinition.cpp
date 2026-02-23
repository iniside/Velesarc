// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildingDefinition.h"

#include "MassEntityConfigAsset.h"
#include "ArcMass/ArcMassEntityVisualizationTrait.h"
#include "MassVisualizationTrait.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// -------------------------------------------------------------------
// Primary asset ID
// -------------------------------------------------------------------

FPrimaryAssetId UArcBuildingDefinition::GetPrimaryAssetId() const
{
	if (!BuildingId.IsValid())
	{
		return FPrimaryAssetId();
	}

	if (BuildingType.IsValid())
	{
		return FPrimaryAssetId(BuildingType, *BuildingId.ToString(EGuidFormats::DigitsWithHyphens));
	}

	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcBuilding")), *BuildingId.ToString(EGuidFormats::DigitsWithHyphens));
}

// -------------------------------------------------------------------
// GUID lifecycle
// -------------------------------------------------------------------

void UArcBuildingDefinition::PostInitProperties()
{
	Super::PostInitProperties();

	if (!BuildingId.IsValid())
	{
		BuildingId = FGuid::NewGuid();
	}
}

void UArcBuildingDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	if (SaveContext.IsCooking())
	{
		Super::PreSave(SaveContext);
		return;
	}

	if (!BuildingId.IsValid())
	{
		BuildingId = FGuid::NewGuid();
	}

	Super::PreSave(SaveContext);
}

void UArcBuildingDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (DuplicateMode == EDuplicateMode::Normal)
	{
		BuildingId = FGuid::NewGuid();
	}
}

void UArcBuildingDefinition::RegenerateBuildingId()
{
	BuildingId = FGuid::NewGuid();
	MarkPackageDirty();
}

// -------------------------------------------------------------------
// Actor class resolution
// -------------------------------------------------------------------

UClass* UArcBuildingDefinition::ResolvePreviewActorClass() const
{
	// Try the explicit ActorClass first.
	UClass* Result = ActorClass.LoadSynchronous();
	if (Result)
	{
		return Result;
	}

	// Fall back to MassEntityConfig visualization traits.
	if (MassEntityConfig)
	{
		// Try ArcMass visualization trait.
		if (const UArcEntityVisualizationTrait* ArcVisTrait =
			Cast<UArcEntityVisualizationTrait>(MassEntityConfig->FindTrait(UArcEntityVisualizationTrait::StaticClass())))
		{
			if (ArcVisTrait->VisualizationConfig.ActorClass)
			{
				return ArcVisTrait->VisualizationConfig.ActorClass;
			}
		}

		// Try engine MassVisualizationTrait (HighResTemplateActor).
		if (const UMassVisualizationTrait* EngineVisTrait =
			Cast<UMassVisualizationTrait>(MassEntityConfig->FindTrait(UMassVisualizationTrait::StaticClass())))
		{
			if (EngineVisTrait->HighResTemplateActor)
			{
				return EngineVisTrait->HighResTemplateActor;
			}
		}
	}

	return nullptr;
}

// -------------------------------------------------------------------
// Validation
// -------------------------------------------------------------------

#if WITH_EDITOR
EDataValidationResult UArcBuildingDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!BuildingId.IsValid())
	{
		Context.AddWarning(FText::FromString(TEXT("BuildingId is not valid. Save the asset to auto-generate one.")));
	}

	if (ActorClass.IsNull() && !MassEntityConfig)
	{
		Context.AddWarning(FText::FromString(TEXT("Neither ActorClass nor MassEntityConfig is set. Building cannot be spawned.")));
	}

	return Result;
}
#endif
