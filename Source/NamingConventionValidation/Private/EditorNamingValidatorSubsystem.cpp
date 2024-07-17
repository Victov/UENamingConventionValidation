#include "EditorNamingValidatorSubsystem.h"

#include "NamingConventionValidationSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorNamingValidatorBase.h"
#include "EditorUtilityBlueprint.h"
#include "Logging/MessageLog.h"
#include "MessageLogInitializationOptions.h"
#include "MessageLogModule.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "NamingConventionValidationManager"

bool TryGetAssetDataRealClass(FName& AssetClass, const FAssetData& AssetData)
{
    static const FName NativeParentClassKey("NativeParentClass");
    static const FName NativeClassKey("NativeClass");

    if (!AssetData.GetTagValue(NativeParentClassKey, AssetClass))
    {
        if (!AssetData.GetTagValue(NativeClassKey, AssetClass))
        {
            if (const UObject* Asset = AssetData.GetAsset())
            {
                const FSoftClassPath ClassPath(Asset->GetClass());
                AssetClass = *ClassPath.ToString();
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}

UEditorNamingValidatorSubsystem::UEditorNamingValidatorSubsystem()
{
    bAllowBlueprintValidators = true;
}

void UEditorNamingValidatorSubsystem::Initialize(FSubsystemCollectionBase& /*Collection*/)
{
    const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    if (!AssetRegistryModule.Get().IsLoadingAssets())
    {
        RegisterBlueprintValidators();
    }
    else
    {
        if (!AssetRegistryModule.Get().OnFilesLoaded().IsBoundToObject(this))
        {
            AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &UEditorNamingValidatorSubsystem::RegisterBlueprintValidators);
        }
    }

    TArray<UClass*> ValidatorClasses;
    GetDerivedClasses(UEditorNamingValidatorBase::StaticClass(), ValidatorClasses);
    for (const UClass* ValidatorClass : ValidatorClasses)
    {
        if (!ValidatorClass->HasAllClassFlags(CLASS_Abstract))
        {
            if (const UPackage* ClassPackage = ValidatorClass->GetOuterUPackage())
            {
                const FName& ModuleName = FPackageName::GetShortFName(ClassPackage->GetFName());
                if (FModuleManager::Get().IsModuleLoaded(ModuleName))
                {
                    UEditorNamingValidatorBase* Validator = NewObject<UEditorNamingValidatorBase>(GetTransientPackage(), ValidatorClass);
                    AddValidator(Validator);
                }
            }
        }
    }

    FMessageLogInitializationOptions InitOptions;
    InitOptions.bShowFilters = true;

    FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked< FMessageLogModule >("MessageLog");
    MessageLogModule.RegisterLogListing("NamingConventionValidation", LOCTEXT("NamingConventionValidation", "Naming Convention Validation"), InitOptions);

    UNamingConventionValidationSettings* Settings = GetMutableDefault<UNamingConventionValidationSettings>();
    Settings->PostProcessSettings();
}

void UEditorNamingValidatorSubsystem::Deinitialize()
{
    CleanupValidators();
    Super::Deinitialize();
}

int32 UEditorNamingValidatorSubsystem::ValidateAssets(const TArray< FAssetData >& AssetDataList, bool /*bSkipIncludedDirectories*/, const bool bShowIfNoFailures) const
{
    const UNamingConventionValidationSettings* Settings = GetDefault<UNamingConventionValidationSettings>();

    FScopedSlowTask SlowTask(1.0f, LOCTEXT("NamingConventionValidatingDataTask", "Validating Naming Convention..."));
    SlowTask.Visibility = bShowIfNoFailures ? ESlowTaskVisibility::ForceVisible : ESlowTaskVisibility::Invisible;

    if (bShowIfNoFailures)
    {
        SlowTask.MakeDialogDelayed(0.1f);
    }

    FMessageLog DataValidationLog("NamingConventionValidation");

    int32 NumFilesChecked = 0;
    int32 NumValidFiles = 0;
    int32 NumInvalidFiles = 0;
    int32 NumFilesSkipped = 0;
    int32 NumFilesUnableToValidate = 0;

    const int32 NumFilesToValidate = AssetDataList.Num();

    for (const FAssetData& AssetData : AssetDataList)
    {
        SlowTask.EnterProgressFrame(1.0f / static_cast<float>(NumFilesToValidate), FText::Format(LOCTEXT("ValidatingNamingConventionFilename", "Validating Naming Convention {0}"), FText::FromString(AssetData.GetFullName())));

        FText ErrorMessage;
        const ENamingConventionValidationResult Result = IsAssetNamedCorrectly(ErrorMessage, AssetData);

        switch (Result)
        {
        case ENamingConventionValidationResult::Excluded:
        {
            DataValidationLog.Info()
                ->AddToken(FAssetNameToken::Create(AssetData.PackageName.ToString()))
                ->AddToken(FTextToken::Create(LOCTEXT("ExcludedNamingConventionResult", "has not been tested based on the configuration.")))
                ->AddToken(FTextToken::Create(ErrorMessage));

            ++NumFilesSkipped;
        }
        break;
        case ENamingConventionValidationResult::Valid:
        {
            ++NumValidFiles;
            ++NumFilesChecked;
        }
        break;
        case ENamingConventionValidationResult::Invalid:
        {
            DataValidationLog.Error()
                ->AddToken(FAssetNameToken::Create(AssetData.PackageName.ToString()))
                ->AddToken(FTextToken::Create(LOCTEXT("InvalidNamingConventionResult", "does not match naming convention.")))
                ->AddToken(FTextToken::Create(ErrorMessage));

            ++NumInvalidFiles;
            ++NumFilesChecked;
        }
        break;
        case ENamingConventionValidationResult::Unknown:
        {
            if (bShowIfNoFailures && Settings->bLogWarningWhenNoClassDescriptionForAsset)
            {
                FFormatNamedArguments Arguments;
                Arguments.Add(TEXT("ClassName"), FText::FromString(AssetData.AssetClassPath.ToString()));

                DataValidationLog.Warning()
                    ->AddToken(FAssetNameToken::Create(AssetData.PackageName.ToString()))
                    ->AddToken(FTextToken::Create(LOCTEXT("UnknownNamingConventionResult", "has no known naming convention.")))
                    ->AddToken(FTextToken::Create(FText::Format(LOCTEXT("UnknownClass", " Class = {ClassName}"), Arguments)));
            }
            ++NumFilesChecked;
            ++NumFilesUnableToValidate;
        }
        break;
        }
    }

    const bool bHasFailed = NumInvalidFiles > 0;

    if (bHasFailed || bShowIfNoFailures)
    {
        FFormatNamedArguments Arguments;
        Arguments.Add(TEXT("Result"), bHasFailed ? LOCTEXT("Failed", "FAILED") : LOCTEXT("Succeeded", "SUCCEEDED"));
        Arguments.Add(TEXT("NumChecked"), NumFilesChecked);
        Arguments.Add(TEXT("NumValid"), NumValidFiles);
        Arguments.Add(TEXT("NumInvalid"), NumInvalidFiles);
        Arguments.Add(TEXT("NumSkipped"), NumFilesSkipped);
        Arguments.Add(TEXT("NumUnableToValidate"), NumFilesUnableToValidate);

        TSharedRef<FTokenizedMessage> ValidationLog = bHasFailed ? DataValidationLog.Error() : DataValidationLog.Info();
        ValidationLog->AddToken(FTextToken::Create(FText::Format(LOCTEXT("SuccessOrFailure", "NamingConvention Validation {Result}."), Arguments)));
        ValidationLog->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ResultsSummary", "Files Checked: {NumChecked}, Passed: {NumValid}, Failed: {NumInvalid}, Skipped: {NumSkipped}, Unable to validate: {NumUnableToValidate}"), Arguments)));

        DataValidationLog.Open(EMessageSeverity::Info, true);
    }

    return NumInvalidFiles;
}

void UEditorNamingValidatorSubsystem::ValidateSavedPackage(const FName PackageName)
{
    if (ensure(GEditor))
    {
        const UNamingConventionValidationSettings* Settings = GetDefault<UNamingConventionValidationSettings>();
        if (!Settings->bDoesValidateOnSave || GEditor->IsAutosaving())
        {
            return;
        }

        SavedPackagesToValidate.AddUnique(PackageName);
        GEditor->GetTimerManager()->SetTimerForNextTick(this, &UEditorNamingValidatorSubsystem::ValidateAllSavedPackages);
    }
}

void UEditorNamingValidatorSubsystem::AddValidator(UEditorNamingValidatorBase* Validator)
{
    if (IsValid(Validator))
    {
        Validators.Add(Validator->GetClass(), Validator);
    }
}

ENamingConventionValidationResult UEditorNamingValidatorSubsystem::IsAssetNamedCorrectly(FText& ErrorMessage, const FAssetData& AssetData, bool bCanUseEditorValidators) const
{
    const UNamingConventionValidationSettings* Settings = GetDefault<UNamingConventionValidationSettings>();
    if (Settings->IsPathExcludedFromValidation(AssetData.PackageName.ToString()))
    {
        ErrorMessage = LOCTEXT("ExcludedFolder", "The asset is in an excluded directory");
        return ENamingConventionValidationResult::Excluded;
    }

    FName AssetClassName;
    if (!TryGetAssetDataRealClass(AssetClassName, AssetData))
    {
        ErrorMessage = LOCTEXT("UnknownClass", "The asset is of a class which has not been set up in the settings");
        return ENamingConventionValidationResult::Unknown;
    }

    return DoesAssetMatchNameConvention(ErrorMessage, AssetData, AssetClassName, bCanUseEditorValidators);
}

void UEditorNamingValidatorSubsystem::RegisterBlueprintValidators()
{
    if (!bAllowBlueprintValidators)
    {
        return;
    }

    // Locate all validators (include unloaded)
    const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> AllBlueprintAssetData;
    AssetRegistryModule.Get().GetAssetsByClass(UEditorUtilityBlueprint::StaticClass()->GetClassPathName(), AllBlueprintAssetData, true);

    for (FAssetData& AssetData : AllBlueprintAssetData)
    {
        FString ParentClassName;

        if (!AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassName))
        {
            AssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassName);
        }

        if (!ParentClassName.IsEmpty())
        {
            UObject* Outer = nullptr;
            ResolveName(Outer, ParentClassName, false, false);
            const UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
            if (!ParentClass || !ParentClass->IsChildOf(UEditorNamingValidatorBase::StaticClass()))
            {
                continue;
            }
        }

        // If this object isn't currently loaded, load it
        UObject* ValidatorObject = AssetData.ToSoftObjectPath().ResolveObject();
        if (ValidatorObject == nullptr)
        {
            ValidatorObject = AssetData.ToSoftObjectPath().TryLoad();
        }
        if (ValidatorObject)
        {
            const UEditorUtilityBlueprint* ValidatorBlueprint = Cast<UEditorUtilityBlueprint>(ValidatorObject);
            UEditorNamingValidatorBase* Validator = NewObject< UEditorNamingValidatorBase >(GetTransientPackage(), ValidatorBlueprint->GeneratedClass);
            AddValidator(Validator);
        }
    }
}

