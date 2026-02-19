// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"

#include "ArcThermalConditionProcessor.generated.h"

// ---------------------------------------------------------------------------
// Thermal Application Processor
//
// Handles application of: Burning, Chilled, Wet, Oiled
// Cross-condition interactions:
//   - Thermal Cascade (Fire vs Wet/Chilled, Cold vs Burning)
//   - Fuel Conversion (Oil + Fire = Ignition, Oil on Burning = Stoking)
//   - Flash Freeze (Cold on Wet target = x3 multiplier)
//   - Fire on Frozen (shatter ice)
//   - Cauterization (Fire removes Bleeding)
//   - Sterilization (Fire removes Disease)
//   - Evaporation generates Blind (steam)
// ---------------------------------------------------------------------------

UCLASS()
class ARCCONDITIONEFFECTS_API UArcThermalConditionProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcThermalConditionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
