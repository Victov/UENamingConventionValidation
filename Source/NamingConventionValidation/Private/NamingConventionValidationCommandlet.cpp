#include "NamingConventionValidationCommandlet.h"

#include "NamingConventionValidationLog.h"
#include "EditorNamingValidatorSubsystem.h"

#include "Editor.h"
#include "AssetRegistryHelpers.h"
#include "AssetRegistryModule.h"
#include "IAssetRegistry.h"

UNamingConventionValidationCommandlet::UNamingConventionValidationCommandlet()
{
    LogToConsole = false;
}

int32 UNamingConventionValidationCommandlet::Main(const FString& Params)
{
    UE_LOG(LogNamingConventionValidation, Log, TEXT("--------------------------------------------------------------------------------------------"));
    UE_LOG(LogNamingConventionValidation, Log, TEXT("Running NamingConventionValidation Commandlet"));
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    if (!ValidateData())
    {
        UE_LOG(LogNamingConventionValidation, Warning, TEXT("Errors occurred while validating naming convention"));
        return 2; // return something other than 1 for error since the engine will return 1 if any other system (possibly unrelated) logged errors during execution.
    }

    UE_LOG(LogNamingConventionValidation, Log, TEXT("Successfully finished running NamingConventionValidation Commandlet"));
    UE_LOG(LogNamingConventionValidation, Log, TEXT("--------------------------------------------------------------------------------------------"));
    return 0;
}

bool UNamingConventionValidationCommandlet::ValidateData()
{
    TArray<FAssetData> AssetDataList;
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add("/Game");

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

    if (GEditor)
    {
        UEditorNamingValidatorSubsystem* EditorValidatorSubsystem = GEditor->GetEditorSubsystem<UEditorNamingValidatorSubsystem>();
        check(EditorValidatorSubsystem);

        // ReSharper disable once CppExpressionWithoutSideEffects
        EditorValidatorSubsystem->ValidateAssets(AssetDataList);
    }

    return true;
}
