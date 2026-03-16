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

#include "ArcPropertyBindingWidgetArgs.h"
#include "Widgets/SCompoundWidget.h"
#include "IPropertyAccessEditor.h"

namespace ETextCommit { enum Type : int; }

class FMenuBuilder;
class UEdGraph;
class UBlueprint;
struct FEditorPropertyPath;

class ARCEDITORTOOLS_API SArcPropertyBinding : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SArcPropertyBinding){}

	SLATE_ARGUMENT(FArcPropertyBindingWidgetArgs, Args)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UBlueprint* InBlueprint, const TArray<FBindingContextStruct>& InBindingContextStructs);

protected:
	struct FFunctionInfo
	{
		FFunctionInfo()
			: Function(nullptr)
		{
		}

		FFunctionInfo(UFunction* InFunction)
			: DisplayName(InFunction->HasMetaData("ScriptName") ? InFunction->GetMetaDataText("ScriptName") : FText::FromName(InFunction->GetFName()))
			, Tooltip(InFunction->GetMetaData("Tooltip"))
			, FuncName(InFunction->GetFName())
			, Function(InFunction)
		{}

		FText DisplayName;
		FString Tooltip;

		FName FuncName;
		UFunction* Function;
	};

	TSharedRef<SWidget> OnGenerateDelegateMenu();
	void FillPropertyMenu(FMenuBuilder& MenuBuilder, UStruct* InOwnerStruct, TArray<TSharedPtr<FBindingChainElement>> InBindingChain);

	const FSlateBrush* GetCurrentBindingImage() const;
	FText GetCurrentBindingText() const;
	FText GetCurrentBindingToolTipText() const;
	FSlateColor GetCurrentBindingColor() const;

	bool CanRemoveBinding();
	void HandleRemoveBinding();

	void HandleAddBinding(TArray<TSharedPtr<FBindingChainElement>> InBindingChain);
	void HandleSetBindingArrayIndex(int32 InArrayIndex, ETextCommit::Type InCommitType, FProperty* InProperty, TArray<TSharedPtr<FBindingChainElement>> InBindingChain);

	void HandleCreateAndAddBinding();

	UStruct* ResolveIndirection(const TArray<TSharedPtr<FBindingChainElement>>& BindingChain) const;

	EVisibility GetGotoBindingVisibility() const;

	FReply HandleGotoBindingClicked();

	// Helper function to call the OnCanAcceptProperty* delegates, handles conversion of binding chain to TConstArrayView<FBindingChainElement> as expected by the delegate.
	bool CanAcceptPropertyOrChildren(FProperty* InProperty, TConstArrayView<TSharedPtr<FBindingChainElement>> InBindingChain) const;
	
	// Helper function to call the OnCanBindProperty* delegates, handles conversion of binding chain to TConstArrayView<FBindingChainElement> as expected by the delegate.
	bool CanBindProperty(FProperty* InProperty, TConstArrayView<TSharedPtr<FBindingChainElement>> InBindingChain) const;

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	bool IsClassDenied(UClass* OwnerClass) const;
	bool IsFieldFromDeniedClass(FFieldVariant Field) const;
	bool HasBindableProperties(UStruct* InStruct, TArray<TSharedPtr<FBindingChainElement>>& BindingChain) const;
	bool HasBindablePropertiesRecursive(UStruct* InStruct, TSet<UStruct*>& VisitedStructs, TArray<TSharedPtr<FBindingChainElement>>& BindingChain) const;

	/**
	 * Note that an ArrayView is not used to pass the BindingChain around since the predicate can modify the array
	 * and this will invalidate the ArrayView if reallocation is performed.
	 */
	template <typename Predicate>
	void ForEachBindableProperty(UStruct* InStruct, const TArray<TSharedPtr<FBindingChainElement>>& BindingChain, Predicate Pred) const;

	template <typename Predicate>
	void ForEachBindableFunction(UClass* FromClass, Predicate Pred) const;

	UBlueprint* Blueprint = nullptr;
	TArray<FBindingContextStruct> BindingContextStructs;
	FArcPropertyBindingWidgetArgs Args;
	FName PropertyName;
};
