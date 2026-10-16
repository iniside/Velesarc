// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryReferenceDetails.h"

#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryReference.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyBagDetails.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"

namespace ArcTQSQueryReferenceDetailsPrivate
{
	class FParametersDetails final : public FPropertyBagInstanceDataDetails
	{
	public:
		FParametersDetails(
			TSharedPtr<IPropertyHandle> InReferenceHandle,
			TSharedPtr<IPropertyHandle> InParametersHandle,
			const TSharedPtr<IPropertyUtilities>& InPropertyUtilities)
			: FPropertyBagInstanceDataDetails(FConstructParams{
				.BagStructProperty = InParametersHandle,
				.PropUtils = InPropertyUtilities,
				.bAllowContainers = true,
				.ChildRowFeatures = EPropertyBagChildRowFeatures::Fixed })
			, ReferenceHandle(MoveTemp(InReferenceHandle))
		{
		}

	protected:
		struct FOverrideProvider final : public IPropertyBagOverrideProvider
		{
			explicit FOverrideProvider(FArcTQSQueryReference& InReference)
				: Reference(InReference)
			{
			}

			virtual bool IsPropertyOverridden(const FGuid PropertyID) const override
			{
				return Reference.IsPropertyOverridden(PropertyID);
			}

			virtual void SetPropertyOverride(const FGuid PropertyID, const bool bIsOverridden) const override
			{
				Reference.SetPropertyOverridden(PropertyID, bIsOverridden);
			}

		private:
			FArcTQSQueryReference& Reference;
		};

		virtual bool HasPropertyOverrides() const override
		{
			return true;
		}

		virtual void PreChangeOverrides() override
		{
			ReferenceHandle->NotifyPreChange();
		}

		virtual void PostChangeOverrides() override
		{
			ReferenceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			ReferenceHandle->NotifyFinishedChangingProperties();
		}

		virtual void EnumeratePropertyBags(
			TSharedPtr<IPropertyHandle> PropertyBagHandle,
			const EnumeratePropertyBagFuncRef& Func) const override
		{
			ReferenceHandle->EnumerateRawData([&Func](void* RawData, const int32 DataIndex, const int32 NumDatas)
			{
				FArcTQSQueryReference* Reference = static_cast<FArcTQSQueryReference*>(RawData);
				const UArcTQSQueryDefinition* Definition = Reference ? Reference->GetDefinition() : nullptr;
				if (!Definition)
				{
					return true;
				}
				FOverrideProvider OverrideProvider(*Reference);
				return Func(Definition->Parameters, Reference->GetMutableParameters(), OverrideProvider);
			});
		}

	private:
		TSharedPtr<IPropertyHandle> ReferenceHandle;
	};
}

TSharedRef<IPropertyTypeCustomization> FArcTQSQueryReferenceDetails::MakeInstance()
{
	return MakeShared<FArcTQSQueryReferenceDetails>();
}

TSharedRef<IPropertyTypeCustomization> FArcTQSQueryReferenceDetails::MakeFixedDefinitionInstance()
{
	const TSharedRef<FArcTQSQueryReferenceDetails> Details = MakeShared<FArcTQSQueryReferenceDetails>();
	Details->bFixedDefinition = true;
	return Details;
}

bool FArcTQSQueryReferenceDetails::RequiresParametersSync(const FArcTQSQueryReference& Reference)
{
	FArcTQSQueryReference Synced = Reference;
	Synced.SyncParameters();
	return Synced.PropertyOverrides != Reference.PropertyOverrides
		|| !Synced.Parameters.Identical(&Reference.Parameters, PPF_None);
}

void FArcTQSQueryReferenceDetails::CustomizeHeader(
	const TSharedRef<IPropertyHandle> InStructPropertyHandle,
	FDetailWidgetRow& InHeaderRow,
	IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	PropertyUtilities = InCustomizationUtils.GetPropertyUtilities();
	ParametersHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTQSQueryReference, Parameters));
	const TSharedPtr<IPropertyHandle> DefinitionHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcTQSQueryReference, Definition));
	check(ParametersHandle.IsValid() && DefinitionHandle.IsValid());

	if (!GIsTransacting)
	{
		SyncParameters();
	}

	if (bFixedDefinition)
	{
		InHeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ShouldAutoExpand(true);
	}
	else
	{
		InHeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(DefinitionHandle)
			.AllowedClass(UArcTQSQueryDefinition::StaticClass())
			.ThumbnailPool(InCustomizationUtils.GetThumbnailPool())
		]
		.ShouldAutoExpand(true);
	}

	DefinitionHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSPLambda(AsShared(), [this]()
	{
		SyncParameters();
	}));

	ArcTQS::Delegates::OnPostCompile.AddSP(this, &FArcTQSQueryReferenceDetails::HandleDefinitionCompiled);
}

void FArcTQSQueryReferenceDetails::CustomizeChildren(
	const TSharedRef<IPropertyHandle> InStructPropertyHandle,
	IDetailChildrenBuilder& InChildrenBuilder,
	IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	InChildrenBuilder.AddCustomBuilder(MakeShared<ArcTQSQueryReferenceDetailsPrivate::FParametersDetails>(
		StructPropertyHandle, ParametersHandle, PropertyUtilities));
}

void FArcTQSQueryReferenceDetails::HandleDefinitionCompiled(const UArcTQSQueryDefinition& Definition) const
{
	SyncParameters(&Definition);
}

void FArcTQSQueryReferenceDetails::SyncParameters(const UArcTQSQueryDefinition* DefinitionToSync) const
{
	if (!StructPropertyHandle.IsValid() || !StructPropertyHandle->IsValidHandle())
	{
		return;
	}
	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);
	bool bDidSync = false;
	for (void* Data : RawData)
	{
		FArcTQSQueryReference* Reference = static_cast<FArcTQSQueryReference*>(Data);
		if (!Reference)
		{
			continue;
		}
		if ((DefinitionToSync == nullptr || Reference->GetDefinition() == DefinitionToSync)
			&& RequiresParametersSync(*Reference))
		{
			Reference->SyncParameters();
			bDidSync = true;
		}
	}
	if (bDidSync && PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestRefresh();
	}
}
