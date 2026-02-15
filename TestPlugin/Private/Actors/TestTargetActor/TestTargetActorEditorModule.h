// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneBinding.h"

class AActor;
class ULevelSequence;

class TESTPLUGIN_API TargetActorEditorModule
{
public:
	static ULevelSequence* FindActiveLevelSequenceForActor(const AActor* Actor);

	static void SetFrameTrackIfNeed(
		const ULevelSequence* LevelSequence, const FMovieSceneBinding* Binding);
};