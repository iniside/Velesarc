#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#if WITH_EDITOR

#include "ArcTQSBindings.h"
#include "ArcTQSCompiler.h"
#include "ArcTQSDefinitionJson.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryExecution.h"
#include "ArcTQSQueryReference.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Generators/ArcTQSGenerator_Circle.h"
#include "Mass/EntityBuilder.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "Memory/ArcMassMemory.h"
#include "NativeGameplayTags.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TargetQuery/Functions/ArcTQSFunctions_Gameplay.h"
#include "UObject/UnrealType.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsTQSTest_Center, "Test.ArcTactics.TQS.Center");

namespace ArcTacticsTQSTestsPrivate
{
	const FVector RememberedCenter(500.0, 0.0, 33.0);
	const FVector FallbackCenter(0.0, 0.0, 77.0);

	FGuid GetGeneratorID(const UArcTQSQueryDefinition& Definition)
	{
		const FArcTQSGenerator* Generator = Definition.Generator.GetPtr<FArcTQSGenerator>();
		return Generator ? Generator->ID : FGuid();
	}

	UArcTQSQueryDefinition* MakeMemoryCenterQuery(const FGameplayTag Key, FGuid& OutFunctionID)
	{
		UArcTQSQueryDefinition* Definition = NewObject<UArcTQSQueryDefinition>(GetTransientPackage());
		FArcTQSGenerator_Circle Generator;
		Generator.NumRings = 1;
		Generator.DegreesPerPoint = 360.0f;
		Generator.DistanceBetweenRings = 10.0f;
		Definition->Generator.InitializeAs<FArcTQSGenerator_Circle>(Generator);
		Definition->Parameters.AddProperty(TEXT("KeyTag"), EPropertyBagPropertyType::Struct, FGameplayTag::StaticStruct());
		Definition->RepairNodeIDs();

		FInstancedStruct Function;
		FArcTQSFunction_MemoryLocation Read;
		Read.Key = Key;
		Read.Fallback = FallbackCenter;
		Function.InitializeAs<FArcTQSFunction_MemoryLocation>(Read);
		OutFunctionID = FGuid::NewGuid();
		Function.GetMutable<FArcTQSPropertyFunction>().ID = OutFunctionID;
		Definition->EditorBindings.AddCompiledBinding(FArcTQSBinding(
			FConstStructView(Function),
			FPropertyBindingPath(OutFunctionID, GET_MEMBER_NAME_CHECKED(FArcTQSFunction_MemoryLocation, Result)),
			FPropertyBindingPath(GetGeneratorID(*Definition), GET_MEMBER_NAME_CHECKED(FArcTQSGenerator_Circle, Center))));
		return Definition;
	}

	bool CompileQuery(UArcTQSQueryDefinition& Definition, TArray<FArcTQSCompilerDiagnostic>& Diagnostics)
	{
		Diagnostics.Reset();
		const bool bCompiled = Definition.CompileEditorData(Diagnostics);
		for (const FArcTQSCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			UE_LOGF(LogTemp, Display, "ArcTacticsTQSTests compile diagnostic: %ls", *Diagnostic.Message.ToString());
		}
		return bCompiled && Definition.IsExecutable();
	}

