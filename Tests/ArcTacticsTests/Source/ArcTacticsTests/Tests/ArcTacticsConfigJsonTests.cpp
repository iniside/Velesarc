#include "CQTest.h"

#if WITH_EDITOR

#include "ArcTacticsConfigJson.h"
#include "Actions/ArcTacticsSequenceAction.h"
#include "Decision/ArcTacticsConfig.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Fixtures/ArcTacticsTestTypes.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Steps/ArcTacticsMoveToStep.h"

namespace ArcTacticsConfigJsonTestsPrivate
{
	const TCHAR* const RoundTripJson = TEXT(R"JSON(
{
	"version": 2,
	"decisionSettings": { "minScore": 0.25 },
	"globalParameters": [
		{ "name": "HasEnemy", "type": "Bool", "value": true },
		{ "name": "Scores", "type": "Float", "container": "Array", "value": [1.5, 2.5] },
		{ "name": "Published", "type": "Int32" }
	],
	"globalTasks": [
		{ "name": "Perceive", "type": "ArcTacticsTestTask", "data": { "mode": 3 } }
	],
	"actions": [
		{
			"name": "Idle",
			"type": "ArcTacticsTestAction",
			"data": { "literal": 5 },
			"parameters": { "weight": 2.0, "priority": 2, "cooldown": 1.5, "cooldownStart": "OnEnter" },
			"parameterBag": [ { "name": "Published", "type": "Int32" } ]
		}
	],
	"selectors": [
		{
			"name": "EachLocation",
			"type": "ArcTacticsLocationSelector",
			"parameters": { "priority": 3, "cooldown": 2.5, "cooldownStart": "OnEnter" },
			"parameterBag": [
				{ "name": "Where", "type": "Struct", "valueType": "Vector" },
				{ "name": "ApproachRange", "type": "Float", "value": 300 }
			],
			"action": {
				"name": "Strike",
				"type": "ArcTacticsSequenceAction",
				"steps": [
					{ "name": "PatrolMove", "type": "ArcTacticsMoveToStep", "data": { "queryApproachDistance": 50 } }
				]
			},
			"considerations": [ { "name": "SelScore", "type": "ArcTacticsTestConsideration" } ]
		}
	],
	"bindings": [
		"Idle.Entity <- Context.Self",
		"Idle.Scalar <- GlobalParameters.Published",
		"Idle.Records <- Perceive.Records",
		"Idle.ParameterBag.Published <- Idle.OutputInteger",
		"GlobalParameters.Published <- Perceive.Scalar",
		"EachLocation.Items <- Perceive.Locations",
		"EachLocation.ParameterBag.Where <- EachLocation.CurrentLocation",
		"PatrolMove.Destination <- EachLocation.CurrentLocation",
		"PatrolMove.QueryApproachDistance <- EachLocation.ParameterBag.ApproachRange",
		"SelScore.BoundScore <- EachLocation.ParameterBag.ApproachRange"
	]
}
)JSON");

	struct FNegativeDoc
	{
		FString TaskType = TEXT("ArcTacticsTestTask");
		FString IdleType = TEXT("ArcTacticsTestAction");
		FString SelectorType = TEXT("ArcTacticsLocationSelector");
		FString IdleConsiderations;
		FString ExtraAction;
		FString ExtraBindings;

		FString ToText() const
		{
			FString Text = TEXT(R"JSON(
{
	"version": 2,
	"globalParameters": [ { "name": "Published", "type": "Int32" } ],
	"globalTasks": [ { "name": "Perceive", "type": "%TASK%", "data": { "mode": 3 } } ],
	"actions": [
		{ "name": "Idle", "type": "%IDLE%", "considerations": [ %CONSIDERATIONS% ] }
		%EXTRAACTION%
	],
	"selectors": [
		{
			"name": "EachLocation",
			"type": "%SELECTOR%",
			"action": { "name": "Strike", "type": "ArcTacticsTestAction" }
		}
	],
	"bindings": [
		"Idle.Entity <- Context.Self",
		"Strike.Entity <- Context.Self",
		"EachLocation.Items <- Perceive.Locations"
		%EXTRABINDINGS%
	]
}
)JSON");
			Text.ReplaceInline(TEXT("%TASK%"), *TaskType);
			Text.ReplaceInline(TEXT("%IDLE%"), *IdleType);
			Text.ReplaceInline(TEXT("%SELECTOR%"), *SelectorType);
			Text.ReplaceInline(TEXT("%CONSIDERATIONS%"), *IdleConsiderations);
			Text.ReplaceInline(TEXT("%EXTRAACTION%"), *ExtraAction);
			Text.ReplaceInline(TEXT("%EXTRABINDINGS%"), *ExtraBindings);
			return Text;
		}
	};

