#include "CQTest.h"

#if WITH_EDITOR

#include "Actions/ArcTacticsSequenceAction.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTacticsConfigJson.h"
#include "ArcTacticsEditorViewModel.h"
#include "Decision/ArcTacticsConfig.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Fixtures/ArcTacticsTestTypes.h"
#include "Functions/ArcTQSFunctions_Vector.h"
#include "HAL/PlatformApplicationMisc.h"
#include "NativeGameplayTags.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Steps/ArcTacticsMemorySteps.h"
#include "TargetQuery/Functions/ArcTQSFunctions_Gameplay.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsFunctionAuthoringTest_Key, "Test.ArcTactics.FunctionAuthoring.Key");

namespace ArcTacticsFunctionAuthoringTestsPrivate
{
	UArcTacticsConfig* MakeConfig()
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage(), NAME_None, RF_Transactional);
		Config->ContextID = FGuid(90, 0, 0, 1);
		return Config;
	}

	FPropertyBindingPath MakePath(const FGuid ID, const TCHAR* Path)
	{
		FPropertyBindingPath Result(ID);
		Result.FromString(Path);
		return Result;
	}

	template <typename TFunction>
	void BindFunctionWithID(UArcTacticsConfig& Config, const TFunction& Function, const FGuid FunctionID, const FGuid TargetID, const TCHAR* TargetPath)
	{
		FInstancedStruct Node;
		Node.InitializeAs<TFunction>(Function);
		Node.GetMutable<FArcTQSPropertyFunction>().ID = FunctionID;
		const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(TFunction::StaticStruct());
		check(OutputProperty);
		Config.EditorBindings.AddCompiledBinding(FArcTacticsBinding(
			FConstStructView(Node),
			FPropertyBindingPath(FunctionID, OutputProperty->GetFName()),
			MakePath(TargetID, TargetPath)));
	}

	const FArcTacticsBinding* FindBindingTo(const UArcTacticsConfig& Config, const FGuid TargetID, const FName TargetRoot)
	{
		return Config.EditorBindings.GetBindings().FindByPredicate([TargetID, TargetRoot](const FArcTacticsBinding& Binding)
		{
			return Binding.GetTargetPath().GetStructID() == TargetID
				&& Binding.GetTargetPath().NumSegments() > 0
				&& Binding.GetTargetPath().GetSegment(0).GetName() == TargetRoot;
		});
	}

	int32 CountBindingsTargeting(const UArcTacticsConfig& Config, const FGuid TargetID)
	{
		int32 Count = 0;
		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			Count += Binding.GetTargetPath().GetStructID() == TargetID ? 1 : 0;
		}
		return Count;
	}

	const FArcTQSPropertyFunction* GetFunction(const FArcTacticsBinding* Binding)
	{
		return Binding ? Binding->GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>() : nullptr;
	}

	struct FNestedChain
	{
		bool bValid = false;
		FGuid OuterID;
		FGuid InnerID;
		FGuid InnerSourceID;
		const UScriptStruct* OuterType = nullptr;
		const UScriptStruct* InnerType = nullptr;
	};

	FNestedChain ReadChain(const UArcTacticsConfig& Config, const FGuid NodeID, const FName NodeField, const FName OuterField, const FName InnerField)
	{
		FNestedChain Chain;
		const FArcTacticsBinding* OuterBinding = FindBindingTo(Config, NodeID, NodeField);
		const FArcTQSPropertyFunction* Outer = GetFunction(OuterBinding);
		if (!Outer || OuterBinding->GetSourcePath().GetStructID() != Outer->ID)
		{
			return Chain;
		}
		const FArcTacticsBinding* InnerBinding = FindBindingTo(Config, Outer->ID, OuterField);
		const FArcTQSPropertyFunction* Inner = GetFunction(InnerBinding);
		if (!Inner || InnerBinding->GetSourcePath().GetStructID() != Inner->ID)
		{
			return Chain;
		}
		const FArcTacticsBinding* InputBinding = FindBindingTo(Config, Inner->ID, InnerField);
		if (!InputBinding)
		{
			return Chain;
		}
		Chain.bValid = true;
		Chain.OuterID = Outer->ID;
		Chain.InnerID = Inner->ID;
		Chain.InnerSourceID = InputBinding->GetSourcePath().GetStructID();
		Chain.OuterType = OuterBinding->GetPropertyFunctionNode().GetScriptStruct();
		Chain.InnerType = InnerBinding->GetPropertyFunctionNode().GetScriptStruct();
		return Chain;
	}

	FArcTacticsActionDefinition MakeTestAction(const FGuid ActionID)
	{
		FArcTacticsActionDefinition Action;
		Action.Action.ID = ActionID;
		Action.Action.Node.InitializeAs<FArcTacticsTestAction>();
		Action.Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		Action.ParameterBag.AddProperty(TEXT("Where"), EPropertyBagPropertyType::Struct, TBaseStructure<FVector>::Get());
		return Action;
	}

	const FGuid OuterFunctionID(91, 1, 0, 0);
	const FGuid InnerFunctionID(91, 2, 0, 0);

	UArcTacticsConfig* MakeNestedFunctionConfig()
	{
		UArcTacticsConfig* Config = MakeConfig();
		const FGuid ActionID(90, 1, 0, 0);
		Config->Actions.Add(MakeTestAction(ActionID));
		Config->EnsureValidNodeIDs();
		Config->EditorBindings.AddBinding(FPropertyBindingPath(Config->ContextID, TEXT("Self")), FPropertyBindingPath(ActionID, TEXT("Entity")));
		FArcTQSFunction_VectorScale Scale;
		Scale.Scale = 3.0f;
		BindFunctionWithID(*Config, Scale, InnerFunctionID, OuterFunctionID, TEXT("A"));
		BindFunctionWithID(*Config, FArcTQSFunction_VectorAdd(), OuterFunctionID, ActionID, TEXT("Location"));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Config->Actions[0].ParameterBagID, TEXT("Where")),
			FPropertyBindingPath(InnerFunctionID, TEXT("Vector")));
		return Config;
	}

	class FClipboardRestore final
	{
	public:
		FClipboardRestore()
		{
			FPlatformApplicationMisc::ClipboardPaste(PreviousText);
		}

		~FClipboardRestore()
		{
			FPlatformApplicationMisc::ClipboardCopy(*PreviousText);
		}

	private:
		FString PreviousText;
	};

	TSharedRef<FJsonObject> Parse(const FString& Text, bool& bOutParsed)
	{
		TSharedPtr<FJsonObject> Object;
		bOutParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid();
		return bOutParsed ? Object.ToSharedRef() : MakeShared<FJsonObject>();
	}

	FString Serialize(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Object, Writer);
		return Text;
	}

	bool ResultFlag(const TSharedRef<FJsonObject>& Result, const TCHAR* Field)
	{
		bool bValue = false;
		return Result->TryGetBoolField(Field, bValue) && bValue;
	}

	FString JoinErrors(const TSharedRef<FJsonObject>& Result)
	{
		FString Joined;
		const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
		if (Result->TryGetArrayField(TEXT("errors"), Errors))
		{
			for (const TSharedPtr<FJsonValue>& Error : *Errors)
			{
				Joined += Error->AsString() + TEXT("\n");
			}
		}
		return Joined;
	}

	bool HasLint(const TSharedRef<FJsonObject>& Result, const TCHAR* Code, const TCHAR* MessagePart)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Result->TryGetArrayField(TEXT("lint"), Items))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			const TSharedPtr<FJsonObject> Object = Item->AsObject();
			if (Object.IsValid()
				&& Object->GetStringField(TEXT("code")) == Code
				&& Object->GetStringField(TEXT("message")).Contains(MessagePart))
			{
				return true;
			}
		}
		return false;
	}

	TSharedPtr<FJsonObject> FindFunctionBindingObject(const TArray<TSharedPtr<FJsonValue>>& Bindings, const TCHAR* To)
	{
		for (const TSharedPtr<FJsonValue>& Value : Bindings)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object->IsValid() && (*Object)->GetStringField(TEXT("to")) == To)
			{
				return *Object;
			}
		}
		return nullptr;
	}

	const TCHAR* const FunctionJson = TEXT(R"JSON(
{
	"version": 2,
	"globalParameters": [
		{ "name": "OffsetY", "type": "Double", "value": 0.5 },
		{ "name": "Unread", "type": "Int32" }
	],
	"globalTasks": [ { "name": "Perceive", "type": "ArcTacticsTestTask" } ],
	"actions": [ { "name": "Idle", "type": "ArcTacticsTestAction" } ],
	"bindings": [
		"Idle.Entity <- Context.Self",
		{
			"to": "Idle.Location",
			"function": {
				"type": "ArcTQSFunction_VectorAdd",
				"bindings": [
					{ "to": "A", "function": { "type": "ArcTQSFunction_VectorScale", "data": { "scale": 3 }, "bindings": [ "Vector <- Perceive.Record.Location" ] } },
					{ "to": "B", "function": { "type": "ArcTQSFunction_MakeVector", "data": { "z": 100 }, "bindings": [ "X <- Context.Self.Index", "Y <- GlobalParameters.OffsetY" ] } }
				]
			}
		}
	]
}
)JSON");

	const TCHAR* const LiteralOnlyFunctionJson = TEXT(R"JSON(
{
	"version": 2,
	"globalParameters": [ { "name": "KeyTag", "type": "Struct", "valueType": "GameplayTag" } ],
	"actions": [ { "name": "Idle", "type": "ArcTacticsTestAction" } ],
	"bindings": [
		"Idle.Entity <- Context.Self",
		{ "to": "Idle.NestedValue", "function": { "type": "ArcTQSFunction_MemoryFloat", "data": { "key": "Test.ArcTactics.FunctionAuthoring.Key" }, "bindings": [ %KEYBINDING% ] } }
	]
}
)JSON");

	const TCHAR* const LiteralOnlyStepJson = TEXT(R"JSON(
{
	"version": 2,
	"globalParameters": [ { "name": "KeyTag", "type": "Struct", "valueType": "GameplayTag" } ],
	"actions": [
		{
			"name": "Remember",
			"type": "ArcTacticsSequenceAction",
			"steps": [ { "name": "Write", "type": "ArcTacticsWriteMemoryFloatStep", "data": { "key": "Test.ArcTactics.FunctionAuthoring.Key" } } ]
		}
	],
	"bindings": [ %KEYBINDING% ]
}
)JSON");

	FString WithKeyBinding(const TCHAR* Template, const TCHAR* KeyBinding)
	{
		FString Text = Template;
		Text.ReplaceInline(TEXT("%KEYBINDING%"), KeyBinding);
		return Text;
	}

	TSharedPtr<FJsonObject> FindType(const TSharedRef<FJsonObject>& Listing, const TCHAR* Name)
	{
		const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
		if (!Listing->TryGetArrayField(TEXT("types"), Types))
		{
			return nullptr;
		}
		for (const TSharedPtr<FJsonValue>& Type : *Types)
		{
			const TSharedPtr<FJsonObject> Object = Type->AsObject();
			if (Object.IsValid() && Object->GetStringField(TEXT("name")) == Name)
			{
				return Object;
			}
		}
		return nullptr;
	}

	TSharedPtr<FJsonObject> FindField(const TSharedPtr<FJsonObject>& Type, const TCHAR* Name)
	{
		const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
		if (!Type.IsValid() || !Type->TryGetArrayField(TEXT("fields"), Fields))
		{
			return nullptr;
		}
		for (const TSharedPtr<FJsonValue>& Field : *Fields)
		{
			const TSharedPtr<FJsonObject> Object = Field->AsObject();
			if (Object.IsValid() && Object->GetStringField(TEXT("name")) == Name)
			{
				return Object;
			}
		}
		return nullptr;
	}
}

