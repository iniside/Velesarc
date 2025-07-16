/**
 * This file is part of ArcX.
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

#include "ConnectionDrawingPolicy.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "ArcComboTreeGraphSchema.generated.h"

// Action types
UENUM(BlueprintType)
enum class EMkSchemaActionType : uint8
{
	None
	, All
	,
};

// Action to add a node to the graph
USTRUCT()
struct ARCCOREEDITOR_API FArcComboTreeSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_BODY()
	;

	// Template of node we want to create
	UPROPERTY()
	TObjectPtr<class UEdGraphNode> NodeTemplate = nullptr;

	FArcComboTreeSchemaAction()
		: FEdGraphSchemaAction()
		, NodeTemplate(nullptr)
	{
	}

	FArcComboTreeSchemaAction(const FText& InNodeCategory
							  , const FText& InMenuDesc
							  , const FText& InToolTip
							  , const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory
			, InMenuDesc
			, InToolTip
			, InGrouping)
		, NodeTemplate(nullptr)
	{
	}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph
										, UEdGraphPin* FromPin
										, const FVector2D Location
										, bool bSelectNewNode = true) override;

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph
										, TArray<UEdGraphPin*>& FromPins
										, const FVector2D Location
										, bool bSelectNewNode = true) override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// End of FEdGraphSchemaAction interface
};

class FArcComboTreeGraphConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
protected:
	UEdGraph* GraphObj;
	TMap<UEdGraphNode*, int32> NodeWidgetMap;

public:
	FArcComboTreeGraphConnectionDrawingPolicy(int32 InBackLayerID
											  , int32 InFrontLayerID
											  , float ZoomFactor
											  , const FSlateRect& InClippingRect
											  , FSlateWindowElementList& InDrawElements
											  , UEdGraph* InGraphObj);

	// FConnectionDrawingPolicy interface
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin
									  , UEdGraphPin* InputPin
									  ,
									  /*inout*/ FConnectionParams& Params) override;

	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& PinGeometries
					  , FArrangedChildren& ArrangedNodes) override;

	virtual void DrawSplineWithArrow(const FGeometry& StartGeom
									 , const FGeometry& EndGeom
									 , const FConnectionParams& Params) override;

	virtual void DrawSplineWithArrow(const FVector2D& StartPoint
									 , const FVector2D& EndPoint
									 , const FConnectionParams& Params) override;

	virtual void DrawPreviewConnector(const FGeometry& PinGeometry
									  , const FVector2D& StartPoint
									  , const FVector2D& EndPoint
									  , UEdGraphPin* Pin) override;

	virtual FVector2D ComputeSplineTangent(const FVector2D& Start
										   , const FVector2D& End) const override;

	// End of FConnectionDrawingPolicy interface

protected:
	void Internal_DrawLineWithArrow(const FVector2D& StartAnchorPoint
									, const FVector2D& EndAnchorPoint
									, const FConnectionParams& Params);
};

/**
 *
 */
UCLASS()
class ARCCOREEDITOR_API UArcComboTreeGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

	// Begin EdGraphSchema interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A
															 , const UEdGraphPin* B) const override;

	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;

	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;

	virtual bool TryCreateConnection(UEdGraphPin* A
									 , UEdGraphPin* B) const override;

	// End EdGraphSchema interface
	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID
																		  , int32 InFrontLayerID
																		  , float InZoomFactor
																		  , const FSlateRect& InClippingRect
																		  , class FSlateWindowElementList&
																		  InDrawElements
																		  , class UEdGraph* InGraphObj) const override;

public:
	void GetActionList(TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions
					   , const UEdGraph* Graph
					   , EMkSchemaActionType ActionType) const;

private:
	// Begin EdGraphSchema interface
	virtual void DroppedAssetsOnGraph(const TArray<struct FAssetData>& Assets
									  , const FVector2D& GraphPosition
									  , UEdGraph* Graph) const override;

	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets
											, const UEdGraph* HoverGraph
											, FString& OutTooltipText
											, bool& OutOkIcon) const override;

	// End EdGraphSchema interface

	template <typename T>
	static void AddAction(FString Title
						  , FString Tooltip
						  , TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions
						  , UObject* Owner)
	{
		const FText MenuDesc = FText::FromString(Title);
		const FText Category = FText::FromString(TEXT("Ability Tree"));
		TSharedPtr<FArcComboTreeSchemaAction> NewActorNodeAction = AddNewNodeAction(OutActions
			, Category
			, MenuDesc
			, Tooltip);
		T* ActorNode = NewObject<T>(Owner);
		NewActorNodeAction->NodeTemplate = ActorNode;
	}

	static TSharedPtr<FArcComboTreeSchemaAction> AddNewNodeAction(TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions
																  , const FText& Category
																  , const FText& MenuDesc
																  , const FString& Tooltip)
	{
		TSharedPtr<FArcComboTreeSchemaAction> NewAction = TSharedPtr<FArcComboTreeSchemaAction>(
			new FArcComboTreeSchemaAction(Category
				, MenuDesc
				, FText::FromString(Tooltip)
				, 0));

		OutActions.Add(NewAction);
		return NewAction;
	}
};
