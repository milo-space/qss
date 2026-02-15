// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/TestTargetActor/TestTargetActorModule.h"

#include "GameFramework/Actor.h"
#include "LevelSequence.h"

#include "MovieScene.h"
#include "MovieSceneBindingProxy.h"
#include "MovieSceneCommonHelpers.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"
#include "ExtensionLibraries/MovieSceneBindingExtensions.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"



TestTargetActorModule::TestTargetActorModule()
{
}

TestTargetActorModule::~TestTargetActorModule()
{
}

bool TestTargetActorModule::TryGetBoundActorFloatTrackValue(
	const FMovieSceneBinding* Binding,
	const FString& FloatTrackName,
	const FFrameTime& FrameTime,
	float& OutValue)
{
	if (!Binding) {
		return false;
	}


	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(Track);
		if (!FloatTrack)
		{
			continue;
		}

		const FString TrackDisplayName = FloatTrack->GetDisplayName().ToString();
		if (!FloatTrackName.IsEmpty() && TrackDisplayName != FloatTrackName)
		{
			continue;
		}

		for (UMovieSceneSection* Section : FloatTrack->GetAllSections())
		{
			UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section);
			if (!FloatSection)
			{
				continue;
			}

			if (!FloatSection->GetRange().Contains(FrameTime.FrameNumber))
			{
				continue;
			}

			const FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
			float Value = 0.0f;
			if (Channel.Evaluate(FrameTime, Value))
			{
				OutValue = Value;
				return true;
			}
		}
	}

	return false;
}

FFrameTime TestTargetActorModule::MakeFrameTime(
	ULevelSequence* LevelSequence,
	float Frame,
	float OffsetSeconds)
{
	if (!LevelSequence)
	{
		return FFrameTime();
	}

	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	if (!MovieScene)
	{
		return FFrameTime();
	}

	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();

	const int32 BaseWholeFrames = FMath::FloorToInt(Frame);
	const float BaseSubFrame = Frame - static_cast<float>(BaseWholeFrames);
	const FFrameTime BaseDisplayTime(FFrameNumber(BaseWholeFrames), BaseSubFrame);

	const FFrameTime OffsetDisplayTime = DisplayRate.AsFrameTime(OffsetSeconds);

	const FFrameTime BaseTickTime = FFrameRate::TransformTime(BaseDisplayTime, DisplayRate, TickResolution);
	const FFrameTime OffsetTickTime = FFrameRate::TransformTime(OffsetDisplayTime, DisplayRate, TickResolution);

	return BaseTickTime + OffsetTickTime;
}


const FMovieSceneBinding* TestTargetActorModule::InitialSequenceTrack(
	ULevelSequence* LevelSequence, const AActor* Actor)
{
	const FMovieSceneBinding* TargetBinding = nullptr;
	FMovieSceneBindingProxy* TargetBindingProxy = nullptr;

	if (!LevelSequence || !Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitialSequenceTrack: Invalid args. LevelSequence=%s Actor=%s"),
			LevelSequence ? TEXT("Valid") : TEXT("Null"),
			Actor ? TEXT("Valid") : TEXT("Null"));
		return TargetBinding;
	}

	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	

	if (!MovieScene)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitialSequenceTrack: MovieScene is null."));
		return TargetBinding;
	}

	const UClass* ActorClass = Actor->GetClass();
	if (!ActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitialSequenceTrack: ActorClass is null."));
		return TargetBinding;
	}

	const TArray<FMovieSceneBindingProxy> Spawnables = UMovieSceneSequenceExtensions::GetSpawnables(LevelSequence);
	
	// Spawnables
	for (const FMovieSceneBindingProxy& BindingProxy : Spawnables)
	{
		const UObject* TemplateObject = UMovieSceneBindingExtensions::GetObjectTemplate(BindingProxy);
		const UClass* TemplateClass = TemplateObject ? TemplateObject->GetClass() : nullptr;

		if (TemplateClass && TemplateClass == ActorClass)
		{
			TargetBinding = MovieScene->FindBinding(BindingProxy.BindingID);
			break;
		}
	}

	// Possessables
	if (!TargetBinding)
	{
		const UObject* ActorObject = Actor;
		const TArray<FMovieSceneBindingProxy> Possessables = UMovieSceneSequenceExtensions::GetPossessables(LevelSequence);

		for (const FMovieSceneBindingProxy& BindingProxy : Possessables)
		{
			TArray<UObject*> BoundObjects = UMovieSceneSequenceExtensions::LocateBoundObjects(LevelSequence, BindingProxy, Actor->GetWorld());

			if (BoundObjects.Contains(const_cast<UObject*>(ActorObject)))
			{
				TargetBinding = MovieScene->FindBinding(BindingProxy.BindingID);
				break;
			}
		}
	}

	if (!TargetBinding)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("InitialSequenceTrack: No binding found for Actor=%s"), *Actor->GetName());

		return TargetBinding;
	}



	return TargetBinding;
}


