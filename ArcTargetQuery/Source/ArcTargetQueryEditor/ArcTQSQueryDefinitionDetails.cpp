// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryDefinitionDetails.h"

#include "ArcPropertyFunctionEditor.h"
#include "ArcTQSBindingExtension.h"
#include "ArcTQSGenerator.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSStep.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailDragDropHandler.h"
#include "IDetailPropertyRow.h"
#include "IDetailsView.h"
#include "IPropertyUtilities.h"
#include "InstancedStructDetails.h"
#include "PropertyBindingExtension.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "SInstancedStructPicker.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "ArcTQSQueryDefinitionDetails"

namespace ArcTQSQueryDefinitionDetailsPrivate
{
	FGuid GetNodeID(const FInstancedStruct& Node)
	{
		if (const FArcTQSStep* Step = Node.GetPtr<FArcTQSStep>())
		{
			return Step->ID;
		}
		if (const FArcTQSGenerator* Generator = Node.GetPtr<FArcTQSGenerator>())
		{
			return Generator->ID;
		}
		return FGuid();
	}

	class FStepDragDropOp final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FStepDragDropOp, FDecoratedDragDropOp)

		FStepDragDropOp(TWeakPtr<IPropertyHandle> InArrayHandle, const int32 InSourceIndex)
			: ArrayHandle(MoveTemp(InArrayHandle))
			, SourceIndex(InSourceIndex)
		{
		}

		TWeakPtr<IPropertyHandle> ArrayHandle;
		int32 SourceIndex = INDEX_NONE;
	};

	class FStepDragDropHandler final : public IDetailDragDropHandler
	{
	public:
		FStepDragDropHandler(
			TSharedRef<IPropertyHandle> InArrayHandle,
			TSharedRef<IPropertyHandle> InElementHandle,
			TSharedPtr<IPropertyUtilities> InPropertyUtilities)
			: ArrayHandle(MoveTemp(InArrayHandle))
			, ElementHandle(MoveTemp(InElementHandle))
			, PropertyUtilities(MoveTemp(InPropertyUtilities))
		{
		}

		virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation() const override
		{
			const TSharedRef<FStepDragDropOp> DragOp = MakeShared<FStepDragDropOp>(ArrayHandle, ElementHandle->GetIndexInArray());
			DragOp->CurrentHoverText = ElementHandle->GetPropertyDisplayName();
			DragOp->SetupDefaults();
			DragOp->Construct();
			return DragOp;
		}

		virtual TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& DragDropEvent, const EItemDropZone DropZone) const override
		{
			const EItemDropZone TargetZone = DropZone == EItemDropZone::OntoItem ? EItemDropZone::BelowItem : DropZone;
			if (ComputeNewIndex(DragDropEvent, TargetZone) == INDEX_NONE)
			{
				return TOptional<EItemDropZone>();
			}
			return TargetZone;
		}

		virtual bool AcceptDrop(const FDragDropEvent& DragDropEvent, const EItemDropZone DropZone) const override
		{
			const EItemDropZone TargetZone = DropZone == EItemDropZone::OntoItem ? EItemDropZone::BelowItem : DropZone;
			const int32 NewIndex = ComputeNewIndex(DragDropEvent, TargetZone);
			const TSharedPtr<IPropertyHandleArray> Array = ArrayHandle->AsArray();
			if (NewIndex == INDEX_NONE || !Array.IsValid())
			{
				return false;
			}
			const int32 SourceIndex = DragDropEvent.GetOperationAs<FStepDragDropOp>()->SourceIndex;

			FScopedTransaction Transaction(LOCTEXT("MoveStep", "Move Step"));
			ArrayHandle->NotifyPreChange();
			Array->MoveElementTo(SourceIndex, NewIndex);
			ArrayHandle->NotifyPostChange(EPropertyChangeType::ArrayMove);
			if (PropertyUtilities.IsValid())
			{
				const FPropertyChangedEvent MoveEvent(ArrayHandle->GetProperty(), EPropertyChangeType::ArrayMove);
				PropertyUtilities->NotifyFinishedChangingProperties(MoveEvent);
			}
			return true;
		}

	private:
		int32 ComputeNewIndex(const FDragDropEvent& DragDropEvent, const EItemDropZone TargetZone) const
		{
			const TSharedPtr<FStepDragDropOp> DragOp = DragDropEvent.GetOperationAs<FStepDragDropOp>();
			if (!DragOp.IsValid() || DragOp->ArrayHandle.Pin() != ArrayHandle || !ArrayHandle->IsValidHandle()
				|| !ElementHandle->IsValidHandle())
			{
				return INDEX_NONE;
			}
			const int32 SourceIndex = DragOp->SourceIndex;
			const int32 TargetIndex = ElementHandle->GetIndexInArray();
			if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
			{
				return INDEX_NONE;
			}
			int32 NewIndex = TargetZone == EItemDropZone::BelowItem ? TargetIndex + 1 : TargetIndex;
			if (SourceIndex < NewIndex)
			{
				--NewIndex;
			}
			return NewIndex == SourceIndex || NewIndex < 0 ? INDEX_NONE : NewIndex;
		}

		TSharedRef<IPropertyHandle> ArrayHandle;
		TSharedRef<IPropertyHandle> ElementHandle;
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
	};

	class FNodeDetails final : public FInstancedStructDataDetails
	{
	public:
		FNodeDetails(
			TSharedPtr<IPropertyHandle> InStructProperty,
			TSharedPtr<IPropertyUtilities> InPropertyUtilities,
			TSharedPtr<IPropertyHandle> InArrayHandle = nullptr)
			: FInstancedStructDataDetails(InStructProperty)
			, PropertyUtilities(MoveTemp(InPropertyUtilities))
			, ArrayHandle(MoveTemp(InArrayHandle))
		{
		}

		virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
		{
			FInstancedStructDataDetails::GenerateHeaderRowContent(NodeRow);
			if (ArrayHandle.IsValid() && StructProperty.IsValid())
			{
				NodeRow.DragDropHandler(MakeShared<FStepDragDropHandler>(
					ArrayHandle.ToSharedRef(), StructProperty.ToSharedRef(), PropertyUtilities));
			}
			const TSharedRef<SHorizontalBox> ValueBox = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SInstancedStructPicker, StructProperty, PropertyUtilities)
					.RefreshDetailsOnPick(false)
				];
			if (StructProperty->GetIndexInArray() != INDEX_NONE)
			{
				ValueBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					StructProperty->CreateDefaultPropertyButtonWidgets()
				];
			}
			NodeRow
			.NameContent()
			[
				StructProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(250.0f)
			[
				ValueBox
			]
			.ShouldAutoExpand(true);
		}

		virtual void GenerateChildContent(IDetailChildrenBuilder& ChildBuilder) override
		{
			StampedID = ReadNodeID();
			FInstancedStructDataDetails::GenerateChildContent(ChildBuilder);
		}

		virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = ChildRow.GetPropertyHandle();
			if (ChildHandle.IsValid() && StampedID.IsValid())
			{
				ChildHandle->SetInstanceMetaData(UE::PropertyBinding::MetaDataStructIDName, LexToString(StampedID));
			}
		}

		virtual void Tick(const float DeltaTime) override
		{
			if (ReadNodeID() != StampedID)
			{
				OnRegenerateChildren.ExecuteIfBound();
				return;
			}
			FInstancedStructDataDetails::Tick(DeltaTime);
		}

		virtual FName GetName() const override
		{
			return StructProperty.IsValid() && StructProperty->IsValidHandle()
				? FName(*StructProperty->GeneratePathToProperty())
				: FInstancedStructDataDetails::GetName();
		}

	private:
		FGuid ReadNodeID() const
		{
			FGuid ID;
			if (!StructProperty.IsValid() || !StructProperty->IsValidHandle())
			{
				return ID;
			}
			StructProperty->EnumerateConstRawData([&ID](const void* RawData, const int32 DataIndex, const int32 NumDatas)
			{
				if (RawData)
				{
					ID = GetNodeID(*static_cast<const FInstancedStruct*>(RawData));
				}
				return false;
			});
			return ID;
		}

		TSharedPtr<IPropertyUtilities> PropertyUtilities;
		TSharedPtr<IPropertyHandle> ArrayHandle;
		FGuid StampedID;
	};

	void AddNode(IDetailLayoutBuilder& DetailBuilder, const FName PropertyName, const FName CategoryName)
	{
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(PropertyName);
		DetailBuilder.HideProperty(Handle);
		DetailBuilder.EditCategory(CategoryName).AddCustomBuilder(
			MakeShared<FNodeDetails>(Handle, DetailBuilder.GetPropertyUtilities()));
	}
}

