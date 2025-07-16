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

#include "CoreMinimal.h"
#include "SlateIM.h"
#include "SlateIMWidgetBase.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/Anchors.h"

struct FArcItemInstance;
struct FArcItemData;

class FArcDebuggerWidgetMenu
{
public:
	void Draw();
};

class FArcGameplayDebuggerTools : public FSlateIMWindowBase
{
public:
	FArcGameplayDebuggerTools(const TCHAR* Command, const TCHAR* CommandHelp)
		: FSlateIMWindowBase(TEXT("Arc Gameplay Debugger"), FVector2f(1400, 900), Command, CommandHelp)
	{
		Layout.Anchors = FAnchors(0.5f, 0);
		Layout.Alignment = FVector2f(0.5f, 0);
		Layout.Size = FVector2f(1400, 900);
	}
	
	virtual void DrawWindow(float DeltaTime) override;

	static TMap<FName, TFunction<void(const FArcItemData*, const FArcItemInstance*)>> ItemInstancesDraw;
	
private:
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