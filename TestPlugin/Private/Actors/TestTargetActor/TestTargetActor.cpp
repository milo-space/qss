// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/TestTargetActor/TestTargetActor.h"

#if WITH_EDITOR
#include "Actors/TestTargetActor/TestTargetActorEditorModule.h"
#endif
#include "Actors/TestTargetActor/TestTargetActorModule.h"

// Sets default values
ATestTargetActor::ATestTargetActor()
{
 	// Set this actor to call Tick() every frame.  Y
	// ou can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}


void ATestTargetActor::RunTargetActor()
{


	ULevelSequence* Sequence = nullptr;

	Sequence = TargetActorEditorModule::FindActiveLevelSequenceForActor(this);
	
	if (Sequence && LevelSequence != Sequence) {
		LevelSequence = Sequence;
		UE_LOG(LogTemp, Log, TEXT("[%s] LevelSequence Setted %s"),
			*GetName(), *LevelSequence->GetDisplayName().ToString());
	}

	const FMovieSceneBinding* Binding = nullptr;

	if (LevelSequence) {

		Binding = TestTargetActorModule::InitialSequenceTrack(LevelSequence, this);

		if (Binding) {

			TargetActorEditorModule::SetFrameTrackIfNeed(LevelSequence, Binding);

			float Value = 0.0f;

			FFrameTime TimeA = TestTargetActorModule::MakeFrameTime(LevelSequence, Frame, 1.0f);

			TestTargetActorModule::TryGetBoundActorFloatTrackValue(Binding, TEXT("Frame"), TimeA, Value);

			UE_LOG(LogTemp, Log, TEXT("[%s] RunTargetActor %s current: %f future: %f"), *GetName(), *LevelSequence->GetDisplayName().ToString(), Frame, Value);

		}

	}
	else {
		//UE_LOG(LogTemp, Log, TEXT("[%s] RunTargetActor No Active LevelSequence"), *GetName());
	}


}
