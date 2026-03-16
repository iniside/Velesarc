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
#include "IPropertyAccessEditor.h"

struct ARCEDITORTOOLS_API FArcPropertyBindingWidgetArgs
{
	// Macro needed to avoid deprecation errors when the struct is copied or created in the default methods.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FArcPropertyBindingWidgetArgs() = default;
	FArcPropertyBindingWidgetArgs(const FArcPropertyBindingWidgetArgs&) = default;
	FArcPropertyBindingWidgetArgs(FArcPropertyBindingWidgetArgs&&) = default;
	FArcPropertyBindingWidgetArgs& operator=(const FArcPropertyBindingWidgetArgs&) = default;
	FArcPropertyBindingWidgetArgs& operator=(FArcPropertyBindingWidgetArgs&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	/** An optional bindable property */
	FProperty* Property = nullptr;

	/** An optional signature to use to match binding functions */
	UFunction* BindableSignature = nullptr;

	/** Delegate used to generate a new binding function's name */
	FOnGenerateBindingName OnGenerateBindingName;

	/** Delegate used to open a bound generated function */
	FOnGotoBinding OnGotoBinding;

	/** Delegate used to see if we can open a binding (e.g. a function) */
	FOnCanGotoBinding OnCanGotoBinding;

	/** Delegate used to check whether a property is considered for binding. Returning false will discard the property and all child properties. */
	FOnCanAcceptPropertyOrChildrenWithBindingChain OnCanAcceptPropertyOrChildrenWithBindingChain;

	UE_DEPRECATED(5.4, "Please use OnCanAcceptPropertyOrChildrenWithBindingChain instead.")
	FOnCanAcceptPropertyOrChildren OnCanAcceptPropertyOrChildren;
	
	/** Delegate used to check whether a property can be bound to the property in question */
	FOnCanBindPropertyWithBindingChain OnCanBindPropertyWithBindingChain;

	UE_DEPRECATED(5.4, "Please use OnCanBindPropertyWithBindingChain instead.")
	FOnCanBindProperty OnCanBindProperty;

	/** Delegate used to check whether a function can be bound to the property in question */
	FOnCanBindFunction OnCanBindFunction;

	/** Delegate called to see if a class can be bound to */
	FOnCanBindToClass OnCanBindToClass;

	UE_DEPRECATED(5.5, "Please use OnCanBindToContextStructWithIndex instead.")
	FOnCanBindToContextStruct OnCanBindToContextStruct;

	/** Delegate called to see if a context struct can be directly bound to */
	FOnCanBindToContextStructWithIndex OnCanBindToContextStructWithIndex;
	
	/** Delegate called to see if a subobject can be bound to */
	FOnCanBindToSubObjectClass OnCanBindToSubObjectClass;

	/** Delegate called to add a binding */
	FOnAddBinding OnAddBinding;

	/** Delegate called to remove a binding */
	FOnRemoveBinding OnRemoveBinding;

	/** Delegate called to see if we can remove remove a binding (ie. if it exists) */
	FOnCanRemoveBinding OnCanRemoveBinding;

	/** Delegate called once a new function binding has been created */
	FOnNewFunctionBindingCreated OnNewFunctionBindingCreated;

	/** Delegate called to resolve true type of the instance */
	FOnResolveIndirection OnResolveIndirection;
	
	/** Delegate called when a property is dropped on the property binding widget */
	FOnDrop OnDrop;

	/** The current binding's text label */
	TAttribute<FText> CurrentBindingText;

	/** The current binding's tooltip text label */
	TAttribute<FText> CurrentBindingToolTipText;
	
	/** The current binding's image */
	TAttribute<const FSlateBrush*> CurrentBindingImage;

	/** The current binding's color */
	TAttribute<FLinearColor> CurrentBindingColor;

	/** Menu extender */
	TSharedPtr<FExtender> MenuExtender;

	/** Optional style override for bind button */
	const FButtonStyle* BindButtonStyle = nullptr;

	/** The maximum level of depth to generate */
	uint8 MaxDepth = 10;
	
	/** Whether to generate pure bindings */
	bool bGeneratePureBindings = true;

	/** Whether to allow function bindings (to "this" class of the blueprint in question) */
	bool bAllowFunctionBindings = true;

	/** Whether to allow function library bindings in addition to the passed-in blueprint's class */
	bool bAllowFunctionLibraryBindings = false;
	
	/** Whether to allow property bindings */
	bool bAllowPropertyBindings = true;	
	
	/** Whether to allow array element bindings */
	bool bAllowArrayElementBindings = false;

	/** Whether to allow struct member bindings */
	bool bAllowStructMemberBindings = true;

	/** Whether to allow new bindings to be made from within the widget's UI */
	bool bAllowNewBindings = true;

	/** Whether to allow UObject functions as non-leaf nodes */
	bool bAllowUObjectFunctions = false;
	
	/** Whether to allow only functions marked thread safe */
	bool bAllowOnlyThreadSafeFunctions = false;

	/** Whether to allow UScriptStruct functions as non-leaf nodes */
	bool bAllowStructFunctions = false;	
	bool bGenerateAllowNonPureBindings = false;
};
