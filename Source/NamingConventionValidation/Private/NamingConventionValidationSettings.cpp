#include "NamingConventionValidation/Public/NamingConventionValidationSettings.h"
#include "NamingConventionValidationLog.h"

UNamingConventionValidationSettings::UNamingConventionValidationSettings()
{
    bLogWarningWhenNoClassDescriptionForAsset = false;
    bAllowValidationInDevelopersFolder = false;
    bAllowValidationOnlyInGameFolder = true;
    bDoesValidateOnSave = true;
    BlueprintsPrefix = "BP_";

    ResetValidatorClassDescriptionsToEpicDefaults();
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

void UNamingConventionValidationSettings::ResetValidatorClassDescriptionsToEpicDefaults()
{
    auto AddDefaultClassDescriptionWithPrefix = [this](FString InPath, FString Prefix) -> void
    {
        const FSoftObjectPath Path(InPath);
        FNamingConventionValidationClassDescription Desc;
        Desc.ClassPath = Path;
        Desc.Prefix = Prefix;
        ClassDescriptions.Add(Desc);
    };

    ClassDescriptions.Empty();

    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.ActorComponent"), TEXT("AC_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.AnimInstance"), TEXT("ABP_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.BlueprintFunctionLibrary"), TEXT("BFL_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/AIModule.BTDecorator_BlueprintBase"), TEXT("BTD_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/AIModule.BTService_BlueprintBase"), TEXT("BTS_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/AIModule.BTTask_BlueprintBase"), TEXT("BTT_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/AIModule.BehaviorTree"), TEXT("BT_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.CurveTable"), TEXT("CT_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.DataTable"), TEXT("DT_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/CoreUObject.Enum"), TEXT("E_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/GameplayAbilities.GameplayAbility"), TEXT("GA_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/GameplayAbilities.GameplayAbilityTargetActor"), TEXT("GATA_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/GameplayAbilities.GameplayEffect"), TEXT("GE_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/GameplayAbilities.GameplayCueNotify_Actor"), TEXT("GCN_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/GameplayTasks.GameplayTask"), TEXT("GT_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.Material"), TEXT("M_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/PhysicsCore.PhysicalMaterial"), TEXT("PM_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.PhysicsAsset"), TEXT("PHYS_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.SkeletalMesh"), TEXT("SK_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/StateTreeModule.StateTree"), TEXT("ST_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/StateTreeModule.StateTreeConditionBlueprintBase"), TEXT("STC_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/StateTreeModule.StateTreeTaskBlueprintBase"), TEXT("STT_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.StaticMesh"), TEXT("SM_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/CoreUObject.Struct"), TEXT("F_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/UMG.Widget"), TEXT("WBP_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.Texture"), TEXT("T_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/Engine.AnimMontage"), TEXT("AM_"));
    AddDefaultClassDescriptionWithPrefix(TEXT("/Script/LevelSequence.LevelSequence"), TEXT("LS_"));
}

#if WITH_EDITOR
void UNamingConventionValidationSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    PostProcessSettings();
}
#endif
