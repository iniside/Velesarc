// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsConfigJson.h"

#include "ArcTacticsAuthoring.h"
#include "ArcTacticsConfigLint.h"
#include "Json/ArcPropertyJson.h"
#include "Json/ArcStructTypeNames.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryReference.h"
#include "AssetToolsModule.h"
#include "Considerations/ArcTacticsConsiderations.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "FileHelpers.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingPath.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "ArcTacticsConfigJson"

namespace ArcTacticsConfigJsonPrivate
{
	using ArcPropertyJson::CloneObject;
	using ArcPropertyJson::FindField;
	using ArcStructTypeNames::FindType;
	using ArcStructTypeNames::GetTypeName;
	using ArcPropertyJson::ToJsonStrings;

	const TCHAR* const ContextName = TEXT("Context");
	const TCHAR* const GlobalParametersName = TEXT("GlobalParameters");
	const TCHAR* const DecisionSettingsName = TEXT("DecisionSettings");
	const TCHAR* const ParametersName = TEXT("Parameters");
	const TCHAR* const ParameterBagName = TEXT("ParameterBag");
	const TCHAR* const QueryDefinitionKey = TEXT("definition");
	const TCHAR* const QueryParametersKey = TEXT("parameters");
	const TCHAR* const QueryOverridesKey = TEXT("overrides");
	const TCHAR* const ExampleAssetPath = TEXT("/Game/AI/TacticsConfig_DPS");
	constexpr int32 FormatVersion = 2;

	const FArcPropertyJsonHooks& GetHooks();
	TSharedRef<FJsonObject> ExportEditableDelta(const UScriptStruct& Struct, const void* Memory);

	const TCHAR* GetNodeRoleName(const EArcTacticsNodeRole Role)
	{
		switch (Role)
		{
		case EArcTacticsNodeRole::GlobalTask:
			return TEXT("GlobalTask");
		case EArcTacticsNodeRole::Action:
			return TEXT("Action");
		case EArcTacticsNodeRole::Consideration:
			return TEXT("Consideration");
		case EArcTacticsNodeRole::Selector:
			return TEXT("Selector");
		case EArcTacticsNodeRole::Step:
			return TEXT("Step");
		case EArcTacticsNodeRole::ActiveStep:
			return TEXT("ActiveStep");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* GetPropertyRoleName(const EArcTacticsPropertyRole Role)
	{
		switch (Role)
		{
		case EArcTacticsPropertyRole::Input:
			return TEXT("Input");
		case EArcTacticsPropertyRole::Output:
			return TEXT("Output");
		case EArcTacticsPropertyRole::Parameter:
			return TEXT("Parameter");
		default:
			return TEXT("None");
		}
	}

	const TCHAR* GetValidationResultName(const EArcTacticsBindingValidationResult Result)
	{
		switch (Result)
		{
		case EArcTacticsBindingValidationResult::Valid:
			return TEXT("Valid");
		case EArcTacticsBindingValidationResult::InvalidTarget:
			return TEXT("InvalidTarget");
		case EArcTacticsBindingValidationResult::InvalidScope:
			return TEXT("InvalidScope (the source is not readable from the target)");
		case EArcTacticsBindingValidationResult::InvalidSourcePath:
			return TEXT("InvalidSourcePath");
		case EArcTacticsBindingValidationResult::InvalidTargetPath:
			return TEXT("InvalidTargetPath");
		case EArcTacticsBindingValidationResult::InvalidRole:
			return TEXT("InvalidRole (the root property has no single ArcTactics role)");
		case EArcTacticsBindingValidationResult::SourceNotOutput:
			return TEXT("SourceNotOutput (node and context sources must be Output properties)");
		case EArcTacticsBindingValidationResult::TargetNotInput:
			return TEXT("TargetNotInput (targets must be Input or Parameter properties)");
		case EArcTacticsBindingValidationResult::InvalidOutputTarget:
			return TEXT("InvalidOutputTarget");
		case EArcTacticsBindingValidationResult::LiteralOnlyTarget:
			return TEXT("LiteralOnlyTarget (a memory key accepts only a literal value)");
		case EArcTacticsBindingValidationResult::IncompatibleTypes:
			return TEXT("IncompatibleTypes");
		default:
			return TEXT("Unknown");
		}
	}

	bool ParseNodeRole(const FString& Text, EArcTacticsNodeRole& OutRole)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		const EArcTacticsNodeRole Roles[] = {
			EArcTacticsNodeRole::GlobalTask,
			EArcTacticsNodeRole::Action,
			EArcTacticsNodeRole::Consideration,
			EArcTacticsNodeRole::Selector,
			EArcTacticsNodeRole::Step,
			EArcTacticsNodeRole::ActiveStep
		};
		if (Trimmed.IsEmpty())
		{
			OutRole = EArcTacticsNodeRole::Invalid;
			return true;
		}
		for (const EArcTacticsNodeRole Role : Roles)
		{
			if (Trimmed.Equals(GetNodeRoleName(Role), ESearchCase::IgnoreCase))
			{
				OutRole = Role;
				return true;
			}
		}
		return false;
	}

	bool IsReservedName(const FString& Name)
	{
		return Name.Equals(ContextName, ESearchCase::IgnoreCase)
			|| Name.Equals(GlobalParametersName, ESearchCase::IgnoreCase)
			|| Name.Equals(DecisionSettingsName, ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("None"), ESearchCase::IgnoreCase);
	}

	bool HasIllegalNameCharacters(const FString& Name)
	{
		int32 Index = INDEX_NONE;
		return Name.FindChar(TEXT('.'), Index)
			|| Name.FindChar(TEXT('['), Index)
			|| Name.FindChar(TEXT(']'), Index)
			|| Name.Contains(TEXT("<-"));
	}

	FString MakeLegalName(const FString& InName)
	{
		FString Name = InName;
		Name.ReplaceInline(TEXT("<-"), TEXT("_"));
		Name.ReplaceCharInline(TEXT('.'), TEXT('_'));
		Name.ReplaceCharInline(TEXT('['), TEXT('_'));
		Name.ReplaceCharInline(TEXT(']'), TEXT('_'));
		Name.TrimStartAndEndInline();
		return Name.IsEmpty() ? FString(TEXT("Node")) : Name;
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

	FString MakeBranchKey(const FString& BranchName, const TCHAR* Suffix)
	{
		return BranchName + TEXT(".") + Suffix;
	}

	TSharedPtr<FJsonValue> ExportQueryReference(FProperty* Property, const void* ValuePtr)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty || StructProperty->Struct != FArcTQSQueryReference::StaticStruct() || !ValuePtr)
		{
			return TSharedPtr<FJsonValue>();
		}
		const FArcTQSQueryReference& Reference = *static_cast<const FArcTQSQueryReference*>(ValuePtr);
		const UArcTQSQueryDefinition* Definition = Reference.GetDefinition();
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(QueryDefinitionKey, Definition ? Definition->GetPathName() : FString());
		TSharedRef<FJsonObject> Parameters = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Overrides;
		const UPropertyBag* BagStruct = Reference.GetParameters().GetPropertyBagStruct();
		const void* Memory = Reference.GetParameters().GetValue().GetMemory();
		if (BagStruct && Memory)
		{
			for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
			{
				if (!Reference.IsPropertyOverridden(Desc.ID) || !Desc.CachedProperty)
				{
					continue;
				}
				const TSharedPtr<FJsonValue> Value = ArcPropertyJson::ExportPropertyValue(
					*Desc.CachedProperty, Desc.CachedProperty->ContainerPtrToValuePtr<void>(Memory), GetHooks());
				if (!Value.IsValid())
				{
					continue;
				}
				Parameters->SetField(Desc.Name.ToString(), Value);
				Overrides.Add(MakeShared<FJsonValueString>(Desc.Name.ToString()));
			}
		}
		Out->SetObjectField(QueryParametersKey, Parameters);
		Out->SetArrayField(QueryOverridesKey, Overrides);
		return MakeShared<FJsonValueObject>(Out);
	}

	void SetObjectIfNotEmpty(FJsonObject& Target, const TCHAR* Key, const TSharedRef<FJsonObject>& Value)
	{
		if (Value->Values.Num() > 0)
		{
			Target.SetObjectField(Key, Value);
		}
	}

	struct FExportNames
	{
		TSet<FString> Used;
		TMap<FGuid, FString> ByID;

		FString Allocate(const FArcTacticsNodeDefinition& Node)
		{
			FString Base;
			if (!Node.Name.IsNone())
			{
				Base = Node.Name.ToString();
			}
			else if (const FArcTacticsNode* NodeValue = Node.Node.GetPtr<FArcTacticsNode>())
			{
				Base = NodeValue->GetDisplayName().ToString();
			}
			Base = MakeLegalName(Base);
			FString Candidate = Base;
			int32 Suffix = 2;
			while (IsReservedName(Candidate) || Used.Contains(Candidate))
			{
				Candidate = FString::Printf(TEXT("%s_%d"), *Base, Suffix++);
			}
			Used.Add(Candidate);
			ByID.Add(Node.ID, Candidate);
			return Candidate;
		}
	};

