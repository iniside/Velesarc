// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcEffectSummaryText.h"
#include "PropertyHandle.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcScalableFloat.h"
#include "GameplayTagContainer.h"

namespace ArcEffectSummaryTextImpl
{
	static const FText NoneText = NSLOCTEXT("ArcEffectSummary", "None", "None");
	static const FText MultipleText = NSLOCTEXT("ArcEffectSummary", "Multiple", "—");
	static const FText AnyText = NSLOCTEXT("ArcEffectSummary", "Any", "Any");
}

namespace ArcEffectSummaryText
{
	FText FormatAttributeRef(const TSharedPtr<IPropertyHandle>& AttributeHandle)
	{
		if (!AttributeHandle.IsValid())
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		TSharedPtr<IPropertyHandle> FragmentTypeHandle = AttributeHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeRef, FragmentType));
		TSharedPtr<IPropertyHandle> PropertyNameHandle = AttributeHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FArcAttributeRef, PropertyName));

		UObject* FragmentTypeObj = nullptr;
		if (FragmentTypeHandle.IsValid())
		{
			FragmentTypeHandle->GetValue(FragmentTypeObj);
		}

		FName PropName = NAME_None;
		if (PropertyNameHandle.IsValid())
		{
			PropertyNameHandle->GetValue(PropName);
		}

		UScriptStruct* FragStruct = Cast<UScriptStruct>(FragmentTypeObj);
		if (FragStruct && PropName != NAME_None)
		{
			return FText::FromString(FString::Printf(TEXT("%s.%s"), *FragStruct->GetName(), *PropName.ToString()));
		}

		return ArcEffectSummaryTextImpl::NoneText;
	}

	FText OperationToText(const TSharedPtr<IPropertyHandle>& OperationHandle)
	{
		if (!OperationHandle.IsValid())
		{
			return FText::GetEmpty();
		}

		uint8 RawValue = 0;
		const FPropertyAccess::Result Result = OperationHandle->GetValue(RawValue);
		if (Result != FPropertyAccess::Success)
		{
			return FText::GetEmpty();
		}

		const UEnum* EnumClass = StaticEnum<EArcModifierOp>();
		if (!EnumClass)
		{
			return FText::GetEmpty();
		}

		return EnumClass->GetDisplayNameTextByValue(static_cast<int64>(RawValue));
	}

	FText FormatTagRequirements(const TSharedPtr<IPropertyHandle>& TagFilterHandle)
	{
		if (!TagFilterHandle.IsValid())
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		TArray<void*> RawData;
		TagFilterHandle->AccessRawData(RawData);

		if (RawData.Num() != 1)
		{
			return ArcEffectSummaryTextImpl::MultipleText;
		}

		if (RawData[0] == nullptr)
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		const FArcGameplayTagRequirements* Req = static_cast<const FArcGameplayTagRequirements*>(RawData[0]);

		const int32 RequireCount = Req->RequireTags.Num();
		const int32 IgnoreCount  = Req->IgnoreTags.Num();
		const bool  bHasQuery    = !Req->TagQuery.IsEmpty();

		if (RequireCount == 0 && IgnoreCount == 0 && !bHasQuery)
		{
			return ArcEffectSummaryTextImpl::AnyText;
		}

		FString Parts;
		if (RequireCount > 0)
		{
			Parts += FString::Printf(TEXT("Req:%d"), RequireCount);
		}
		if (IgnoreCount > 0)
		{
			if (!Parts.IsEmpty())
			{
				Parts += TEXT("  ");
			}
			Parts += FString::Printf(TEXT("Ign:%d"), IgnoreCount);
		}
		if (bHasQuery)
		{
			if (!Parts.IsEmpty())
			{
				Parts += TEXT("  ");
			}
			Parts += TEXT("+Query");
		}

		return FText::FromString(Parts);
	}

	FText BuildTagRequirementsTooltip(const TSharedPtr<IPropertyHandle>& TagFilterHandle)
	{
		if (!TagFilterHandle.IsValid())
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		TArray<void*> RawData;
		TagFilterHandle->AccessRawData(RawData);

		if (RawData.Num() != 1)
		{
			return ArcEffectSummaryTextImpl::MultipleText;
		}

		if (RawData[0] == nullptr)
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		const FArcGameplayTagRequirements* Req = static_cast<const FArcGameplayTagRequirements*>(RawData[0]);

		const bool bHasRequire = Req->RequireTags.Num() > 0;
		const bool bHasIgnore  = Req->IgnoreTags.Num() > 0;
		const bool bHasQuery   = !Req->TagQuery.IsEmpty();

		if (!bHasRequire && !bHasIgnore && !bHasQuery)
		{
			return ArcEffectSummaryTextImpl::AnyText;
		}

		FString Lines;
		if (bHasRequire)
		{
			Lines += TEXT("Require:\n  ");
			Lines += Req->RequireTags.ToStringSimple();
		}
		if (bHasIgnore)
		{
			if (!Lines.IsEmpty())
			{
				Lines += TEXT("\n");
			}
			Lines += TEXT("Ignore:\n  ");
			Lines += Req->IgnoreTags.ToStringSimple();
		}
		if (bHasQuery)
		{
			if (!Lines.IsEmpty())
			{
				Lines += TEXT("\n");
			}
			const FString& QueryDesc = Req->TagQuery.GetDescription();
			if (!QueryDesc.IsEmpty())
			{
				Lines += TEXT("Query:\n  ");
				Lines += QueryDesc;
			}
			else
			{
				Lines += TEXT("Query:\n  (query set)");
			}
		}

		return FText::FromString(Lines);
	}

	FText InstancedStructTypeName(const TSharedPtr<IPropertyHandle>& InstancedHandle)
	{
		if (!InstancedHandle.IsValid())
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		TArray<void*> RawData;
		InstancedHandle->AccessRawData(RawData);

		if (RawData.Num() != 1)
		{
			return ArcEffectSummaryTextImpl::MultipleText;
		}

		if (RawData[0] == nullptr)
		{
			return ArcEffectSummaryTextImpl::NoneText;
		}

		const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData[0]);
		const UScriptStruct* ScriptStruct = InstancedStruct->GetScriptStruct();

		if (ScriptStruct)
		{
			return ScriptStruct->GetDisplayNameText();
		}

		return ArcEffectSummaryTextImpl::NoneText;
	}

	FText BuildModifierMagnitude(
		const TSharedPtr<IPropertyHandle>& TypeHandle,
		const TSharedPtr<IPropertyHandle>& MagnitudeHandle,
		const TSharedPtr<IPropertyHandle>& SetByCallerTagHandle,
		const TSharedPtr<IPropertyHandle>& CustomMagnitudeHandle)
	{
		if (!TypeHandle.IsValid())
		{
			return FText::GetEmpty();
		}

		uint8 TypeRaw = 0;
		TypeHandle->GetValue(TypeRaw);

		if (TypeRaw == static_cast<uint8>(EArcModifierType::Simple))
		{
			if (!MagnitudeHandle.IsValid())
			{
				return FText::FromString(TEXT("—"));
			}

			TArray<void*> RawData;
			MagnitudeHandle->AccessRawData(RawData);
			if (RawData.Num() == 1 && RawData[0] != nullptr)
			{
				const FArcScalableFloat* ScalableFloat = static_cast<const FArcScalableFloat*>(RawData[0]);
				return FText::FromString(ScalableFloat->ToSimpleString());
			}

			if (RawData.Num() > 1)
			{
				return FText::FromString(TEXT("—"));
			}

			return FText::GetEmpty();
		}
		else if (TypeRaw == static_cast<uint8>(EArcModifierType::SetByCaller))
		{
			if (!SetByCallerTagHandle.IsValid())
			{
				return FText::FromString(TEXT("SetByCaller(None)"));
			}

			TArray<void*> RawData;
			SetByCallerTagHandle->AccessRawData(RawData);
			if (RawData.Num() == 1 && RawData[0] != nullptr)
			{
				const FGameplayTag* Tag = static_cast<const FGameplayTag*>(RawData[0]);
				return FText::FromString(FString::Printf(TEXT("SetByCaller(%s)"), *Tag->ToString()));
			}

			return FText::FromString(TEXT("SetByCaller(None)"));
		}
		else if (TypeRaw == static_cast<uint8>(EArcModifierType::Custom))
		{
			return FText::FromString(FString::Printf(TEXT("Custom(%s)"),
				*InstancedStructTypeName(CustomMagnitudeHandle).ToString()));
		}

		return FText::GetEmpty();
	}
}
