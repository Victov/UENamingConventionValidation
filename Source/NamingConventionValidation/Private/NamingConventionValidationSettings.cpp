#include "NamingConventionValidation/Public/NamingConventionValidationSettings.h"
#include "NamingConventionValidationLog.h"

UNamingConventionValidationSettings::UNamingConventionValidationSettings()
{
    bLogWarningWhenNoClassDescriptionForAsset = false;
    bAllowValidationInDevelopersFolder = false;
    bAllowValidationOnlyInGameFolder = true;
    bDoesValidateOnSave = true;
    BlueprintsPrefix = "BP_";
}

bool UNamingConventionValidationSettings::IsPathExcludedFromValidation(const FString& Path) const
{
    if (!Path.StartsWith("/Game/") && bAllowValidationOnlyInGameFolder)
    {
        bool bCanProcessFolder = NonGameFoldersDirectoriesToProcess.FindByPredicate([&Path](const auto& Directory)
        {
            return Path.StartsWith(Directory.Path);
        }) != nullptr;

        if (!bCanProcessFolder)
        {
            bCanProcessFolder = NonGameFoldersDirectoriesToProcessContainingToken.FindByPredicate([&Path](const auto& Token) {
                return Path.Contains(Token);
            }) != nullptr;
        }

        if (!bCanProcessFolder)
        {
            return true;
        }
    }

    if (Path.StartsWith("/Game/Developers/") && !bAllowValidationInDevelopersFolder)
    {
        return true;
    }

    for (const FDirectoryPath& ExcludedPath : ExcludedDirectories)
    {
        if (Path.StartsWith(ExcludedPath.Path))
        {
            return true;
        }
    }

    return false;
}

void UNamingConventionValidationSettings::PostProcessSettings()
{
    for (FNamingConventionValidationClassDescription& ClassDescription : ClassDescriptions)
    {
        ClassDescription.Class = ClassDescription.ClassPath.LoadSynchronous();
        UE_CLOG(ClassDescription.Class == nullptr, LogNamingConventionValidation, Warning, TEXT("Impossible to get a valid UClass for the class path %s"), *ClassDescription.ClassPath.ToString());
    }

    ClassDescriptions.Sort();

    for (TSoftClassPtr<UObject>& ExcludedClassPath : ExcludedClassPaths)
    {
        UClass* ExcludedClass = ExcludedClassPath.LoadSynchronous();
        UE_CLOG(ExcludedClass == nullptr, LogNamingConventionValidation, Warning, TEXT("Impossible to get a valid UClass for the excluded class path %s"), *ExcludedClassPath.ToString());

        if (IsValid(ExcludedClass))
        {
            ExcludedClasses.Add(ExcludedClass);
        }
    }

    static const FDirectoryPath EngineDirectoryPath({ TEXT("/Engine/") });

    // Cannot use AddUnique since FDirectoryPath does not have operator==
    if (!ExcludedDirectories.ContainsByPredicate([](const FDirectoryPath& Item)
    {
        return Item.Path == EngineDirectoryPath.Path;
    }))
    {
        ExcludedDirectories.Add(EngineDirectoryPath);
    }
}

#if WITH_EDITOR
void UNamingConventionValidationSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    PostProcessSettings();
}
#endif
