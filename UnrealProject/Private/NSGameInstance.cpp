// Fill out your copyright notice in the Description page of Project Settings.


#include "NSGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"

#if UE_SERVER
#include "GSDKUtils.h"
#include "TimerManager.h"

bool UNSGameInstance::bGSDKBootstrapped = false;
bool UNSGameInstance::bGSDKDelegatesBound = false;
#endif

static void JsonToString(const TSharedPtr<FJsonObject>& Obj, FString& Out)
{
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
}

void UNSGameInstance::Init()
{
    Super::Init();

#if !UE_SERVER
    if (UGameUserSettings* GS = GEngine->GetGameUserSettings())
    {
        GS->SetViewDistanceQuality(0);           
        GS->SetShadowQuality(1);                 
        GS->SetPostProcessingQuality(1);
        GS->SetTextureQuality(1);
        GS->SetVisualEffectQuality(1);
        GS->SetFoliageQuality(0);                
        GS->SetShadingQuality(1);
        GS->SetAntiAliasingQuality(1);

        GS->ApplySettings(false);
        GS->SaveSettings();
    }

    IConsoleManager& CM = IConsoleManager::Get();
    if (auto* Var = CM.FindConsoleVariable(TEXT("r.ScreenPercentage"))) Var->Set(75.f, ECVF_SetByGameSetting);
    if (auto* Var = CM.FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"))) Var->Set(0, ECVF_SetByGameSetting);
    if (auto* Var = CM.FindConsoleVariable(TEXT("r.Shadow.CSM.MaxCascades"))) Var->Set(1, ECVF_SetByGameSetting);
    if (auto* Var = CM.FindConsoleVariable(TEXT("r.Shadow.MaxCSMResolution"))) Var->Set(1024, ECVF_SetByGameSetting);
    if (auto* Var = CM.FindConsoleVariable(TEXT("r.Shadow.DistanceScale"))) Var->Set(0.6f, ECVF_SetByGameSetting);
    if (auto* Var = CM.FindConsoleVariable(TEXT("r.ViewDistanceScale"))) Var->Set(0.70f, ECVF_SetByGameSetting);
#endif

#if UE_SERVER
    // 델리게이트 바인딩
    FOnGSDKShutdown_Dyn     Shutdown; Shutdown.BindDynamic(this, &UNSGameInstance::OnGSDKShutdown);
    FOnGSDKHealthCheck_Dyn  Health;   Health.BindDynamic(this, &UNSGameInstance::OnGSDKHealthCheck);
    FOnGSDKServerActive_Dyn Active;   Active.BindDynamic(this, &UNSGameInstance::OnGSDKActive);
    UGSDKUtils::RegisterGSDKShutdownDelegate(Shutdown);
    UGSDKUtils::RegisterGSDKHealthCheckDelegate(Health);
    UGSDKUtils::RegisterGSDKServerActiveDelegate(Active);

    // 플레이어 접속 포트(게임 포트) 기본값 구성
    UGSDKUtils::SetDefaultServerHostPort();

    UGSDKUtils::ReadyForPlayers();

    UE_LOG(LogTemp, Log, TEXT("[GSDK] Init done. (delegates bound, port set, ReadyForPlayers)"));
#endif
}


#if UE_SERVER
void UNSGameInstance::BootstrapGSDK()
{
    if (bGSDKBootstrapped) return;
    bGSDKBootstrapped = true;
}
#endif

void UNSGameInstance::OnStart()
{
	Super::OnStart();

#if UE_SERVER
    if (UWorld* W = GetWorld())
    {
        FTimerHandle Tmp;
        W->GetTimerManager().SetTimer(
            Tmp,
            []() { UGSDKUtils::ReadyForPlayers(); },
            0.1f, false
        );
    }
    UE_LOG(LogTemp, Log, TEXT("[GSDK] ReadyForPlayers() scheduled"));
#endif
}

bool UNSGameInstance::OnGSDKHealthCheck()
{
#if UE_SERVER
    return true;
#else
    return true;
#endif
}

void UNSGameInstance::OnGSDKActive()
{
#if UE_SERVER
    UE_LOG(LogTemp, Log, TEXT("[GSDK] ServerActive (allocated)."));
#endif
}

void UNSGameInstance::OnGSDKShutdown()
{
#if UE_SERVER
    UE_LOG(LogTemp, Warning, TEXT("[GSDK] Shutdown requested. Exiting..."));

    FPlatformMisc::RequestExit(false); 
#endif
}

FString UNSGameInstance::MakeGuestName()
{
    return FString::Printf(TEXT("Guest-%06X"), FMath::Rand() & 0xFFFFFF);
}

void UNSGameInstance::ConnectToDS(const FString& Host, int32 Port, const FString& PlayerName)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[TITLE] World is null"));
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("[TITLE] PlayerController not found"));
        return;
    }

    const FString NameToUse = PlayerName.IsEmpty() ? MakeGuestName() : PlayerName;
    const FString URL = FString::Printf(TEXT("%s:%d?Name=%s"), *Host, Port, *NameToUse);

    UE_LOG(LogTemp, Log, TEXT("[TITLE] ClientTravel -> %s"), *URL);
    PC->ClientTravel(URL, TRAVEL_Absolute);
}

