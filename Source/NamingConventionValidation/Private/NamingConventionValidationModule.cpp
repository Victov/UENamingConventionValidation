#include "NamingConventionValidationModule.h"

#include "EditorNamingValidatorSubsystem.h"
#include "NamingConventionValidationCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "LevelEditor.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "NamingConventionValidationModule"

void FindAssetDependencies(const FAssetRegistryModule& AssetRegistryModule, const FAssetData& AssetData, TSet<FAssetData>& DependentAssets)
{
    if (AssetData.IsValid())
    {
        if (UObject* Object = AssetData.GetAsset())
        {
            FAssetIdentifier AssetIdentifier(Object, NAME_None);

            if (!DependentAssets.Contains(AssetData))
            {
                DependentAssets.Add(AssetData);

                TArray<FAssetDependency> Dependencies;
                AssetRegistryModule.Get().GetDependencies(AssetIdentifier, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);

                for (const FAssetDependency& Dependency : Dependencies)
                {
                    FAssetData DependentAsset = AssetRegistryModule.Get().GetAssetByObjectPath(Dependency.AssetId.ToString());
                    FindAssetDependencies(AssetRegistryModule, DependentAsset, DependentAssets);
                }
            }
        }
    }
}

void OnPackageSaved(const FString& /*PackageFileName*/, UPackage* Package, FObjectPostSaveContext Context)
{
    if (GEditor)
    {
        if (UEditorNamingValidatorSubsystem* EditorValidationSubsystem = GEditor->GetEditorSubsystem<UEditorNamingValidatorSubsystem>())
        {
            EditorValidationSubsystem->ValidateSavedPackage(Package->GetFName());
        }
    }
}

void ValidateAssets(const TArray<FAssetData> SelectedAssets)
{
    if (GEditor)
    {
        if (UEditorNamingValidatorSubsystem* EditorValidationSubsystem = GEditor->GetEditorSubsystem<UEditorNamingValidatorSubsystem>())
        {
            EditorValidationSubsystem->ValidateAssets(SelectedAssets);
        }
    }
}

void ValidateFolders(const TArray< FString > SelectedFolders)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    FARFilter Filter;
    Filter.bRecursivePaths = true;

    for (const FString& Folder : SelectedFolders)
    {
        Filter.PackagePaths.Emplace(*Folder);
    }

    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);

    ValidateAssets(AssetList);
}

void CreateDataValidationContentBrowserAssetMenu(FMenuBuilder& MenuBuilder, const TArray< FAssetData > SelectedAssets)
{
    MenuBuilder.AddMenuSeparator();
    MenuBuilder.AddMenuEntry(
        LOCTEXT("NamingConventionValidateAssetsTabTitle", "Validate Assets Naming Convention"),
        LOCTEXT("NamingConventionValidateAssetsTooltipText", "Run naming convention validation on these assets."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(ValidateAssets, SelectedAssets)));
}

TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
    TSharedRef<FExtender> Extender(new FExtender());

    Extender->AddMenuExtension(
        "AssetContextAdvancedActions",
        EExtensionHook::After,
        nullptr,
        FMenuExtensionDelegate::CreateStatic(CreateDataValidationContentBrowserAssetMenu, SelectedAssets));

    return Extender;
}

void CreateDataValidationContentBrowserPathMenu(FMenuBuilder& MenuBuilder, const TArray<FString> SelectedPaths)
{
    MenuBuilder.AddMenuEntry(
        LOCTEXT("NamingConventionValidateAssetsPathTabTitle", "Validate Assets Naming Convention in Folder"),
        LOCTEXT("NamingConventionValidateAssetsPathTooltipText", "Runs naming convention validation on the assets in the selected folder."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(ValidateFolders, SelectedPaths)));
}

TSharedRef<FExtender> OnExtendContentBrowserPathSelectionMenu(const TArray< FString >& SelectedPaths)
{
    TSharedRef< FExtender > Extender(new FExtender());

    Extender->AddMenuExtension(
        "PathContextBulkOperations",
        EExtensionHook::After,
        nullptr,
        FMenuExtensionDelegate::CreateStatic(CreateDataValidationContentBrowserPathMenu, SelectedPaths));

    return Extender;
}