TEST_CLASS(ArcTacticsFunctionEditorTests, "ArcTactics.Functions.Editor")
{
	TEST_METHOD(DuplicateAndCopyPaste_KeepNestedFunctionsAuthoredInnerFirstWithNewIDsAndRemappedScope)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		UArcTacticsConfig* Config = MakeNestedFunctionConfig();
		const FGuid OriginalActionID = Config->Actions[0].Action.ID;
		const FGuid OriginalBagID = Config->Actions[0].ParameterBagID;
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Compile(), TEXT("Control: the nested function config compiles")));
		const FNestedChain Original = ReadChain(*Config, OriginalActionID, TEXT("Location"), TEXT("A"), TEXT("Vector"));
		ASSERT_THAT(IsTrue(Original.bValid && Original.OuterID == OuterFunctionID && Original.InnerID == InnerFunctionID));

		const FArcTacticsEditorSelection OriginalSelection = {EArcTacticsEditorItemKind::Action, OriginalActionID};
		ASSERT_THAT(IsTrue(ViewModel->Duplicate(OriginalSelection)));
		ASSERT_THAT(AreEqual(2, Config->Actions.Num()));
		if (Config->Actions.Num() != 2)
		{
			return;
		}
		const FGuid DuplicateID = Config->Actions[1].Action.ID;
		const FNestedChain Duplicate = ReadChain(*Config, DuplicateID, TEXT("Location"), TEXT("A"), TEXT("Vector"));
		ASSERT_THAT(IsTrue(Duplicate.bValid, TEXT("The duplicate carries the outer function, the nested function and its input")));
		ASSERT_THAT(IsTrue(Duplicate.OuterType == FArcTQSFunction_VectorAdd::StaticStruct()
			&& Duplicate.InnerType == FArcTQSFunction_VectorScale::StaticStruct()));
		ASSERT_THAT(IsFalse(Duplicate.OuterID == OuterFunctionID || Duplicate.InnerID == InnerFunctionID
			|| Duplicate.OuterID == Duplicate.InnerID, TEXT("Both function IDs are regenerated")));
		ASSERT_THAT(IsTrue(Duplicate.InnerSourceID == Config->Actions[1].ParameterBagID,
			TEXT("The nested function's input reads the duplicate's own bag")));
		ASSERT_THAT(AreEqual(1, CountBindingsTargeting(*Config, OuterFunctionID),
			TEXT("The original outer function keeps exactly its own nested input")));
		const FNestedChain OriginalAfter = ReadChain(*Config, OriginalActionID, TEXT("Location"), TEXT("A"), TEXT("Vector"));
		ASSERT_THAT(IsTrue(OriginalAfter.bValid && OriginalAfter.InnerSourceID == OriginalBagID));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(AreEqual(4, Config->CompiledData.Functions.Num()));

		{
			FClipboardRestore ClipboardRestore;
			ASSERT_THAT(IsTrue(ViewModel->Copy(OriginalSelection)));
			ASSERT_THAT(IsTrue(ViewModel->CanPaste(FArcTacticsEditorSelection::Config())));
			ASSERT_THAT(IsTrue(ViewModel->Paste(FArcTacticsEditorSelection::Config())));
		}
		ASSERT_THAT(AreEqual(3, Config->Actions.Num()));
		if (Config->Actions.Num() != 3)
		{
			return;
		}
		const FGuid PastedID = Config->Actions[2].Action.ID;
		const FNestedChain Pasted = ReadChain(*Config, PastedID, TEXT("Location"), TEXT("A"), TEXT("Vector"));
		ASSERT_THAT(IsTrue(Pasted.bValid, TEXT("Paste carries the nested functions")));
		ASSERT_THAT(IsFalse(Pasted.OuterID == OuterFunctionID || Pasted.InnerID == InnerFunctionID
			|| Pasted.OuterID == Duplicate.OuterID || Pasted.InnerID == Duplicate.InnerID));
		ASSERT_THAT(IsTrue(Pasted.InnerSourceID == Config->Actions[2].ParameterBagID,
			TEXT("The pasted nested function's input reads the pasted action's bag")));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(AreEqual(6, Config->CompiledData.Functions.Num()));
	}

	TEST_METHOD(MoveConsideration_RecreatesItsFunctionWithANewIDReadingTheDestinationBag)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		UArcTacticsConfig* Config = MakeConfig();
		const FGuid SourceActionID(92, 1, 0, 0);
		const FGuid DestinationActionID(92, 2, 0, 0);
		const FGuid ConsiderationID(92, 3, 0, 0);
		const FGuid FunctionID(92, 4, 0, 0);
		FArcTacticsActionDefinition Source = MakeTestAction(SourceActionID);
		FArcTacticsNodeDefinition Consideration;
		Consideration.ID = ConsiderationID;
		Consideration.Node.InitializeAs<FArcTacticsTestConsideration>();
		Consideration.InstanceData.InitializeAs<FArcTacticsTestConsiderationInstanceData>();
		Source.Considerations.Add(Consideration);
		Config->Actions.Add(Source);
		Config->Actions.Add(MakeTestAction(DestinationActionID));
		Config->EnsureValidNodeIDs();
		const FGuid SourceBagID = Config->Actions[0].ParameterBagID;
		const FGuid DestinationBagID = Config->Actions[1].ParameterBagID;
		Config->EditorBindings.AddBinding(FPropertyBindingPath(Config->ContextID, TEXT("Self")), FPropertyBindingPath(SourceActionID, TEXT("Entity")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(Config->ContextID, TEXT("Self")), FPropertyBindingPath(DestinationActionID, TEXT("Entity")));
		BindFunctionWithID(*Config, FArcTQSFunction_VectorDistance(), FunctionID, ConsiderationID, TEXT("BoundScore"));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(SourceBagID, TEXT("Where")), FPropertyBindingPath(FunctionID, TEXT("A")));
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Compile()));

		const FArcTacticsEditorSelection Moving = {EArcTacticsEditorItemKind::Consideration, ConsiderationID};
		const FArcTacticsEditorSelection Destination = {EArcTacticsEditorItemKind::Action, DestinationActionID};
		ASSERT_THAT(IsTrue(ViewModel->CanMove(Moving, Destination, EArcTacticsEditorInsertLocation::Into)));
		ASSERT_THAT(IsTrue(ViewModel->Move(Moving, Destination, EArcTacticsEditorInsertLocation::Into)));
		ASSERT_THAT(AreEqual(0, Config->Actions[0].Considerations.Num()));
		ASSERT_THAT(AreEqual(1, Config->Actions[1].Considerations.Num()));
		const FArcTacticsBinding* Moved = FindBindingTo(*Config, ConsiderationID, TEXT("BoundScore"));
		const FArcTQSPropertyFunction* MovedFunction = GetFunction(Moved);
		ASSERT_THAT(IsNotNull(MovedFunction, TEXT("The moved consideration keeps its function binding")));
		if (!MovedFunction)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Moved->GetPropertyFunctionNode().GetScriptStruct() == FArcTQSFunction_VectorDistance::StaticStruct()));
		ASSERT_THAT(IsFalse(MovedFunction->ID == FunctionID, TEXT("The moved function gets a new ID")));
		ASSERT_THAT(IsTrue(Moved->GetSourcePath().GetStructID() == MovedFunction->ID));
		const FArcTacticsBinding* MovedInput = FindBindingTo(*Config, MovedFunction->ID, TEXT("A"));
		ASSERT_THAT(IsNotNull(MovedInput));
		if (MovedInput)
		{
			ASSERT_THAT(IsTrue(MovedInput->GetSourcePath().GetStructID() == DestinationBagID,
				TEXT("The function input is remapped to the destination action's bag")));
		}
		ASSERT_THAT(AreEqual(0, CountBindingsTargeting(*Config, FunctionID), TEXT("No binding still targets the old function")));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
	}

	TEST_METHOD(MoveStep_ReorderKeepsItsFunctionNodeIDAndInputScope)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		UArcTacticsConfig* Config = MakeConfig();
		const FGuid ActionID(93, 1, 0, 0);
		const FGuid FirstStepID(93, 2, 0, 0);
		const FGuid SecondStepID(93, 3, 0, 0);
		const FGuid FunctionID(93, 4, 0, 0);
		FArcTacticsActionDefinition Action;
		Action.Action.ID = ActionID;
		Action.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
		Action.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		Action.ParameterBag.AddProperty(TEXT("Where"), EPropertyBagPropertyType::Struct, TBaseStructure<FVector>::Get());
		Action.ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		for (const FGuid& StepID : {FirstStepID, SecondStepID})
		{
			FArcTacticsNodeDefinition Step;
			Step.ID = StepID;
			Step.Node.InitializeAs<FArcTacticsTestStep>();
			Step.InstanceData.InitializeAs<FArcTacticsTestStepInstanceData>();
			Action.Steps.Add(Step);
		}
		Config->Actions.Add(Action);
		Config->EnsureValidNodeIDs();
		const FGuid BagID = Config->Actions[0].ParameterBagID;
		Config->EditorBindings.AddBinding(FPropertyBindingPath(BagID, TEXT("Reach")), FPropertyBindingPath(FirstStepID, TEXT("Required")));
		BindFunctionWithID(*Config, FArcTQSFunction_VectorDistance(), FunctionID, SecondStepID, TEXT("Required"));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(BagID, TEXT("Where")), FPropertyBindingPath(FunctionID, TEXT("A")));
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Compile()));

		ASSERT_THAT(IsTrue(ViewModel->Move(
			{EArcTacticsEditorItemKind::Step, SecondStepID},
			{EArcTacticsEditorItemKind::Step, FirstStepID},
			EArcTacticsEditorInsertLocation::Before)));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[0].ID == SecondStepID));
		const FArcTacticsBinding* Moved = FindBindingTo(*Config, SecondStepID, TEXT("Required"));
		const FArcTQSPropertyFunction* MovedFunction = GetFunction(Moved);
		ASSERT_THAT(IsNotNull(MovedFunction, TEXT("The moved step keeps its function binding")));
		if (!MovedFunction)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MovedFunction->ID == FunctionID && Moved->GetSourcePath().GetStructID() == FunctionID,
			TEXT("A reorder inside the action is not a copy: the function node and its ID stay")));
		const FArcTacticsBinding* MovedInput = FindBindingTo(*Config, FunctionID, TEXT("A"));
		ASSERT_THAT(IsTrue(MovedInput && MovedInput->GetSourcePath().GetStructID() == BagID));
		ASSERT_THAT(AreEqual(1, CountBindingsTargeting(*Config, FunctionID)));
		ASSERT_THAT(AreEqual(1, CountBindingsTargeting(*Config, SecondStepID)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
	}
};

