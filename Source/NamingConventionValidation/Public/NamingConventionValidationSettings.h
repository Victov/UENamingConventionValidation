#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>
#include <Engine/EngineTypes.h>

#include "NamingConventionValidationSettings.generated.h"

USTRUCT()
struct FNamingConventionValidationClassDescription
{
    GENERATED_USTRUCT_BODY()

    FNamingConventionValidationClassDescription() :
        Class( nullptr ),
        Priority( 0 )
    {}

    bool operator<(const FNamingConventionValidationClassDescription& Other) const
    {
        return Priority > Other.Priority || ((Class && Other.Class) ? (Class->GetName() < Other.Class->GetName()) : false);
    }

    UPROPERTY( Config, EditAnywhere, Meta = ( AllowAbstract = true ) )
    TSoftClassPtr<UObject> ClassPath;

    //@TODO: Replace with TSubclassOf
    UPROPERTY( Transient )
    UClass* Class;

    UPROPERTY( Config, EditAnywhere )
    FString Prefix;

    UPROPERTY( Config, EditAnywhere )
    FString Suffix;

    UPROPERTY( Config, EditAnywhere )
    int32 Priority;
};

UCLASS( Config = Editor, DefaultConfig )
class NAMINGCONVENTIONVALIDATION_API UNamingConventionValidationSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UNamingConventionValidationSettings();

    bool IsPathExcludedFromValidation( const FString & Path ) const;

    UPROPERTY( Config, EditAnywhere, Meta = ( LongPackageName, ConfigRestartRequired = true ) )
    TArray< FDirectoryPath > ExcludedDirectories;

    UPROPERTY( Config, EditAnywhere )
    uint8 bLogWarningWhenNoClassDescriptionForAsset : 1;

    UPROPERTY( Config, EditAnywhere )
    uint8 bAllowValidationInDevelopersFolder : 1;

    UPROPERTY( Config, EditAnywhere )
    uint8 bAllowValidationOnlyInGameFolder : 1;

    // Add folders located outside of /Game that you still want to process when bAllowValidationOnlyInGameFolder is checked
    UPROPERTY( Config, EditAnywhere, Meta = ( LongPackageName, ConfigRestartRequired = true, EditCondition = "bAllowValidationOnlyInGameFolder" ) )
    TArray<FDirectoryPath> NonGameFoldersDirectoriesToProcess;

    // Add folders located outside of /Game that you still want to process when bAllowValidationOnlyInGameFolder is checked, and which contain one of those tokens in their path
    UPROPERTY( Config, EditAnywhere, Meta = ( LongPackageName, ConfigRestartRequired = true, EditCondition = "bAllowValidationOnlyInGameFolder" ) )
    TArray<FString> NonGameFoldersDirectoriesToProcessContainingToken;

    UPROPERTY( Config, EditAnywhere )
    uint8 bDoesValidateOnSave : 1;

    UPROPERTY( Config, EditAnywhere, Meta = ( ConfigRestartRequired = true ) )
    TArray<FNamingConventionValidationClassDescription> ClassDescriptions;

    UPROPERTY( Config, EditAnywhere )
    TArray<TSoftClassPtr<UObject>> ExcludedClassPaths;

    UPROPERTY( Transient )
    TArray<UClass*> ExcludedClasses;

    UPROPERTY( Config, EditAnywhere )
    FString BlueprintsPrefix;

    void PostProcessSettings();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};