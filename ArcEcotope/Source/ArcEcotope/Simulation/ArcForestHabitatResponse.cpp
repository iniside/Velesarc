// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestHabitatResponse.h"

#include "Simulation/ArcEcotopeSiteChunkReader.h"

namespace ArcForestHabitatResponsePrivate
{
	constexpr double SlopeThresholdBandDeg = 5.0;
	constexpr double UnitScaleThresholdBand = 0.05;
	constexpr double SlopeDomainMaximumDeg = 90.0;
	constexpr double UnitDomainMinimum = 0.0;
	constexpr double UnitDomainMaximum = 1.0;

	double MakeFloorMask(const double Value, const double Floor, const double Band, const double DomainMinimum)
	{
		if (Floor <= DomainMinimum)
		{
			return 1.0;
		}
		return Value <= Floor || !FMath::IsFinite(Value)
			? 0.0
			: FMath::SmoothStep(Floor, Floor + Band, Value);
	}

	double MakeCeilingMask(const double Value, const double Ceiling, const double Band, const double DomainMaximum)
	{
		if (Ceiling >= DomainMaximum)
		{
			return 1.0;
		}
		return Value >= Ceiling || !FMath::IsFinite(Value)
			? 0.0
			: 1.0 - FMath::SmoothStep(Ceiling - Band, Ceiling, Value);
	}

	double MakeSiteTerm(
		const double MoistureFactor,
		const double TemperatureFactor,
		const double ThresholdMask)
	{
		const double SoftProduct = MoistureFactor * TemperatureFactor;
		return SoftProduct > 0.0 ? FMath::Sqrt(SoftProduct) * ThresholdMask : 0.0;
	}
}

namespace UE::ArcForest::Habitat
{
	double EvaluateResponse(const FArcForestFrozenResponseRange& Response, const float Value)
	{
		if (!FMath::IsFinite(Value) || Value < Response.Minimum || Value > Response.Maximum)
		{
			return 0.0;
		}
		if (Value == Response.Optimum)
		{
			return 1.0;
		}
		if (Value < Response.Optimum)
		{
			return Response.Minimum == Response.Optimum
				? 0.0
				: static_cast<double>(Value - Response.Minimum)
					/ static_cast<double>(Response.Optimum - Response.Minimum);
		}
		return Response.Optimum == Response.Maximum
			? 0.0
			: static_cast<double>(Response.Maximum - Value)
				/ static_cast<double>(Response.Maximum - Response.Optimum);
	}

	bool TryReadHabitatSiteSample(
		const FArcEcotopeSiteChunkView& View,
		const int32 GlobalBaseX,
		const int32 GlobalBaseY,
		FHabitatSiteSample& OutSample)
	{
		const auto Read = [&View, GlobalBaseX, GlobalBaseY](const EArcEcotopeSiteField Field, float& OutValue)
		{
			const FArcEcotopeSiteFloatFieldView* FieldView = View.FindFloatField(Field);
			if (FieldView == nullptr || FieldView->Level >= 31)
			{
				return false;
			}
			const int32 FieldX = GlobalBaseX >> FieldView->Level;
			const int32 FieldY = GlobalBaseY >> FieldView->Level;
			const float* Value = FieldView->TryGet(FieldX, FieldY);
			if (Value == nullptr)
			{
				return false;
			}
			OutValue = *Value;
			return true;
		};

		FHabitatSiteSample Candidate;
		if (!Read(EArcEcotopeSiteField::Slope, Candidate.Slope)
			|| !Read(EArcEcotopeSiteField::Moisture, Candidate.Moisture)
			|| !Read(EArcEcotopeSiteField::Fertility, Candidate.RawFertility)
			|| !Read(EArcEcotopeSiteField::SunLoad, Candidate.SunLoad)
			|| !Read(EArcEcotopeSiteField::WindExposure, Candidate.WindExposure)
			|| !Read(EArcEcotopeSiteField::Temperature, Candidate.Temperature)
			|| !Read(EArcEcotopeSiteField::ModificationCut, Candidate.ModificationCut)
			|| !Read(EArcEcotopeSiteField::BareRock, Candidate.BareRock)
			|| !Read(EArcEcotopeSiteField::ChannelBuffer, Candidate.ChannelBuffer))
		{
			return false;
		}
		OutSample = Candidate;
		return true;
	}

	void GetHabitatResponses(
		const FArcForestFrozenSpeciesSetup& Model,
		const FArcForestFrozenResponseRange* (&OutResponses)[HabitatResponseCount])
	{
		OutResponses[0] = &Model.LightResponse;
		OutResponses[1] = &Model.MoistureResponse;
		OutResponses[2] = &Model.TemperatureResponse;
	}