void UNSGameInstance::OpenLocal(const FString& MapName)
{
    UGameplayStatics::OpenLevel(this, FName(*MapName));
}

void UNSGameInstance::StartFromTitle(bool bForceLocal,
    const FString& HostOverride,
    int32 PortOverride,
    const FString& OptionalPlayerName)
{
    const bool bGoLocal = bForceLocal || bLocalTest;

    const FString Host = !HostOverride.IsEmpty() ? HostOverride : DefaultDSHost;
    const int32   Port = (PortOverride > 0) ? PortOverride : DefaultDSPort;

    if (bGoLocal)
    {
        static const TCHAR* IngameMapPath = TEXT("/Game/maps/MainWorld_WP");
        UE_LOG(LogTemp, Log, TEXT("[TITLE] OpenLocal %s"), IngameMapPath);
        OpenLocal(IngameMapPath);
    }
    else
    {
        ConnectToDS(Host, Port, OptionalPlayerName);
    }
}

void UNSGameInstance::CreateRoomAndJoin(const FString& OptionalPlayerName)
{
    auto AfterLogin = [this, OptionalPlayerName](const FString& PlayerET)
        {
            CallExecuteFunction(PlayerET, [this, OptionalPlayerName](const FString& Ip, int32 Port)
                {
                    UE_LOG(LogTemp, Log, TEXT("[TITLE] MPS allocated %s:%d"), *Ip, Port);
                    ConnectToDS(Ip, Port, OptionalPlayerName);
                });
        };

    if (!CachedPlayerEntityToken.IsEmpty())
    {
        AfterLogin(CachedPlayerEntityToken);
    }
    else
    {
        LoginWithCustomId(AfterLogin);
    }
}

void UNSGameInstance::LoginWithCustomId(TFunction<void(const FString&)> OnDone)
{
    // Client/LoginWithCustomID
    const FString Url = FString::Printf(
        TEXT("https://%s.playfabapi.com/Client/LoginWithCustomID"),
        *GetTitleIdSafe());
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("TitleId"), TitleId);
    Body->SetStringField(TEXT("CustomId"), CustomId);
    Body->SetBoolField(TEXT("CreateAccount"), true);

    FString BodyStr; JsonToString(Body, BodyStr);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetContentAsString(BodyStr);
    Req->OnProcessRequestComplete().BindLambda([this, OnDone](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
        {
            if (!bOk || !Res.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Login request failed")); return;
            }

            UE_LOG(LogTemp, Log, TEXT("[Login] HTTP %d, body: %s"),
                Res->GetResponseCode(), *Res->GetContentAsString().Left(512));

            TSharedPtr<FJsonObject> Root; auto Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Login json parse failed")); return;
            }

            // 응답 안에 EntityToken 바로 있음. 없으면 GetEntityToken 한 번 더 호출.
            FString PlayerET;
            if (Root->HasField(TEXT("data")))
            {
                auto Data = Root->GetObjectField(TEXT("data"));
                if (Data->HasField(TEXT("EntityToken")))
                {
                    PlayerET = Data->GetObjectField(TEXT("EntityToken"))->GetStringField(TEXT("EntityToken"));
                }
            }
            if (PlayerET.IsEmpty())
            {
                // Authentication/GetEntityToken (X-Authorization: SessionTicket)
                FString SessionTicket;
                if (Root->HasField(TEXT("data")))
                    SessionTicket = Root->GetObjectField(TEXT("data"))->GetStringField(TEXT("SessionTicket"));

                const FString Url2 = FString::Printf(
                    TEXT("https://%s.playfabapi.com/Authentication/GetEntityToken"),
                    *GetTitleIdSafe());
                auto Req2 = FHttpModule::Get().CreateRequest();
                Req2->SetURL(Url2);
                Req2->SetVerb(TEXT("POST"));
                Req2->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
                Req2->SetHeader(TEXT("X-Authorization"), SessionTicket);
                Req2->SetContentAsString(TEXT("{}"));
                Req2->OnProcessRequestComplete().BindLambda([this, OnDone](FHttpRequestPtr, FHttpResponsePtr Res2, bool bOk2)
                    {
                        if (!bOk2 || !Res2.IsValid()) { UE_LOG(LogTemp, Error, TEXT("GetEntityToken failed")); return; }
                        TSharedPtr<FJsonObject> R2; auto Rd = TJsonReaderFactory<>::Create(Res2->GetContentAsString());
                        if (!FJsonSerializer::Deserialize(Rd, R2) || !R2.IsValid()) { UE_LOG(LogTemp, Error, TEXT("GetEntityToken json parse failed")); return; }
                        FString PlayerET = R2->GetObjectField(TEXT("data"))->GetStringField(TEXT("EntityToken"));
                        CachedPlayerEntityToken = PlayerET;
                        OnDone(PlayerET);
                    });
                Req2->ProcessRequest();
            }
            else
            {
                CachedPlayerEntityToken = PlayerET;
                OnDone(PlayerET);
            }
        });
    Req->ProcessRequest();
}

