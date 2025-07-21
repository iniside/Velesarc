/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "ArcItemDebuggerCreateItemSpec.h"
#include "ArcItemDebuggerItems.h"
#include "SlateIM.h"
#include "SlateIMWidgetBase.h"
#include "Items/ArcItemSpec.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/Anchors.h"

struct FArcItemInstance;
struct FArcItemData;

struct FArcItemFragment_ItemStats;
class FArcDebuggerItemStatsEditorWidget
{
	
public:
	void Draw(FArcItemFragment_ItemStats* InStats);

	float StatValue = 0.0f;
	int32 SelectedAttributeIdx = INDEX_NONE;
};

class FArcDebuggerWidgetMenu
{
	
public:
	void Draw();
	void Initialize();

	void Uninitialize();

	FArcItemDebuggerItems ItemsDebugger;
	FArcItemDebuggerCreateItemSpec ItemSpecEditor;
	FArcDebuggerItemStatsEditorWidget ItemStatseditor;
	
	FString ItemMakerFilterText;
	FArcItemSpec TempNewSpec;
};

class FArcSlateIMWindowBase : public FSlateIMWidgetWithCommandBase
{
public:
	FArcSlateIMWindowBase(FStringView WindowTitle, FVector2f WindowSize, const TCHAR* Command, const TCHAR* CommandHelp)
		: FSlateIMWidgetWithCommandBase(Command, CommandHelp)
		, WindowTitle(WindowTitle)
		, WindowSize(WindowSize)
	{}

protected:

	virtual void PreDraw(float DeltaTime) {}
	virtual void DrawWindow(float DeltaTime) = 0;
	virtual void PostDraw(float DeltaTime) {}
private:
	virtual void DrawWidget(float DeltaTime) final override;

	FString WindowTitle;
	FVector2f WindowSize;
};

class FArcGameplayDebuggerTools : public FArcSlateIMWindowBase
{
public:
	FArcGameplayDebuggerTools(const TCHAR* Command, const TCHAR* CommandHelp)
		: FArcSlateIMWindowBase(TEXT("Arc Gameplay Debugger"), FVector2f(1400, 900), Command, CommandHelp)
	{
		Layout.Anchors = FAnchors(0.5f, 0);
		Layout.Alignment = FVector2f(0.5f, 0);
		Layout.Size = FVector2f(1400, 900);
	}

	virtual void PreDraw(float DeltaTime) override;
	virtual void DrawWindow(float DeltaTime) override;
	virtual void PostDraw(float DeltaTime) override;
	
	void Initialize();
	void Uninitialize();
	
	static TMap<FName, TFunction<void(const FArcItemData*, const FArcItemInstance*)>> ItemInstancesDraw;
	
private:
	enum class EWidgetState
	{
		Enabled,
		Disabled
	} WidgetState = EWidgetState::Disabled;
	
	FArcDebuggerWidgetMenu TestWidget;
	SlateIM::FViewportRootLayout Layout;

};

class ARCGAMEPLAYDEBUGGER_API FArcGameplayDebuggerModule : public IModuleInterface
{
	
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FArcGameplayDebuggerModule();
	
	FArcGameplayDebuggerTools DebuggerTools;
};