// Fill out your copyright notice in the Description page of Project Settings.


#include "LoadingScreenLib.h"
#include "MoviePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

FDelegateHandle ULoadingScreenLib::PostLoadHandle;

void ULoadingScreenLib::StartMovie(const FString& MovieNameNoExt, bool bSkippable, float MinDisplayTime)
{
    if (!IsMoviePlayerEnabled())
        return;

    // 수동 종료 + 루프 재생(맵 로딩이 끝날 때까지 화면 유지)
    FLoadingScreenAttributes Attr;
    Attr.bAutoCompleteWhenLoadingCompletes = false;
    Attr.bWaitForManualStop = true;    
    Attr.MinimumLoadingScreenDisplayTime = MinDisplayTime;
    Attr.bMoviesAreSkippable = bSkippable;
#if ENGINE_MAJOR_VERSION >= 5
    Attr.PlaybackType = EMoviePlaybackType::MT_Looped;
#endif
    if (!MovieNameNoExt.IsEmpty())
        Attr.MoviePaths.Add(MovieNameNoExt);        

    GetMoviePlayer()->SetupLoadingScreen(Attr);
    GetMoviePlayer()->PlayMovie();

    if (!PostLoadHandle.IsValid())
    {
        PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(
            &ULoadingScreenLib::OnPostLoadMap);
    }
}

void ULoadingScreenLib::OnPostLoadMap(UWorld*)
{
    StopTeaser();
}

void ULoadingScreenLib::StopTeaser()
{
    if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
        GetMoviePlayer()->StopMovie();

    if (PostLoadHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
        PostLoadHandle.Reset();
    }
}

void ULoadingScreenLib::PlayTeaserAndOpenLevel(UObject* WorldContextObject,
    FName LevelName, const FString& MovieNameNoExt, bool bSkippable, float MinDisplayTime)
{
    StartMovie(MovieNameNoExt, bSkippable, MinDisplayTime);

    // 여기서부터 실제 월드 로딩 시작 → 로딩되는 동안 영화가 계속 뜸
    UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
    UGameplayStatics::OpenLevel(World, LevelName, true);
}

void ULoadingScreenLib::PlayTeaserAndClientTravel(UObject* WorldContextObject,
    const FString& TravelURL, const FString& MovieNameNoExt, bool bSkippable, float MinDisplayTime)
{
    StartMovie(MovieNameNoExt, bSkippable, MinDisplayTime);

    if (UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject))
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            PC->ClientTravel(TravelURL, TRAVEL_Absolute);
        }
    }
}







