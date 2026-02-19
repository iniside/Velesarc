#pragma once
#include "MassEntityHandle.h"
#include "SlateIMWidgetBase.h"

class ArcAIDebugger
{
public:
	
};

class FArcAIDebuggerWidget
{
public:
	void Draw();
	
	void DrawEntityList();
	void OnPieEnd();
	
	int32 CurrentlySelectedEntity = -1;
	TWeakObjectPtr<UWorld> World;
	
	FMassEntityHandle CurrentEntity;
	
	bool bRefreshEntityList = true;
	bool bShowSpatialHash = false;
	bool bShowPerception = false;
	
	bool bShowDynamicSlots = false;
	
	TArray<FMassEntityHandle> Entities;
	TArray<FVector> Locations;
	TArray<FString> EntityNames;
};

class FArcAIDebuggerWindowWidget : public FSlateIMWindowBase
{
public:
	FArcAIDebuggerWindowWidget(const TCHAR* Command, const TCHAR* CommandHelp)
		: FSlateIMWindowBase(TEXT("Arc AI Debugger"), FVector2f(700, 900), /* Always On Top */ false, Command, CommandHelp)
	{}
	
	virtual void DrawWindow(float DeltaTime) override;
	
public:
	FArcAIDebuggerWidget DebuggerWidget;
};