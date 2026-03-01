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

private:
	void OnSlotStateChanged(FArcAreaHandle AreaHandle, int32 SlotIndex, EArcAreaSlotState NewState);

	void PostVacancy(FArcAreaHandle AreaHandle, int32 SlotIndex);
	void RemoveVacancy(FArcAreaHandle AreaHandle, int32 SlotIndex);

	/** Maps each area slot to its posted knowledge handle. */
	TMap<FArcAreaSlotHandle, FArcKnowledgeHandle> PostedVacancies;

	FDelegateHandle DelegateHandle;
};