void UNSGameInstance::CallExecuteFunction(const FString& PlayerET, TFunction<void(const FString&, int32)> OnDone)
{
    const FString UrlEF = FString::Printf(
        TEXT("https://%s.playfabapi.com/CloudScript/ExecuteFunction"),
        *GetTitleIdSafe());

    // 바디 구성
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("buildId"), TEXT("25bb3a2d-8f54-4772-a57c-f59c11a62dac"));
    TArray<TSharedPtr<FJsonValue>> Regions;
    Regions.Add(MakeShared<FJsonValueString>(TEXT("KoreaCentral")));
    Params->SetArrayField(TEXT("preferredRegions"), Regions);
    Params->SetStringField(TEXT("map"), TEXT("/Game/maps/MainWorld_WP"));

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("FunctionName"), TEXT("CreateMpsSession"));
    Body->SetObjectField(TEXT("FunctionParameter"), Params);

    FString BodyStr; JsonToString(Body, BodyStr);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(UrlEF);
    UE_LOG(LogTemp, Log, TEXT("[HTTP] %s"), *UrlEF);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("X-EntityToken"), PlayerET);
    Req->SetContentAsString(BodyStr);
    Req->OnProcessRequestComplete().BindLambda([this, OnDone](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
        {
            if (!bOk || !Res.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteFunction failed")); return;
            }

            const int32 Code = Res->GetResponseCode();
            const FString BodyTxt = Res->GetContentAsString();
            UE_LOG(LogTemp, Log, TEXT("[ExecuteFunction] HTTP %d, body: %s"),
                Code, *BodyTxt.Left(512));

            if (Code != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("PlayFab returned HTTP %d (check X-EntityToken or Function registry)"), Code);
                return;
            }

            TSharedPtr<FJsonObject> Root;
            auto Reader = TJsonReaderFactory<>::Create(BodyTxt);
            if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("ExecuteFunction json parse failed (not JSON)"));
                return;
            }

            // PlayFab 에러 케이스
            if (Root->HasField(TEXT("error")))
            {
                const FString Err = Root->GetStringField(TEXT("errorMessage"));
                UE_LOG(LogTemp, Error, TEXT("PlayFab error: %s"), *Err);
                return;
            }

            // data
            const TSharedPtr<FJsonObject>* DataObj = nullptr;
            if (!Root->TryGetObjectField(TEXT("data"), DataObj) || !DataObj)
            {
                UE_LOG(LogTemp, Error, TEXT("Missing 'data' field")); return;
            }

            // FunctionResult: 오브젝트 or 문자열 모두 처리
            const TSharedPtr<FJsonObject>* FRObj = nullptr;
            TSharedPtr<FJsonObject> FR;
            if ((*DataObj)->TryGetObjectField(TEXT("FunctionResult"), FRObj) && FRObj)
            {
                FR = *FRObj;
            }
            else
            {
                FString FRString;
                if ((*DataObj)->TryGetStringField(TEXT("FunctionResult"), FRString))
                {
                    // 문자열이면 다시 JSON 시도
                    TSharedPtr<FJsonObject> Inner;
                    auto R2 = TJsonReaderFactory<>::Create(FRString);
                    if (FJsonSerializer::Deserialize(R2, Inner) && Inner.IsValid())
                    {
                        FR = Inner;
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("FunctionResult is not JSON: %s"), *FRString.Left(256));
                        return;
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("FunctionResult not found"));
                    return;
                }
            }

            const FString Ip = FR->GetStringField(TEXT("ip"));
            int32 Port = 30000; // 기본값
            if (FR->HasField(TEXT("port"))) {
                Port = (int32)FR->GetNumberField(TEXT("port"));
                if (Port <= 0) Port = 30000;
            }

            LastServerIP = Ip;
            LastServerPort = Port;

            OnDone(Ip, Port);
        });
    Req->ProcessRequest();
}

void UNSGameInstance::BP_CreateRoom(const FString& OptionalPlayerName)
{
    CreateRoomAndJoin(OptionalPlayerName);
}

void UNSGameInstance::BP_JoinDirect(const FString& Host, int32 Port, const FString& PlayerName)
{
    LastServerIP = Host;
    LastServerPort = Port;
    ConnectToDS(Host, Port, PlayerName);
}

FString UNSGameInstance::GetTitleIdSafe() const
{
    FString T = TitleId;
    T.TrimStartAndEndInline();
    if (T.IsEmpty())
    {
        static const FString Fallback(TEXT("11D45F"));
        UE_LOG(LogTemp, Warning, TEXT("[PlayFab] TitleId empty; fallback -> %s"), *Fallback);
        return Fallback;
    }
    return T;
}

void UNSGameInstance::LeaveToTitle()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->ClientTravel(TEXT("disconnect"), TRAVEL_Absolute);
        FTimerHandle H;
        GetWorld()->GetTimerManager().SetTimer(H, [this]()
            {
                UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/maps/Ingame/Ingame_Title")));
            }, 0.25f, false);
    }
}



