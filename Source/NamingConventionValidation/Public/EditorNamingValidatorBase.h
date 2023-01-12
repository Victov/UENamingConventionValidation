#pragma once

#include "NamingConventionValidationTypes.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "EditorNamingValidatorBase.generated.h"

UCLASS()
class NAMINGCONVENTIONVALIDATION_API UEditorNamingValidatorBase : public UObject
{
    GENERATED_BODY()

public:
    UEditorNamingValidatorBase();

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Asset Naming Validation")
    bool CanValidateAssetNaming(const UClass* AssetClass, const FAssetData& AssetData) const;

    UFUNCTION(BlueprintNativeEvent, Category = "Asset Naming Validation")
    ENamingConventionValidationResult ValidateAssetNaming(FText& ErrorMessage, const UClass* AssetClass, const FAssetData& AssetData);

    virtual bool IsEnabled() const;

protected:
    UPROPERTY(EditAnywhere, Category = "Asset Validation", DisplayName = "IsEnabled", Meta = (BlueprintProtected = true))
    uint8 bIsEnabled : 1;
};