	TSharedRef<FJsonObject> ExportNode(const FArcTacticsNodeDefinition& Node, const FString& Name)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("name"), Name);
		if (const UScriptStruct* NodeType = Node.Node.GetScriptStruct())
		{
			Out->SetStringField(TEXT("type"), GetTypeName<UScriptStruct>(NodeType));
			SetObjectIfNotEmpty(*Out, TEXT("node"), ExportEditableDelta(*NodeType, Node.Node.GetMemory()));
		}
		if (const UScriptStruct* DataType = Node.InstanceData.GetScriptStruct())
		{
			SetObjectIfNotEmpty(*Out, TEXT("data"), ExportEditableDelta(*DataType, Node.InstanceData.GetMemory()));
		}
		return Out;
	}

	void ExportNodes(
		FJsonObject& Out,
		const TCHAR* Key,
		const TArray<FArcTacticsNodeDefinition>& Nodes,
		const FExportNames& Names)
	{
		if (Nodes.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Items;
			for (const FArcTacticsNodeDefinition& Node : Nodes)
			{
				Items.Add(MakeShared<FJsonValueObject>(ExportNode(Node, Names.ByID.FindRef(Node.ID))));
			}
			Out.SetArrayField(Key, Items);
		}
	}

	void ExportBranchContents(
		FJsonObject& Out,
		const FArcTacticsBranchParameters& Parameters,
		const FInstancedPropertyBag& ParameterBag,
		const TArray<FArcTacticsNodeDefinition>& Considerations,
		const FExportNames& Names)
	{
		SetObjectIfNotEmpty(
			Out,
			TEXT("parameters"),
			ExportEditableDelta(*FArcTacticsBranchParameters::StaticStruct(), &Parameters));
		const TArray<TSharedPtr<FJsonValue>> Bag = ArcPropertyJson::ExportBag(ParameterBag, GetHooks());
		if (Bag.Num() > 0)
		{
			Out.SetArrayField(TEXT("parameterBag"), Bag);
		}
		ExportNodes(Out, TEXT("considerations"), Considerations, Names);
	}

	void ImportQueryReference(
		const TSharedPtr<FJsonValue>& JsonValue,
		FArcTQSQueryReference& Reference,
		const FString& Location,
		FArcPropertyJsonReport& Report)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!JsonValue.IsValid() || !JsonValue->TryGetObject(ObjectPtr) || !ObjectPtr->IsValid())
		{
			Report.Error(FString::Printf(
				TEXT("%s: a query reference is an object {\"definition\", \"parameters\", \"overrides\"}."),
				*Location));
			return;
		}
		const FJsonObject& Object = **ObjectPtr;
		for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : Object.Values)
		{
			const FString Key(Pair.Key);
			if (!Key.Equals(QueryDefinitionKey, ESearchCase::IgnoreCase)
				&& !Key.Equals(QueryParametersKey, ESearchCase::IgnoreCase)
				&& !Key.Equals(QueryOverridesKey, ESearchCase::IgnoreCase))
			{
				Report.Error(FString::Printf(
					TEXT("%s: unknown key '%s' (allowed: definition, parameters, overrides)."),
					*Location,
					*Key));
				return;
			}
		}

		FString DefinitionPath;
		const TSharedPtr<FJsonValue> DefinitionValue = FindField(Object, QueryDefinitionKey);
		if (DefinitionValue.IsValid() && DefinitionValue->Type != EJson::Null
			&& (DefinitionValue->Type != EJson::String || !DefinitionValue->TryGetString(DefinitionPath)))
		{
			Report.Error(FString::Printf(TEXT("%s.definition: expected an object path string."), *Location));
			return;
		}
		UArcTQSQueryDefinition* Definition = nullptr;
		if (!DefinitionPath.TrimStartAndEnd().IsEmpty())
		{
			Definition = FindType<UArcTQSQueryDefinition>(DefinitionPath);
			if (!Definition)
			{
				Report.Error(FString::Printf(
					TEXT("%s.definition: query definition '%s' was not found."),
					*Location,
					*DefinitionPath));
				return;
			}
		}
		Reference.SetDefinition(Definition);

		const UPropertyBag* BagStruct = Reference.GetParameters().GetPropertyBagStruct();
		void* Memory = Reference.GetMutableParameters().GetMutableValue().GetMemory();
		TSet<FName> ImportedNames;
		const TSharedPtr<FJsonValue> ParametersValue = FindField(Object, QueryParametersKey);
		const TSharedPtr<FJsonObject>* ParametersObject = nullptr;
		if (ParametersValue.IsValid() && ParametersValue->Type != EJson::Null)
		{
			if (!ParametersValue->TryGetObject(ParametersObject) || !ParametersObject->IsValid())
			{
				Report.Error(FString::Printf(TEXT("%s.parameters: expected an object of parameter values."), *Location));
				return;
			}
			for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair : (*ParametersObject)->Values)
			{
				const FName Name(*FString(Pair.Key));
				const FPropertyBagPropertyDesc* Desc = BagStruct ? BagStruct->FindPropertyDescByName(Name) : nullptr;
				if (!Desc || !Desc->CachedProperty || !Memory)
				{
					Report.Error(FString::Printf(
						TEXT("%s.parameters: '%s' is not a parameter of the query definition."),
						*Location,
						*Name.ToString()));
					continue;
				}
				ArcPropertyJson::ImportPropertyValue(
					*const_cast<FProperty*>(Desc->CachedProperty),
					Desc->CachedProperty->ContainerPtrToValuePtr<void>(Memory),
					Pair.Value,
					FString::Printf(TEXT("%s.parameters.%s"), *Location, *Name.ToString()),
					Report,
					GetHooks());
				ImportedNames.Add(Name);
			}
		}

		TSet<FName> OverriddenNames;
		const TSharedPtr<FJsonValue> OverridesValue = FindField(Object, QueryOverridesKey);
		const TArray<TSharedPtr<FJsonValue>>* OverrideItems = nullptr;
		if (OverridesValue.IsValid() && OverridesValue->Type != EJson::Null)
		{
			if (!OverridesValue->TryGetArray(OverrideItems))
			{
				Report.Error(FString::Printf(TEXT("%s.overrides: expected an array of parameter names."), *Location));
				return;
			}
			for (const TSharedPtr<FJsonValue>& Item : *OverrideItems)
			{
				FString NameText;
				if (!Item.IsValid() || Item->Type != EJson::String || !Item->TryGetString(NameText))
				{
					Report.Error(FString::Printf(TEXT("%s.overrides: expected parameter name strings."), *Location));
					continue;
				}
				const FName Name(*NameText);
				const FPropertyBagPropertyDesc* Desc = BagStruct ? BagStruct->FindPropertyDescByName(Name) : nullptr;
				if (!Desc)
				{
					Report.Error(FString::Printf(
						TEXT("%s.overrides: '%s' is not a parameter of the query definition."),
						*Location,
						*NameText));
					continue;
				}
				Reference.SetPropertyOverridden(Desc->ID, true);
				OverriddenNames.Add(Name);
			}
		}
		for (const FName& Name : ImportedNames)
		{
			if (!OverriddenNames.Contains(Name))
			{
				Report.Error(FString::Printf(
					TEXT("%s.parameters: '%s' has a value but is not listed in overrides; compile would reset it to the definition default."),
					*Location,
					*Name.ToString()));
			}
		}
	}

	const FArcPropertyJsonHooks& GetHooks()
	{
		static const FArcPropertyJsonHooks Hooks = []() -> FArcPropertyJsonHooks
		{
			FArcPropertyJsonHooks Result;
			Result.ExportStruct = &ExportQueryReference;
			Result.ImportStruct = [](
				const TSharedPtr<FJsonValue>& JsonValue,
				FProperty* Property,
				void* Value,
				const FString& Location,
				FArcPropertyJsonReport& Report) -> bool
			{
				const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
				if (!StructProperty || StructProperty->Struct != FArcTQSQueryReference::StaticStruct() || !Value)
				{
					return false;
				}
				ImportQueryReference(JsonValue, *static_cast<FArcTQSQueryReference*>(Value),
					Location + TEXT(".") + Property->GetName(), Report);
				return true;
			};
			Result.SkipPrepass = [](const UScriptStruct& Struct) -> bool
			{
				return &Struct == FArcTQSQueryReference::StaticStruct();
			};
			return Result;
		}();
		return Hooks;
	}

	TSharedRef<FJsonObject> ExportEditableDelta(const UScriptStruct& Struct, const void* Memory)
	{
		return ArcPropertyJson::ExportStructDelta(Struct, Memory, GetHooks(), [](const FProperty& Property) -> bool
		{
			return ArcTacticsNode::ResolvePropertyRole(Property) == EArcTacticsPropertyRole::Output;
		});
	}

	bool ImportStruct(
		const UStruct& Struct,
		void* Memory,
		const TSharedPtr<FJsonObject>& Json,
		const FString& Location,
		FArcPropertyJsonReport& Report)
	{
		return ArcPropertyJson::ImportStruct(Struct, Memory, Json, Location, Report, GetHooks());
	}

	enum class EEndpointKind : uint8
	{
		Context,
		GlobalParameters,
		DecisionSettings,
		GlobalTask,
		BranchNode,
		SelectorNode,
		BranchParameters,
		BranchParameterBag
	};

	struct FEndpointSpec
	{
		EEndpointKind Kind = EEndpointKind::Context;
		const UScriptStruct* InstanceStruct = nullptr;
		int32 BranchIndex = INDEX_NONE;
		bool bSelectorBranch = false;
	};

	struct FFunctionSpec;

	struct FBindingSpec
	{
		FString Text;
		FString ToKey;
		FString FromKey;
		FPropertyBindingPath ToPath;
		FPropertyBindingPath FromPath;
		bool bOutput = false;
		TSharedPtr<FFunctionSpec> Function;
	};

	struct FFunctionSpec
	{
		FInstancedStruct Node;
		FName OutputName;
		TArray<FBindingSpec> Inputs;
	};

	struct FApplySpec
	{
		bool bDryRun = false;
		bool bSave = false;
		FArcTacticsDecisionSettingsParameters DecisionSettings;
		FInstancedPropertyBag GlobalParameters;
		TArray<FArcTacticsNodeDefinition> GlobalTasks;
		TArray<FArcTacticsActionDefinition> Actions;
		TArray<FArcTacticsSelectorDefinition> Selectors;
		TArray<FBindingSpec> Bindings;
		TMap<FString, FEndpointSpec> Endpoints;
		TSet<FString> BranchNames;
		TSet<FString> NodeNames;
	};

	class FApplyParser
	{
	public:
		FApplyParser(const UArcTacticsConfig& InConfig, FApplySpec& InSpec, FArcPropertyJsonReport& InReport)
			: Config(InConfig)
			, Spec(InSpec)
			, Report(InReport)
		{
		}

		void ParseRoot(const FJsonObject& Root)
		{
			CheckKeys(
				Root,
				{
					TEXT("version"), TEXT("options"), TEXT("decisionSettings"),
					TEXT("globalParameters"), TEXT("globalTasks"), TEXT("actions"), TEXT("selectors"),
					TEXT("bindings"), TEXT("warnings")
				},
				TEXT("root"));

			const TSharedPtr<FJsonValue> VersionValue = FindField(Root, TEXT("version"));
			int32 Version = 0;
			if (!VersionValue.IsValid() || !VersionValue->TryGetNumber(Version) || Version != FormatVersion)
			{
				Report.Error(FString::Printf(TEXT("root: 'version' must be %d."), FormatVersion));
			}

			const TSharedPtr<FJsonObject> Options = GetObjectValue(Root, TEXT("options"), TEXT("root"));
			if (Options.IsValid())
			{
				CheckKeys(*Options, {TEXT("dryRun"), TEXT("save")}, TEXT("options"));
				Spec.bDryRun = GetBool(*Options, TEXT("dryRun"), TEXT("options"));
				Spec.bSave = GetBool(*Options, TEXT("save"), TEXT("options"));
			}

			AddRootEndpoint(ContextName, EEndpointKind::Context);
			AddRootEndpoint(GlobalParametersName, EEndpointKind::GlobalParameters);
			AddRootEndpoint(DecisionSettingsName, EEndpointKind::DecisionSettings);

			ImportStruct(
				*FArcTacticsDecisionSettingsParameters::StaticStruct(),
				&Spec.DecisionSettings,
				GetObjectValue(Root, TEXT("decisionSettings"), TEXT("root")),
				TEXT("decisionSettings"),
				Report);
			ParseBag(Root, TEXT("globalParameters"), Spec.GlobalParameters, TEXT("globalParameters"));

			const TArray<TSharedPtr<FJsonValue>> Tasks = GetArrayValue(Root, TEXT("globalTasks"), TEXT("root"));
			for (int32 Index = 0; Index < Tasks.Num(); ++Index)
			{
				const FString Location = FString::Printf(TEXT("globalTasks[%d]"), Index);
				const TSharedPtr<FJsonObject> TaskObject = AsObject(Tasks[Index], Location);
				FArcTacticsNodeDefinition& Task = Spec.GlobalTasks.AddDefaulted_GetRef();
				if (TaskObject.IsValid())
				{
					CheckKeys(*TaskObject, {TEXT("name"), TEXT("type"), TEXT("node"), TEXT("data")}, Location);
					FEndpointSpec Endpoint;
					Endpoint.Kind = EEndpointKind::GlobalTask;
					ParseNode(*TaskObject, EArcTacticsNodeRole::GlobalTask, Location, Endpoint, Task);
				}
			}

			const TArray<TSharedPtr<FJsonValue>> Actions = GetArrayValue(Root, TEXT("actions"), TEXT("root"));
			for (int32 Index = 0; Index < Actions.Num(); ++Index)
			{
				ParseAction(Actions[Index], Index);
			}

			const TArray<TSharedPtr<FJsonValue>> Selectors = GetArrayValue(Root, TEXT("selectors"), TEXT("root"));
			for (int32 Index = 0; Index < Selectors.Num(); ++Index)
			{
				ParseSelector(Selectors[Index], Index);
			}

			const TArray<TSharedPtr<FJsonValue>> Bindings = GetArrayValue(Root, TEXT("bindings"), TEXT("root"));
			TMap<FString, FString> StoredTargets;
			for (int32 Index = 0; Index < Bindings.Num(); ++Index)
			{
				ParseBinding(
					Bindings[Index], FString::Printf(TEXT("bindings[%d]"), Index), StoredTargets, nullptr, Spec.Bindings);
			}
		}

	private:
		const UArcTacticsConfig& Config;
		FApplySpec& Spec;
		FArcPropertyJsonReport& Report;

		void AddRootEndpoint(const TCHAR* Name, const EEndpointKind Kind)
		{
			FEndpointSpec Endpoint;
			Endpoint.Kind = Kind;
			Spec.Endpoints.Add(FString(Name), Endpoint);
		}

		void CheckKeys(const FJsonObject& Object, TConstArrayView<const TCHAR*> Allowed, const FString& Location)
		{
			ArcPropertyJson::CheckKeys(Object, Allowed, Location, Report);
		}

		TSharedPtr<FJsonObject> AsObject(const TSharedPtr<FJsonValue>& Value, const FString& Location)
		{
			return ArcPropertyJson::AsObject(Value, Location, Report);
		}

		TSharedPtr<FJsonObject> GetObjectValue(const FJsonObject& Owner, const TCHAR* Key, const FString& Location)
		{
			return ArcPropertyJson::GetObjectValue(Owner, Key, Location, Report);
		}

		TArray<TSharedPtr<FJsonValue>> GetArrayValue(const FJsonObject& Owner, const TCHAR* Key, const FString& Location)
		{
			return ArcPropertyJson::GetArrayValue(Owner, Key, Location, Report);
		}

		bool GetBool(const FJsonObject& Owner, const TCHAR* Key, const FString& Location)
		{
			return ArcPropertyJson::GetBool(Owner, Key, Location, Report);
		}

		bool GetString(const FJsonObject& Owner, const TCHAR* Key, const FString& Location, FString& OutValue)
		{
			return ArcPropertyJson::GetString(Owner, Key, Location, OutValue, Report);
		}

		bool RegisterName(const FString& Name, const FString& Location)
		{
			if (Name.IsEmpty())
			{
				Report.Error(FString::Printf(TEXT("%s: 'name' is required and must not be empty."), *Location));
				return false;
			}
			if (Name != Name.TrimStartAndEnd() || HasIllegalNameCharacters(Name))
			{
				Report.Error(FString::Printf(
					TEXT("%s: name '%s' must not contain '.', '[', ']', '<-' or leading/trailing spaces."),
					*Location,
					*Name));
				return false;
			}
			if (IsReservedName(Name))
			{
				Report.Error(FString::Printf(TEXT("%s: name '%s' is reserved."), *Location, *Name));
				return false;
			}
			if (Spec.NodeNames.Contains(Name))
			{
				Report.Error(FString::Printf(
					TEXT("%s: name '%s' is already used by another node (names are case-insensitive)."),
					*Location,
					*Name));
				return false;
			}
			Spec.NodeNames.Add(Name);
			return true;
		}

		bool ParseNode(
			const FJsonObject& Object,
			const EArcTacticsNodeRole Role,
			const FString& Location,
			FEndpointSpec Endpoint,
			FArcTacticsNodeDefinition& OutNode,
			FString* OutName = nullptr)
		{
			FString Name;
			GetString(Object, TEXT("name"), Location, Name);
			const bool bNameValid = RegisterName(Name, Location);
			const FString NodeLocation = bNameValid
				? FString::Printf(TEXT("%s '%s'"), *Location, *Name)
				: Location;

			FString TypeName;
			const UScriptStruct* NodeType = nullptr;
			if (!GetString(Object, TEXT("type"), Location, TypeName) || TypeName.IsEmpty())
			{
				Report.Error(FString::Printf(TEXT("%s: 'type' is required."), *NodeLocation));
			}
			else
			{
				NodeType = FindType<UScriptStruct>(TypeName);
				if (!NodeType)
				{
					Report.Error(FString::Printf(TEXT("%s: unknown node type '%s'."), *NodeLocation, *TypeName));
				}
			}

			bool bPrepared = false;
			if (NodeType)
			{
				FText Error;
				bPrepared = ArcTacticsAuthoring::PrepareNode(Config, OutNode, NodeType, Role, &Error);
				if (!bPrepared)
				{
					Report.Error(FString::Printf(
						TEXT("%s: type '%s' cannot be used as a %s node: %s"),
						*NodeLocation,
						*TypeName,
						GetNodeRoleName(Role),
						*Error.ToString()));
				}
			}

			if (bNameValid)
			{
				OutNode.Name = FName(*Name);
				Endpoint.InstanceStruct = bPrepared ? OutNode.InstanceData.GetScriptStruct() : nullptr;
				Spec.Endpoints.Add(Name, Endpoint);
				if (OutName)
				{
					*OutName = Name;
				}
			}
			if (!bPrepared)
			{
				return false;
			}

			ImportStruct(
				*OutNode.Node.GetScriptStruct(),
				OutNode.Node.GetMutableMemory(),
				GetObjectValue(Object, TEXT("node"), NodeLocation),
				NodeLocation + TEXT(".node"),
				Report);
			const TSharedPtr<FJsonObject> Data = GetObjectValue(Object, TEXT("data"), NodeLocation);
			if (Data.IsValid() && OutNode.InstanceData.IsValid())
			{
				ImportStruct(
					*OutNode.InstanceData.GetScriptStruct(),
					OutNode.InstanceData.GetMutableMemory(),
					Data,
					NodeLocation + TEXT(".data"),
					Report);
			}
			return bNameValid;
		}

		void ParseBranchNodes(
			const FJsonObject& Owner,
			const TCHAR* Key,
			const EArcTacticsNodeRole Role,
			const FString& Location,
			const int32 BranchIndex,
			const bool bSelectorBranch,
			TArray<FArcTacticsNodeDefinition>& OutNodes)
		{
			const TArray<TSharedPtr<FJsonValue>> Items = GetArrayValue(Owner, Key, Location);
			for (int32 Index = 0; Index < Items.Num(); ++Index)
			{
				const FString ItemLocation = FString::Printf(TEXT("%s.%s[%d]"), *Location, Key, Index);
				const TSharedPtr<FJsonObject> Item = AsObject(Items[Index], ItemLocation);
				FArcTacticsNodeDefinition& Node = OutNodes.AddDefaulted_GetRef();
				if (Item.IsValid())
				{
					CheckKeys(*Item, {TEXT("name"), TEXT("type"), TEXT("node"), TEXT("data")}, ItemLocation);
					FEndpointSpec Endpoint;
					Endpoint.Kind = EEndpointKind::BranchNode;
					Endpoint.BranchIndex = BranchIndex;
					Endpoint.bSelectorBranch = bSelectorBranch;
					ParseNode(*Item, Role, ItemLocation, Endpoint, Node);
				}
			}
		}

		void RegisterBranch(const FString& BranchName, const int32 BranchIndex, const bool bSelectorBranch)
		{
			Spec.BranchNames.Add(BranchName);
			FEndpointSpec Parameters;
			Parameters.Kind = EEndpointKind::BranchParameters;
			Parameters.BranchIndex = BranchIndex;
			Parameters.bSelectorBranch = bSelectorBranch;
			Spec.Endpoints.Add(MakeBranchKey(BranchName, ParametersName), Parameters);
			FEndpointSpec Bag = Parameters;
			Bag.Kind = EEndpointKind::BranchParameterBag;
			Spec.Endpoints.Add(MakeBranchKey(BranchName, ParameterBagName), Bag);
		}

		void ParseBranchContents(
			const FJsonObject& Object,
			const FString& Location,
			FArcTacticsBranchParameters& OutParameters,
			FInstancedPropertyBag& OutBag)
		{
			ImportStruct(
				*FArcTacticsBranchParameters::StaticStruct(),
				&OutParameters,
				GetObjectValue(Object, TEXT("parameters"), Location),
				Location + TEXT(".parameters"),
				Report);
			ParseBag(Object, TEXT("parameterBag"), OutBag, Location + TEXT(".parameterBag"));
		}

		void ParseAction(const TSharedPtr<FJsonValue>& Value, const int32 Index)
		{
			const FString Location = FString::Printf(TEXT("actions[%d]"), Index);
			FArcTacticsActionDefinition& Action = Spec.Actions.AddDefaulted_GetRef();
			const TSharedPtr<FJsonObject> Object = AsObject(Value, Location);
			if (!Object.IsValid())
			{
				return;
			}
			CheckKeys(
				*Object,
				{
					TEXT("name"), TEXT("type"), TEXT("node"), TEXT("data"), TEXT("steps"), TEXT("activeSteps"),
					TEXT("parameters"), TEXT("parameterBag"), TEXT("considerations")
				},
				Location);
			FEndpointSpec Endpoint;
			Endpoint.Kind = EEndpointKind::BranchNode;
			Endpoint.BranchIndex = Index;
			FString Name;
			ParseNode(*Object, EArcTacticsNodeRole::Action, Location, Endpoint, Action.Action, &Name);
			if (!Name.IsEmpty())
			{
				RegisterBranch(Name, Index, false);
			}
			ParseBranchNodes(*Object, TEXT("steps"), EArcTacticsNodeRole::Step, Location, Index, false, Action.Steps);
			ParseBranchNodes(
				*Object, TEXT("activeSteps"), EArcTacticsNodeRole::ActiveStep, Location, Index, false,
				Action.ActiveSteps);
			ParseBranchContents(*Object, Location, Action.Parameters, Action.ParameterBag);
			ParseBranchNodes(
				*Object, TEXT("considerations"), EArcTacticsNodeRole::Consideration, Location, Index, false,
				Action.Considerations);
		}

		void ParseSelector(const TSharedPtr<FJsonValue>& Value, const int32 Index)
		{
			const FString Location = FString::Printf(TEXT("selectors[%d]"), Index);
			FArcTacticsSelectorDefinition& Selector = Spec.Selectors.AddDefaulted_GetRef();
			const TSharedPtr<FJsonObject> Object = AsObject(Value, Location);
			if (!Object.IsValid())
			{
				return;
			}
			CheckKeys(
				*Object,
				{
					TEXT("name"), TEXT("type"), TEXT("node"), TEXT("data"), TEXT("parameters"),
					TEXT("parameterBag"), TEXT("action"), TEXT("considerations")
				},
				Location);
			FEndpointSpec Endpoint;
			Endpoint.Kind = EEndpointKind::SelectorNode;
			Endpoint.BranchIndex = Index;
			Endpoint.bSelectorBranch = true;
			FString Name;
			ParseNode(*Object, EArcTacticsNodeRole::Selector, Location, Endpoint, Selector.Selector, &Name);
			if (!Name.IsEmpty())
			{
				RegisterBranch(Name, Index, true);
			}
			ParseBranchContents(*Object, Location, Selector.Parameters, Selector.ParameterBag);

			const TSharedPtr<FJsonObject> ActionObject = GetObjectValue(*Object, TEXT("action"), Location);
			if (ActionObject.IsValid())
			{
				const FString ActionLocation = Location + TEXT(".action");
				CheckKeys(
					*ActionObject,
					{TEXT("name"), TEXT("type"), TEXT("node"), TEXT("data"), TEXT("steps"), TEXT("activeSteps")},
					ActionLocation);
				FEndpointSpec ActionEndpoint;
				ActionEndpoint.Kind = EEndpointKind::BranchNode;
				ActionEndpoint.BranchIndex = Index;
				ActionEndpoint.bSelectorBranch = true;
				ParseNode(*ActionObject, EArcTacticsNodeRole::Action, ActionLocation, ActionEndpoint, Selector.Action);
				ParseBranchNodes(
					*ActionObject, TEXT("steps"), EArcTacticsNodeRole::Step, ActionLocation, Index, true, Selector.Steps);
				ParseBranchNodes(
					*ActionObject, TEXT("activeSteps"), EArcTacticsNodeRole::ActiveStep, ActionLocation, Index, true,
					Selector.ActiveSteps);
			}
			ParseBranchNodes(
				*Object, TEXT("considerations"), EArcTacticsNodeRole::Consideration, Location, Index, true,
				Selector.Considerations);
		}

		void ParseBag(
			const FJsonObject& Owner,
			const TCHAR* Key,
			FInstancedPropertyBag& OutBag,
			const FString& Location)
		{
			ArcPropertyJson::ImportBag(GetArrayValue(Owner, Key, Location), OutBag, Location, Report, GetHooks());
		}

		bool ParseEndpoint(const FString& Text, const FString& Location, FString& OutKey, FPropertyBindingPath& OutPath)
		{
			FString Head = Text;
			FString Rest;
			int32 Dot = INDEX_NONE;
			if (Text.FindChar(TEXT('.'), Dot))
			{
				Head = Text.Left(Dot);
				Rest = Text.Mid(Dot + 1);
			}
			Head.TrimStartAndEndInline();
			Rest.TrimStartAndEndInline();
			FString Key = Head;
			if (Spec.BranchNames.Contains(Head) && !Rest.IsEmpty())
			{
				int32 Separator = INDEX_NONE;
				for (int32 Index = 0; Index < Rest.Len(); ++Index)
				{
					if (Rest[Index] == TEXT('.') || Rest[Index] == TEXT('['))
					{
						Separator = Index;
						break;
					}
				}
				const FString Token = Separator == INDEX_NONE ? Rest : Rest.Left(Separator);
				const bool bParameters = Token.Equals(ParametersName, ESearchCase::IgnoreCase);
				const bool bParameterBag = Token.Equals(ParameterBagName, ESearchCase::IgnoreCase);
				if (bParameters || bParameterBag)
				{
					if (Separator != INDEX_NONE && Rest[Separator] == TEXT('['))
					{
						Report.Error(FString::Printf(TEXT("%s: '%s' cannot be indexed."), *Location, *Token));
						return false;
					}
					Key = MakeBranchKey(Head, bParameters ? ParametersName : ParameterBagName);
					Rest = Separator == INDEX_NONE ? FString() : Rest.Mid(Separator + 1);
				}
			}
			if (!Spec.Endpoints.Contains(Key))
			{
				Report.Error(FString::Printf(
					TEXT("%s: unknown endpoint '%s' (use Context, GlobalParameters, DecisionSettings, a node name, or <Branch>.Parameters / <Branch>.ParameterBag)."),
					*Location,
					*Head));
				return false;
			}
			OutKey = Key;
			if (!OutPath.FromString(Rest))
			{
				Report.Error(FString::Printf(TEXT("%s: invalid property path '%s'."), *Location, *Rest));
				return false;
			}
			return true;
		}

		void ParseBinding(
			const TSharedPtr<FJsonValue>& Value,
			const FString& Location,
			TMap<FString, FString>& StoredTargets,
			const FString* FunctionScope,
			TArray<FBindingSpec>& OutBindings)
		{
			FString Text;
			if (Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(Text))
			{
				ParseStringBinding(Text, Location, StoredTargets, FunctionScope, OutBindings);
				return;
			}
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object->IsValid())
			{
				ParseFunctionBinding(**Object, Location, StoredTargets, FunctionScope, OutBindings);
				return;
			}
			Report.Error(FString::Printf(
				TEXT("%s: expected a string 'to <- from' or a function object {\"to\", \"function\"}."),
				*Location));
		}

		bool ParseTarget(
			const FString& ToText,
			const FString* FunctionScope,
			const FString& Location,
			FBindingSpec& Binding,
			FString& OutTargetName)
		{
			const FString Trimmed = ToText.TrimStartAndEnd();
			if (FunctionScope)
			{
				if (Trimmed.IsEmpty() || !Binding.ToPath.FromString(Trimmed))
				{
					Report.Error(FString::Printf(TEXT("%s: invalid function input path '%s'."), *Location, *Trimmed));
					return false;
				}
				OutTargetName = *FunctionScope + TEXT("/") + PathToString(Binding.ToPath);
				return true;
			}
			if (!ParseEndpoint(Trimmed, Location, Binding.ToKey, Binding.ToPath))
			{
				return false;
			}
			OutTargetName = JoinEndpoint(Binding.ToKey, Binding.ToPath);
			return true;
		}

		bool RegisterStoredTarget(
			const FString& StoredTarget,
			const FString& Text,
			const FString& Location,
			TMap<FString, FString>& StoredTargets)
		{
			if (const FString* Existing = StoredTargets.Find(StoredTarget))
			{
				Report.Error(FString::Printf(
					TEXT("%s: writes the same stored target as '%s'; each target path takes one binding."),
					*Location,
					**Existing));
				return false;
			}
			StoredTargets.Add(StoredTarget, Text);
			return true;
		}

		void ParseStringBinding(
			const FString& Text,
			const FString& Location,
			TMap<FString, FString>& StoredTargets,
			const FString* FunctionScope,
			TArray<FBindingSpec>& OutBindings)
		{
			const FString BindingLocation = FString::Printf(TEXT("%s '%s'"), *Location, *Text);
			FString ToText;
			FString FromText;
			if (!Text.Split(TEXT("<-"), &ToText, &FromText) || FromText.Contains(TEXT("<-")))
			{
				Report.Error(FString::Printf(TEXT("%s: expected exactly one 'to <- from'."), *BindingLocation));
				return;
			}
			FBindingSpec Binding;
			Binding.Text = Text;
			FString TargetName;
			if (!ParseTarget(ToText, FunctionScope, BindingLocation, Binding, TargetName)
				|| !ParseEndpoint(FromText.TrimStartAndEnd(), BindingLocation, Binding.FromKey, Binding.FromPath))
			{
				return;
			}

			const FEndpointSpec& From = Spec.Endpoints.FindChecked(Binding.FromKey);
			const FEndpointSpec* To = FunctionScope ? nullptr : &Spec.Endpoints.FindChecked(Binding.ToKey);
			if (To
				&& (From.Kind == EEndpointKind::GlobalTask || From.Kind == EEndpointKind::BranchNode)
				&& From.InstanceStruct
				&& !Binding.FromPath.IsPathEmpty())
			{
				const FProperty* RootProperty = From.InstanceStruct->FindPropertyByName(
					Binding.FromPath.GetSegment(0).GetName());
				if (RootProperty
					&& ArcTacticsNode::ResolvePropertyRole(*RootProperty) == EArcTacticsPropertyRole::Output)
				{
					Binding.bOutput = (From.Kind == EEndpointKind::GlobalTask
							&& To->Kind == EEndpointKind::GlobalParameters)
						|| (From.Kind == EEndpointKind::BranchNode
							&& To->Kind == EEndpointKind::BranchParameterBag
							&& To->BranchIndex == From.BranchIndex
							&& To->bSelectorBranch == From.bSelectorBranch);
				}
			}

			const FString StoredTarget = Binding.bOutput
				? Binding.FromKey + TEXT("|") + PathToString(Binding.FromPath)
				: (FunctionScope ? TargetName : Binding.ToKey + TEXT("|") + PathToString(Binding.ToPath));
			if (!RegisterStoredTarget(StoredTarget, Text, BindingLocation, StoredTargets))
			{
				return;
			}
			OutBindings.Add(MoveTemp(Binding));
		}

		void ParseFunctionBinding(
			const FJsonObject& Object,
			const FString& Location,
			TMap<FString, FString>& StoredTargets,
			const FString* FunctionScope,
			TArray<FBindingSpec>& OutBindings)
		{
			CheckKeys(Object, {TEXT("to"), TEXT("function")}, Location);
			FString ToText;
			if (!GetString(Object, TEXT("to"), Location, ToText) || ToText.TrimStartAndEnd().IsEmpty())
			{
				Report.Error(FString::Printf(TEXT("%s: 'to' is required."), *Location));
				return;
			}
			const FString BindingLocation = FString::Printf(TEXT("%s (to '%s')"), *Location, *ToText);
			FBindingSpec Binding;
			FString TargetName;
			if (!ParseTarget(ToText, FunctionScope, BindingLocation, Binding, TargetName))
			{
				return;
			}
			const FString FunctionLocation = BindingLocation + TEXT(".function");
			const TSharedPtr<FJsonObject> FunctionObject = GetObjectValue(Object, TEXT("function"), BindingLocation);
			if (!FunctionObject.IsValid())
			{
				Report.Error(FString::Printf(TEXT("%s: 'function' is required."), *BindingLocation));
				return;
			}
			CheckKeys(*FunctionObject, {TEXT("type"), TEXT("data"), TEXT("bindings")}, FunctionLocation);
			FString TypeName;
			if (!GetString(*FunctionObject, TEXT("type"), FunctionLocation, TypeName) || TypeName.IsEmpty())
			{
				Report.Error(FString::Printf(TEXT("%s: 'type' is required."), *FunctionLocation));
				return;
			}
			const UScriptStruct* Type = FindType<UScriptStruct>(TypeName);
			const UScriptStruct* BaseStruct = FArcTQSPropertyFunction::StaticStruct();
			if (!Type || Type == BaseStruct || !Type->IsChildOf(BaseStruct))
			{
				Report.Error(FString::Printf(
					TEXT("%s: '%s' is not a property function type (derive from %s)."),
					*FunctionLocation,
					*TypeName,
					*BaseStruct->GetName()));
				return;
			}
			int32 NumOutputs = 0;
			const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(Type, &NumOutputs);
			if (!OutputProperty || NumOutputs != 1)
			{
				Report.Error(FString::Printf(
					TEXT("%s: function '%s' must declare exactly one ArcTQSOutput field (it has %d)."),
					*FunctionLocation,
					*TypeName,
					NumOutputs));
				return;
			}
			TSharedRef<FFunctionSpec> Function = MakeShared<FFunctionSpec>();
			Function->Node.InitializeAs(Type);
			Function->OutputName = OutputProperty->GetFName();
			const TSharedPtr<FJsonObject> Data = GetObjectValue(*FunctionObject, TEXT("data"), FunctionLocation);
			if (Data.IsValid())
			{
				if (FindField(*Data, Type->GetAuthoredNameForField(OutputProperty)).IsValid())
				{
					Report.Error(FString::Printf(
						TEXT("%s.data.%s: the function output is read-only."),
						*FunctionLocation,
						*OutputProperty->GetName()));
					return;
				}
				ImportStruct(*Type, Function->Node.GetMutableMemory(), Data, FunctionLocation + TEXT(".data"), Report);
			}

			Binding.Text = FString::Printf(TEXT("%s <- function %s"), *TargetName, *Type->GetName());
			const FString StoredTarget = FunctionScope
				? TargetName
				: Binding.ToKey + TEXT("|") + PathToString(Binding.ToPath);
			if (!RegisterStoredTarget(StoredTarget, Binding.Text, BindingLocation, StoredTargets))
			{
				return;
			}
			const TArray<TSharedPtr<FJsonValue>> Inputs = GetArrayValue(*FunctionObject, TEXT("bindings"), FunctionLocation);
			for (int32 Index = 0; Index < Inputs.Num(); ++Index)
			{
				ParseBinding(
					Inputs[Index],
					FString::Printf(TEXT("%s.bindings[%d]"), *FunctionLocation, Index),
					StoredTargets,
					&TargetName,
					Function->Inputs);
			}
			Binding.Function = Function;
			OutBindings.Add(MoveTemp(Binding));
		}
	};

	void AddNodeID(TMap<FString, FGuid>& IDs, const FArcTacticsNodeDefinition& Node)
	{
		if (!Node.Name.IsNone())
		{
			IDs.Add(Node.Name.ToString(), Node.ID);
		}
	}

	TMap<FString, FGuid> CollectEndpointIDs(const UArcTacticsConfig& Config)
	{
		TMap<FString, FGuid> IDs;
		IDs.Add(ContextName, Config.ContextID);
		IDs.Add(GlobalParametersName, Config.GlobalParametersID);
		IDs.Add(DecisionSettingsName, Config.DecisionSettingsID);
		for (const FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
		{
			AddNodeID(IDs, Task);
		}
		for (const FArcTacticsActionDefinition& Action : Config.Actions)
		{
			AddNodeID(IDs, Action.Action);
			const FString Branch = Action.Action.Name.ToString();
			IDs.Add(MakeBranchKey(Branch, ParametersName), Action.ParametersID);
			IDs.Add(MakeBranchKey(Branch, ParameterBagName), Action.ParameterBagID);
			for (const FArcTacticsNodeDefinition& Step : Action.Steps)
			{
				AddNodeID(IDs, Step);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
			{
				AddNodeID(IDs, ActiveStep);
			}
			for (const FArcTacticsNodeDefinition& Consideration : Action.Considerations)
			{
				AddNodeID(IDs, Consideration);
			}
		}
		for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
		{
			AddNodeID(IDs, Selector.Selector);
			const FString Branch = Selector.Selector.Name.ToString();
			IDs.Add(MakeBranchKey(Branch, ParametersName), Selector.ParametersID);
			IDs.Add(MakeBranchKey(Branch, ParameterBagName), Selector.ParameterBagID);
			AddNodeID(IDs, Selector.Action);
			for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
			{
				AddNodeID(IDs, Step);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
			{
				AddNodeID(IDs, ActiveStep);
			}
			for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
			{
				AddNodeID(IDs, Consideration);
			}
		}
		return IDs;
	}

	void AddBindingWithDirection(
		FArcTacticsBindingCollection& Bindings,
		const FPropertyBindingPath& SourcePath,
		const FPropertyBindingPath& TargetPath,
		const bool bOutput)
	{
		Bindings.AddBinding(SourcePath, TargetPath);
		if (!bOutput)
		{
			return;
		}
		Bindings.VisitMutableBindings([&TargetPath](FPropertyBindingBinding& Binding)
		{
			if (Binding.GetTargetPath() == TargetPath)
			{
				static_cast<FArcTacticsBinding&>(Binding).SetIsOutputBinding(true);
				return FPropertyBindingBindingCollection::EVisitResult::Break;
			}
			return FPropertyBindingBindingCollection::EVisitResult::Continue;
		});
	}

	bool ValidateScratchBinding(
		UArcTacticsConfig& Scratch,
		const FArcTacticsBinding& Candidate,
		const FString& Location,
		FArcPropertyJsonReport& Report)
	{
		const FGuid SourceID = Candidate.GetSourcePath().GetStructID();
		const FGuid TargetID = Candidate.GetTargetPath().GetStructID();
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> SourceDescriptor;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> TargetDescriptor;
		FArcTacticsBindingEndpoint SourceEndpoint;
		FArcTacticsBindingEndpoint TargetEndpoint;
		if (!Scratch.GetBindableStructByID(SourceID, SourceDescriptor)
			|| !Scratch.GetBindableStructByID(TargetID, TargetDescriptor)
			|| !Scratch.ResolveBindingEndpoint(SourceID, SourceEndpoint)
			|| !Scratch.ResolveBindingEndpoint(TargetID, TargetEndpoint))
		{
			Report.Error(FString::Printf(TEXT("%s: an endpoint is not bindable."), *Location));
			return false;
		}
		const bool bAccessible = Scratch.IsBindingSourceAccessible(SourceEndpoint, TargetEndpoint, Candidate.IsOutputBinding());
		FString ValidationError;
		const EArcTacticsBindingValidationResult Result = Scratch.EditorBindings.ValidateEditorBinding(
			Candidate,
			SourceDescriptor.Get().Struct,
			SourceEndpoint,
			TargetDescriptor.Get().Struct,
			TargetEndpoint,
			bAccessible,
			&ValidationError);
		if (Result != EArcTacticsBindingValidationResult::Valid)
		{
			Report.Error(FString::Printf(
				TEXT("%s (%s binding): %s%s%s"),
				*Location,
				Candidate.IsOutputBinding() ? TEXT("output") : TEXT("forward"),
				GetValidationResultName(Result),
				ValidationError.IsEmpty() ? TEXT("") : TEXT(": "),
				*ValidationError));
			return false;
		}
		return true;
	}

	bool ResolveScratchPath(
		const UArcTacticsConfig& Scratch,
		const FGuid ID,
		const FPropertyBindingPath& Segments,
		const FString& EndpointName,
		const TCHAR* Side,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		FPropertyBindingPath& OutPath)
	{
		OutPath = FPropertyBindingPath(ID, Segments.GetSegments());
		FPropertyBindingDataView View;
		if (!Scratch.GetBindingDataViewByID(ID, View))
		{
			Report.Error(FString::Printf(TEXT("%s: '%s' has no data to bind (empty bag?)."), *Location, *EndpointName));
			return false;
		}
		FString PathError;
		if (!OutPath.IsPathEmpty() && !OutPath.UpdateSegmentsFromValue(View, &PathError))
		{
			Report.Error(FString::Printf(
				TEXT("%s: %s path '%s' does not resolve: %s"),
				*Location,
				Side,
				*PathToString(Segments),
				*PathError));
			return false;
		}
		return true;
	}

	void AddScratchBinding(
		UArcTacticsConfig& Scratch,
		const TMap<FString, FGuid>& IDs,
		const FBindingSpec& Binding,
		const FGuid& FunctionTargetID,
		FArcPropertyJsonReport& Report,
		int32& InOutExpectedBindings)
	{
		const FString Location = FString::Printf(TEXT("binding '%s'"), *Binding.Text);
		const FString& SourceKey = Binding.bOutput ? Binding.ToKey : Binding.FromKey;
		const FString& TargetKey = Binding.bOutput ? Binding.FromKey : Binding.ToKey;
		const FPropertyBindingPath& SourceSegments = Binding.bOutput ? Binding.ToPath : Binding.FromPath;
		const FPropertyBindingPath& TargetSegments = Binding.bOutput ? Binding.FromPath : Binding.ToPath;
		const FGuid* TargetID = FunctionTargetID.IsValid() ? &FunctionTargetID : IDs.Find(TargetKey);
		const FString TargetName = FunctionTargetID.IsValid() ? FString(TEXT("function")) : TargetKey;
		if (!TargetID)
		{
			Report.Error(FString::Printf(TEXT("%s: an endpoint has no authoring ID."), *Location));
			return;
		}
		FPropertyBindingPath TargetPath;
		if (!ResolveScratchPath(Scratch, *TargetID, TargetSegments, TargetName, TEXT("target"), Location, Report, TargetPath))
		{
			return;
		}

		if (Binding.Function.IsValid())
		{
			FInstancedStruct Node = Binding.Function->Node;
			const FGuid FunctionID = FGuid::NewGuid();
			Node.GetMutable<FArcTQSPropertyFunction>().ID = FunctionID;
			FPropertyBindingPath SourcePath(FunctionID, Binding.Function->OutputName);
			FString PathError;
			if (!SourcePath.UpdateSegmentsFromValue(FPropertyBindingDataView(Node), &PathError))
			{
				Report.Error(FString::Printf(TEXT("%s: the function output does not resolve: %s"), *Location, *PathError));
				return;
			}
			Scratch.EditorBindings.AddCompiledBinding(FArcTacticsBinding(FConstStructView(Node), SourcePath, TargetPath));
			++InOutExpectedBindings;
			if (!ValidateScratchBinding(Scratch, Scratch.EditorBindings.GetBindings().Last(), Location, Report))
			{
				return;
			}
			for (const FBindingSpec& Input : Binding.Function->Inputs)
			{
				AddScratchBinding(Scratch, IDs, Input, FunctionID, Report, InOutExpectedBindings);
			}
			return;
		}

		const FGuid* SourceID = IDs.Find(SourceKey);
		if (!SourceID)
		{
			Report.Error(FString::Printf(TEXT("%s: an endpoint has no authoring ID."), *Location));
			return;
		}
		FPropertyBindingPath SourcePath;
		if (!ResolveScratchPath(Scratch, *SourceID, SourceSegments, SourceKey, TEXT("source"), Location, Report, SourcePath))
		{
			return;
		}
		if (!ValidateScratchBinding(Scratch, FArcTacticsBinding(SourcePath, TargetPath, Binding.bOutput), Location, Report))
		{
			return;
		}
		AddBindingWithDirection(Scratch.EditorBindings, SourcePath, TargetPath, Binding.bOutput);
		++InOutExpectedBindings;
	}

	bool BuildScratch(UArcTacticsConfig& Scratch, FApplySpec& Spec, FArcPropertyJsonReport& Report)
	{
		Scratch.DecisionSettings = Spec.DecisionSettings;
		Scratch.GlobalParameters = MoveTemp(Spec.GlobalParameters);
		Scratch.GlobalTasks = MoveTemp(Spec.GlobalTasks);
		Scratch.Actions = MoveTemp(Spec.Actions);
		Scratch.Selectors = MoveTemp(Spec.Selectors);
		Scratch.NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);

		const TMap<FString, FGuid> IDs = CollectEndpointIDs(Scratch);
		int32 ExpectedBindings = 0;
		for (const FBindingSpec& Binding : Spec.Bindings)
		{
			AddScratchBinding(Scratch, IDs, Binding, FGuid(), Report, ExpectedBindings);
		}
		if (Report.Errors.Num() > 0)
		{
			return false;
		}

		Scratch.NotifyAuthoringChanged(EArcTacticsAuthoringChange::Value);
		if (Scratch.EditorBindings.GetNumBindings() != ExpectedBindings)
		{
			Report.Error(FString::Printf(
				TEXT("%d of %d bindings were removed by binding validation."),
				ExpectedBindings - Scratch.EditorBindings.GetNumBindings(),
				ExpectedBindings));
			return false;
		}
		return true;
	}

	void CopyAuthoringData(const UArcTacticsConfig& Source, UArcTacticsConfig& Target)
	{
		Target.ContextID = Source.ContextID;
		Target.DecisionSettingsID = Source.DecisionSettingsID;
		Target.GlobalParametersID = Source.GlobalParametersID;
		Target.DecisionSettings = Source.DecisionSettings;
		Target.GlobalParameters = Source.GlobalParameters;
		Target.GlobalTasks = Source.GlobalTasks;
		Target.Actions = Source.Actions;
		Target.Selectors = Source.Selectors;
		Target.EditorBindings.RemoveBindings([](FPropertyBindingBinding&)
		{
			return true;
		});
		for (const FArcTacticsBinding& Binding : Source.EditorBindings.GetBindings())
		{
			Target.EditorBindings.AddCompiledBinding(FArcTacticsBinding(Binding));
		}
	}

	TSharedRef<FJsonObject> MakeResult(
		const bool bSuccess,
		const FString& Asset,
		const FArcPropertyJsonReport& Report,
		const UArcTacticsConfig* Config,
		const TArray<FArcTacticsCompilerDiagnostic>& Diagnostics)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("asset"), Asset);
		Result->SetBoolField(TEXT("compiled"), Config && Config->CompiledData.IsExecutable());
		Result->SetNumberField(TEXT("revision"), Config ? Config->CompiledData.Revision : 0);
		Result->SetArrayField(TEXT("errors"), ToJsonStrings(Report.Errors));
		Result->SetArrayField(TEXT("warnings"), ToJsonStrings(Report.Warnings));
		TArray<TSharedPtr<FJsonValue>> DiagnosticItems;
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(
				TEXT("node"),
				Config ? Config->GetAuthoringName(Diagnostic.NodeID).ToString() : FString());
			Item->SetStringField(TEXT("message"), Diagnostic.Message.ToString());
			Item->SetBoolField(TEXT("error"), Diagnostic.bIsError);
			DiagnosticItems.Add(MakeShared<FJsonValueObject>(Item));
		}
		Result->SetArrayField(TEXT("diagnostics"), DiagnosticItems);
		TArray<TSharedPtr<FJsonValue>> LintItems;
		if (Config)
		{
			for (const ArcTacticsConfigLint::FFinding& Finding : ArcTacticsConfigLint::Run(*Config))
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(
					TEXT("severity"),
					Finding.Severity == ArcTacticsConfigLint::ESeverity::Warning ? TEXT("warning") : TEXT("info"));
				Item->SetStringField(TEXT("code"), Finding.Code);
				Item->SetStringField(TEXT("node"), Finding.Node);
				Item->SetStringField(TEXT("message"), Finding.Message);
				LintItems.Add(MakeShared<FJsonValueObject>(Item));
			}
		}
		Result->SetArrayField(TEXT("lint"), LintItems);
		return Result;
	}

	TSharedRef<FJsonObject> MakeFailure(const FString& Asset, const FArcPropertyJsonReport& Report)
	{
		return MakeResult(false, Asset, Report, nullptr, TArray<FArcTacticsCompilerDiagnostic>());
	}

	UArcTacticsConfig* LoadConfig(const FArcAssetPath& Path, FArcPropertyJsonReport& Report)
	{
		UObject* Object = LoadObject<UObject>(nullptr, *Path.ObjectPath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
		UArcTacticsConfig* Config = Cast<UArcTacticsConfig>(Object);
		if (!Config)
		{
			Report.Error(Object
				? FString::Printf(TEXT("'%s' is a %s, not an ArcTacticsConfig."), *Path.ObjectPath, *Object->GetClass()->GetName())
				: FString::Printf(TEXT("ArcTacticsConfig '%s' was not found."), *Path.ObjectPath));
		}
		return Config;
	}

	TSharedRef<FJsonObject> Apply(
		UArcTacticsConfig* Target,
		const FArcAssetPath* CreatePath,
		const TSharedRef<FJsonObject>& Json)
	{
		FArcPropertyJsonReport Report;
		const FString AssetName = Target ? Target->GetPathName() : (CreatePath ? CreatePath->ObjectPath : FString());
		const TSharedRef<FJsonObject> Root = CloneObject(*Json);
		TStrongObjectPtr<UArcTacticsConfig> Scratch(
			NewObject<UArcTacticsConfig>(GetTransientPackage(), NAME_None, RF_Transient));

		FApplySpec Spec;
		FApplyParser Parser(*Scratch, Spec, Report);
		Parser.ParseRoot(*Root);
		if (Report.Errors.Num() > 0 || !BuildScratch(*Scratch, Spec, Report))
		{
			return MakeFailure(AssetName, Report);
		}

		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		if (Spec.bDryRun)
		{
			if (Spec.bSave)
			{
				Report.Warning(TEXT("options.save is ignored for a dry run."));
			}
			Scratch->CompileEditorData(Diagnostics);
			TSharedRef<FJsonObject> Result = MakeResult(true, AssetName, Report, Scratch.Get(), Diagnostics);
			Result->SetBoolField(TEXT("dryRun"), true);
			Result->SetNumberField(TEXT("revision"), 0);
			return Result;
		}

		bool bCreated = false;
		if (!Target)
		{
			if (!CreatePath)
			{
				Report.Error(TEXT("No target configuration."));
				return MakeFailure(AssetName, Report);
			}
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
			Target = Cast<UArcTacticsConfig>(AssetTools.CreateAsset(
				CreatePath->AssetName,
				FPackageName::GetLongPackagePath(CreatePath->PackageName),
				UArcTacticsConfig::StaticClass(),
				nullptr));
			if (!Target)
			{
				Report.Error(FString::Printf(TEXT("Failed to create ArcTacticsConfig '%s'."), *CreatePath->ObjectPath));
				return MakeFailure(AssetName, Report);
			}
			bCreated = true;
		}

		{
			const FScopedTransaction Transaction(LOCTEXT("ApplyTransaction", "Apply Arc Tactics Configuration"));
			Target->Modify();
			CopyAuthoringData(*Scratch, *Target);
			Target->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
			if (Target->EditorBindings.GetNumBindings() != Scratch->EditorBindings.GetNumBindings())
			{
				Report.Warning(FString::Printf(
					TEXT("%d bindings were removed after the commit."),
					Scratch->EditorBindings.GetNumBindings() - Target->EditorBindings.GetNumBindings()));
			}
			Target->CompileEditorData(Diagnostics);
			Target->MarkPackageDirty();
		}

		if (Spec.bSave && !UEditorLoadingAndSavingUtils::SavePackages({Target->GetPackage()}, false))
		{
			Report.Warning(FString::Printf(TEXT("Saving '%s' failed."), *Target->GetPackage()->GetName()));
		}

		TSharedRef<FJsonObject> Result = MakeResult(true, Target->GetPathName(), Report, Target, Diagnostics);
		Result->SetBoolField(TEXT("dryRun"), false);
		Result->SetBoolField(TEXT("created"), bCreated);
		return Result;
	}

	TArray<TSharedPtr<FJsonValue>> DescribeRequirements(const FArcTacticsNodeDefinition& Definition)
	{
		static const TPair<EArcTacticsExecutionRequirement, const TCHAR*> FlagNames[] = {
			{EArcTacticsExecutionRequirement::Transform, TEXT("Transform")},
			{EArcTacticsExecutionRequirement::OwnedTags, TEXT("OwnedTags")},
			{EArcTacticsExecutionRequirement::Team, TEXT("Team")},
			{EArcTacticsExecutionRequirement::Sight, TEXT("Sight")},
			{EArcTacticsExecutionRequirement::AbilityCollection, TEXT("AbilityCollection")},
			{EArcTacticsExecutionRequirement::PathFollow, TEXT("PathFollow")},
			{EArcTacticsExecutionRequirement::KnowledgeSubsystem, TEXT("KnowledgeSubsystem")},
			{EArcTacticsExecutionRequirement::TQSSubsystem, TEXT("TQSSubsystem")},
			{EArcTacticsExecutionRequirement::AbilitySubsystem, TEXT("AbilitySubsystem")},
			{EArcTacticsExecutionRequirement::Focus, TEXT("Focus")},
		};
		const FConstStructView InstanceData(Definition.InstanceData);
		EArcTacticsExecutionRequirement Requirements = EArcTacticsExecutionRequirement::None;
		if (const FArcTacticsGlobalTask* Task = Definition.Node.GetPtr<FArcTacticsGlobalTask>())
		{
			Requirements = Task->GetRequirements(InstanceData);
		}
		else if (const FArcTacticsActionNode* Action = Definition.Node.GetPtr<FArcTacticsActionNode>())
		{
			Requirements = Action->GetRequirements(InstanceData);
		}
		else if (const FArcTacticsStepNode* Step = Definition.Node.GetPtr<FArcTacticsStepNode>())
		{
			Requirements = Step->GetRequirements(InstanceData);
		}
		else if (const FArcTacticsActiveStepNode* ActiveStep = Definition.Node.GetPtr<FArcTacticsActiveStepNode>())
		{
			Requirements = ActiveStep->GetRequirements(InstanceData);
		}
		else if (const FArcTacticsConsiderationNode* Consideration = Definition.Node.GetPtr<FArcTacticsConsiderationNode>())
		{
			Requirements = Consideration->GetRequirements(InstanceData);
		}
		TArray<TSharedPtr<FJsonValue>> Names;
		EArcTacticsExecutionRequirement Named = EArcTacticsExecutionRequirement::None;
		for (const TPair<EArcTacticsExecutionRequirement, const TCHAR*>& Flag : FlagNames)
		{
			if (EnumHasAnyFlags(Requirements, Flag.Key))
			{
				Names.Add(MakeShared<FJsonValueString>(Flag.Value));
				Named |= Flag.Key;
			}
		}
		checkf(Named == Requirements, TEXT("Execution requirement flags 0x%x of %s have no name"), static_cast<uint32>(Requirements & ~Named), *GetNameSafe(Definition.Node.GetScriptStruct()));
		return Names;
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
					Field.SetStringField(TEXT("role"), GetPropertyRoleName(ArcTacticsNode::ResolvePropertyRole(*Property)));
					Field.SetBoolField(TEXT("required"), ArcTacticsNode::IsRequiredInput(*Property));
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

	TSharedRef<FJsonObject> DescribeFunctionType(const UScriptStruct& Struct, const FProperty& OutputProperty)
	{
		FInstancedStruct Probe;
		Probe.InitializeAs(&Struct);
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), GetTypeName<UScriptStruct>(&Struct));
		Json->SetStringField(TEXT("displayName"), Struct.GetDisplayNameText().ToString());
		Json->SetStringField(TEXT("role"), TEXT("Function"));
		const FString& Tooltip = Struct.GetMetaData(TEXT("ToolTip"));
		if (!Tooltip.IsEmpty())
		{
			Json->SetStringField(TEXT("description"), Tooltip);
		}
		FString ExtendedType;
		const FString CppType = OutputProperty.GetCPPType(&ExtendedType, 0);
		Json->SetStringField(TEXT("output"), OutputProperty.GetName());
		Json->SetStringField(TEXT("outputType"), CppType + ExtendedType);
		Json->SetArrayField(TEXT("fields"), DescribeFields(&Struct, Probe.GetMemory(), true));
		return Json;
	}

	bool RefreshExportPath(const UArcTacticsConfig& Config, FPropertyBindingPath& Path, FString& OutError)
	{
		if (Path.IsPathEmpty())
		{
			return true;
		}
		FPropertyBindingDataView View;
		if (!Config.GetBindingDataViewByID(Path.GetStructID(), View))
		{
			OutError = TEXT("the endpoint has no data to bind");
			return false;
		}
		return Path.UpdateSegmentsFromValue(View, &OutError);
	}

	FString FormatExportBinding(const FArcTacticsBinding& Binding, const FString& Source, const FString& Target)
	{
		return Binding.IsOutputBinding() ? Source + TEXT(" <- ") + Target : Target + TEXT(" <- ") + Source;
	}

	TSharedPtr<FJsonValue> ExportBindingValue(
		const UArcTacticsConfig& Config,
		const FArcTacticsBinding& Binding,
		const TMap<FGuid, FString>& Endpoints,
		bool bNested,
		int32 Depth,
		TArray<FString>& Warnings);

	TSharedRef<FJsonObject> ExportFunction(
		const UArcTacticsConfig& Config,
		const FConstStructView FunctionView,
		const FGuid& FunctionID,
		const TMap<FGuid, FString>& Endpoints,
		const int32 Depth,
		TArray<FString>& Warnings)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		const UScriptStruct* Type = FunctionView.GetScriptStruct();
		Out->SetStringField(TEXT("type"), GetTypeName<UScriptStruct>(Type));
		SetObjectIfNotEmpty(*Out, TEXT("data"), ExportEditableDelta(*Type, FunctionView.GetMemory()));
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Inputs;
		for (const FArcTacticsBinding& Input : Config.EditorBindings.GetBindings())
		{
			if (Input.GetTargetPath().GetStructID() != FunctionID)
			{
				continue;
			}
			const TSharedPtr<FJsonValue> Value = ExportBindingValue(Config, Input, Endpoints, true, Depth, Warnings);
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
		const UArcTacticsConfig& Config,
		const FArcTacticsBinding& Binding,
		const TMap<FGuid, FString>& Endpoints,
		const bool bNested,
		const int32 Depth,
		TArray<FString>& Warnings)
	{
		const FConstStructView FunctionView = Binding.GetPropertyFunctionNode();
		const FString* SourceName = Endpoints.Find(Binding.GetSourcePath().GetStructID());
		const FString* TargetName = Endpoints.Find(Binding.GetTargetPath().GetStructID());
		const FString SourceText = FunctionView.IsValid()
			? FString::Printf(TEXT("function %s"), *GetTypeName<UScriptStruct>(FunctionView.GetScriptStruct()))
			: JoinEndpoint(SourceName ? *SourceName : Binding.GetSourcePath().GetStructID().ToString(), Binding.GetSourcePath());
		const FString TargetText = bNested
			? PathToString(Binding.GetTargetPath())
			: JoinEndpoint(TargetName ? *TargetName : Binding.GetTargetPath().GetStructID().ToString(), Binding.GetTargetPath());
		const FString Description = FormatExportBinding(Binding, SourceText, TargetText);
		if ((!SourceName && !FunctionView.IsValid()) || (!TargetName && !bNested))
		{
			Warnings.Add(FString::Printf(
				TEXT("Dropped binding %s: %s not in the configuration."),
				*Description,
				!SourceName && !FunctionView.IsValid() && !TargetName && !bNested ? TEXT("the source and target endpoints are")
					: (!SourceName && !FunctionView.IsValid() ? TEXT("the source endpoint is") : TEXT("the target endpoint is"))));
			return TSharedPtr<FJsonValue>();
		}
		FPropertyBindingPath TargetPath = Binding.GetTargetPath();
		FString PathError;
		if (!RefreshExportPath(Config, TargetPath, PathError))
		{
			Warnings.Add(FString::Printf(TEXT("Dropped binding %s: a path does not resolve: %s"), *Description, *PathError));
			return TSharedPtr<FJsonValue>();
		}
		const FString ToText = bNested ? PathToString(TargetPath) : JoinEndpoint(*TargetName, TargetPath);

		if (FunctionView.IsValid())
		{
			const FArcTQSPropertyFunction* Function = FunctionView.GetPtr<const FArcTQSPropertyFunction>();
			if (!Function || Binding.GetSourcePath().GetStructID() != Function->ID || Binding.IsOutputBinding())
			{
				Warnings.Add(FString::Printf(
					TEXT("Dropped binding %s: the bound function is not a property function sourcing its own output."),
					*Description));
				return TSharedPtr<FJsonValue>();
			}
			if (Depth > Config.EditorBindings.GetNumBindings())
			{
				Warnings.Add(FString::Printf(TEXT("Dropped binding %s: bound functions form a cycle."), *Description));
				return TSharedPtr<FJsonValue>();
			}
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetStringField(TEXT("to"), ToText);
			Out->SetObjectField(TEXT("function"), ExportFunction(Config, FunctionView, Function->ID, Endpoints, Depth + 1, Warnings));
			return MakeShared<FJsonValueObject>(Out);
		}

		FPropertyBindingPath SourcePath = Binding.GetSourcePath();
		if (!RefreshExportPath(Config, SourcePath, PathError))
		{
			Warnings.Add(FString::Printf(TEXT("Dropped binding %s: a path does not resolve: %s"), *Description, *PathError));
			return TSharedPtr<FJsonValue>();
		}
		return MakeShared<FJsonValueString>(FormatExportBinding(Binding, JoinEndpoint(*SourceName, SourcePath), ToText));
	}
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::ListNodeTypes(const FString& Role, const FString& Filter)
{
	using namespace ArcTacticsConfigJsonPrivate;
	FArcPropertyJsonReport Report;
	const bool bFunctionRole = Role.TrimStartAndEnd().Equals(TEXT("Function"), ESearchCase::IgnoreCase);
	EArcTacticsNodeRole RequestedRole = EArcTacticsNodeRole::Invalid;
	if (!bFunctionRole && !ParseNodeRole(Role, RequestedRole))
	{
		Report.Error(FString::Printf(
			TEXT("Unknown role '%s' (use GlobalTask, Action, Consideration, Selector, Step, ActiveStep, Function or empty)."),
			*Role));
		return MakeFailure(FString(), Report);
	}
	const bool bListFunctions = bFunctionRole || RequestedRole == EArcTacticsNodeRole::Invalid;
	const int32 FunctionOrder = static_cast<int32>(EArcTacticsNodeRole::ActiveStep) + 1;

	struct FEntry
	{
		int32 Order;
		FString Name;
		TSharedRef<FJsonObject> Json;
	};
	TArray<FEntry> Entries;
	const FString TrimmedFilter = Filter.TrimStartAndEnd();
	const UArcTacticsConfig& Config = *GetDefault<UArcTacticsConfig>();
	const UScriptStruct* FunctionBase = FArcTQSPropertyFunction::StaticStruct();
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		if (Struct != FunctionBase && Struct->IsChildOf(FunctionBase))
		{
			int32 NumOutputs = 0;
			const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(Struct, &NumOutputs);
			const FString Name = GetTypeName<UScriptStruct>(Struct);
			const FString DisplayName = Struct->GetDisplayNameText().ToString();
			if (bListFunctions
				&& OutputProperty
				&& NumOutputs == 1
				&& !Struct->HasAnyFlags(RF_NewerVersionExists)
				&& !Struct->HasMetaData(TEXT("Hidden"))
				&& (TrimmedFilter.IsEmpty() || Name.Contains(TrimmedFilter) || DisplayName.Contains(TrimmedFilter)))
			{
				Entries.Add({FunctionOrder, Name, DescribeFunctionType(*Struct, *OutputProperty)});
			}
			continue;
		}
		if (bFunctionRole
			|| Struct == FArcTacticsNode::StaticStruct()
			|| !Struct->IsChildOf(FArcTacticsNode::StaticStruct())
			|| ArcTacticsAuthoring::IsRoleBaseNodeType(Struct)
			|| ArcTacticsAuthoring::IsNodeTypeHidden(Struct))
		{
			continue;
		}
		FInstancedStruct Probe;
		Probe.InitializeAs(Struct);
		const FArcTacticsNode& Node = Probe.Get<FArcTacticsNode>();
		const EArcTacticsNodeRole NodeRole = Node.GetRole();
		if (NodeRole == EArcTacticsNodeRole::Invalid
			|| (RequestedRole != EArcTacticsNodeRole::Invalid && NodeRole != RequestedRole))
		{
			continue;
		}
		FArcTacticsNodeDefinition Definition;
		if (!ArcTacticsAuthoring::PrepareNode(Config, Definition, Struct, NodeRole))
		{
			continue;
		}
		const FString Name = GetTypeName<UScriptStruct>(Struct);
		const FString DisplayName = Node.GetDisplayName().ToString();
		if (!TrimmedFilter.IsEmpty()
			&& !Name.Contains(TrimmedFilter)
			&& !DisplayName.Contains(TrimmedFilter))
		{
			continue;
		}
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Name);
		Json->SetStringField(TEXT("displayName"), DisplayName);
		Json->SetStringField(TEXT("role"), GetNodeRoleName(NodeRole));
		const FString& Tooltip = Struct->GetMetaData(TEXT("ToolTip"));
		if (!Tooltip.IsEmpty())
		{
			Json->SetStringField(TEXT("description"), Tooltip);
		}
		Json->SetStringField(
			TEXT("instanceDataType"),
			GetTypeName<UScriptStruct>(Definition.InstanceData.GetScriptStruct()));
		Json->SetArrayField(TEXT("nodeFields"), DescribeFields(Struct, Definition.Node.GetMemory(), false));
		Json->SetArrayField(
			TEXT("fields"),
			DescribeFields(Definition.InstanceData.GetScriptStruct(), Definition.InstanceData.GetMemory(), false));
		Json->SetArrayField(TEXT("requirements"), DescribeRequirements(Definition));
		Entries.Add({static_cast<int32>(NodeRole), Name, Json});
	}
	Entries.Sort([](const FEntry& A, const FEntry& B)
	{
		return A.Order != B.Order ? A.Order < B.Order : A.Name < B.Name;
	});

	TArray<TSharedPtr<FJsonValue>> Types;
	for (const FEntry& Entry : Entries)
	{
		Types.Add(MakeShared<FJsonValueObject>(Entry.Json));
	}
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), Types.Num());
	Result->SetArrayField(TEXT("types"), Types);
	return Result;
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::ExportConfig(const UArcTacticsConfig& Config)
{
	using namespace ArcTacticsConfigJsonPrivate;
	FExportNames Names;
	for (const FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
	{
		Names.Allocate(Task);
	}
	for (const FArcTacticsActionDefinition& Action : Config.Actions)
	{
		Names.Allocate(Action.Action);
		for (const FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			Names.Allocate(Step);
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
		{
			Names.Allocate(ActiveStep);
		}
		for (const FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			Names.Allocate(Consideration);
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
	{
		Names.Allocate(Selector.Selector);
		if (Selector.Action.Node.IsValid())
		{
			Names.Allocate(Selector.Action);
			for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
			{
				Names.Allocate(Step);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
			{
				Names.Allocate(ActiveStep);
			}
		}
		for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			Names.Allocate(Consideration);
		}
	}

	TMap<FGuid, FString> Endpoints = Names.ByID;
	Endpoints.Add(Config.ContextID, ContextName);
	Endpoints.Add(Config.GlobalParametersID, GlobalParametersName);
	Endpoints.Add(Config.DecisionSettingsID, DecisionSettingsName);
	for (const FArcTacticsActionDefinition& Action : Config.Actions)
	{
		const FString Branch = Names.ByID.FindRef(Action.Action.ID);
		Endpoints.Add(Action.ParametersID, MakeBranchKey(Branch, ParametersName));
		Endpoints.Add(Action.ParameterBagID, MakeBranchKey(Branch, ParameterBagName));
	}
	for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
	{
		const FString Branch = Names.ByID.FindRef(Selector.Selector.ID);
		Endpoints.Add(Selector.ParametersID, MakeBranchKey(Branch, ParametersName));
		Endpoints.Add(Selector.ParameterBagID, MakeBranchKey(Branch, ParameterBagName));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), FormatVersion);
	SetObjectIfNotEmpty(
		*Root,
		TEXT("decisionSettings"),
		ExportEditableDelta(*FArcTacticsDecisionSettingsParameters::StaticStruct(), &Config.DecisionSettings));
	Root->SetArrayField(TEXT("globalParameters"), ArcPropertyJson::ExportBag(Config.GlobalParameters, GetHooks()));

	TArray<TSharedPtr<FJsonValue>> Tasks;
	for (const FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
	{
		Tasks.Add(MakeShared<FJsonValueObject>(ExportNode(Task, Names.ByID.FindRef(Task.ID))));
	}
	Root->SetArrayField(TEXT("globalTasks"), Tasks);

	TArray<TSharedPtr<FJsonValue>> Actions;
	for (const FArcTacticsActionDefinition& Action : Config.Actions)
	{
		const TSharedRef<FJsonObject> ActionJson = ExportNode(Action.Action, Names.ByID.FindRef(Action.Action.ID));
		ExportNodes(*ActionJson, TEXT("steps"), Action.Steps, Names);
		ExportNodes(*ActionJson, TEXT("activeSteps"), Action.ActiveSteps, Names);
		ExportBranchContents(*ActionJson, Action.Parameters, Action.ParameterBag, Action.Considerations, Names);
		Actions.Add(MakeShared<FJsonValueObject>(ActionJson));
	}
	Root->SetArrayField(TEXT("actions"), Actions);

	TArray<FString> Warnings;
	TArray<TSharedPtr<FJsonValue>> Selectors;
	for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
	{
		const TSharedRef<FJsonObject> SelectorJson =
			ExportNode(Selector.Selector, Names.ByID.FindRef(Selector.Selector.ID));
		if (Selector.Action.Node.IsValid())
		{
			const TSharedRef<FJsonObject> ActionJson = ExportNode(Selector.Action, Names.ByID.FindRef(Selector.Action.ID));
			ExportNodes(*ActionJson, TEXT("steps"), Selector.Steps, Names);
			ExportNodes(*ActionJson, TEXT("activeSteps"), Selector.ActiveSteps, Names);
			SelectorJson->SetObjectField(TEXT("action"), ActionJson);
		}
		else if (Selector.Steps.Num() > 0 || Selector.ActiveSteps.Num() > 0)
		{
			Warnings.Add(FString::Printf(
				TEXT("Dropped %d steps and %d active steps of selector '%s': its action slot is empty."),
				Selector.Steps.Num(),
				Selector.ActiveSteps.Num(),
				*Names.ByID.FindRef(Selector.Selector.ID)));
		}
		ExportBranchContents(
			*SelectorJson, Selector.Parameters, Selector.ParameterBag, Selector.Considerations, Names);
		Selectors.Add(MakeShared<FJsonValueObject>(SelectorJson));
	}
	Root->SetArrayField(TEXT("selectors"), Selectors);

	TArray<TSharedPtr<FJsonValue>> Bindings;
	for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
	{
		const FGuid TargetID = Binding.GetTargetPath().GetStructID();
		if (Config.FindFunctionBinding(TargetID))
		{
			FArcTacticsBindingEndpoint FunctionEndpoint;
			if (!Config.ResolveBindingEndpoint(TargetID, FunctionEndpoint))
			{
				Warnings.Add(FString::Printf(
					TEXT("Dropped function input binding to '%s': its function does not feed a tactics node, parameters or decision settings."),
					*PathToString(Binding.GetTargetPath())));
			}
			continue;
		}
		const TSharedPtr<FJsonValue> Value = ExportBindingValue(Config, Binding, Endpoints, false, 0, Warnings);
		if (Value.IsValid())
		{
			Bindings.Add(Value);
		}
	}
	Root->SetArrayField(TEXT("bindings"), Bindings);
	if (Warnings.Num() > 0)
	{
		Root->SetArrayField(TEXT("warnings"), ToJsonStrings(Warnings));
	}
	return Root;
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::ExportAsset(const FString& AssetPath)
{
	using namespace ArcTacticsConfigJsonPrivate;
	FArcPropertyJsonReport Report;
	FArcAssetPath Path;
	if (!ArcPropertyJson::ParseAssetPath(AssetPath, ExampleAssetPath, Path, Report))
	{
		return MakeFailure(AssetPath, Report);
	}
	const UArcTacticsConfig* Config = LoadConfig(Path, Report);
	return Config ? ExportConfig(*Config) : MakeFailure(Path.ObjectPath, Report);
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::ApplyToConfig(
	UArcTacticsConfig& Config,
	const TSharedRef<FJsonObject>& Json)
{
	return ArcTacticsConfigJsonPrivate::Apply(&Config, nullptr, Json);
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::ApplyToAsset(
	const FString& AssetPath,
	const TSharedRef<FJsonObject>& Json)
{
	using namespace ArcTacticsConfigJsonPrivate;
	FArcPropertyJsonReport Report;
	FArcAssetPath Path;
	if (!ArcPropertyJson::ParseAssetPath(AssetPath, ExampleAssetPath, Path, Report))
	{
		return MakeFailure(AssetPath, Report);
	}
	UObject* Existing = LoadObject<UObject>(nullptr, *Path.ObjectPath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
	if (Existing && !Existing->IsA<UArcTacticsConfig>())
	{
		Report.Error(FString::Printf(
			TEXT("'%s' is a %s, not an ArcTacticsConfig."),
			*Path.ObjectPath,
			*Existing->GetClass()->GetName()));
		return MakeFailure(Path.ObjectPath, Report);
	}
	return Apply(Cast<UArcTacticsConfig>(Existing), &Path, Json);
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::CompileConfig(UArcTacticsConfig& Config)
{
	using namespace ArcTacticsConfigJsonPrivate;
	TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
	{
		const FScopedTransaction Transaction(LOCTEXT("CompileTransaction", "Compile Arc Tactics Configuration"));
		Config.Modify();
		Config.CompileEditorData(Diagnostics);
		Config.MarkPackageDirty();
	}
	return MakeResult(true, Config.GetPathName(), FArcPropertyJsonReport(), &Config, Diagnostics);
}

TSharedRef<FJsonObject> ArcTacticsConfigJson::CompileAsset(const FString& AssetPath)
{
	using namespace ArcTacticsConfigJsonPrivate;
	FArcPropertyJsonReport Report;
	FArcAssetPath Path;
	if (!ArcPropertyJson::ParseAssetPath(AssetPath, ExampleAssetPath, Path, Report))
	{
		return MakeFailure(AssetPath, Report);
	}
	UArcTacticsConfig* Config = LoadConfig(Path, Report);
	return Config ? CompileConfig(*Config) : MakeFailure(Path.ObjectPath, Report);
}

#undef LOCTEXT_NAMESPACE