TEST_CLASS(ArcTacticsFunctionJsonTests, "ArcTactics.Functions.Json")
{
	TEST_METHOD(FunctionBindings_SurviveApplyExportApplyExportWithDataAndNoGuids)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		UArcTacticsConfig* First = MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> FirstResult = ArcTacticsConfigJson::ApplyToConfig(*First, Parse(FunctionJson, bParsed));
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsTrue(ResultFlag(FirstResult, TEXT("success")), *JoinErrors(FirstResult)));
		ASSERT_THAT(IsTrue(ResultFlag(FirstResult, TEXT("compiled"))));
		ASSERT_THAT(AreEqual(7, First->EditorBindings.GetNumBindings(), TEXT("1 plain binding, 3 function bindings, 3 function inputs")));
		ASSERT_THAT(AreEqual(3, First->CompiledData.Functions.Num()));

		const TSharedRef<FJsonObject> FirstExport = ArcTacticsConfigJson::ExportConfig(*First);
		const FString FirstText = Serialize(FirstExport);
		ASSERT_THAT(IsFalse(FirstExport->HasField(TEXT("warnings")), *FirstText));
		for (const FArcTacticsBinding& Binding : First->EditorBindings.GetBindings())
		{
			if (const FArcTQSPropertyFunction* Function = Binding.GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>())
			{
				ASSERT_THAT(IsFalse(FirstText.Contains(Function->ID.ToString()), TEXT("The export names functions by type, never by GUID")));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
		ASSERT_THAT(IsTrue(FirstExport->TryGetArrayField(TEXT("bindings"), Bindings)));
		if (!Bindings)
		{
			return;
		}
		const TSharedPtr<FJsonObject> Outer = FindFunctionBindingObject(*Bindings, TEXT("Idle.Location"));
		ASSERT_THAT(IsTrue(Outer.IsValid(), *FirstText));
		if (!Outer.IsValid())
		{
			return;
		}
		const TSharedPtr<FJsonObject> OuterFunction = Outer->GetObjectField(TEXT("function"));
		ASSERT_THAT(AreEqual(FString(TEXT("ArcTQSFunction_VectorAdd")), OuterFunction->GetStringField(TEXT("type"))));
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		ASSERT_THAT(IsTrue(OuterFunction->TryGetArrayField(TEXT("bindings"), Inputs)));
		if (!Inputs)
		{
			return;
		}
		const TSharedPtr<FJsonObject> Inner = FindFunctionBindingObject(*Inputs, TEXT("A"));
		ASSERT_THAT(IsTrue(Inner.IsValid(), *FirstText));
		if (Inner.IsValid())
		{
			const TSharedPtr<FJsonObject> InnerFunction = Inner->GetObjectField(TEXT("function"));
			ASSERT_THAT(AreEqual(FString(TEXT("ArcTQSFunction_VectorScale")), InnerFunction->GetStringField(TEXT("type"))));
			ASSERT_THAT(IsTrue(Serialize(InnerFunction.ToSharedRef()).Contains(TEXT("\"Vector <- Perceive.Record.Location\"")), *FirstText));
		}

		UArcTacticsConfig* Second = MakeConfig();
		const TSharedRef<FJsonObject> SecondResult = ArcTacticsConfigJson::ApplyToConfig(*Second, FirstExport);
		ASSERT_THAT(IsTrue(ResultFlag(SecondResult, TEXT("success")), *JoinErrors(SecondResult)));
		ASSERT_THAT(IsTrue(ResultFlag(SecondResult, TEXT("compiled"))));
		ASSERT_THAT(AreEqual(7, Second->EditorBindings.GetNumBindings()));
		const FArcTacticsBinding* ScaleBinding = Second->EditorBindings.GetBindings().FindByPredicate([](const FArcTacticsBinding& Binding)
		{
			return Binding.GetPropertyFunctionNode().GetScriptStruct() == FArcTQSFunction_VectorScale::StaticStruct();
		});
		ASSERT_THAT(IsNotNull(ScaleBinding));
		if (ScaleBinding)
		{
			const FArcTQSFunction_VectorScale* Scale = ScaleBinding->GetPropertyFunctionNode().GetPtr<const FArcTQSFunction_VectorScale>();
			ASSERT_THAT(IsTrue(Scale && FMath::IsNearlyEqual(3.0f, Scale->Scale), TEXT("Function data survives the round trip")));
		}
		const FString SecondText = Serialize(ArcTacticsConfigJson::ExportConfig(*Second));
		ASSERT_THAT(IsTrue(FirstText == SecondText, TEXT("Export -> Apply -> Export is identical")));
	}

	TEST_METHOD(Lint_OutputsAndParametersReadOnlyByFunctionInputsAreNotReportedUnused)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		UArcTacticsConfig* Config = MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> Result = ArcTacticsConfigJson::ApplyToConfig(*Config, Parse(FunctionJson, bParsed));
		ASSERT_THAT(IsTrue(ResultFlag(Result, TEXT("success")), *JoinErrors(Result)));
		const FString Dump = Serialize(Result);
		ASSERT_THAT(IsTrue(HasLint(Result, TEXT("UnusedOutput"), TEXT("'Scalar'")), *FString::Printf(TEXT("Control: an unread task output is reported. %s"), *Dump)));
		ASSERT_THAT(IsFalse(HasLint(Result, TEXT("UnusedOutput"), TEXT("'Record'")),
			*FString::Printf(TEXT("Record is read only by the nested function input. %s"), *Dump)));
		ASSERT_THAT(IsTrue(HasLint(Result, TEXT("UnreadParameter"), TEXT("'Unread'")), *Dump));
		ASSERT_THAT(IsFalse(HasLint(Result, TEXT("UnreadParameter"), TEXT("'OffsetY'")),
			*FString::Printf(TEXT("OffsetY is read only by the nested function input. %s"), *Dump)));
	}

	TEST_METHOD(Apply_RejectsABindingIntoAFunctionOrStepMemoryKey)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		bool bParsed = false;
		UArcTacticsConfig* FunctionControl = MakeConfig();
		const TSharedRef<FJsonObject> FunctionControlResult = ArcTacticsConfigJson::ApplyToConfig(
			*FunctionControl, Parse(WithKeyBinding(LiteralOnlyFunctionJson, TEXT("")), bParsed));
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsTrue(ResultFlag(FunctionControlResult, TEXT("success")), *JoinErrors(FunctionControlResult)));
		ASSERT_THAT(IsTrue(ResultFlag(FunctionControlResult, TEXT("compiled")),
			*FString::Printf(TEXT("Control: a literal function key compiles. %s"), *Serialize(FunctionControlResult))));
		const FArcTacticsBinding* ControlFunctionBinding = FunctionControl->EditorBindings.GetBindings().FindByPredicate([](const FArcTacticsBinding& Binding)
		{
			return Binding.GetPropertyFunctionNode().GetScriptStruct() == FArcTQSFunction_MemoryFloat::StaticStruct();
		});
		const FArcTQSFunction_MemoryFloat* ControlFunction = ControlFunctionBinding
			? ControlFunctionBinding->GetPropertyFunctionNode().GetPtr<const FArcTQSFunction_MemoryFloat>()
			: nullptr;
		ASSERT_THAT(IsTrue(ControlFunction && ControlFunction->Key == TAG_ArcTacticsFunctionAuthoringTest_Key.GetTag(),
			TEXT("The control's literal function key is applied from data")));

		UArcTacticsConfig* FunctionTarget = MakeConfig();
		const TSharedRef<FJsonObject> FunctionResult = ArcTacticsConfigJson::ApplyToConfig(
			*FunctionTarget, Parse(WithKeyBinding(LiteralOnlyFunctionJson, TEXT("\"Key <- GlobalParameters.KeyTag\"")), bParsed));
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsFalse(ResultFlag(FunctionResult, TEXT("success"))));
		ASSERT_THAT(IsTrue(JoinErrors(FunctionResult).Contains(TEXT("LiteralOnlyTarget")), *JoinErrors(FunctionResult)));
		ASSERT_THAT(AreEqual(0, FunctionTarget->EditorBindings.GetNumBindings(), TEXT("A rejected Apply leaves the target untouched")));

		UArcTacticsConfig* StepControl = MakeConfig();
		const TSharedRef<FJsonObject> StepControlResult = ArcTacticsConfigJson::ApplyToConfig(
			*StepControl, Parse(WithKeyBinding(LiteralOnlyStepJson, TEXT("")), bParsed));
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsTrue(ResultFlag(StepControlResult, TEXT("success")), *JoinErrors(StepControlResult)));
		ASSERT_THAT(IsTrue(ResultFlag(StepControlResult, TEXT("compiled")),
			*FString::Printf(TEXT("Control: a literal step key compiles. %s"), *Serialize(StepControlResult))));
		const FArcTacticsWriteMemoryFloatStepInstanceData* ControlStep =
			StepControl->Actions.Num() == 1 && StepControl->Actions[0].Steps.Num() == 1
				? StepControl->Actions[0].Steps[0].InstanceData.GetPtr<const FArcTacticsWriteMemoryFloatStepInstanceData>()
				: nullptr;
		ASSERT_THAT(IsTrue(ControlStep && ControlStep->Key == TAG_ArcTacticsFunctionAuthoringTest_Key.GetTag(),
			TEXT("The control's literal step key is applied from data")));

		UArcTacticsConfig* StepTarget = MakeConfig();
		const TSharedRef<FJsonObject> StepResult = ArcTacticsConfigJson::ApplyToConfig(
			*StepTarget, Parse(WithKeyBinding(LiteralOnlyStepJson, TEXT("\"Write.Key <- GlobalParameters.KeyTag\"")), bParsed));
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsFalse(ResultFlag(StepResult, TEXT("success"))));
		ASSERT_THAT(IsTrue(JoinErrors(StepResult).Contains(TEXT("LiteralOnlyTarget")), *JoinErrors(StepResult)));
	}

	TEST_METHOD(ListNodeTypes_FunctionRoleListsVisibleFunctionsWithOutputAndMemoryMetadata)
	{
		using namespace ArcTacticsFunctionAuthoringTestsPrivate;
		const TSharedRef<FJsonObject> Functions = ArcTacticsConfigJson::ListNodeTypes(TEXT("Function"), FString());
		ASSERT_THAT(IsTrue(ResultFlag(Functions, TEXT("success"))));
		const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
		ASSERT_THAT(IsTrue(Functions->TryGetArrayField(TEXT("types"), Types)));
		if (!Types)
		{
			return;
		}
		bool bOnlyFunctions = Types->Num() > 0;
		for (const TSharedPtr<FJsonValue>& Type : *Types)
		{
			bOnlyFunctions &= Type->AsObject()->GetStringField(TEXT("role")) == TEXT("Function");
		}
		ASSERT_THAT(IsTrue(bOnlyFunctions));
		ASSERT_THAT(IsTrue(FindType(Functions, TEXT("ArcTQSFunction_VectorAdd")).IsValid()));
		ASSERT_THAT(IsFalse(FindType(Functions, TEXT("ArcTQSPropertyFunction")).IsValid(), TEXT("The base struct is not listed")));
		ASSERT_THAT(IsFalse(FindType(Functions, TEXT("ArcTacticsTestAccumulateFunction")).IsValid(), TEXT("Hidden functions are not listed")));
		ASSERT_THAT(IsFalse(FindType(Functions, TEXT("ArcTacticsSequenceAction")).IsValid()));

		const TSharedPtr<FJsonObject> MemoryFloat = FindType(Functions, TEXT("ArcTQSFunction_MemoryFloat"));
		ASSERT_THAT(IsTrue(MemoryFloat.IsValid()));
		if (!MemoryFloat.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Result")), MemoryFloat->GetStringField(TEXT("output"))));
		const TSharedPtr<FJsonObject> Key = FindField(MemoryFloat, TEXT("Key"));
		const TSharedPtr<FJsonObject> Fallback = FindField(MemoryFloat, TEXT("Fallback"));
		const TSharedPtr<FJsonObject> Result = FindField(MemoryFloat, TEXT("Result"));
		ASSERT_THAT(IsTrue(Key.IsValid() && Fallback.IsValid() && Result.IsValid()));
		if (!Key.IsValid() || !Fallback.IsValid() || !Result.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Read")), Key->GetStringField(TEXT("memoryAccess"))));
		ASSERT_THAT(AreEqual(FString(TEXT("/Script/ArcAI.ArcMemoryFloat")), Key->GetStringField(TEXT("memoryType"))));
		ASSERT_THAT(IsTrue(Result->GetBoolField(TEXT("output"))));
		ASSERT_THAT(AreEqual(FString(TEXT("Output")), Result->GetStringField(TEXT("role"))));
		ASSERT_THAT(IsFalse(Fallback->GetBoolField(TEXT("output"))));
		ASSERT_THAT(AreEqual(FString(TEXT("Input")), Fallback->GetStringField(TEXT("role"))));

		const TSharedRef<FJsonObject> All = ArcTacticsConfigJson::ListNodeTypes(FString(), FString());
		ASSERT_THAT(IsTrue(FindType(All, TEXT("ArcTQSFunction_MemoryFloat")).IsValid(), TEXT("An empty role lists functions too")));
		const TSharedRef<FJsonObject> Steps = ArcTacticsConfigJson::ListNodeTypes(TEXT("Step"), FString());
		ASSERT_THAT(IsFalse(FindType(Steps, TEXT("ArcTQSFunction_MemoryFloat")).IsValid()));
		const TSharedPtr<FJsonObject> WriteStepKey = FindField(FindType(Steps, TEXT("ArcTacticsWriteMemoryFloatStep")), TEXT("Key"));
		ASSERT_THAT(IsTrue(WriteStepKey.IsValid()));
		if (WriteStepKey.IsValid())
		{
			ASSERT_THAT(AreEqual(FString(TEXT("Write")), WriteStepKey->GetStringField(TEXT("memoryAccess"))));
		}
	}
};

#endif
