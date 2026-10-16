// Copyright Lukasz Baran. All Rights Reserved.

#include "Ecotope/ArcEcotopeModel.h"

#if WITH_EDITOR

EDataValidationResult UArcEcotopeModel::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const TFunction<void(const FString&)> AddError = [&Context, &Result](const FString& Message) -> void
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	FText SoilPoolSettingsError;
	if (!SoilPoolSettings.IsValid(SoilPoolSettingsError))
	{
		AddError(FString::Printf(TEXT("SoilPoolSettings is invalid: %s"), *SoilPoolSettingsError.ToString()));
	}

	FText DisturbanceSettingsError;
	if (!DisturbanceSettings.IsValid(DisturbanceSettingsError))
	{
		AddError(FString::Printf(TEXT("DisturbanceSettings is invalid: %s"), *DisturbanceSettingsError.ToString()));
	}

	if (!FMath::IsFinite(MaximumCrownOccupancy) || MaximumCrownOccupancy <= 0.0)
	{
		AddError(TEXT("MaximumCrownOccupancy must be finite and strictly positive."));
	}

	if (!FMath::IsFinite(UnderstoryShade) || UnderstoryShade < 0.05 || UnderstoryShade > 4.0)
	{
		AddError(TEXT("UnderstoryShade must be finite and within [0.05, 4.0]."));
	}

	if (!FMath::IsFinite(GroundCoverDensity) || GroundCoverDensity < 0.0 || GroundCoverDensity > 4.0)
	{
		AddError(TEXT("GroundCoverDensity must be finite and within [0.0, 4.0]."));
	}

	if (!FMath::IsFinite(GroundCoverPatchiness) || GroundCoverPatchiness < 0.0 || GroundCoverPatchiness > 0.95)
	{
		AddError(TEXT("GroundCoverPatchiness must be finite and within [0.0, 0.95]."));
	}

	TSet<const UArcEcotopeSpeciesModel*> SeenSpecies;
	TSet<FGuid> SeenSpeciesIds;
	for (int32 SetupIndex = 0; SetupIndex < SpeciesSetups.Num(); ++SetupIndex)
	{
		const FArcEcotopeSpeciesSetup& Setup = SpeciesSetups[SetupIndex];
		if (!Setup.bEnabled)
		{
			continue;
		}

		const FString Prefix = FString::Printf(TEXT("SpeciesSetups[%d]"), SetupIndex);

		if (Setup.Species == nullptr)
		{
			AddError(Prefix + TEXT(".Species must be set."));
		}
		else
		{
			if (SeenSpecies.Contains(Setup.Species.Get()))
			{
				AddError(Prefix + TEXT(" duplicates an earlier species asset setup."));
			}
			SeenSpecies.Add(Setup.Species.Get());

			if (!Setup.Species->SpeciesId.IsValid())
			{
				AddError(Prefix + TEXT(".Species has no valid SpeciesId."));
			}
			else
			{
				if (SeenSpeciesIds.Contains(Setup.Species->SpeciesId))
				{
					AddError(Prefix + TEXT(" duplicates an earlier SpeciesId."));
				}
				SeenSpeciesIds.Add(Setup.Species->SpeciesId);
			}
		}

		if (!FMath::IsFinite(Setup.InitialDensityPerHectare) || Setup.InitialDensityPerHectare < 0.0f)
		{
			AddError(Prefix + TEXT(".InitialDensityPerHectare must be finite and non-negative."));
		}
		if (!FMath::IsFinite(Setup.RegionalArrivalPerHectarePerYear)
			|| Setup.RegionalArrivalPerHectarePerYear < 0.0f)
		{
			AddError(Prefix + TEXT(".RegionalArrivalPerHectarePerYear must be finite and non-negative."));
		}
	}

	return Result;
}

#endif // WITH_EDITOR
