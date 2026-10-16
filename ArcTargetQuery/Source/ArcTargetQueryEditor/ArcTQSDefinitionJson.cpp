// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSDefinitionJson.h"

#include "Json/ArcPropertyJson.h"
#include "Json/ArcStructTypeNames.h"
#include "ArcTQSBindings.h"
#include "ArcTQSCompiler.h"
#include "ArcTQSGenerator.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryReference.h"
#include "ArcTQSStep.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingPath.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "ArcTQSDefinitionJson"

namespace ArcTQSDefinitionJsonPrivate
{
	using ArcPropertyJson::CloneObject;
	using ArcPropertyJson::FindField;
	using ArcStructTypeNames::FindType;
	using ArcStructTypeNames::GetTypeName;
	using ArcPropertyJson::ToJsonStrings;

	constexpr int32 FormatVersion = 1;
	const TCHAR* const ExampleAssetPath = TEXT("/ArcRPG/Enemies/TQS_Patrol");
	const TCHAR* const GeneratorName = TEXT("Generator");
	const TCHAR* const StepsName = TEXT("Steps");
	const TCHAR* const ParametersName = TEXT("Parameters");
	const TCHAR* const QuerierName = TEXT("Querier");
	const TCHAR* const ItemName = TEXT("Item");

	enum class ERole : uint8
	{
		Invalid,
		Generator,
		Step,
		Function
	};

	const FArcPropertyJsonHooks& GetHooks()
	{
		static const FArcPropertyJsonHooks Hooks;
		return Hooks;
	}

	const TCHAR* GetRoleName(const ERole Role)
	{
		switch (Role)
		{
		case ERole::Generator:
			return TEXT("Generator");
		case ERole::Step:
			return TEXT("Step");
		case ERole::Function:
			return TEXT("Function");
		default:
			return TEXT("Invalid");
		}
	}

	const UScriptStruct* GetRoleBase(const ERole Role)
	{
		switch (Role)
		{
		case ERole::Generator:
			return FArcTQSGenerator::StaticStruct();
		case ERole::Step:
			return FArcTQSStep::StaticStruct();
		case ERole::Function:
			return FArcTQSPropertyFunction::StaticStruct();
		default:
			return nullptr;
		}
	}

	ERole GetStructRole(const UScriptStruct* Struct)
	{
		const ERole Roles[] = {ERole::Generator, ERole::Step, ERole::Function};
		for (const ERole Role : Roles)
		{
			const UScriptStruct* Base = GetRoleBase(Role);
			if (Struct && Struct != Base && Struct->IsChildOf(Base))
			{
				return Role;
			}
		}
		return ERole::Invalid;
	}

