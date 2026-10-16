// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcBuilderDebugger.h"
#include "ArcGunDebugger.h"
#include "ArcGameplayTagTreeWidget.h"
#include "ArcAIDebugger.h"
#include "ArcPlanFeasibilityDebugger.h"
#include "ArcPerceptionDebugger.h"
#include "ArcMassEntityDebugger.h"
#include "ArcMassPhysicsDebugger.h"
#include "ArcPathDebugger.h"
#include "ArcVisEntityDebugger.h"
#include "ArcKnowledgeDebugger.h"
#include "ArcAreaDebugger.h"
#include "ArcLootTableDebugger.h"
#include "ArcLevelBrowserDebugger.h"
#include "ArcTraceDebugger.h"
#include "ArcMassProcessorDebugger.h"
#include "ArcCharacterPersistenceDebugger.h"
#include "ArcSpatialHashMinimapDebugger.h"
#include "ArcVisualizationMinimapDebugger.h"
#include "ArcMobileVisEntityDebugger.h"
#include "ArcNeedsDebugger.h"
#include "ArcWeatherMinimapDebugger.h"
#include "ArcInteractionDebugger.h"
#include "ArcWorldPersistenceDebugger.h"
#include "ArcTQSDebugger.h"
#include "ArcTacticsDebugger.h"
#include "ArcEconomyDebugger.h"
#include "ArcMassAbilitiesDebugger.h"
#include "ArcMassItemsDebugger.h"
#include "ArcTargetingDebugger.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcGameplayDebuggerSubsystem.generated.h"

extern TAutoConsoleVariable<bool> CVarArcDebugDraw;

/**
 * 
 */
UCLASS()
class ARCGAMEPLAYDEBUGGER_API UArcGameplayDebuggerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

private:
	bool bDrawDebug = false;

	FArcGunDebugger ArcGunDebugger;

	FArcBuilderDebugger BuilderDebugger;

	FArcGameplayTagTreeWidget GameplayTagTreeWidget;

	FArcMassEntityDebugger MassEntityDebugger;

	FArcMassPhysicsDebugger PhysicsDebugger;

	FArcAIDebugger AIDebugger;

	FArcPlanFeasibilityDebugger PlanFeasibilityDebugger;

	FArcPerceptionDebugger PerceptionDebugger;

	FArcPathDebugger PathDebugger;

	FArcTQSDebugger TQSDebugger;

	FArcVisEntityDebugger VisEntityDebugger;

	FArcKnowledgeDebugger KnowledgeDebugger;

	FArcAreaDebugger AreaDebugger;

	FArcLootTableDebugger LootTableDebugger;
	FArcCharacterPersistenceDebugger CharacterPersistenceDebugger;

	FArcSpatialHashMinimapDebugger SpatialHashMinimapDebugger;

	FArcVisualizationMinimapDebugger VisualizationMinimapDebugger;

	FArcMobileVisEntityDebugger MobileVisEntityDebugger;

	FArcNeedsDebugger NeedsDebugger;
	FArcTacticsDebugger TacticsDebugger;


	FArcWeatherMinimapDebugger WeatherMinimapDebugger;

	FArcInteractionDebugger InteractionDebugger;

	FArcWorldPersistenceDebugger WorldPersistenceDebugger;

	FArcEconomyDebugger EconomyDebugger;

	FArcMassAbilitiesDebugger MassAbilitiesDebugger;

	FArcMassItemsDebugger MassItemsDebugger;

	FArcTargetingDebugger TargetingDebugger;

	FArcLevelBrowserDebugger LevelBrowserDebugger;

	FArcTraceDebugger TraceDebugger;

	FArcMassProcessorDebugger MassProcessorDebugger;

	void Toggle();

	/** Smoothed screen-space position of the pawn overlay marker (reduces per-frame projection jitter). */
	FVector2D PawnOverlayScreenPos = FVector2D::ZeroVector;
	/** False until the pawn overlay has a valid position to smooth from (forces a snap on first draw / after teleport). */
	bool bPawnOverlayHasPos = false;

public:
	//TMap<FName, TFunction<void(const FArcItemData*, const FArcItemInstance*)>> ItemInstancesDraw;
	
	UArcGameplayDebuggerSubsystem();
	/** Implement this for initialization of instances of the system */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void OnCVarChanged(IConsoleVariable* CVar);
	void OnToggleGameInputChanged(IConsoleVariable* CVar);
	
	/** Implement this for deinitialization of instances of the system */
	virtual void Deinitialize() override;

	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}
	virtual bool IsTickable() const override final;
	
	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

	UFUNCTION(exec)
	void ToggleDebug();
};
