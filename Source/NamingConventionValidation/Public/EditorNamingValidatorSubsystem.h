#pragma once

#include "NamingConventionValidationTypes.h"

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "EditorNamingValidatorSubsystem.generated.h"

class UEditorNamingValidatorBase;
struct FAssetData;

UCLASS( Config = Editor )
class NAMINGCONVENTIONVALIDATION_API UEditorNamingValidatorSubsystem final : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    UEditorNamingValidatorSubsystem();

    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;

    int32 ValidateAssets(const TArray<FAssetData>& AssetDataList, bool bSkipExcludedDirectories = true, bool bShowIfNoFailures = true) const;
    void ValidateSavedPackage(FName PackageName);
    void AddValidator(UEditorNamingValidatorBase* Validator);
    ENamingConventionValidationResult IsAssetNamedCorrectly(FText& ErrorMessage, const FAssetData& AssetData, bool bCanUseEditorValidators = true) const;

private:
    void RegisterBlueprintValidators();
    void CleanupValidators();
    void ValidateAllSavedPackages();
    void ValidateOnSave(const TArray<FAssetData>& AssetDataList) const;
    ENamingConventionValidationResult DoesAssetMatchNameConvention(FText& ErrorMessage, const FAssetData& AssetData, FName AssetClass, bool bCanUseEditorValidators = true) const;
    bool IsClassExcluded(FText& ErrorMessage, const UClass* AssetClass) const;
    ENamingConventionValidationResult DoesAssetMatchesClassDescriptions(FText& ErrorMessage, const UClass* AssetClass, const FString& AssetName) const;
    ENamingConventionValidationResult DoesAssetMatchesValidators(FText& ErrorMessage, const UClass* AssetClass, const FAssetData& AssetData) const;

    UPROPERTY(Config)
    uint8 bAllowBlueprintValidators : 1;

    UPROPERTY(Transient)
    TMap<UClass*, UEditorNamingValidatorBase*> Validators;

    TArray< FName > SavedPackagesToValidate;
};