TSharedRef<IDetailCustomization> FArcTQSQueryDefinitionDetails::MakeInstance()
{
	return MakeShared<FArcTQSQueryDefinitionDetails>();
}

void FArcTQSQueryDefinitionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace ArcTQSQueryDefinitionDetailsPrivate;

	const TSharedPtr<IDetailsView> DetailsView = DetailBuilder.GetDetailsViewSharedPtr();
	if (DetailsView.IsValid() && !DetailsView->GetExtensionHandler().IsValid())
	{
		DetailsView->SetExtensionHandler(MakeShared<FArcTQSBindingExtension>());
	}
	if (DetailsView.IsValid() && !DetailsView->GetChildrenCustomizationHandler().IsValid())
	{
		DetailsView->SetChildrenCustomizationHandler(MakeShared<FArcPropertyFunctionChildrenCustomization>(
			UArcTQSQueryDefinition::StaticClass()));
	}

	DetailBuilder.EditCategory(TEXT("Parameters"));
	AddNode(DetailBuilder, GET_MEMBER_NAME_CHECKED(UArcTQSQueryDefinition, Generator), TEXT("Generator"));

	const TSharedRef<IPropertyHandle> StepsHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UArcTQSQueryDefinition, Steps));
	const TSharedRef<FDetailArrayBuilder> StepsBuilder = MakeShared<FDetailArrayBuilder>(StepsHandle);
	StepsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda(
		[PropertyUtilities = DetailBuilder.GetPropertyUtilities(), StepsHandle](
			const TSharedRef<IPropertyHandle> ElementHandle,
			const int32 ArrayIndex,
			IDetailChildrenBuilder& ChildrenBuilder)
		{
			ChildrenBuilder.AddCustomBuilder(MakeShared<FNodeDetails>(ElementHandle, PropertyUtilities, StepsHandle));
		}));
	DetailBuilder.EditCategory(TEXT("Steps")).AddCustomBuilder(StepsBuilder);
}

#undef LOCTEXT_NAMESPACE