void UEditorNamingValidatorSubsystem::CleanupValidators()
{
    Validators.Empty();
}

void UEditorNamingValidatorSubsystem::ValidateAllSavedPackages()
{
    const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> Assets;

    for (const FName& PackageName : SavedPackagesToValidate)
    {
        // We need to query the in-memory data as the disk cache may not be accurate
        AssetRegistryModule.Get().GetAssetsByPackageName(PackageName, Assets);
    }

    ValidateOnSave(Assets);

    SavedPackagesToValidate.Empty();
}

void UEditorNamingValidatorSubsystem::ValidateOnSave(const TArray<FAssetData>& AssetDataList) const
{
    if (ensure(GEditor))
    {
        const UNamingConventionValidationSettings* Settings = GetDefault<UNamingConventionValidationSettings>();
        if (!Settings->bDoesValidateOnSave || GEditor->IsAutosaving())
        {
            return;
        }

        FMessageLog DataValidationLog("NamingConventionValidation");

        if (ValidateAssets(AssetDataList, true, false) > 0)
        {
            const auto ErrorMessageNotification = FText::Format(
                LOCTEXT("ValidationFailureNotification", "Naming Convention Validation failed when saving {0}, check Naming Convention Validation log"),
                AssetDataList.Num() == 1 ? FText::FromName(AssetDataList[0].AssetName) : LOCTEXT("MultipleErrors", "multiple assets"));
            DataValidationLog.Notify(ErrorMessageNotification, EMessageSeverity::Warning, /*bForce=*/true);
        }
    }
}

