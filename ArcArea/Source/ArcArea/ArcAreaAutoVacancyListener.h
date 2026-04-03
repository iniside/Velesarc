// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcAreaTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcAreaAutoVacancyListener.generated.h"

class UArcAreaSubsystem;
class UArcKnowledgeSubsystem;

/**
 * Listens to UArcAreaSubsystem::OnSlotStateChanged and automatically posts/removes
 * vacancy advertisements to ArcKnowledge for slots with bAutoPostVacancy enabled.
 *
 * This keeps the area subsystem decoupled from the knowledge system.
 * For manual vacancy control (e.g., a town manager), disable bAutoPostVacancy
 * and post advertisements directly to UArcKnowledgeSubsystem.
 */
UCLASS()
class ARCAREA_API UArcAreaAutoVacancyListener : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Look up the vacancy advertisement handle for a slot. Returns invalid handle if not found. */
	FArcKnowledgeHandle FindVacancyHandle(const FArcAreaSlotHandle& SlotHandle) const;

	/** Transfer a vacancy from PostedVacancies to ClaimedVacancies for the given entity.
	  * Called when a slot transitions to Assigned. Returns the handle (invalid if not found). */
	FArcKnowledgeHandle ClaimVacancy(const FArcAreaSlotHandle& SlotHandle, FMassEntityHandle Entity);

	/** Remove a claimed vacancy for an entity and remove the knowledge entry.
	  * Called when the advertisement is done (completed, cancelled, or slot unassigned). */
	void ReleaseClaimedVacancy(FMassEntityHandle Entity);

private:
	void OnSlotStateChanged(const FArcAreaSlotHandle& SlotHandle, EArcAreaSlotState NewState);

	void PostVacancy(const FArcAreaSlotHandle& SlotHandle);
	void RemoveVacancy(const FArcAreaSlotHandle& SlotHandle);

	/** Maps each area slot to its posted knowledge handle. */
	TMap<FArcAreaSlotHandle, FArcKnowledgeHandle> PostedVacancies;

	/** Vacancies that have been claimed by an assigned entity. Keyed by entity. */
	TMap<FMassEntityHandle, FArcKnowledgeHandle> ClaimedVacancies;

	FDelegateHandle DelegateHandle;
};