	bool ParseRole(const FString& Text, ERole& OutRole)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			OutRole = ERole::Invalid;
			return true;
		}
		const ERole Roles[] = {ERole::Generator, ERole::Step, ERole::Function};
		for (const ERole Role : Roles)
		{
			if (Trimmed.Equals(GetRoleName(Role), ESearchCase::IgnoreCase))
			{
				OutRole = Role;
				return true;
			}
		}
		return false;
	}

	bool IsRootKind(const EArcTQSDataKind Kind)
	{
		return Kind == EArcTQSDataKind::Generator
			|| Kind == EArcTQSDataKind::Step;
	}

	bool IsSourceKind(const EArcTQSDataKind Kind)
	{
		return Kind == EArcTQSDataKind::Parameters
			|| Kind == EArcTQSDataKind::Querier
			|| Kind == EArcTQSDataKind::Item;
	}

	bool IsReadOnlyProperty(const FProperty& Property)
	{
		return Property.HasAnyPropertyFlags(CPF_EditConst) || ArcTQS::IsFunctionOutputProperty(Property);
	}

	FString PathToString(const FPropertyBindingPath& Path)
	{
		TStringBuilder<256> Result;
		for (const FPropertyBindingPathSegment& Segment : Path.GetSegments())
		{
			if (Result.Len() > 0)
			{
				Result << TEXT('.');
			}
			Result << Segment.GetName();
			if (Segment.GetArrayIndex() >= 0)
			{
				Result << TEXT('[') << Segment.GetArrayIndex() << TEXT(']');
			}
		}
		return FString(Result);
	}

	FString JoinEndpoint(const FString& Name, const FPropertyBindingPath& Path)
	{
		return Path.IsPathEmpty() ? Name : Name + TEXT(".") + PathToString(Path);
	}

	FString GetHandleName(const FArcTQSDataHandle& Handle)
	{
		switch (Handle.GetKind())
		{
		case EArcTQSDataKind::Parameters:
			return ParametersName;
		case EArcTQSDataKind::Querier:
			return QuerierName;
		case EArcTQSDataKind::Item:
			return ItemName;
		case EArcTQSDataKind::Generator:
			return GeneratorName;
		case EArcTQSDataKind::Step:
			return FString::Printf(TEXT("%s[%d]"), StepsName, Handle.GetIndex());
		default:
			return FString();
		}
	}

	FString GetNodeName(const UArcTQSQueryDefinition& Definition, const FGuid& ID, const int32 Depth = 0)
	{
		const FArcTQSDataHandle Handle = Definition.ResolveEndpoint(ID);
		if (Handle.GetKind() != EArcTQSDataKind::Function)
		{
			return GetHandleName(Handle);
		}
		if (Depth > Definition.EditorBindings.GetNumBindings())
		{
			return TEXT("<function cycle>");
		}
		const FArcTQSBinding& Binding = Definition.EditorBindings.GetBinding(Handle.GetIndex());
		const FGuid ParentID = Binding.GetTargetPath().GetStructID();
		const bool bParentIsFunction = Definition.ResolveEndpoint(ParentID).GetKind() == EArcTQSDataKind::Function;
		FString ParentName = GetNodeName(Definition, ParentID, Depth + 1);
		if (ParentName.IsEmpty())
		{
			ParentName = TEXT("<unbound>");
		}
		return ParentName + (bParentIsFunction ? TEXT("/") : TEXT(".")) + PathToString(Binding.GetTargetPath());
	}

	FGuid GetEndpointID(const UArcTQSQueryDefinition& Definition, const EArcTQSDataKind Kind, const int32 Index)
	{
		switch (Kind)
		{
		case EArcTQSDataKind::Parameters:
			return Definition.ParametersID;
		case EArcTQSDataKind::Querier:
			return Definition.QuerierID;
		case EArcTQSDataKind::Item:
			return Definition.ItemID;
		case EArcTQSDataKind::Generator:
			{
				const FArcTQSGenerator* Generator = Definition.Generator.GetPtr<FArcTQSGenerator>();
				return Generator ? Generator->ID : FGuid();
			}
		case EArcTQSDataKind::Step:
			{
				const FArcTQSStep* Step = Definition.Steps.IsValidIndex(Index)
					? Definition.Steps[Index].GetPtr<FArcTQSStep>()
					: nullptr;
				return Step ? Step->ID : FGuid();
			}
		default:
			return FGuid();
		}
	}

	TSharedRef<FJsonObject> ExportNode(const FConstStructView Node)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		const UScriptStruct* Type = Node.GetScriptStruct();
		Out->SetStringField(TEXT("type"), GetTypeName<UScriptStruct>(Type));
		if (Type)
		{
			const TSharedRef<FJsonObject> Data = ArcPropertyJson::ExportStructDelta(
				*Type, Node.GetMemory(), GetHooks(), [](const FProperty& Property) -> bool
			{
				return IsReadOnlyProperty(Property);
			});
			if (Data->Values.Num() > 0)
			{
				Out->SetObjectField(TEXT("data"), Data);
			}
		}
		return Out;
	}

	TSharedPtr<FJsonValue> ExportOptionalNode(const FInstancedStruct& Node)
	{
		if (!Node.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}
		return MakeShared<FJsonValueObject>(ExportNode(FConstStructView(Node)));
	}

	bool RefreshPath(const UArcTQSQueryDefinition& Definition, FPropertyBindingPath& Path, FString& OutError)
	{
		if (Path.IsPathEmpty())
		{
			return true;
		}
		FPropertyBindingDataView View;
		if (!Definition.GetBindingDataViewByID(Path.GetStructID(), View))
		{
			OutError = TEXT("the endpoint has no data to bind");
			return false;
		}
		return Path.UpdateSegmentsFromValue(View, &OutError);
	}

	FString DescribeBinding(const UArcTQSQueryDefinition& Definition, const FArcTQSBinding& Binding)
	{
		const FString Target = JoinEndpoint(GetNodeName(Definition, Binding.GetTargetPath().GetStructID()), Binding.GetTargetPath());
		const FConstStructView Function = Binding.GetPropertyFunctionNode();
		const FString Source = Function.IsValid()
			? FString::Printf(TEXT("function %s"), *GetTypeName<UScriptStruct>(Function.GetScriptStruct()))
			: JoinEndpoint(GetHandleName(Definition.ResolveEndpoint(Binding.GetSourcePath().GetStructID())), Binding.GetSourcePath());
		return FString::Printf(TEXT("'%s <- %s'"), *Target, *Source);
	}

	TSharedPtr<FJsonValue> ExportBindingValue(
		const UArcTQSQueryDefinition& Definition,
		const FArcTQSBinding& Binding,
		bool bNested,
		int32 Depth,
		TArray<FString>& Warnings);

	TSharedRef<FJsonObject> ExportFunction(
		const UArcTQSQueryDefinition& Definition,
		const FConstStructView FunctionView,
		const FGuid& FunctionID,
		const int32 Depth,
		TArray<FString>& Warnings)
	{
		TSharedRef<FJsonObject> Out = ExportNode(FunctionView);
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Inputs;
		for (const FArcTQSBinding& Input : Definition.EditorBindings.GetBindings())
		{
			if (Input.GetTargetPath().GetStructID() != FunctionID)
			{
				continue;
			}
			const TSharedPtr<FJsonValue> Value = ExportBindingValue(Definition, Input, true, Depth, Warnings);
			if (Value.IsValid())
			{
				Inputs.Emplace(PathToString(Input.GetTargetPath()), Value);
			}
		}
		Inputs.StableSort([](const TPair<FString, TSharedPtr<FJsonValue>>& A, const TPair<FString, TSharedPtr<FJsonValue>>& B) -> bool
		{
			return A.Key < B.Key;
		});
		if (Inputs.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Items;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Input : Inputs)
			{
				Items.Add(Input.Value);
			}
			Out->SetArrayField(TEXT("bindings"), Items);
		}
		return Out;
	}

	TSharedPtr<FJsonValue> ExportBindingValue(
		const UArcTQSQueryDefinition& Definition,
		const FArcTQSBinding& Binding,
		const bool bNested,
		const int32 Depth,
		TArray<FString>& Warnings)
	{
		FPropertyBindingPath TargetPath = Binding.GetTargetPath();
		FString PathError;
		if (!RefreshPath(Definition, TargetPath, PathError))
		{
			Warnings.Add(FString::Printf(
				TEXT("Dropped binding %s: the target path does not resolve: %s"),
				*DescribeBinding(Definition, Binding),
				*PathError));
			return TSharedPtr<FJsonValue>();
		}
		const FString ToText = bNested
			? PathToString(TargetPath)
			: JoinEndpoint(GetNodeName(Definition, TargetPath.GetStructID()), TargetPath);

		const FConstStructView FunctionView = Binding.GetPropertyFunctionNode();
		if (FunctionView.IsValid())
		{
			const FArcTQSPropertyFunction* Function = FunctionView.GetPtr<const FArcTQSPropertyFunction>();
			if (!Function || Binding.GetSourcePath().GetStructID() != Function->ID || Binding.GetSourcePath().NumSegments() != 1)
			{
				Warnings.Add(FString::Printf(
					TEXT("Dropped binding %s: the bound function is not an ArcTQS property function sourcing its own output directly (the source path must be the function output field itself)."),
					*DescribeBinding(Definition, Binding)));
				return TSharedPtr<FJsonValue>();
			}
			if (Depth > Definition.EditorBindings.GetNumBindings())
			{
				Warnings.Add(FString::Printf(
					TEXT("Dropped binding %s: bound functions form a cycle."),
					*DescribeBinding(Definition, Binding)));
				return TSharedPtr<FJsonValue>();
			}
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetStringField(TEXT("to"), ToText);
			Out->SetObjectField(TEXT("function"), ExportFunction(Definition, FunctionView, Function->ID, Depth + 1, Warnings));
			return MakeShared<FJsonValueObject>(Out);
		}

		const FArcTQSDataHandle SourceHandle = Definition.ResolveEndpoint(Binding.GetSourcePath().GetStructID());
		if (!IsSourceKind(SourceHandle.GetKind()))
		{
			Warnings.Add(FString::Printf(
				TEXT("Dropped binding %s: its source is not Parameters, Querier or Item."),
				*DescribeBinding(Definition, Binding)));
			return TSharedPtr<FJsonValue>();
		}
		FPropertyBindingPath SourcePath = Binding.GetSourcePath();
		if (!RefreshPath(Definition, SourcePath, PathError))
		{
			Warnings.Add(FString::Printf(
				TEXT("Dropped binding %s: the source path does not resolve: %s"),
				*DescribeBinding(Definition, Binding),
				*PathError));
			return TSharedPtr<FJsonValue>();
		}
		return MakeShared<FJsonValueString>(ToText + TEXT(" <- ") + JoinEndpoint(GetHandleName(SourceHandle), SourcePath));
	}

	TSharedRef<FJsonObject> ExportSelection(const UArcTQSQueryDefinition& Definition)
	{
		TSharedRef<FJsonObject> Selection = MakeShared<FJsonObject>();
		Selection->SetStringField(
			TEXT("mode"),
			StaticEnum<EArcTQSSelectionMode>()->GetNameStringByValue(static_cast<int64>(Definition.SelectionMode)));
		Selection->SetNumberField(TEXT("topN"), Definition.TopN);
		Selection->SetNumberField(TEXT("minPassingScore"), Definition.MinPassingScore);
		Selection->SetNumberField(TEXT("topPercent"), Definition.TopPercent);
		return Selection;
	}

	TSharedRef<FJsonObject> MakeResult(
		const bool bSuccess,
		const FString& Asset,
		const FArcPropertyJsonReport& Report,
		const UArcTQSQueryDefinition* Definition,
		const TArray<FArcTQSCompilerDiagnostic>& Diagnostics)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("asset"), Asset);
		Result->SetBoolField(TEXT("compiled"), Definition && Definition->IsExecutable());
		Result->SetNumberField(TEXT("revision"), Definition ? Definition->CompiledData.Revision : 0);
		Result->SetArrayField(TEXT("errors"), ToJsonStrings(Report.Errors));
		Result->SetArrayField(TEXT("warnings"), ToJsonStrings(Report.Warnings));
		TArray<TSharedPtr<FJsonValue>> DiagnosticItems;
		for (const FArcTQSCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(
				TEXT("node"),
				Definition && Diagnostic.NodeID.IsValid() ? GetNodeName(*Definition, Diagnostic.NodeID) : FString());
			Item->SetStringField(TEXT("message"), Diagnostic.Message.ToString());
			Item->SetBoolField(TEXT("error"), Diagnostic.bIsError);
			DiagnosticItems.Add(MakeShared<FJsonValueObject>(Item));
		}
		Result->SetArrayField(TEXT("diagnostics"), DiagnosticItems);
		return Result;
	}

	TSharedRef<FJsonObject> MakeFailure(const FString& Asset, const FArcPropertyJsonReport& Report)
	{
		return MakeResult(false, Asset, Report, nullptr, TArray<FArcTQSCompilerDiagnostic>());
	}

	TSharedRef<FJsonObject> MakeApplyFailure(const FString& Asset, const FArcPropertyJsonReport& Report, const bool bDryRun)
	{
		TSharedRef<FJsonObject> Result = MakeFailure(Asset, Report);
		Result->SetBoolField(TEXT("dryRun"), bDryRun);
		Result->SetBoolField(TEXT("created"), false);
		return Result;
	}

	UArcTQSQueryDefinition* LoadDefinition(const FArcAssetPath& Path, FArcPropertyJsonReport& Report)
	{
		UObject* Object = LoadObject<UObject>(nullptr, *Path.ObjectPath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
		UArcTQSQueryDefinition* Definition = Cast<UArcTQSQueryDefinition>(Object);
		if (!Definition)
		{
			Report.Error(Object
				? FString::Printf(TEXT("'%s' is a %s, not an ArcTQSQueryDefinition."), *Path.ObjectPath, *Object->GetClass()->GetName())
				: FString::Printf(TEXT("ArcTQSQueryDefinition '%s' was not found."), *Path.ObjectPath));
		}
		return Definition;
	}

	struct FEndpointText
	{
		EArcTQSDataKind Kind = EArcTQSDataKind::Invalid;
		int32 Index = INDEX_NONE;
		FString Name;
		FPropertyBindingPath Segments;
	};

	struct FFunctionScope
	{
		FGuid FunctionID;
		EArcTQSDataKind RootKind = EArcTQSDataKind::Invalid;
		FString Name;
	};

	class FApplier
	{
	public:
		FApplier(UArcTQSQueryDefinition& InScratch, FArcPropertyJsonReport& InReport, const UPropertyBag* InReuseBag)
			: Scratch(InScratch)
			, Report(InReport)
			, ReuseBag(InReuseBag)
		{
		}

		bool bDryRun = false;
		bool bSave = false;

		void Run(const FJsonObject& Root)
		{
			ArcPropertyJson::CheckKeys(
				Root,
				{
					TEXT("version"), TEXT("options"), TEXT("parameters"), TEXT("selection"), TEXT("generator"), TEXT("steps"), TEXT("bindings"), TEXT("warnings")
				},
				TEXT("root"),
				Report);

			const TSharedPtr<FJsonValue> VersionValue = FindField(Root, TEXT("version"));
			int32 Version = 0;
			if (!VersionValue.IsValid() || !VersionValue->TryGetNumber(Version) || Version != FormatVersion)
			{
				Report.Error(FString::Printf(TEXT("root: 'version' must be %d."), FormatVersion));
			}

			const TSharedPtr<FJsonObject> Options = ArcPropertyJson::GetObjectValue(Root, TEXT("options"), TEXT("root"), Report);
			if (Options.IsValid())
			{
				ArcPropertyJson::CheckKeys(*Options, {TEXT("dryRun"), TEXT("save")}, TEXT("options"), Report);
				bDryRun = ArcPropertyJson::GetBool(*Options, TEXT("dryRun"), TEXT("options"), Report);
				bSave = ArcPropertyJson::GetBool(*Options, TEXT("save"), TEXT("options"), Report);
			}

			ArcPropertyJson::ImportBag(
				ArcPropertyJson::GetArrayValue(Root, TEXT("parameters"), TEXT("root"), Report),
				Scratch.Parameters,
				TEXT("parameters"),
				Report,
				GetHooks(),
				ReuseBag);
			ParseSelection(Root);
			ParseOptionalNode(Root, TEXT("generator"), ERole::Generator, Scratch.Generator);
			const TArray<TSharedPtr<FJsonValue>> Steps = ArcPropertyJson::GetArrayValue(Root, TEXT("steps"), TEXT("root"), Report);
			for (int32 Index = 0; Index < Steps.Num(); ++Index)
			{
				ParseNode(Steps[Index], ERole::Step, FString::Printf(TEXT("steps[%d]"), Index), Scratch.Steps.AddDefaulted_GetRef());
			}
			if (Report.Errors.Num() > 0)
			{
				return;
			}

			Scratch.RepairNodeIDs();
			const TArray<TSharedPtr<FJsonValue>> Bindings = ArcPropertyJson::GetArrayValue(Root, TEXT("bindings"), TEXT("root"), Report);
			for (int32 Index = 0; Index < Bindings.Num(); ++Index)
			{
				ParseBindingValue(Bindings[Index], FString::Printf(TEXT("bindings[%d]"), Index), nullptr);
			}
			if (Report.Errors.Num() == 0 && Scratch.EditorBindings.GetNumBindings() != ExpectedBindings)
			{
				Report.Error(FString::Printf(
					TEXT("%d bindings were authored but the definition holds %d."),
					ExpectedBindings,
					Scratch.EditorBindings.GetNumBindings()));
			}
		}

	private:
		UArcTQSQueryDefinition& Scratch;
		FArcPropertyJsonReport& Report;
		const UPropertyBag* ReuseBag = nullptr;
		TMap<FString, FString> StoredTargets;
		int32 ExpectedBindings = 0;

		bool GetNumber(const FJsonObject& Owner, const TCHAR* Key, const FString& Location, double& OutValue)
		{
			const TSharedPtr<FJsonValue> Value = FindField(Owner, Key);
			if (!Value.IsValid() || Value->Type == EJson::Null)
			{
				return false;
			}
			if (Value->Type != EJson::Number || !Value->TryGetNumber(OutValue))
			{
				Report.Error(FString::Printf(TEXT("%s.%s: expected a number."), *Location, Key));
				return false;
			}
			return true;
		}

		void ParseSelection(const FJsonObject& Root)
		{
			const TSharedPtr<FJsonObject> Selection = ArcPropertyJson::GetObjectValue(Root, TEXT("selection"), TEXT("root"), Report);
			if (!Selection.IsValid())
			{
				return;
			}
			const FString Location(TEXT("selection"));
			ArcPropertyJson::CheckKeys(
				*Selection,
				{TEXT("mode"), TEXT("topN"), TEXT("minPassingScore"), TEXT("topPercent")},
				Location,
				Report);

			FString ModeText;
			if (ArcPropertyJson::GetString(*Selection, TEXT("mode"), Location, ModeText, Report))
			{
				const UEnum* ModeEnum = StaticEnum<EArcTQSSelectionMode>();
				const int64 ModeValue = ModeEnum->GetValueByNameString(ModeText);
				if (ModeValue == INDEX_NONE)
				{
					TArray<FString> Names;
					for (int32 Index = 0; Index < ModeEnum->NumEnums() - 1; ++Index)
					{
						Names.Add(ModeEnum->GetNameStringByIndex(Index));
					}
					Report.Error(FString::Printf(
						TEXT("selection.mode: unknown mode '%s' (use %s)."),
						*ModeText,
						*FString::Join(Names, TEXT(", "))));
				}
				else
				{
					Scratch.SelectionMode = static_cast<EArcTQSSelectionMode>(ModeValue);
				}
			}

			double Number = 0.0;
			if (GetNumber(*Selection, TEXT("topN"), Location, Number))
			{
				if (Number < 1.0 || Number != FMath::FloorToDouble(Number) || Number > static_cast<double>(MAX_int32))
				{
					Report.Error(TEXT("selection.topN: must be an integer of at least 1."));
				}
				else
				{
					Scratch.TopN = static_cast<int32>(Number);
				}
			}
			if (GetNumber(*Selection, TEXT("minPassingScore"), Location, Number))
			{
				Scratch.MinPassingScore = static_cast<float>(Number);
			}
			if (GetNumber(*Selection, TEXT("topPercent"), Location, Number))
			{
				if (Number < 1.0 || Number > 100.0)
				{
					Report.Error(TEXT("selection.topPercent: must be between 1 and 100."));
				}
				else
				{
					Scratch.TopPercent = static_cast<float>(Number);
				}
			}
		}

		void RejectReadOnlyFields(const UScriptStruct& Struct, const FJsonObject& Data, const FString& Location)
		{
			for (TFieldIterator<FProperty> It(&Struct); It; ++It)
			{
				const FProperty* Property = *It;
				if (IsReadOnlyProperty(*Property) && FindField(Data, Struct.GetAuthoredNameForField(Property)).IsValid())
				{
					Report.Error(FString::Printf(
						TEXT("%s.%s: the field is read-only (a function output or a visible-only field)."),
						*Location,
						*Property->GetName()));
				}
			}
		}

		const UScriptStruct* ResolveNodeType(
			const FJsonObject& Object,
			const ERole Role,
			const FString& Location)
		{
			FString TypeName;
			if (!ArcPropertyJson::GetString(Object, TEXT("type"), Location, TypeName, Report) || TypeName.IsEmpty())
			{
				Report.Error(FString::Printf(TEXT("%s: 'type' is required."), *Location));
				return nullptr;
			}
			const UScriptStruct* Type = FindType<UScriptStruct>(TypeName);
			if (!Type)
			{
				Report.Error(FString::Printf(TEXT("%s: unknown node type '%s'."), *Location, *TypeName));
				return nullptr;
			}
			if (GetStructRole(Type) != Role)
			{
				Report.Error(FString::Printf(
					TEXT("%s: type '%s' is not a %s (derive from %s)."),
					*Location,
					*TypeName,
					GetRoleName(Role),
					*GetRoleBase(Role)->GetName()));
				return nullptr;
			}
			if (Role == ERole::Function)
			{
				int32 NumOutputs = 0;
				ArcTQS::FindFunctionOutputProperty(Type, &NumOutputs);
				if (NumOutputs != 1)
				{
					Report.Error(FString::Printf(
						TEXT("%s: function '%s' must declare exactly one ArcTQSOutput field (it has %d)."),
						*Location,
						*TypeName,
						NumOutputs));
					return nullptr;
				}
			}
			return Type;
		}

		bool ImportNodeData(const FJsonObject& Object, FInstancedStruct& Node, const FString& Location)
		{
			const TSharedPtr<FJsonObject> Data = ArcPropertyJson::GetObjectValue(Object, TEXT("data"), Location, Report);
			if (!Data.IsValid())
			{
				return true;
			}
			const int32 ErrorCount = Report.Errors.Num();
			RejectReadOnlyFields(*Node.GetScriptStruct(), *Data, Location + TEXT(".data"));
			if (Report.Errors.Num() != ErrorCount)
			{
				return false;
			}
			return ArcPropertyJson::ImportStruct(
				*Node.GetScriptStruct(),
				Node.GetMutableMemory(),
				Data,
				Location + TEXT(".data"),
				Report,
				GetHooks());
		}

		void ParseNode(const TSharedPtr<FJsonValue>& Value, const ERole Role, const FString& Location, FInstancedStruct& OutNode)
		{
			const TSharedPtr<FJsonObject> Object = ArcPropertyJson::AsObject(Value, Location, Report);
			if (!Object.IsValid())
			{
				return;
			}
			ArcPropertyJson::CheckKeys(*Object, {TEXT("type"), TEXT("data")}, Location, Report);
			const UScriptStruct* Type = ResolveNodeType(*Object, Role, Location);
			if (!Type)
			{
				return;
			}
			OutNode.InitializeAs(Type);
			ImportNodeData(*Object, OutNode, Location);
		}

		void ParseOptionalNode(const FJsonObject& Root, const TCHAR* Key, const ERole Role, FInstancedStruct& OutNode)
		{
			const TSharedPtr<FJsonValue> Value = FindField(Root, Key);
			if (!Value.IsValid() || Value->Type == EJson::Null)
			{
				OutNode.Reset();
				return;
			}
			ParseNode(Value, Role, Key, OutNode);
		}

		bool ParseEndpoint(const FString& InText, const bool bTarget, const FString& Location, FEndpointText& Out)
		{
			const FString Text = InText.TrimStartAndEnd();
			FString Rest;
			const FString StepsPrefix = FString(StepsName) + TEXT("[");
			if (Text.StartsWith(StepsPrefix, ESearchCase::IgnoreCase))
			{
				int32 Close = INDEX_NONE;
				const FString IndexText = Text.FindChar(TEXT(']'), Close) ? Text.Mid(StepsPrefix.Len(), Close - StepsPrefix.Len()) : FString();
				if (Close == INDEX_NONE || IndexText.IsEmpty() || !IndexText.IsNumeric() || IndexText.Contains(TEXT("-")) || IndexText.Contains(TEXT(".")))
				{
					Report.Error(FString::Printf(TEXT("%s: '%s' needs a step index, e.g. Steps[0].Field."), *Location, *Text));
					return false;
				}
				Out.Kind = EArcTQSDataKind::Step;
				Out.Index = FCString::Atoi(*IndexText);
				Out.Name = FString::Printf(TEXT("%s[%d]"), StepsName, Out.Index);
				Rest = Text.Mid(Close + 1);
				if (!Rest.IsEmpty())
				{
					if (Rest[0] != TEXT('.'))
					{
						Report.Error(FString::Printf(TEXT("%s: expected '.' after '%s'."), *Location, *Out.Name));
						return false;
					}
					Rest = Rest.Mid(1);
				}
			}
			else
			{
				FString Head = Text;
				int32 Dot = INDEX_NONE;
				if (Text.FindChar(TEXT('.'), Dot))
				{
					Head = Text.Left(Dot);
					Rest = Text.Mid(Dot + 1);
				}
				Head.TrimStartAndEndInline();
				const TPair<const TCHAR*, EArcTQSDataKind> Names[] = {
					{GeneratorName, EArcTQSDataKind::Generator},
					{ParametersName, EArcTQSDataKind::Parameters},
					{QuerierName, EArcTQSDataKind::Querier},
					{ItemName, EArcTQSDataKind::Item},
				};
				for (const TPair<const TCHAR*, EArcTQSDataKind>& Name : Names)
				{
					if (Head.Equals(Name.Key, ESearchCase::IgnoreCase))
					{
						Out.Kind = Name.Value;
						Out.Name = Name.Key;
						break;
					}
				}
				if (Out.Kind == EArcTQSDataKind::Invalid)
				{
					Report.Error(FString::Printf(
						TEXT("%s: unknown endpoint '%s' (targets: Generator, Steps[i]; sources: Parameters, Querier, Item)."),
						*Location,
						*Head));
					return false;
				}
			}
			if (bTarget && !IsRootKind(Out.Kind))
			{
				Report.Error(FString::Printf(
					TEXT("%s: '%s' cannot be a binding target (use Generator or Steps[i]; function inputs are set inside the function object)."),
					*Location,
					*Out.Name));
				return false;
			}
			if (!bTarget && !IsSourceKind(Out.Kind))
			{
				Report.Error(FString::Printf(
					TEXT("%s: '%s' cannot be a binding source (use Parameters, Querier or Item; a function is written as an object)."),
					*Location,
					*Out.Name));
				return false;
			}
			Rest.TrimStartAndEndInline();
			if (!Out.Segments.FromString(Rest))
			{
				Report.Error(FString::Printf(TEXT("%s: invalid property path '%s'."), *Location, *Rest));
				return false;
			}
			return true;
		}

		bool ResolveEndpointID(const FEndpointText& Endpoint, const FString& Location, FGuid& OutID)
		{
			OutID = GetEndpointID(Scratch, Endpoint.Kind, Endpoint.Index);
			if (OutID.IsValid())
			{
				return true;
			}
			if (Endpoint.Kind == EArcTQSDataKind::Step)
			{
				Report.Error(FString::Printf(
					TEXT("%s: '%s' does not exist (the definition has %d steps)."),
					*Location,
					*Endpoint.Name,
					Scratch.Steps.Num()));
			}
			else
			{
				Report.Error(FString::Printf(TEXT("%s: '%s' is not set."), *Location, *Endpoint.Name));
			}
			return false;
		}

		bool ResolvePath(
			const FGuid& ID,
			const FPropertyBindingPath& Segments,
			const FString& EndpointName,
			const bool bRequireField,
			const FString& Location,
			FPropertyBindingPath& OutPath)
		{
			OutPath = FPropertyBindingPath(ID, Segments.GetSegments());
			if (OutPath.IsPathEmpty())
			{
				if (bRequireField)
				{
					Report.Error(FString::Printf(TEXT("%s: the target '%s' needs a field path."), *Location, *EndpointName));
					return false;
				}
				return true;
			}
			FPropertyBindingDataView View;
			if (!Scratch.GetBindingDataViewByID(ID, View))
			{
				Report.Error(FString::Printf(
					TEXT("%s: '%s' has no data to bind (empty parameter bag?)."),
					*Location,
					*EndpointName));
				return false;
			}
			FString PathError;
			if (!OutPath.UpdateSegmentsFromValue(View, &PathError))
			{
				Report.Error(FString::Printf(
					TEXT("%s: path '%s' does not resolve on %s: %s"),
					*Location,
					*PathToString(Segments),
					*EndpointName,
					*PathError));
				return false;
			}
			return true;
		}

		bool ResolveTarget(
			const FString& ToText,
			const FFunctionScope* Scope,
			const FString& Location,
			FPropertyBindingPath& OutPath,
			EArcTQSDataKind& OutRootKind,
			FString& OutName)
		{
			if (Scope)
			{
				FPropertyBindingPath Segments;
				const FString Trimmed = ToText.TrimStartAndEnd();
				if (!Segments.FromString(Trimmed))
				{
					Report.Error(FString::Printf(TEXT("%s: invalid property path '%s'."), *Location, *Trimmed));
					return false;
				}
				OutRootKind = Scope->RootKind;
				OutName = Scope->Name + TEXT("/") + Trimmed;
				return ResolvePath(Scope->FunctionID, Segments, Scope->Name, true, Location, OutPath)
					&& RejectLiteralOnlyTarget(OutPath, OutName, Location);
			}
			FEndpointText To;
			FGuid TargetID;
			if (!ParseEndpoint(ToText, true, Location, To) || !ResolveEndpointID(To, Location, TargetID))
			{
				return false;
			}
			OutRootKind = To.Kind;
			OutName = JoinEndpoint(To.Name, To.Segments);
			return ResolvePath(TargetID, To.Segments, To.Name, true, Location, OutPath)
				&& RejectLiteralOnlyTarget(OutPath, OutName, Location);
		}

		bool RejectLiteralOnlyTarget(const FPropertyBindingPath& TargetPath, const FString& TargetName, const FString& Location)
		{
			FPropertyBindingDataView View;
			TArray<FPropertyBindingPathIndirection> Indirections;
			if (!Scratch.GetBindingDataViewByID(TargetPath.GetStructID(), View) || !TargetPath.ResolveIndirectionsWithValue(View, Indirections))
			{
				return true;
			}
			for (const FPropertyBindingPathIndirection& Indirection : Indirections)
			{
				const FProperty* Property = Indirection.GetProperty();
				if (Property && ArcTQS::IsLiteralOnlyProperty(*Property))
				{
					Report.Error(FString::Printf(
						TEXT("%s: '%s' is a memory key and accepts only a literal value in 'data'."),
						*Location,
						*TargetName));
					return false;
				}
			}
			return true;
		}

		bool RegisterTarget(const FPropertyBindingPath& TargetPath, const FString& Text, const FString& Location)
		{
			const FString Key = TargetPath.GetStructID().ToString() + TEXT("|") + PathToString(TargetPath);
			if (const FString* Existing = StoredTargets.Find(Key))
			{
				Report.Error(FString::Printf(
					TEXT("%s: binds the same target as '%s'; each target path takes one binding."),
					*Location,
					**Existing));
				return false;
			}
			StoredTargets.Add(Key, Text);
			return true;
		}

		void ParseBindingValue(const TSharedPtr<FJsonValue>& Value, const FString& Location, const FFunctionScope* Scope)
		{
			FString Text;
			if (Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(Text))
			{
				ParseStringBinding(Text, Location, Scope);
				return;
			}
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object->IsValid())
			{
				ParseFunctionBinding(**Object, Location, Scope);
				return;
			}
			Report.Error(FString::Printf(
				TEXT("%s: expected a string 'to <- from' or a function object {\"to\", \"function\"}."),
				*Location));
		}

		void ParseStringBinding(const FString& Text, const FString& Location, const FFunctionScope* Scope)
		{
			const FString BindingLocation = FString::Printf(TEXT("%s '%s'"), *Location, *Text);
			FString ToText;
			FString FromText;
			if (!Text.Split(TEXT("<-"), &ToText, &FromText) || FromText.Contains(TEXT("<-")))
			{
				Report.Error(FString::Printf(TEXT("%s: expected exactly one 'to <- from'."), *BindingLocation));
				return;
			}
			FPropertyBindingPath TargetPath;
			EArcTQSDataKind RootKind = EArcTQSDataKind::Invalid;
			FString TargetName;
			if (!ResolveTarget(ToText, Scope, BindingLocation, TargetPath, RootKind, TargetName))
			{
				return;
			}
			FEndpointText From;
			FGuid SourceID;
			if (!ParseEndpoint(FromText, false, BindingLocation, From) || !ResolveEndpointID(From, BindingLocation, SourceID))
			{
				return;
			}
			if (!ArcTQS::IsBindingSourceAccessible(From.Kind, RootKind))
			{
				Report.Error(FString::Printf(
					TEXT("%s: '%s' cannot be read here (Generator reads Parameters and Querier; Steps also read Item)."),
					*BindingLocation,
					*From.Name));
				return;
			}
			FPropertyBindingPath SourcePath;
			if (!ResolvePath(SourceID, From.Segments, From.Name, false, BindingLocation, SourcePath)
				|| !RegisterTarget(TargetPath, Text, BindingLocation))
			{
				return;
			}
			Scratch.EditorBindings.AddCompiledBinding(FArcTQSBinding(SourcePath, TargetPath));
			++ExpectedBindings;
		}

		void ParseFunctionBinding(const FJsonObject& Object, const FString& Location, const FFunctionScope* Scope)
		{
			ArcPropertyJson::CheckKeys(Object, {TEXT("to"), TEXT("function")}, Location, Report);
			FString ToText;
			if (!ArcPropertyJson::GetString(Object, TEXT("to"), Location, ToText, Report) || ToText.TrimStartAndEnd().IsEmpty())
			{
				Report.Error(FString::Printf(TEXT("%s: 'to' is required."), *Location));
				return;
			}
			const FString BindingLocation = FString::Printf(TEXT("%s (to '%s')"), *Location, *ToText);
			FPropertyBindingPath TargetPath;
			EArcTQSDataKind RootKind = EArcTQSDataKind::Invalid;
			FString TargetName;
			if (!ResolveTarget(ToText, Scope, BindingLocation, TargetPath, RootKind, TargetName))
			{
				return;
			}
			const FString FunctionLocation = BindingLocation + TEXT(".function");
			const TSharedPtr<FJsonObject> FunctionObject = ArcPropertyJson::GetObjectValue(Object, TEXT("function"), BindingLocation, Report);
			if (!FunctionObject.IsValid())
			{
				Report.Error(FString::Printf(TEXT("%s: 'function' is required."), *BindingLocation));
				return;
			}
			ArcPropertyJson::CheckKeys(*FunctionObject, {TEXT("type"), TEXT("data"), TEXT("bindings")}, FunctionLocation, Report);
			const UScriptStruct* Type = ResolveNodeType(*FunctionObject, ERole::Function, FunctionLocation);
			if (!Type)
			{
				return;
			}
			FInstancedStruct Node;
			Node.InitializeAs(Type);
			if (!ImportNodeData(*FunctionObject, Node, FunctionLocation))
			{
				return;
			}
			const FGuid FunctionID = FGuid::NewGuid();
			Node.GetMutable<FArcTQSPropertyFunction>().ID = FunctionID;
			const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(Type);
			FPropertyBindingPath SourcePath(FunctionID, OutputProperty->GetFName());
			FString PathError;
			if (!SourcePath.UpdateSegmentsFromValue(FPropertyBindingDataView(Type, Node.GetMutableMemory()), &PathError))
			{
				Report.Error(FString::Printf(TEXT("%s: the function output does not resolve: %s"), *FunctionLocation, *PathError));
				return;
			}
			if (!RegisterTarget(TargetPath, FString::Printf(TEXT("%s <- function %s"), *TargetName, *Type->GetName()), BindingLocation))
			{
				return;
			}
			Scratch.EditorBindings.AddCompiledBinding(FArcTQSBinding(FConstStructView(Node), SourcePath, TargetPath));
			++ExpectedBindings;

			FFunctionScope InnerScope;
			InnerScope.FunctionID = FunctionID;
			InnerScope.RootKind = RootKind;
			InnerScope.Name = TargetName;
			const TArray<TSharedPtr<FJsonValue>> Inputs = ArcPropertyJson::GetArrayValue(*FunctionObject, TEXT("bindings"), FunctionLocation, Report);
			for (int32 Index = 0; Index < Inputs.Num(); ++Index)
			{
				ParseBindingValue(Inputs[Index], FString::Printf(TEXT("%s.bindings[%d]"), *FunctionLocation, Index), &InnerScope);
			}
		}
	};

	void CopyAuthoringData(const UArcTQSQueryDefinition& Source, UArcTQSQueryDefinition& Target)
	{
		Target.ParametersID = Source.ParametersID;
		Target.QuerierID = Source.QuerierID;
		Target.ItemID = Source.ItemID;
		Target.Parameters = Source.Parameters;
		Target.Generator = Source.Generator;
		Target.Steps = Source.Steps;
		Target.SelectionMode = Source.SelectionMode;
		Target.TopN = Source.TopN;
		Target.MinPassingScore = Source.MinPassingScore;
		Target.TopPercent = Source.TopPercent;
		Target.EditorBindings.RemoveBindings([](FPropertyBindingBinding&) -> bool
		{
			return true;
		});
		for (const FArcTQSBinding& Binding : Source.EditorBindings.GetBindings())
		{
			Target.EditorBindings.AddCompiledBinding(FArcTQSBinding(Binding));
		}
	}

	int32 CountErrors(const TArray<FArcTQSCompilerDiagnostic>& Diagnostics)
	{
		int32 Count = 0;
		for (const FArcTQSCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			Count += Diagnostic.bIsError ? 1 : 0;
		}
		return Count;
	}

	void ReportSchemaChange(const UArcTQSQueryDefinition& Definition, FArcPropertyJsonReport& Report)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(Definition.GetPackage()->GetFName(), Referencers);
		TArray<FString> Names;
		for (const FName& Referencer : Referencers)
		{
			Names.Add(Referencer.ToString());
		}
		Names.Sort();
		Report.Warning(FString::Printf(
			TEXT("The parameter schema changed, so existing references to this definition no longer match its layout. Re-apply or recompile each ArcTactics config that uses it (ApplyTacticsConfig / CompileTacticsConfig) and re-set every other reference (SetTQSReference). Referencing packages: %s."),
			Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("none found in the asset registry")));
	}

	TSharedRef<FJsonObject> Apply(
		UArcTQSQueryDefinition* Target,
		const FArcAssetPath& Path,
		const TSharedRef<FJsonObject>& Json)
	{
		FArcPropertyJsonReport Report;
		const FString AssetName = Target ? Target->GetPathName() : Path.ObjectPath;
		const TSharedRef<FJsonObject> Root = CloneObject(*Json);
		TStrongObjectPtr<UArcTQSQueryDefinition> Scratch(
			NewObject<UArcTQSQueryDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		if (Target)
		{
			Scratch->ParametersID = Target->ParametersID;
			Scratch->QuerierID = Target->QuerierID;
			Scratch->ItemID = Target->ItemID;
		}

		FApplier Applier(*Scratch, Report, Target ? Target->Parameters.GetPropertyBagStruct() : nullptr);
		Applier.Run(*Root);
		if (Report.Errors.Num() > 0)
		{
			return MakeApplyFailure(AssetName, Report, Applier.bDryRun);
		}

		TArray<FArcTQSCompilerDiagnostic> Diagnostics;
		Scratch->CompileEditorData(Diagnostics);
		const int32 NumErrors = CountErrors(Diagnostics);
		if (NumErrors > 0)
		{
			Report.Error(FString::Printf(
				TEXT("The definition does not compile (%d errors, see diagnostics); nothing was changed."),
				NumErrors));
			TSharedRef<FJsonObject> Result = MakeResult(false, AssetName, Report, Scratch.Get(), Diagnostics);
			Result->SetBoolField(TEXT("dryRun"), Applier.bDryRun);
			Result->SetBoolField(TEXT("created"), false);
			Result->SetNumberField(TEXT("revision"), 0);
			return Result;
		}

		if (Applier.bDryRun)
		{
			if (Applier.bSave)
			{
				Report.Warning(TEXT("options.save is ignored for a dry run."));
			}
			TSharedRef<FJsonObject> Result = MakeResult(true, AssetName, Report, Scratch.Get(), Diagnostics);
			Result->SetBoolField(TEXT("dryRun"), true);
			Result->SetBoolField(TEXT("created"), false);
			Result->SetNumberField(TEXT("revision"), 0);
			return Result;
		}

		bool bCreated = false;
		const UPropertyBag* PreviousBag = Target ? Target->Parameters.GetPropertyBagStruct() : nullptr;
		if (!Target)
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
			Target = Cast<UArcTQSQueryDefinition>(AssetTools.CreateAsset(
				Path.AssetName,
				FPackageName::GetLongPackagePath(Path.PackageName),
				UArcTQSQueryDefinition::StaticClass(),
				nullptr));
			if (!Target)
			{
				Report.Error(FString::Printf(TEXT("Failed to create ArcTQSQueryDefinition '%s'."), *Path.ObjectPath));
				return MakeApplyFailure(AssetName, Report, false);
			}
			bCreated = true;
		}

		{
			const FScopedTransaction Transaction(LOCTEXT("ApplyTransaction", "Apply Target Query Definition"));
			Target->Modify();
			CopyAuthoringData(*Scratch, *Target);
			Target->RepairNodeIDs();
			if (Target->EditorBindings.GetNumBindings() != Scratch->EditorBindings.GetNumBindings())
			{
				Report.Warning(FString::Printf(
					TEXT("%d bindings were removed after the commit."),
					Scratch->EditorBindings.GetNumBindings() - Target->EditorBindings.GetNumBindings()));
			}
			Target->CompileEditorData(Diagnostics);
			ArcTQS::Delegates::OnPostCompile.Broadcast(*Target);
			Target->MarkPackageDirty();
		}

		if (!bCreated && Target->Parameters.GetPropertyBagStruct() != PreviousBag)
		{
			ReportSchemaChange(*Target, Report);
		}

		if (Applier.bSave && !UEditorLoadingAndSavingUtils::SavePackages({Target->GetPackage()}, false))
		{
			Report.Warning(FString::Printf(TEXT("Saving '%s' failed."), *Target->GetPackage()->GetName()));
		}

		TSharedRef<FJsonObject> Result = MakeResult(true, Target->GetPathName(), Report, Target, Diagnostics);
		Result->SetBoolField(TEXT("dryRun"), false);
		Result->SetBoolField(TEXT("created"), bCreated);
		return Result;
	}

	TArray<TSharedPtr<FJsonValue>> DescribeFields(const UScriptStruct* Struct, const void* DefaultMemory, const bool bFunction)
	{
		TArray<TSharedPtr<FJsonValue>> Fields;
		if (!Struct)
		{
			return Fields;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Deprecated))
			{
				continue;
			}
			Fields.Add(MakeShared<FJsonValueObject>(ArcPropertyJson::DescribeField(
				*Property,
				DefaultMemory,
				GetHooks(),
				[Property, bFunction](FJsonObject& Field)
				{
					Field.SetBoolField(TEXT("required"), ArcTQS::IsRequiredBinding(*Property));
					if (bFunction)
					{
						Field.SetBoolField(TEXT("output"), ArcTQS::IsFunctionOutputProperty(*Property));
					}
					if (Property->HasMetaData(TEXT("ArcMemoryAccess")))
					{
						Field.SetStringField(TEXT("memoryAccess"), Property->GetMetaData(TEXT("ArcMemoryAccess")));
					}
					if (Property->HasMetaData(TEXT("ArcMemoryType")))
					{
						Field.SetStringField(TEXT("memoryType"), Property->GetMetaData(TEXT("ArcMemoryType")));
					}
				})));
		}
		return Fields;
	}

	TSharedRef<FJsonObject> DescribeSource(const TCHAR* Name, const UScriptStruct* Struct, const TArray<const TCHAR*>& ReadableBy)
	{
		TSharedRef<FJsonObject> Source = MakeShared<FJsonObject>();
		Source->SetStringField(TEXT("name"), Name);
		TArray<TSharedPtr<FJsonValue>> Readers;
		for (const TCHAR* Reader : ReadableBy)
		{
			Readers.Add(MakeShared<FJsonValueString>(Reader));
		}
		Source->SetArrayField(TEXT("readableBy"), Readers);
		if (Struct)
		{
			TArray<TSharedPtr<FJsonValue>> Fields;
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FString ExtendedType;
				const FString CppType = It->GetCPPType(&ExtendedType, 0);
				TSharedRef<FJsonObject> Field = MakeShared<FJsonObject>();
				Field->SetStringField(TEXT("name"), It->GetName());
				Field->SetStringField(TEXT("type"), CppType + ExtendedType);
				Fields.Add(MakeShared<FJsonValueObject>(Field));
			}
			Source->SetArrayField(TEXT("fields"), Fields);
		}
		return Source;
	}

	UObject* ResolveReferenceOwner(const FString& InPath, UBlueprint*& OutBlueprint, FArcPropertyJsonReport& Report)
	{
		OutBlueprint = nullptr;
		const FString Path = InPath.TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			Report.Error(TEXT("ObjectPath is required (e.g. /ArcRPG/Enemies/BP_TestEnemySpawner)."));
			return nullptr;
		}
		int32 Dot = INDEX_NONE;
		const FString ObjectPath = Path.FindChar(TEXT('.'), Dot) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
		UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
		if (!Object)
		{
			Report.Error(FString::Printf(TEXT("Object '%s' was not found."), *ObjectPath));
			return nullptr;
		}
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			if (!Blueprint->GeneratedClass)
			{
				Report.Error(FString::Printf(TEXT("Blueprint '%s' has no generated class; compile it first."), *ObjectPath));
				return nullptr;
			}
			OutBlueprint = Blueprint;
			return Blueprint->GeneratedClass->GetDefaultObject();
		}
		if (UClass* Class = Cast<UClass>(Object))
		{
			OutBlueprint = UBlueprint::GetBlueprintFromClass(Class);
			return Class->GetDefaultObject();
		}
		return Object;
	}

	TArray<FString> ListReferenceProperties(const UClass* Class)
	{
		TArray<FString> Names;
		for (TFieldIterator<FStructProperty> It(Class); It; ++It)
		{
			if (It->Struct == FArcTQSQueryReference::StaticStruct())
			{
				Names.Add(It->GetName());
			}
		}
		return Names;
	}

	TSharedRef<FJsonObject> MakeReferenceResult(
		const bool bSuccess,
		const UObject* Owner,
		const FString& Property,
		const FArcPropertyJsonReport& Report)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("object"), Owner ? Owner->GetPathName() : FString());
		Result->SetStringField(TEXT("property"), Property);
		Result->SetArrayField(TEXT("errors"), ToJsonStrings(Report.Errors));
		Result->SetArrayField(TEXT("warnings"), ToJsonStrings(Report.Warnings));
		return Result;
	}

	void WriteReference(UObject& Object, FStructProperty& Property, const FArcTQSQueryReference& Value)
	{
		Object.Modify();
		Object.PreEditChange(&Property);
		*Property.ContainerPtrToValuePtr<FArcTQSQueryReference>(&Object) = Value;
		FPropertyChangedEvent Event(&Property, EPropertyChangeType::ValueSet);
		Object.PostEditChangeProperty(Event);
		Object.MarkPackageDirty();
	}
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::ListNodeTypes(const FString& Role, const FString& Filter)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	FArcPropertyJsonReport Report;
	ERole RequestedRole = ERole::Invalid;
	if (!ParseRole(Role, RequestedRole))
	{
		Report.Error(FString::Printf(
			TEXT("Unknown role '%s' (use Generator, Step, Function or empty)."),
			*Role));
		return MakeFailure(FString(), Report);
	}

	struct FEntry
	{
		ERole Role;
		FString Name;
		TSharedRef<FJsonObject> Json;
	};
	TArray<FEntry> Entries;
	const FString TrimmedFilter = Filter.TrimStartAndEnd();
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		const ERole StructRole = GetStructRole(Struct);
		if (StructRole == ERole::Invalid
			|| (RequestedRole != ERole::Invalid && StructRole != RequestedRole)
			|| Struct->HasAnyFlags(RF_NewerVersionExists)
			|| Struct->HasMetaData(TEXT("Hidden")))
		{
			continue;
		}
		const FProperty* OutputProperty = nullptr;
		if (StructRole == ERole::Function)
		{
			int32 NumOutputs = 0;
			OutputProperty = ArcTQS::FindFunctionOutputProperty(Struct, &NumOutputs);
			if (NumOutputs != 1)
			{
				continue;
			}
		}
		const FString Name = GetTypeName<UScriptStruct>(Struct);
		const FString DisplayName = Struct->GetDisplayNameText().ToString();
		if (!TrimmedFilter.IsEmpty() && !Name.Contains(TrimmedFilter) && !DisplayName.Contains(TrimmedFilter))
		{
			continue;
		}
		FInstancedStruct Probe;
		Probe.InitializeAs(Struct);
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Name);
		Json->SetStringField(TEXT("displayName"), DisplayName);
		Json->SetStringField(TEXT("role"), GetRoleName(StructRole));
		const FString& Tooltip = Struct->GetMetaData(TEXT("ToolTip"));
		if (!Tooltip.IsEmpty())
		{
			Json->SetStringField(TEXT("description"), Tooltip);
		}
		if (OutputProperty)
		{
			FString ExtendedType;
			const FString CppType = OutputProperty->GetCPPType(&ExtendedType, 0);
			Json->SetStringField(TEXT("output"), OutputProperty->GetName());
			Json->SetStringField(TEXT("outputType"), CppType + ExtendedType);
		}
		Json->SetArrayField(TEXT("fields"), DescribeFields(Struct, Probe.GetMemory(), StructRole == ERole::Function));
		Entries.Add({StructRole, Name, Json});
	}
	Entries.Sort([](const FEntry& A, const FEntry& B) -> bool
	{
		return A.Role != B.Role ? A.Role < B.Role : A.Name < B.Name;
	});

	TArray<TSharedPtr<FJsonValue>> Types;
	for (const FEntry& Entry : Entries)
	{
		Types.Add(MakeShared<FJsonValueObject>(Entry.Json));
	}
	TArray<TSharedPtr<FJsonValue>> Sources;
	Sources.Add(MakeShared<FJsonValueObject>(DescribeSource(
		ParametersName, nullptr, {TEXT("Generator"), TEXT("Step")})));
	Sources.Add(MakeShared<FJsonValueObject>(DescribeSource(
		QuerierName, FArcTQSQuerierInputs::StaticStruct(), {TEXT("Generator"), TEXT("Step")})));
	Sources.Add(MakeShared<FJsonValueObject>(DescribeSource(
		ItemName, FArcTQSItemInputs::StaticStruct(), {TEXT("Step")})));

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), Types.Num());
	Result->SetArrayField(TEXT("types"), Types);
	Result->SetArrayField(TEXT("sources"), Sources);
	return Result;
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::ExportDefinition(const UArcTQSQueryDefinition& Definition)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	TArray<FString> Warnings;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), FormatVersion);
	Root->SetArrayField(TEXT("parameters"), ArcPropertyJson::ExportBag(Definition.Parameters, GetHooks()));
	Root->SetObjectField(TEXT("selection"), ExportSelection(Definition));
	Root->SetField(TEXT("generator"), ExportOptionalNode(Definition.Generator));
	TArray<TSharedPtr<FJsonValue>> Steps;
	for (const FInstancedStruct& Step : Definition.Steps)
	{
		Steps.Add(ExportOptionalNode(Step));
	}
	Root->SetArrayField(TEXT("steps"), Steps);

	struct FSortedBinding
	{
		int32 Order = 0;
		FString Path;
		TSharedPtr<FJsonValue> Value;
	};
	TArray<FSortedBinding> Sorted;
	for (const FArcTQSBinding& Binding : Definition.EditorBindings.GetBindings())
	{
		const FGuid TargetID = Binding.GetTargetPath().GetStructID();
		const FArcTQSDataHandle TargetHandle = Definition.ResolveEndpoint(TargetID);
		if (TargetHandle.GetKind() == EArcTQSDataKind::Function)
		{
			if (!IsRootKind(Definition.ResolveRootNodeKind(TargetID)))
			{
				Warnings.Add(FString::Printf(
					TEXT("Dropped binding %s: its function does not feed a query node."),
					*DescribeBinding(Definition, Binding)));
			}
			continue;
		}
		if (!IsRootKind(TargetHandle.GetKind()))
		{
			Warnings.Add(FString::Printf(
				TEXT("Dropped binding %s: the target is not Generator or a step."),
				*DescribeBinding(Definition, Binding)));
			continue;
		}
		const TSharedPtr<FJsonValue> Value = ExportBindingValue(Definition, Binding, false, 0, Warnings);
		if (!Value.IsValid())
		{
			continue;
		}
		FSortedBinding& Entry = Sorted.AddDefaulted_GetRef();
		Entry.Order = TargetHandle.GetKind() == EArcTQSDataKind::Generator ? 0 : 1 + TargetHandle.GetIndex();
		Entry.Path = PathToString(Binding.GetTargetPath());
		Entry.Value = Value;
	}
	Sorted.StableSort([](const FSortedBinding& A, const FSortedBinding& B) -> bool
	{
		return A.Order != B.Order ? A.Order < B.Order : A.Path < B.Path;
	});
	TArray<TSharedPtr<FJsonValue>> Bindings;
	for (const FSortedBinding& Entry : Sorted)
	{
		Bindings.Add(Entry.Value);
	}
	Root->SetArrayField(TEXT("bindings"), Bindings);
	if (Warnings.Num() > 0)
	{
		Root->SetArrayField(TEXT("warnings"), ToJsonStrings(Warnings));
	}
	return Root;
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::ExportAsset(const FString& AssetPath)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	FArcPropertyJsonReport Report;
	FArcAssetPath Path;
	if (!ArcPropertyJson::ParseAssetPath(AssetPath, ExampleAssetPath, Path, Report))
	{
		return MakeFailure(AssetPath, Report);
	}
	const UArcTQSQueryDefinition* Definition = LoadDefinition(Path, Report);
	return Definition ? ExportDefinition(*Definition) : MakeFailure(Path.ObjectPath, Report);
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::ApplyToAsset(const FString& AssetPath, const TSharedRef<FJsonObject>& Json)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	FArcPropertyJsonReport Report;
	FArcAssetPath Path;
	if (!ArcPropertyJson::ParseAssetPath(AssetPath, ExampleAssetPath, Path, Report))
	{
		return MakeApplyFailure(AssetPath, Report, false);
	}
	UObject* Existing = LoadObject<UObject>(nullptr, *Path.ObjectPath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
	if (Existing && !Existing->IsA<UArcTQSQueryDefinition>())
	{
		Report.Error(FString::Printf(
			TEXT("'%s' is a %s, not an ArcTQSQueryDefinition."),
			*Path.ObjectPath,
			*Existing->GetClass()->GetName()));
		return MakeApplyFailure(Path.ObjectPath, Report, false);
	}
	return Apply(Cast<UArcTQSQueryDefinition>(Existing), Path, Json);
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::CompileDefinition(UArcTQSQueryDefinition& Definition)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	TArray<FArcTQSCompilerDiagnostic> Diagnostics;
	{
		const FScopedTransaction Transaction(LOCTEXT("CompileTransaction", "Compile Target Query Definition"));
		Definition.Modify();
		Definition.RepairNodeIDs();
		Definition.CompileEditorData(Diagnostics);
		ArcTQS::Delegates::OnPostCompile.Broadcast(Definition);
		Definition.MarkPackageDirty();
	}
	return MakeResult(true, Definition.GetPathName(), FArcPropertyJsonReport(), &Definition, Diagnostics);
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::CompileAsset(const FString& AssetPath)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	FArcPropertyJsonReport Report;
	FArcAssetPath Path;
	if (!ArcPropertyJson::ParseAssetPath(AssetPath, ExampleAssetPath, Path, Report))
	{
		return MakeFailure(AssetPath, Report);
	}
	UArcTQSQueryDefinition* Definition = LoadDefinition(Path, Report);
	return Definition ? CompileDefinition(*Definition) : MakeFailure(Path.ObjectPath, Report);
}