FText MenuValidateDataGetTitle()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    if (AssetRegistryModule.Get().IsLoadingAssets())
    {
        return LOCTEXT("NamingConventionValidationTitleDA", "Validate Naming Convention [Discovering Assets]");
    }
    return LOCTEXT("NamingConventionValidationTitle", "Naming Convention...");
}

void MenuValidateData()
{
    //Make sure the asset registry is finished building
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    if (AssetRegistryModule.Get().IsLoadingAssets())
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AssetsStillScanningError", "Cannot run naming convention validation while still discovering assets."));
        return;
    }

    const bool bSuccess = UNamingConventionValidationCommandlet::ValidateData();

    if (!bSuccess)
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NamingConventionValidationError", "An error was encountered during naming convention validation. See the log for details."));
    }
}

void NamingConventionValidationMenuCreationDelegate(FMenuBuilder& MenuBuilder)
{
    MenuBuilder.BeginSection("NamingConventionValidation", LOCTEXT("NamingConventionValidation", "NamingConventionValidation"));
    MenuBuilder.AddMenuEntry(
        TAttribute<FText>::Create(&MenuValidateDataGetTitle),
        LOCTEXT("NamingConventionValidationTooltip", "Validates all naming convention in content directory."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon"),
        FUIAction(FExecuteAction::CreateStatic(&MenuValidateData)));
    MenuBuilder.EndSection();
}

class FNamingConventionValidationModule : public INamingConventionValidationModule
{
public:
    void StartupModule() override;
    void ShutdownModule() override;

private:
    TSharedPtr<FExtender> MenuExtender;
    FDelegateHandle ContentBrowserAssetExtenderDelegateHandle;
    FDelegateHandle ContentBrowserPathExtenderDelegateHandle;
    FDelegateHandle OnPackageSavedDelegateHandle;
};

IMPLEMENT_MODULE(FNamingConventionValidationModule, NamingConventionValidation)

void FNamingConventionValidationModule::StartupModule()
{
    if (!IsRunningCommandlet() && !IsRunningGame() && FSlateApplication::IsInitialized())
    {
        FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        TArray<FContentBrowserMenuExtender_SelectedAssets>& ContentBrowserMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

        ContentBrowserMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(OnExtendContentBrowserAssetSelectionMenu));
        ContentBrowserAssetExtenderDelegateHandle = ContentBrowserMenuExtenderDelegates.Last().GetHandle();

        TArray<FContentBrowserMenuExtender_SelectedPaths>& ContentBrowserFolderMenuExtenderDelegates = ContentBrowserModule.GetAllPathViewContextMenuExtenders();

        ContentBrowserFolderMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedPaths::CreateStatic(OnExtendContentBrowserPathSelectionMenu));
        ContentBrowserPathExtenderDelegateHandle = ContentBrowserFolderMenuExtenderDelegates.Last().GetHandle();

        MenuExtender = MakeShareable(new FExtender);
        MenuExtender->AddMenuExtension("FileLoadAndSave", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateStatic(NamingConventionValidationMenuCreationDelegate));

        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
        LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

        OnPackageSavedDelegateHandle = UPackage::PackageSavedWithContextEvent.AddStatic(OnPackageSaved);
    }
}

void FNamingConventionValidationModule::ShutdownModule()
{
    if (!IsRunningCommandlet() && !IsRunningGame() && !IsRunningDedicatedServer())
    {
        if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
        {
            TArray<FContentBrowserMenuExtender_SelectedAssets>& ContentBrowserMenuExtenderDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();
            ContentBrowserMenuExtenderDelegates.RemoveAll([&ExtenderDelegate = ContentBrowserAssetExtenderDelegateHandle](const FContentBrowserMenuExtender_SelectedAssets& Delegate) {
                return Delegate.GetHandle() == ExtenderDelegate;
            });
            //@TODO: why is this being removed twice?!
            ContentBrowserMenuExtenderDelegates.RemoveAll([&ExtenderDelegate = ContentBrowserAssetExtenderDelegateHandle](const FContentBrowserMenuExtender_SelectedAssets& Delegate) {
                return Delegate.GetHandle() == ExtenderDelegate;
            });
        }

        if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
        {
            LevelEditorModule->GetMenuExtensibilityManager()->RemoveExtender(MenuExtender);
        }
        MenuExtender = nullptr;

        UPackage::PackageSavedWithContextEvent.Remove(OnPackageSavedDelegateHandle);
    }
}

#undef LOCTEXT_NAMESPACE