	double EvaluateHabitatThresholdMask(
		const FHabitatSiteSample& Sample,
		const FArcForestFrozenSpeciesSetup& Model,
		EHabitatHardZeroReason* OutZeroReason)
	{
		using ArcForestHabitatResponsePrivate::MakeCeilingMask;
		using ArcForestHabitatResponsePrivate::MakeFloorMask;
		using ArcForestHabitatResponsePrivate::SlopeDomainMaximumDeg;
		using ArcForestHabitatResponsePrivate::SlopeThresholdBandDeg;
		using ArcForestHabitatResponsePrivate::UnitDomainMaximum;
		using ArcForestHabitatResponsePrivate::UnitDomainMinimum;
		using ArcForestHabitatResponsePrivate::UnitScaleThresholdBand;

		const double SlopeMask = MakeCeilingMask(
			static_cast<double>(Sample.Slope),
			static_cast<double>(Model.MaxSlopeDeg),
			SlopeThresholdBandDeg,
			SlopeDomainMaximumDeg);
		const double SunLoadMask = MakeFloorMask(
			static_cast<double>(Sample.SunLoad),
			static_cast<double>(Model.SunLoadMin),
			UnitScaleThresholdBand,
			UnitDomainMinimum);
		const double WindMask = MakeCeilingMask(
			static_cast<double>(Sample.WindExposure),
			static_cast<double>(Model.WindExposureMax),
			UnitScaleThresholdBand,
			UnitDomainMaximum);
		const double FertilityMask = MakeFloorMask(
			static_cast<double>(Sample.RawFertility),
			static_cast<double>(Model.FertilityMin),
			UnitScaleThresholdBand,
			UnitDomainMinimum);

		if (OutZeroReason != nullptr)
		{
			*OutZeroReason = SlopeMask <= 0.0 ? EHabitatHardZeroReason::Slope
				: SunLoadMask <= 0.0 ? EHabitatHardZeroReason::SunLoad
				: WindMask <= 0.0 ? EHabitatHardZeroReason::Wind
				: FertilityMask <= 0.0 ? EHabitatHardZeroReason::Fertility
				: EHabitatHardZeroReason::None;
		}

		return SlopeMask * SunLoadMask * WindMask * FertilityMask;
	}

	void EvaluateHabitatResponseTerms(
		const FHabitatSiteSample& Sample,
		const FHabitatResponseTable& Responses,
		const double Light,
		double (&OutOutputs)[HabitatResponseCount])
	{
		OutOutputs[0] = EvaluateResponse(*Responses.Responses[0], static_cast<float>(Light));
		OutOutputs[1] = EvaluateResponse(*Responses.Responses[1], Sample.Moisture);
		OutOutputs[2] = EvaluateResponse(*Responses.Responses[2], Sample.Temperature);
	}

	double AggregateHabitat(
		const double (&Outputs)[HabitatResponseCount],
		const double ThresholdMask)
	{
		return Outputs[0] * ArcForestHabitatResponsePrivate::MakeSiteTerm(
			Outputs[1], Outputs[2], ThresholdMask);
	}

	void BuildStaticHabitatFactor(
		const FHabitatSiteSample& Sample,
		const FHabitatResponseTable& Responses,
		const FArcForestFrozenSpeciesSetup& Model,
		FHabitatStaticFactor& OutFactor)
	{
		double Outputs[HabitatResponseCount];
		EvaluateHabitatResponseTerms(Sample, Responses, 1.0, Outputs);
		EHabitatHardZeroReason ThresholdZeroReason = EHabitatHardZeroReason::None;
		const double ThresholdMask = EvaluateHabitatThresholdMask(Sample, Model, &ThresholdZeroReason);
		const double SiteHabitat = ArcForestHabitatResponsePrivate::MakeSiteTerm(
			Outputs[1], Outputs[2], ThresholdMask);
		OutFactor.SiteHabitat = static_cast<float>(SiteHabitat);
		OutFactor.bHardZero = !(SiteHabitat > 0.0);
		OutFactor.HardZeroReason = ThresholdZeroReason != EHabitatHardZeroReason::None
			? ThresholdZeroReason
			: Outputs[2] <= 0.0
				? EHabitatHardZeroReason::Temperature
				: Outputs[1] <= 0.0
					? EHabitatHardZeroReason::Moisture
					: EHabitatHardZeroReason::None;
	}
}