TSharedRef<FJsonObject> ArcTQSDefinitionJson::SetQueryReference(
	const FString& ObjectPath,
	const FString& PropertyName,
	const FString& DefinitionPath,
	const TSharedPtr<FJsonObject>& Parameters)
{
	using namespace ArcTQSDefinitionJsonPrivate;
	FArcPropertyJsonReport Report;

	UBlueprint* Blueprint = nullptr;
	UObject* Owner = ResolveReferenceOwner(ObjectPath, Blueprint, Report);
	if (!Owner)
	{
		return MakeReferenceResult(false, nullptr, PropertyName, Report);
	}
	FStructProperty* Property = FindFProperty<FStructProperty>(Owner->GetClass(), FName(*PropertyName.TrimStartAndEnd()));
	if (!Property || Property->Struct != FArcTQSQueryReference::StaticStruct())
	{
		const TArray<FString> Candidates = ListReferenceProperties(Owner->GetClass());
		Report.Error(FString::Printf(
			TEXT("'%s' has no FArcTQSQueryReference property '%s' (available: %s)."),
			*Owner->GetClass()->GetName(),
			*PropertyName,
			Candidates.Num() > 0 ? *FString::Join(Candidates, TEXT(", ")) : TEXT("none")));
		return MakeReferenceResult(false, Owner, PropertyName, Report);
	}

	UArcTQSQueryDefinition* Definition = nullptr;
	if (!DefinitionPath.TrimStartAndEnd().IsEmpty())
	{
		FArcAssetPath Path;
		if (!ArcPropertyJson::ParseAssetPath(DefinitionPath, ExampleAssetPath, Path, Report))
		{
			return MakeReferenceResult(false, Owner, Property->GetName(), Report);
		}
		Definition = LoadDefinition(Path, Report);
		if (!Definition)
		{
			return MakeReferenceResult(false, Owner, Property->GetName(), Report);
		}
	}

	FArcTQSQueryReference NewValue;
	NewValue.SetDefinition(Definition);
	if (Parameters.IsValid())
	{
		const UPropertyBag* BagStruct = NewValue.GetParameters().GetPropertyBagStruct();
		void* Memory = NewValue.GetMutableParameters().GetMutableValue().GetMemory();
		for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Parameters->Values)
		{
			const FName Name(*FString(Pair.Key));
			const FPropertyBagPropertyDesc* Desc = BagStruct ? BagStruct->FindPropertyDescByName(Name) : nullptr;
			if (!Desc || !Desc->CachedProperty || !Memory)
			{
				Report.Error(FString::Printf(
					TEXT("parameters: '%s' is not a parameter of the query definition."),
					*Name.ToString()));
				continue;
			}
			if (ArcPropertyJson::ImportPropertyValue(
				*const_cast<FProperty*>(Desc->CachedProperty),
				Desc->CachedProperty->ContainerPtrToValuePtr<void>(Memory),
				Pair.Value,
				FString::Printf(TEXT("parameters.%s"), *Name.ToString()),
				Report,
				GetHooks()))
			{
				NewValue.SetPropertyOverridden(Desc->ID, true);
			}
		}
	}
	if (Report.Errors.Num() > 0)
	{
		return MakeReferenceResult(false, Owner, Property->GetName(), Report);
	}
	if (Definition && !Definition->IsExecutable())
	{
		Report.Warning(FString::Printf(
			TEXT("'%s' is not compiled; queries using it are refused until CompileTQSDefinition succeeds."),
			*Definition->GetPathName()));
	}

	int32 NumPropagated = 0;
	{
		const FScopedTransaction Transaction(LOCTEXT("SetReferenceTransaction", "Set Target Query Reference"));
		const FArcTQSQueryReference OldValue = *Property->ContainerPtrToValuePtr<FArcTQSQueryReference>(Owner);
		WriteReference(*Owner, *Property, NewValue);
		if (Owner->HasAnyFlags(RF_ClassDefaultObject))
		{
			const UClass* OwnerClass = Owner->GetClass();
			TArray<UObject*> Following;
			TArray<FString> Overriding;
			for (TObjectIterator<UObject> It; It; ++It)
			{
				UObject* Instance = *It;
				if (Instance == Owner
					|| !IsValid(Instance)
					|| !Instance->IsA(OwnerClass)
					|| Instance->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists)
					|| Instance->GetOutermost() == GetTransientPackage())
				{
					continue;
				}
				const FArcTQSQueryReference* InstanceValue = Property->ContainerPtrToValuePtr<FArcTQSQueryReference>(Instance);
				if (Property->Identical(&OldValue, InstanceValue, PPF_None))
				{
					Following.Add(Instance);
				}
				else
				{
					Overriding.Add(Instance->GetPathName());
				}
			}
			for (UObject* Instance : Following)
			{
				WriteReference(*Instance, *Property, NewValue);
			}
			NumPropagated = Following.Num();
			Overriding.Sort();
			for (const FString& Instance : Overriding)
			{
				Report.Warning(FString::Printf(
					TEXT("'%s' overrides %s and keeps its own value; set it separately if it should follow the default."),
					*Instance,
					*Property->GetName()));
			}
			Report.Warning(TEXT("Only loaded objects were checked: placed instances in unloaded levels or World Partition cells that override this property were not inspected."));
		}
		if (Blueprint)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}

	const FArcTQSQueryReference& Written = *Property->ContainerPtrToValuePtr<FArcTQSQueryReference>(Owner);
	TSharedRef<FJsonObject> Result = MakeReferenceResult(true, Owner, Property->GetName(), Report);
	Result->SetStringField(TEXT("definition"), Written.GetDefinition() ? Written.GetDefinition()->GetPathName() : FString());
	TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
	const UPropertyBag* WrittenBag = Written.GetParameters().GetPropertyBagStruct();
	const void* WrittenMemory = Written.GetParameters().GetValue().GetMemory();
	if (WrittenBag && WrittenMemory)
	{
		for (const FPropertyBagPropertyDesc& Desc : WrittenBag->GetPropertyDescs())
		{
			if (!Written.IsPropertyOverridden(Desc.ID) || !Desc.CachedProperty)
			{
				continue;
			}
			const TSharedPtr<FJsonValue> Value = ArcPropertyJson::ExportPropertyValue(
				*Desc.CachedProperty, Desc.CachedProperty->ContainerPtrToValuePtr<void>(WrittenMemory), GetHooks());
			if (Value.IsValid())
			{
				Values->SetField(Desc.Name.ToString(), Value);
			}
		}
	}
	Result->SetObjectField(TEXT("parameters"), Values);
	Result->SetNumberField(TEXT("propagated"), NumPropagated);
	return Result;
}

#undef LOCTEXT_NAMESPACE