ENamingConventionValidationResult UEditorNamingValidatorSubsystem::DoesAssetMatchNameConvention(FText& ErrorMessage, const FAssetData& AssetData, const FName AssetClass, bool bCanUseEditorValidators) const
{
    static const FTopLevelAssetPath BlueprintGeneratedClassName("/Script/Engine.BlueprintGeneratedClass");
    static const FTopLevelAssetPath BlueprintClassName("/Script/Engine.Blueprint");

    const UNamingConventionValidationSettings* Settings = GetDefault< UNamingConventionValidationSettings >();
    FString AssetName = AssetData.AssetName.ToString();

    // Starting UE4.27 (?) some blueprints now have BlueprintGeneratedClass as their AssetClass, and their name ends with a _C.
    if (AssetData.AssetClassPath == BlueprintGeneratedClassName)
    {
        AssetName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
    }

    const FSoftClassPath AssetClassPath(AssetClass.ToString());

    if (const UClass* AssetRealClass = AssetClassPath.TryLoadClass<UObject>())
    {
        if (IsClassExcluded(ErrorMessage, AssetRealClass))
        {
            return ENamingConventionValidationResult::Excluded;
        }

        ENamingConventionValidationResult Result = ENamingConventionValidationResult::Unknown;

        if (bCanUseEditorValidators)
        {
            Result = DoesAssetMatchesValidators(ErrorMessage, AssetRealClass, AssetData);
            if (Result != ENamingConventionValidationResult::Unknown)
            {
                return Result;
            }
        }

        Result = DoesAssetMatchesClassDescriptions(ErrorMessage, AssetRealClass, AssetName);
        if (Result != ENamingConventionValidationResult::Unknown)
        {
            return Result;
        }
    }

    if (AssetData.AssetClassPath == BlueprintClassName || AssetData.AssetClassPath == BlueprintGeneratedClassName)
    {
        if (!AssetName.StartsWith(Settings->BlueprintsPrefix))
        {
            ErrorMessage = FText::FromString(TEXT("Generic blueprint assets must start with BP_"));
            return ENamingConventionValidationResult::Invalid;
        }

        return ENamingConventionValidationResult::Valid;
    }

    return ENamingConventionValidationResult::Unknown;
}

