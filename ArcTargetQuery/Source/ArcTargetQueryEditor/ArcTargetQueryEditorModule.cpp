// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetQueryEditorModule.h"

#include "ArcTQSBindings.h"
#include "ArcTQSCompiler.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryDefinitionDetails.h"
#include "ArcTQSQueryReference.h"
#include "ArcTQSQueryReferenceDetails.h"
#include "ArcTQSTestingActor.h"
#include "ArcTQSTestingActorDetails.h"
#include "ArcTQSToolset.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#include "Misc/CoreDelegates.h"
#include "Misc/UObjectToken.h"
#include "PropertyEditorModule.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "ArcTargetQueryEditor"

namespace ArcTargetQueryEditorModulePrivate
{
	const FName MessageLogName(TEXT("ArcTQS"));

	uint32 HashDiagnostics(const TArray<FArcTQSCompilerDiagnostic>& Diagnostics)
	{
		uint32 Hash = 0;
		for (const FArcTQSCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			Hash = HashCombine(Hash, GetTypeHash(Diagnostic.NodeID));
			Hash = HashCombine(Hash, GetTypeHash(Diagnostic.Message.ToString()));
			Hash = HashCombine(Hash, GetTypeHash(Diagnostic.bIsError));
		}
		return Hash;
	}
}

void FArcTargetQueryEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(
		UArcTQSQueryDefinition::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcTQSQueryDefinitionDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(
		AArcTQSTestingActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcTQSTestingActorDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FArcTQSQueryReference::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcTQSQueryReferenceDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
	FMessageLogInitializationOptions Options;
	Options.bShowPages = true;
	Options.bAllowClear = true;
	MessageLogModule.RegisterLogListing(
		ArcTargetQueryEditorModulePrivate::MessageLogName,
		LOCTEXT("MessageLogLabel", "Arc Target Query"),
		Options);

	PostCompileHandle = ArcTQS::Delegates::OnPostCompile.AddRaw(this, &FArcTargetQueryEditorModule::HandleDefinitionCompiled);

	ToolsetRegistrationHandle = FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([]()
	{
		UToolsetRegistry::RegisterToolsetClass(UArcTQSToolset::StaticClass());
	});
}

void FArcTargetQueryEditorModule::ShutdownModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.Remove(ToolsetRegistrationHandle);
	if (UObjectInitialized())
	{
		UToolsetRegistry::UnregisterToolsetClass(UArcTQSToolset::StaticClass());
	}
	ArcTQS::Delegates::OnPostCompile.Remove(PostCompileHandle);
	ReportedDiagnostics.Reset();

	if (FMessageLogModule* MessageLogModule = FModuleManager::GetModulePtr<FMessageLogModule>(TEXT("MessageLog")))
	{
		MessageLogModule->UnregisterLogListing(ArcTargetQueryEditorModulePrivate::MessageLogName);
	}

	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		PropertyModule->UnregisterCustomClassLayout(UArcTQSQueryDefinition::StaticClass()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(AArcTQSTestingActor::StaticClass()->GetFName());
		PropertyModule->UnregisterCustomPropertyTypeLayout(FArcTQSQueryReference::StaticStruct()->GetFName());
		PropertyModule->NotifyCustomizationModuleChanged();
	}
}

void FArcTargetQueryEditorModule::HandleDefinitionCompiled(const UArcTQSQueryDefinition& Definition)
{
	using namespace ArcTargetQueryEditorModulePrivate;

	FArcTQSCompiledData Scratch;
	TArray<FArcTQSCompilerDiagnostic> Diagnostics;
	FArcTQSCompiler::Compile(Definition, Definition.CompiledData.Revision, Scratch, Diagnostics);

	const uint32 Hash = HashDiagnostics(Diagnostics);
	const TObjectKey<UArcTQSQueryDefinition> Key(&Definition);
	const uint32* Previous = ReportedDiagnostics.Find(Key);
	if ((Previous ? *Previous : 0) == Hash)
	{
		return;
	}
	ReportedDiagnostics.Add(Key, Hash);

	FMessageLog Log(MessageLogName);
	Log.NewPage(FText::FromString(Definition.GetName()));
	if (Diagnostics.IsEmpty())
	{
		Log.Info()
			->AddToken(FUObjectToken::Create(&Definition))
			->AddToken(FTextToken::Create(LOCTEXT("CompiledClean", "compiled without diagnostics.")));
		return;
	}

	bool bHasErrors = false;
	for (const FArcTQSCompilerDiagnostic& Diagnostic : Diagnostics)
	{
		bHasErrors |= Diagnostic.bIsError;
		const FName NodeName = Diagnostic.NodeID.IsValid() ? Definition.GetEndpointName(Diagnostic.NodeID) : NAME_None;
		const FText Text = NodeName.IsNone()
			? Diagnostic.Message
			: FText::Format(LOCTEXT("NodeDiagnostic", "{0}: {1}"), FText::FromName(NodeName), Diagnostic.Message);
		const TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(
			Diagnostic.bIsError ? EMessageSeverity::Error : EMessageSeverity::Warning);
		Message->AddToken(FUObjectToken::Create(&Definition));
		Message->AddToken(FTextToken::Create(Text));
		Log.AddMessage(Message);
	}
	if (bHasErrors)
	{
		Log.Notify(FText::Format(LOCTEXT("CompileFailed", "{0} failed to compile."), FText::FromString(Definition.GetName())),
			EMessageSeverity::Error);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcTargetQueryEditorModule, ArcTargetQueryEditor)