	bool HasErrorOnNode(const TArray<FArcTQSCompilerDiagnostic>& Diagnostics, const FGuid NodeID, const TCHAR* Message)
	{
		return Diagnostics.ContainsByPredicate([NodeID, Message](const FArcTQSCompilerDiagnostic& Diagnostic)
		{
			return Diagnostic.bIsError && Diagnostic.NodeID == NodeID && Diagnostic.Message.ToString() == Message;
		});
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

	bool HasDiagnosticContaining(const TArray<FArcTQSCompilerDiagnostic>& Diagnostics, const TCHAR* Needle)
	{
		return Diagnostics.ContainsByPredicate([Needle](const FArcTQSCompilerDiagnostic& Diagnostic)
		{
			return Diagnostic.bIsError && Diagnostic.Message.ToString().Contains(Needle, ESearchCase::IgnoreCase);
		});
	}

	bool RunQuery(UArcTQSQueryDefinition& Definition, const FArcTQSQueryContext& Context, FVector& OutLocation)
	{
		FArcTQSQueryReference Reference;
		Reference.SetDefinition(&Definition);
		FArcTQSQueryResult Result;
		if (ArcTQS::RunQuerySync(Reference, Context, FArcTQSRunOptions(), Result) != EArcTQSCreateFailure::None
			|| Result.Items.Num() != 1)
		{
			return false;
		}
		OutLocation = Result.Items[0].Location;
		return true;
	}

	const FProperty* FindProperty(const UStruct* Struct, const TCHAR* Name)
	{
		return Struct ? Struct->FindPropertyByName(FName(Name)) : nullptr;
	}

	const TCHAR* const ProbeAssetPath = TEXT("/Game/ArcTacticsTests/TQS_MemoryKeyProbe");

	FString MakeDryRunDocument(const TCHAR* KeyFunctionBinding)
	{
		FString Text = TEXT(R"JSON(
{
	"version": 1,
	"options": { "dryRun": true },
	"parameters": [ { "name": "KeyTag", "type": "Struct", "valueType": "GameplayTag" } ],
	"generator": { "type": "ArcTQSGenerator_Circle", "data": { "numRings": 1 } },
	"bindings": [
		{ "to": "Generator.Center", "function": { "type": "ArcTQSFunction_MemoryLocation", "data": { "key": "Test.ArcTactics.TQS.Center" }, "bindings": [ %BINDING% ] } }
	]
}
)JSON");
		Text.ReplaceInline(TEXT("%BINDING%"), KeyFunctionBinding);
		return Text;
	}

	TSharedRef<FJsonObject> ApplyDryRun(const FString& Text, bool& bOutParsed)
	{
		TSharedPtr<FJsonObject> Object;
		bOutParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid();
		return ArcTQSDefinitionJson::ApplyToAsset(ProbeAssetPath, bOutParsed ? Object.ToSharedRef() : MakeShared<FJsonObject>());
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
}

TEST_CLASS(ArcTacticsTQSFunctionTests, "ArcTactics.TQS.Functions")
{
	FActorTestSpawner Spawner;

	TEST_METHOD(MemoryLocation_ReadsTheQuerierAsSelfAndReturnsFallbackWithoutOne)
	{
		using namespace ArcTacticsTQSTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UE::Mass::FEntityBuilder RememberingBuilder = EntityManager.MakeEntityBuilder();
		RememberingBuilder.Add<FTransformFragment>();
		const FMassEntityHandle Remembering = RememberingBuilder.Commit();
		UE::Mass::FEntityBuilder ForgetfulBuilder = EntityManager.MakeEntityBuilder();
		ForgetfulBuilder.Add<FTransformFragment>();
		const FMassEntityHandle Forgetful = ForgetfulBuilder.Commit();
		FArcMassMemoryFragment* Memory = EntityManager.AddSparseElementToEntity(Remembering, FArcMassMemoryFragment::StaticStruct())
			.GetPtr<FArcMassMemoryFragment>();
		ASSERT_THAT(IsNotNull(Memory));
		ArcMassMemory::Write(*Memory, TAG_ArcTacticsTQSTest_Center, RememberedCenter);

		FGuid FunctionID;
		UArcTQSQueryDefinition* Definition = MakeMemoryCenterQuery(TAG_ArcTacticsTQSTest_Center, FunctionID);
		TArray<FArcTQSCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileQuery(*Definition, Diagnostics)));

		FArcTQSQueryContext Context;
		Context.World = &World;
		Context.EntityManager = &EntityManager;
		Context.QuerierEntity = Remembering;
		FVector Location = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(RunQuery(*Definition, Context, Location)));
		ASSERT_THAT(IsNear(RememberedCenter.Z, Location.Z, 0.001,
			*FString::Printf(TEXT("The generator is centred on the querier's remembered location; got %s"), *Location.ToString())));
		ASSERT_THAT(IsNear(RememberedCenter.X - 10.0, Location.X, 0.01));

		Context.QuerierEntity = Forgetful;
		ASSERT_THAT(IsTrue(RunQuery(*Definition, Context, Location)));
		ASSERT_THAT(IsNear(FallbackCenter.Z, Location.Z, 0.001, TEXT("A querier without memory gets Fallback")));