bool UEditorNamingValidatorSubsystem::IsClassExcluded(FText& ErrorMessage, const UClass* AssetClass) const
{
    const UNamingConventionValidationSettings* Settings = GetDefault<UNamingConventionValidationSettings>();

    for (const UClass* ExcludedClass : Settings->ExcludedClasses)
    {
        if (AssetClass->IsChildOf(ExcludedClass))
        {
            ErrorMessage = FText::Format(LOCTEXT("ExcludedClass", "Assets of class '{0}' are excluded from naming convention validation"), FText::FromString(ExcludedClass->GetDefaultObjectName().ToString()));
            return true;
        }
    }

    return false;
}

ENamingConventionValidationResult UEditorNamingValidatorSubsystem::DoesAssetMatchesClassDescriptions(FText& ErrorMessage, const UClass* AssetClass, const FString& AssetName) const
{
    const UNamingConventionValidationSettings* Settings = GetDefault<UNamingConventionValidationSettings>();
    UClass* MostPreciseClass = UObject::StaticClass();
    ENamingConventionValidationResult Result = ENamingConventionValidationResult::Unknown;

    for (const FNamingConventionValidationClassDescription& ClassDescription : Settings->ClassDescriptions)
    {
        if (!IsValid(ClassDescription.Class))
            continue;

        const bool bClassFilterMatches = AssetClass->IsChildOf(ClassDescription.Class);
        const bool bClassIsMoreOrSamePrecise = ClassDescription.Class->IsChildOf(MostPreciseClass);
        const bool bClassIsSamePrecise = bClassIsMoreOrSamePrecise && ClassDescription.Class == MostPreciseClass;
        const bool bClassIsMorePrecise = bClassIsMoreOrSamePrecise && ClassDescription.Class != MostPreciseClass;
        // had an error on this precision level before. but there could be another filter that passes
        const bool bSamePrecisionCanBeValid = bClassIsSamePrecise && Result != ENamingConventionValidationResult::Valid;

        const bool bCheckAffixes = bClassFilterMatches && (bClassIsMorePrecise || bSamePrecisionCanBeValid);
        if (bCheckAffixes)
        {
            MostPreciseClass = ClassDescription.Class;

            ErrorMessage = FText::GetEmpty();
            Result = ENamingConventionValidationResult::Valid;

            if (!ClassDescription.Prefix.IsEmpty())
            {
                if (!AssetName.StartsWith(ClassDescription.Prefix))
                {
                    ErrorMessage = FText::Format(LOCTEXT("WrongPrefix", "Assets of class '{0}' must have a name which starts with {1}"), FText::FromString(ClassDescription.ClassPath.ToString()), FText::FromString(ClassDescription.Prefix));
                    Result = ENamingConventionValidationResult::Invalid;
                }
            }

            if (!ClassDescription.Suffix.IsEmpty())
            {
                if (!AssetName.EndsWith(ClassDescription.Suffix))
                {
                    ErrorMessage = FText::Format(LOCTEXT("WrongSuffix", "Assets of class '{0}' must have a name which ends with {1}"), FText::FromString(ClassDescription.ClassPath.ToString()), FText::FromString(ClassDescription.Suffix));
                    Result = ENamingConventionValidationResult::Invalid;
                }
            }
        }
    }

    return Result;
}

ENamingConventionValidationResult UEditorNamingValidatorSubsystem::DoesAssetMatchesValidators(FText& ErrorMessage, const UClass* AssetClass, const FAssetData& AssetData) const
{
    for (const TPair<UClass*, UEditorNamingValidatorBase*>& ValidatorPair : Validators)
    {
        if (ValidatorPair.Value != nullptr && ValidatorPair.Value->IsEnabled() && ValidatorPair.Value->CanValidateAssetNaming(AssetClass, AssetData))
        {
            const ENamingConventionValidationResult Result = ValidatorPair.Value->ValidateAssetNaming(ErrorMessage, AssetClass, AssetData);

            if (Result != ENamingConventionValidationResult::Unknown)
            {
                return Result;
            }
        }
    }

    return ENamingConventionValidationResult::Unknown;
}

#undef LOCTEXT_NAMESPACE
