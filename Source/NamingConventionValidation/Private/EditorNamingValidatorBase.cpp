#include "EditorNamingValidatorBase.h"

UEditorNamingValidatorBase::UEditorNamingValidatorBase()
{
    bIsEnabled = true;
}

bool UEditorNamingValidatorBase::CanValidateAssetNaming_Implementation(const UClass* /*AssetClass*/, const FAssetData& /*AssetData*/) const
{
    return false;
}

ENamingConventionValidationResult UEditorNamingValidatorBase::ValidateAssetNaming_Implementation(FText& /*ErrorMessage*/, const UClass* /*AssetClass*/, const FAssetData& /*AssetData*/)
{
    return ENamingConventionValidationResult::Unknown;
}

bool UEditorNamingValidatorBase::IsEnabled() const
{
    return bIsEnabled;
}