		Context.QuerierEntity = FMassEntityHandle();
		ASSERT_THAT(IsTrue(RunQuery(*Definition, Context, Location)));
		ASSERT_THAT(IsNear(FallbackCenter.Z, Location.Z, 0.001, TEXT("A querier-less query gets Fallback")));
	}

	TEST_METHOD(MemoryKey_IsLiteralOnlyForTheBindingValidator)
	{
		using namespace ArcTacticsTQSTestsPrivate;
		const UScriptStruct* MemoryFunctions[] = {
			FArcTQSFunction_MemoryLocation::StaticStruct(),
			FArcTQSFunction_MemoryEntity::StaticStruct(),
			FArcTQSFunction_MemoryFloat::StaticStruct(),
			FArcTQSFunction_MemoryBool::StaticStruct(),
			FArcTQSFunction_MemoryTag::StaticStruct()
		};
		for (const UScriptStruct* Struct : MemoryFunctions)
		{
			const FProperty* Key = FindProperty(Struct, TEXT("Key"));
			ASSERT_THAT(IsNotNull(Key));
			if (Key)
			{
				ASSERT_THAT(IsTrue(ArcTQS::IsLiteralOnlyProperty(*Key)));
				ASSERT_THAT(IsFalse(ArcTQS::IsBindableProperty(*Key),
					*FString::Printf(TEXT("%s.Key must not be bindable"), *Struct->GetName())));
			}
		}
		const FProperty* Fallback = FindProperty(FArcTQSFunction_MemoryLocation::StaticStruct(), TEXT("Fallback"));
		ASSERT_THAT(IsTrue(Fallback && ArcTQS::IsBindableProperty(*Fallback), TEXT("Control: Fallback stays bindable")));
	}

	TEST_METHOD(Compiler_RejectsABindingIntoAMemoryKey)
	{
		using namespace ArcTacticsTQSTestsPrivate;
		FGuid FunctionID;
		UArcTQSQueryDefinition* Definition = MakeMemoryCenterQuery(TAG_ArcTacticsTQSTest_Center, FunctionID);
		Definition->EditorBindings.AddBinding(
			FPropertyBindingPath(Definition->QuerierID, TEXT("Location")),
			FPropertyBindingPath(FunctionID, TEXT("Fallback")));
		TArray<FArcTQSCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileQuery(*Definition, Diagnostics), TEXT("Control: a bound Fallback compiles")));

		Definition->EditorBindings.AddBinding(
			FPropertyBindingPath(Definition->ParametersID, TEXT("KeyTag")),
			FPropertyBindingPath(FunctionID, TEXT("Key")));
		ASSERT_THAT(IsFalse(CompileQuery(*Definition, Diagnostics)));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("memory key"))));
		ASSERT_THAT(IsFalse(Definition->IsExecutable()));
	}

	TEST_METHOD(Json_RejectsABindingIntoAMemoryKey)
	{
		using namespace ArcTacticsTQSTestsPrivate;
		bool bParsed = false;
		const TSharedRef<FJsonObject> Control = ApplyDryRun(MakeDryRunDocument(TEXT("\"Fallback <- Querier.Location\"")), bParsed);
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsTrue(ResultFlag(Control, TEXT("success")), *JoinErrors(Control)));
		ASSERT_THAT(IsTrue(ResultFlag(Control, TEXT("compiled")), TEXT("Control: a literal key and a bound Fallback compile")));
		ASSERT_THAT(IsTrue(ResultFlag(Control, TEXT("dryRun"))));

		const TSharedRef<FJsonObject> Rejected = ApplyDryRun(MakeDryRunDocument(TEXT("\"Key <- Parameters.KeyTag\"")), bParsed);
		ASSERT_THAT(IsTrue(bParsed));
		ASSERT_THAT(IsFalse(ResultFlag(Rejected, TEXT("success"))));
		ASSERT_THAT(IsTrue(JoinErrors(Rejected).Contains(TEXT("is a memory key")), *JoinErrors(Rejected)));
	}

	TEST_METHOD(Compiler_RejectsAMemoryFunctionWithAnEmptyKey)
	{
		using namespace ArcTacticsTQSTestsPrivate;
		FGuid SetKeyFunctionID;
		UArcTQSQueryDefinition* SetKey = MakeMemoryCenterQuery(TAG_ArcTacticsTQSTest_Center, SetKeyFunctionID);
		TArray<FArcTQSCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileQuery(*SetKey, Diagnostics), TEXT("Control: a Memory Location function with its key set compiles")));

		FGuid EmptyKeyFunctionID;
		UArcTQSQueryDefinition* EmptyKey = MakeMemoryCenterQuery(FGameplayTag(), EmptyKeyFunctionID);
		ASSERT_THAT(IsFalse(CompileQuery(*EmptyKey, Diagnostics), TEXT("A Memory Location function with an empty key must not compile")));
		ASSERT_THAT(IsFalse(EmptyKey->IsExecutable()));
		ASSERT_THAT(IsTrue(HasErrorOnNode(Diagnostics, EmptyKeyFunctionID, TEXT("Memory key 'Key' on 'Memory Location' is not set.")),
			TEXT("The error names the key field and the function node, and points at the function")));
		ASSERT_THAT(AreEqual(1, CountErrors(Diagnostics), TEXT("The empty key is the only reason the definition fails")));
	}
};

#endif