	struct FRejectOutcome
	{
		bool bTargetBuilt = false;
		bool bParsed = false;
		bool bSucceeded = true;
		bool bReportedExpected = false;
		bool bExportUnchanged = false;
		bool bStillExecutable = false;
		int32 BindingsBefore = 0;
		int32 BindingsAfter = 0;
		FString Errors;
	};

	UArcTacticsConfig* MakeConfig()
	{
		return NewObject<UArcTacticsConfig>(GetTransientPackage(), NAME_None, RF_Transactional);
	}

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

	TSharedRef<FJsonObject> ApplyText(UArcTacticsConfig& Config, const FString& Text, bool& bOutParsed)
	{
		return ArcTacticsConfigJson::ApplyToConfig(Config, Parse(Text, bOutParsed));
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

	bool ExportContainsBinding(const TSharedRef<FJsonObject>& Export, const FString& Binding)
	{
		const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
		if (!Export->TryGetArrayField(TEXT("bindings"), Bindings))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Item : *Bindings)
		{
			if (Item->AsString() == Binding)
			{
				return true;
			}
		}
		return false;
	}

	FRejectOutcome ApplyRejected(const FString& NegativeText, const TCHAR* ExpectedError)
	{
		FRejectOutcome Outcome;
		UArcTacticsConfig* Target = MakeConfig();
		bool bBuiltParsed = false;
		const TSharedRef<FJsonObject> Built = ApplyText(*Target, RoundTripJson, bBuiltParsed);
		Outcome.bTargetBuilt = bBuiltParsed && ResultFlag(Built, TEXT("success")) && Target->CompiledData.IsExecutable();
		Outcome.BindingsBefore = Target->EditorBindings.GetNumBindings();
		const FString Before = Serialize(ArcTacticsConfigJson::ExportConfig(*Target));

		const TSharedRef<FJsonObject> Result = ApplyText(*Target, NegativeText, Outcome.bParsed);
		Outcome.bSucceeded = ResultFlag(Result, TEXT("success"));
		Outcome.Errors = JoinErrors(Result);
		Outcome.bReportedExpected = Outcome.Errors.Contains(ExpectedError);

		Outcome.bExportUnchanged = Before == Serialize(ArcTacticsConfigJson::ExportConfig(*Target));
		Outcome.bStillExecutable = Target->CompiledData.IsExecutable();
		Outcome.BindingsAfter = Target->EditorBindings.GetNumBindings();
		return Outcome;
	}
}

