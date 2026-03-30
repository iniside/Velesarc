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


#include "Modules/ModuleManager.h"

#include "IPropertyAccessEditor.h"
#include "ArcPropertyBindingWidgetArgs.h"

class SDockTab;
class FSpawnTabArgs;

class ARCEDITORTOOLS_API IArcEditorTools : public IModuleInterface
{

public:
	static IArcEditorTools& Get();
	
	/** 
	 * Make a property binding widget.
	 * @param	InBlueprint		The blueprint that the binding will exist within
	 * @param	InArgs			Optional arguments for the widget
	 * @return a new binding widget
	 */
	virtual TSharedRef<SWidget> MakePropertyBindingWidget(UBlueprint* InBlueprint, const FArcPropertyBindingWidgetArgs& InArgs = FArcPropertyBindingWidgetArgs()) const = 0;

	/**
	 * Make a property binding widget.
	 * @param	InBindingContextStructs		An array of structs the binding will exist within
	 * @param	InArgs						Optional arguments for the widget
	 * @return a new binding widget
	 */
	virtual TSharedRef<SWidget> MakePropertyBindingWidget(const TArray<FBindingContextStruct>& InBindingContextStructs, const FArcPropertyBindingWidgetArgs& InArgs = FArcPropertyBindingWidgetArgs()) const = 0;	
};

class ARCEDITORTOOLS_API FArcEditorToolsModule : public IArcEditorTools
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	
	/** 
	 * Make a property binding widget.
	 * @param	InBlueprint		The blueprint that the binding will exist within
	 * @param	InArgs			Optional arguments for the widget
	 * @return a new binding widget
	 */
	virtual TSharedRef<SWidget> MakePropertyBindingWidget(UBlueprint* InBlueprint, const FArcPropertyBindingWidgetArgs& InArgs = FArcPropertyBindingWidgetArgs()) const override;

	/**
	 * Make a property binding widget.
	 * @param	InBindingContextStructs		An array of structs the binding will exist within
	 * @param	InArgs						Optional arguments for the widget
	 * @return a new binding widget
	 */
	virtual TSharedRef<SWidget> MakePropertyBindingWidget(const TArray<FBindingContextStruct>& InBindingContextStructs, const FArcPropertyBindingWidgetArgs& InArgs = FArcPropertyBindingWidgetArgs()) const override;

private:
	TSharedRef<SDockTab> SpawnMinimapTab(const FSpawnTabArgs& Args);
	static const FName MinimapTabId;
};
