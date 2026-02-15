// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameTime.h"
#include "MovieSceneBinding.h"

class ULevelSequence;

/**
 *
 */
class TestTargetActorModule
{
public:
	TestTargetActorModule();
	~TestTargetActorModule();

	static bool TryGetBoundActorFloatTrackValue(
		const FMovieSceneBinding* Binding,
		const FString& FloatTrackName,
		const FFrameTime& FrameTime,
		float& OutValue);

	static FFrameTime MakeFrameTime(
		ULevelSequence* LevelSequence,
		float Frame,
		float OffsetSeconds);


	static const FMovieSceneBinding* InitialSequenceTrack(
		ULevelSequence* LevelSequence, const AActor* Actor);

	
};