TEST_CLASS(ArcTacticsConfigJsonTests, "ArcTactics.ConfigJson")
{
	void ExpectRejectedAndUnchanged(const ArcTacticsConfigJsonTestsPrivate::FNegativeDoc& Doc, const TCHAR* ExpectedError)
	{
		const ArcTacticsConfigJsonTestsPrivate::FRejectOutcome Outcome =
			ArcTacticsConfigJsonTestsPrivate::ApplyRejected(Doc.ToText(), ExpectedError);
		ASSERT_THAT(IsTrue(Outcome.bTargetBuilt, TEXT("The target config must be built from the valid document first")));
		ASSERT_THAT(IsTrue(Outcome.bParsed, TEXT("The negative document must be valid JSON")));
		ASSERT_THAT(IsFalse(Outcome.bSucceeded, *FString::Printf(TEXT("Apply must fail. Errors: %s"), *Outcome.Errors)));
		ASSERT_THAT(IsTrue(Outcome.bReportedExpected, *FString::Printf(TEXT("Expected error '%s' not reported. Errors: %s"), ExpectedError, *Outcome.Errors)));
		ASSERT_THAT(IsTrue(Outcome.bExportUnchanged, TEXT("A rejected Apply must leave the target export unchanged")));
		ASSERT_THAT(AreEqual(Outcome.BindingsBefore, Outcome.BindingsAfter));
		ASSERT_THAT(IsTrue(Outcome.bStillExecutable, TEXT("A rejected Apply must keep the target's compiled artifact")));
	}

	TEST_METHOD(ExportApplyExport_IsIdenticalAndBothCompile)
	{
		UArcTacticsConfig* First = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> FirstResult =
			ArcTacticsConfigJsonTestsPrivate::ApplyText(*First, ArcTacticsConfigJsonTestsPrivate::RoundTripJson, bParsed);
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(FirstResult, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(FirstResult)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(FirstResult, TEXT("compiled"))));
		ASSERT_THAT(IsTrue(First->CompiledData.IsExecutable()));
		ASSERT_THAT(AreEqual(10, First->EditorBindings.GetNumBindings()));
		ASSERT_THAT(IsNear(0.25f, First->DecisionSettings.MinScore, 0.0001f));
		ASSERT_THAT(AreEqual(1, First->Actions.Num()));
		if (First->Actions.Num() == 1)
		{
			ASSERT_THAT(AreEqual(2, First->Actions[0].Parameters.Priority));
			ASSERT_THAT(IsNear(1.5f, First->Actions[0].Parameters.Cooldown, 0.0001f));
			ASSERT_THAT(IsTrue(First->Actions[0].Parameters.CooldownStart == EArcTacticsCooldownStart::OnEnter));
		}

		const TSharedRef<FJsonObject> FirstExport = ArcTacticsConfigJson::ExportConfig(*First);
		const FString ExportDump = ArcTacticsConfigJsonTestsPrivate::Serialize(FirstExport);
		const FString FirstText = ExportDump;
		ASSERT_THAT(IsTrue(FirstText.Contains(TEXT("\"container\":\"Array\""))));
		ASSERT_THAT(IsTrue(FirstText.Contains(TEXT("\"steps\":[{\"name\":\"PatrolMove\",\"type\":\"ArcTacticsMoveToStep\"")),
			*FString::Printf(TEXT("The selector action must export its step. Export: %s"), *FirstText)));

		UArcTacticsConfig* Second = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		const TSharedRef<FJsonObject> SecondResult = ArcTacticsConfigJson::ApplyToConfig(*Second, FirstExport);
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(SecondResult, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(SecondResult)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(SecondResult, TEXT("compiled"))));
		ASSERT_THAT(IsTrue(Second->CompiledData.IsExecutable()));
		ASSERT_THAT(AreEqual(First->EditorBindings.GetNumBindings(), Second->EditorBindings.GetNumBindings()));
		ASSERT_THAT(IsNear(0.25f, Second->DecisionSettings.MinScore, 0.0001f));
		ASSERT_THAT(AreEqual(1, Second->Actions.Num()));
		if (Second->Actions.Num() == 1)
		{
			ASSERT_THAT(AreEqual(2, Second->Actions[0].Parameters.Priority,
				TEXT("Export must carry the branch Priority")));
			ASSERT_THAT(IsNear(1.5f, Second->Actions[0].Parameters.Cooldown, 0.0001f));
			ASSERT_THAT(IsTrue(Second->Actions[0].Parameters.CooldownStart == EArcTacticsCooldownStart::OnEnter,
				TEXT("Export must carry the branch CooldownStart")));
		}
		ASSERT_THAT(AreEqual(1, Second->Selectors.Num()));
		if (Second->Selectors.Num() == 1)
		{
			ASSERT_THAT(AreEqual(3, Second->Selectors[0].Parameters.Priority,
				TEXT("Export must carry the selector branch Priority")));
			ASSERT_THAT(IsNear(2.5f, Second->Selectors[0].Parameters.Cooldown, 0.0001f));
			ASSERT_THAT(IsTrue(Second->Selectors[0].Parameters.CooldownStart == EArcTacticsCooldownStart::OnEnter,
				TEXT("Export must carry the selector branch CooldownStart")));
		}

		const FString SecondText = ArcTacticsConfigJsonTestsPrivate::Serialize(ArcTacticsConfigJson::ExportConfig(*Second));
		ASSERT_THAT(IsTrue(FirstText == SecondText, TEXT("Export -> Apply -> Export must be identical")));
	}

	TEST_METHOD(Export_WritesBindingsInDataFlowDirectionWithPropertyNames)
	{
		UArcTacticsConfig* Config = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> Result =
			ArcTacticsConfigJsonTestsPrivate::ApplyText(*Config, ArcTacticsConfigJsonTestsPrivate::RoundTripJson, bParsed);
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(Result, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(Result)));
		const TSharedRef<FJsonObject> Export = ArcTacticsConfigJson::ExportConfig(*Config);
		const FString ExportDump = ArcTacticsConfigJsonTestsPrivate::Serialize(Export);
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("Idle.Entity <- Context.Self"))));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("Idle.ParameterBag.Published <- Idle.OutputInteger")),
			*FString::Printf(TEXT("Node Output -> own bag must export in data-flow direction. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("GlobalParameters.Published <- Perceive.Scalar")),
			*FString::Printf(TEXT("Global task Output -> GlobalParameters must export in data-flow direction. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("EachLocation.ParameterBag.Where <- EachLocation.CurrentLocation")),
			*FString::Printf(TEXT("Selector Current* -> own ParameterBag must survive. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("PatrolMove.Destination <- EachLocation.CurrentLocation")),
			*FString::Printf(TEXT("A step input bound to the selector Current* output must survive. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("PatrolMove.QueryApproachDistance <- EachLocation.ParameterBag.ApproachRange")),
			*FString::Printf(TEXT("A step parameter bound to the branch bag must survive. Export: %s"), *ExportDump)));
	}

	TEST_METHOD(Export_WritesRenamedBagBindingsByCurrentName_AndApplyAcceptsIt)
	{
		UArcTacticsConfig* Config = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> Result =
			ArcTacticsConfigJsonTestsPrivate::ApplyText(*Config, ArcTacticsConfigJsonTestsPrivate::RoundTripJson, bParsed);
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(Result, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(Result)));
		ASSERT_THAT(AreEqual(1, Config->Selectors.Num()));
		ASSERT_THAT(IsTrue(Config->GlobalParameters.RenameProperty(TEXT("Published"), TEXT("Shared")) == EPropertyBagAlterationResult::Success));
		ASSERT_THAT(IsTrue(Config->Selectors[0].ParameterBag.RenameProperty(TEXT("ApproachRange"), TEXT("Reach")) == EPropertyBagAlterationResult::Success));

		const TSharedRef<FJsonObject> Export = ArcTacticsConfigJson::ExportConfig(*Config);
		const FString ExportDump = ArcTacticsConfigJsonTestsPrivate::Serialize(Export);
		ASSERT_THAT(IsFalse(Export->HasField(TEXT("warnings")), *FString::Printf(TEXT("No binding may be dropped. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("Idle.Scalar <- GlobalParameters.Shared")),
			*FString::Printf(TEXT("A renamed source bag property must export by its current name. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("GlobalParameters.Shared <- Perceive.Scalar")),
			*FString::Printf(TEXT("A renamed output-binding bag property must export by its current name. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("PatrolMove.QueryApproachDistance <- EachLocation.ParameterBag.Reach")),
			*FString::Printf(TEXT("A renamed branch bag property must export by its current name. Export: %s"), *ExportDump)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ExportContainsBinding(Export, TEXT("SelScore.BoundScore <- EachLocation.ParameterBag.Reach")),
			*FString::Printf(TEXT("A renamed branch bag property must export by its current name. Export: %s"), *ExportDump)));

		UArcTacticsConfig* Reapplied = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		const TSharedRef<FJsonObject> ReappliedResult = ArcTacticsConfigJson::ApplyToConfig(*Reapplied, Export);
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(ReappliedResult, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(ReappliedResult)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(ReappliedResult, TEXT("compiled"))));
		ASSERT_THAT(AreEqual(Config->EditorBindings.GetNumBindings(), Reapplied->EditorBindings.GetNumBindings()));
	}

	TEST_METHOD(ControlDocument_AppliesAndCompiles)
	{
		UArcTacticsConfig* Config = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> Result = ArcTacticsConfigJsonTestsPrivate::ApplyText(
			*Config, ArcTacticsConfigJsonTestsPrivate::FNegativeDoc().ToText(), bParsed);
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(Result, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(Result)));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(Result, TEXT("compiled"))));
	}

	TEST_METHOD(Apply_RejectsUnknownType_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.TaskType = TEXT("ArcTacticsNoSuchTaskType");
		ExpectRejectedAndUnchanged(Doc, TEXT("unknown node type"));
	}

	TEST_METHOD(Apply_RejectsRoleBaseGlobalTask_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.TaskType = TEXT("ArcTacticsGlobalTask");
		ExpectRejectedAndUnchanged(Doc, TEXT("cannot be used"));
	}

	TEST_METHOD(Apply_RejectsRoleBaseAction_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.IdleType = TEXT("ArcTacticsActionNode");
		ExpectRejectedAndUnchanged(Doc, TEXT("cannot be used"));
	}

	TEST_METHOD(Apply_RejectsRoleBaseConsideration_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.IdleConsiderations = TEXT(R"JSON({ "name": "Base", "type": "ArcTacticsConsiderationNode" })JSON");
		ExpectRejectedAndUnchanged(Doc, TEXT("cannot be used"));
	}

	TEST_METHOD(Apply_RejectsRoleBaseSelector_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.SelectorType = TEXT("ArcTacticsSelectorNode");
		ExpectRejectedAndUnchanged(Doc, TEXT("cannot be used"));
	}

	TEST_METHOD(Apply_RejectsDuplicateNodeName_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.ExtraAction = TEXT(R"JSON(, { "name": "idle", "type": "ArcTacticsTestAction" })JSON");
		ExpectRejectedAndUnchanged(Doc, TEXT("already used"));
	}

	TEST_METHOD(Apply_RejectsDuplicateBindingTarget_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.ExtraBindings = TEXT(R"JSON(, "Idle.Entity <- Context.Self")JSON");
		ExpectRejectedAndUnchanged(Doc, TEXT("same stored target"));
	}

	TEST_METHOD(Apply_RejectsInaccessibleBinding_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.ExtraBindings = TEXT(R"JSON(, "Idle.Location <- EachLocation.CurrentLocation")JSON");
		ExpectRejectedAndUnchanged(Doc, TEXT("InvalidScope"));
	}

	TEST_METHOD(Apply_RejectsIncompatibleTypes_AndLeavesTargetUnchanged)
	{
		ArcTacticsConfigJsonTestsPrivate::FNegativeDoc Doc;
		Doc.ExtraBindings = TEXT(R"JSON(, "Idle.Scalar <- Perceive.Locations")JSON");
		ExpectRejectedAndUnchanged(Doc, TEXT("IncompatibleTypes"));
	}

	TEST_METHOD(Steps_SurviveExportApplyExportWithDataAndBindings)
	{
		UArcTacticsConfig* First = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		bool bParsed = false;
		const TSharedRef<FJsonObject> FirstResult =
			ArcTacticsConfigJsonTestsPrivate::ApplyText(*First, ArcTacticsConfigJsonTestsPrivate::RoundTripJson, bParsed);
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(FirstResult, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(FirstResult)));
		ASSERT_THAT(AreEqual(1, First->Selectors.Num()));
		ASSERT_THAT(AreEqual(1, First->Selectors[0].Steps.Num()));

		UArcTacticsConfig* Second = ArcTacticsConfigJsonTestsPrivate::MakeConfig();
		const TSharedRef<FJsonObject> SecondResult =
			ArcTacticsConfigJson::ApplyToConfig(*Second, ArcTacticsConfigJson::ExportConfig(*First));
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(SecondResult, TEXT("success")),
			*ArcTacticsConfigJsonTestsPrivate::JoinErrors(SecondResult)));
		ASSERT_THAT(AreEqual(1, Second->Selectors.Num()));
		ASSERT_THAT(AreEqual(1, Second->Selectors[0].Steps.Num()));
		if (Second->Selectors.Num() < 1 || Second->Selectors[0].Steps.Num() < 1)
		{
			return;
		}
		const FArcTacticsNodeDefinition& Step = Second->Selectors[0].Steps[0];
		ASSERT_THAT(IsTrue(Step.Node.GetScriptStruct() == FArcTacticsMoveToStep::StaticStruct()));
		ASSERT_THAT(AreEqual(FName(TEXT("PatrolMove")), Step.Name));
		const FArcTacticsMoveToStepInstanceData* StepData = Step.InstanceData.GetPtr<FArcTacticsMoveToStepInstanceData>();
		ASSERT_THAT(IsNotNull(StepData));
		if (StepData)
		{
			ASSERT_THAT(IsNear(50.0f, StepData->QueryApproachDistance, 0.001f));
		}
		ASSERT_THAT(IsTrue(Second->Selectors[0].Action.Node.GetScriptStruct() == FArcTacticsSequenceAction::StaticStruct()));

		bool bDestinationBound = false;
		bool bParameterBound = false;
		for (const FArcTacticsBinding& Binding : Second->EditorBindings.GetBindings())
		{
			if (Binding.GetTargetPath().GetStructID() != Step.ID)
			{
				continue;
			}
			const FName TargetName = Binding.GetTargetPath().NumSegments() == 1
				? Binding.GetTargetPath().GetSegment(0).GetName()
				: FName();
			bDestinationBound |= TargetName == FName(TEXT("Destination"))
				&& Binding.GetSourcePath().GetStructID() == Second->Selectors[0].Selector.ID;
			bParameterBound |= TargetName == FName(TEXT("QueryApproachDistance"))
				&& Binding.GetSourcePath().GetStructID() == Second->Selectors[0].ParameterBagID;
		}
		ASSERT_THAT(IsTrue(bDestinationBound, TEXT("The step input binding must resolve to the step's ID after Apply")));
		ASSERT_THAT(IsTrue(bParameterBound, TEXT("The step parameter binding must resolve to the branch bag after Apply")));
	}

	TEST_METHOD(Apply_RejectsRoleBaseStep_AndLeavesTargetUnchanged)
	{
		FString Text = ArcTacticsConfigJsonTestsPrivate::RoundTripJson;
		ASSERT_THAT(IsTrue(Text.ReplaceInline(TEXT("\"ArcTacticsMoveToStep\""), TEXT("\"ArcTacticsStepNode\"")) == 1));
		const ArcTacticsConfigJsonTestsPrivate::FRejectOutcome Outcome =
			ArcTacticsConfigJsonTestsPrivate::ApplyRejected(Text, TEXT("cannot be used"));
		ASSERT_THAT(IsTrue(Outcome.bTargetBuilt));
		ASSERT_THAT(IsFalse(Outcome.bSucceeded, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bReportedExpected, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bExportUnchanged));
	}

	TEST_METHOD(Apply_RejectsActionTypeInStepSlot_AndLeavesTargetUnchanged)
	{
		FString Text = ArcTacticsConfigJsonTestsPrivate::RoundTripJson;
		ASSERT_THAT(IsTrue(Text.ReplaceInline(TEXT("\"ArcTacticsMoveToStep\""), TEXT("\"ArcTacticsTestAction\"")) == 1));
		const ArcTacticsConfigJsonTestsPrivate::FRejectOutcome Outcome =
			ArcTacticsConfigJsonTestsPrivate::ApplyRejected(Text, TEXT("cannot be used"));
		ASSERT_THAT(IsTrue(Outcome.bTargetBuilt));
		ASSERT_THAT(IsFalse(Outcome.bSucceeded, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bReportedExpected, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bExportUnchanged));
	}

	TEST_METHOD(Apply_RejectsStepTypeInActionSlot_AndLeavesTargetUnchanged)
	{
		FString Text = ArcTacticsConfigJsonTestsPrivate::RoundTripJson;
		ASSERT_THAT(IsTrue(Text.ReplaceInline(TEXT("\"ArcTacticsSequenceAction\""), TEXT("\"ArcTacticsMoveToStep\"")) == 1));
		const ArcTacticsConfigJsonTestsPrivate::FRejectOutcome Outcome =
			ArcTacticsConfigJsonTestsPrivate::ApplyRejected(Text, TEXT("cannot be used"));
		ASSERT_THAT(IsTrue(Outcome.bTargetBuilt));
		ASSERT_THAT(IsFalse(Outcome.bSucceeded, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bReportedExpected, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bExportUnchanged));
	}

	TEST_METHOD(Apply_RejectsOldFormatVersion_AndLeavesTargetUnchanged)
	{
		FString Text = ArcTacticsConfigJsonTestsPrivate::RoundTripJson;
		ASSERT_THAT(IsTrue(Text.ReplaceInline(TEXT("\"version\": 2"), TEXT("\"version\": 1")) == 1));
		const ArcTacticsConfigJsonTestsPrivate::FRejectOutcome Outcome =
			ArcTacticsConfigJsonTestsPrivate::ApplyRejected(Text, TEXT("version"));
		ASSERT_THAT(IsTrue(Outcome.bTargetBuilt));
		ASSERT_THAT(IsFalse(Outcome.bSucceeded, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bReportedExpected, *Outcome.Errors));
		ASSERT_THAT(IsTrue(Outcome.bExportUnchanged));
	}

	TEST_METHOD(ListNodeTypes_ListsStepTypesUnderStepRole)
	{
		const TSharedRef<FJsonObject> Steps = ArcTacticsConfigJson::ListNodeTypes(TEXT("Step"), FString());
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(Steps, TEXT("success"))));
		const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
		ASSERT_THAT(IsTrue(Steps->TryGetArrayField(TEXT("types"), Types)));
		if (!Types)
		{
			return;
		}
		TSet<FString> Names;
		for (const TSharedPtr<FJsonValue>& Type : *Types)
		{
			Names.Add(Type->AsObject()->GetStringField(TEXT("name")));
		}
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsMoveToStep"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsUseAbilityStep"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsUseAbilityFocusStep"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsStepNode"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsTestStep"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsSequenceAction"))));
	}

	TEST_METHOD(ListNodeTypes_ListsOnlyVisibleConcreteTypes)
	{
		const TSharedRef<FJsonObject> All = ArcTacticsConfigJson::ListNodeTypes(FString(), FString());
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(All, TEXT("success"))));
		TSet<FString> Names;
		const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
		ASSERT_THAT(IsTrue(All->TryGetArrayField(TEXT("types"), Types)));
		if (!Types)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Type : *Types)
		{
			Names.Add(Type->AsObject()->GetStringField(TEXT("name")));
		}
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsTestTask"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsTestAction"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsTestConsideration"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsLocationSelector"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsTargetSelector"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsSequenceAction"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsCollectTargetsTask"))));
		ASSERT_THAT(IsTrue(Names.Contains(TEXT("ArcTacticsConstantConsiderationNode"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsGlobalTask"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsActionNode"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsConsiderationNode"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsSelectorNode"))));
		ASSERT_THAT(IsFalse(Names.Contains(TEXT("ArcTacticsNode"))));

		const TSharedRef<FJsonObject> Selectors = ArcTacticsConfigJson::ListNodeTypes(TEXT("Selector"), FString());
		ASSERT_THAT(IsTrue(ArcTacticsConfigJsonTestsPrivate::ResultFlag(Selectors, TEXT("success"))));
		const TArray<TSharedPtr<FJsonValue>>* SelectorTypes = nullptr;
		ASSERT_THAT(IsTrue(Selectors->TryGetArrayField(TEXT("types"), SelectorTypes)));
		if (!SelectorTypes)
		{
			return;
		}
		bool bHasLocationSelector = false;
		bool bOnlySelectors = true;
		bool bHasBase = false;
		for (const TSharedPtr<FJsonValue>& Type : *SelectorTypes)
		{
			const TSharedPtr<FJsonObject> Object = Type->AsObject();
			bOnlySelectors &= Object->GetStringField(TEXT("role")) == TEXT("Selector");
			bHasLocationSelector |= Object->GetStringField(TEXT("name")) == TEXT("ArcTacticsLocationSelector");
			bHasBase |= Object->GetStringField(TEXT("name")) == TEXT("ArcTacticsSelectorNode");
		}
		ASSERT_THAT(IsTrue(bHasLocationSelector));
		ASSERT_THAT(IsTrue(bOnlySelectors));
		ASSERT_THAT(IsFalse(bHasBase));
	}
};

#endif
