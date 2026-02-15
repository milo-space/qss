// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/TestTargetActor/TestTargetActorEditorModule.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "ILevelSequenceEditorToolkit.h"

#include "ISequencer.h"

#include "MovieScene.h"
#include "MovieSceneBindingProxy.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSequenceID.h"

#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Evaluation/MovieSceneSequenceTransform.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"

#endif

ULevelSequence* TargetActorEditorModule::FindActiveLevelSequenceForActor(const AActor* Actor)
{
#if WITH_EDITOR
	if (!Actor)
	{
		return nullptr;
	}


	if (!GEditor)
	{
		return nullptr;
	}

	ULevelSequence* CurrentRootLevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
	if (!CurrentRootLevelSequence)
	{
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return nullptr;
	}

	IAssetEditorInstance* AssetEditor = AssetEditorSubsystem->FindEditorForAsset(CurrentRootLevelSequence, false);
	if (!AssetEditor)
	{
		return nullptr;
	}

	ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
	if (!LevelSequenceEditor)
	{
		return nullptr;
	}

	TSharedPtr<ISequencer> Sequencer = LevelSequenceEditor->GetSequencer();
	if (!Sequencer.IsValid())
	{
		return nullptr;
	}

	UMovieSceneSequence* RootSequence = Sequencer->GetRootMovieSceneSequence();
	ULevelSequence* RootLevelSequence = Cast<ULevelSequence>(RootSequence);
	if (!RootLevelSequence)
	{
		return nullptr;
	}

	auto IsActorBoundInSequence = [Sequencer, Actor](FMovieSceneSequenceIDRef SequenceID) -> bool
	{
		AActor* MutableActor = const_cast<AActor*>(Actor);
		FGuid Guid = Sequencer->FindCachedObjectId(*MutableActor, SequenceID);
		if (Guid.IsValid())
		{
			return true;
		}

		Guid = Sequencer->FindObjectId(*MutableActor, SequenceID);
		return Guid.IsValid();
	};

	if (IsActorBoundInSequence(MovieSceneSequenceID::Root))
	{
		return RootLevelSequence;
	}

	UMovieSceneCompiledDataManager* CompiledMgr =
		UMovieSceneCompiledDataManager::GetPrecompiledData(EMovieSceneServerClientMask::All);
	if (!CompiledMgr)
	{
		return nullptr;
	}

	const FMovieSceneCompiledDataID RootDataID = CompiledMgr->GetDataID(RootSequence);

	// 変更後の ShotTrack を確実に反映させる
	CompiledMgr->Compile(RootDataID, RootSequence, EMovieSceneServerClientMask::All);

	const FMovieSceneSequenceHierarchy* Hierarchy = CompiledMgr->FindHierarchy(RootDataID);
	if (!Hierarchy)
	{
		return nullptr;
	}

	const FQualifiedFrameTime GlobalTime = Sequencer->GetGlobalTime();
	const FFrameRate RootTickResolution = Sequencer->GetRootTickResolution();
	const FFrameTime RootTime = FFrameRate::TransformTime(GlobalTime.Time, GlobalTime.Rate, RootTickResolution);

	const TMovieSceneEvaluationTree<FMovieSceneSubSequenceTreeEntry>& SubSequenceTree = Hierarchy->GetTree();
	FMovieSceneEvaluationTreeRangeIterator RangeIt =
		SubSequenceTree.IterateFromTime(RootTime.FrameNumber);

	if (RangeIt)
	{
		for (const FMovieSceneSubSequenceTreeEntry& Entry : SubSequenceTree.GetAllData(RangeIt.Node()))
		{
			const FMovieSceneSequenceID SeqID = Entry.SequenceID;
			if (!SeqID.IsValid() || SeqID == MovieSceneSequenceID::Root)
			{
				continue;
			}

			if (!IsActorBoundInSequence(SeqID))
			{
				continue;
			}

			UMovieSceneSequence* SubSequence = Hierarchy->FindSubSequence(SeqID);
			return Cast<ULevelSequence>(SubSequence);
		}
	}
#endif

	return nullptr;
}


void TargetActorEditorModule::SetFrameTrackIfNeed(
	const ULevelSequence* LevelSequence, const FMovieSceneBinding* Binding)
{
#if WITH_EDITOR

	if (!LevelSequence || !Binding)
	{
		return;
	}

	// Set Key Frame
	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	UMovieSceneTrack* FrameTrack = nullptr;

	const FString TrackName = TEXT("Frame");

	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		if (!Track)
		{
			continue;
		}

		UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track);
		if (!PropertyTrack)
		{
			continue;
		}

		if (PropertyTrack->GetPropertyName() == FName(*TrackName))
		{
			FrameTrack = Track;
			break;
		}
	}


	bool bIsNewTrackCreated = false;

	if (!FrameTrack)
	{
		UMovieSceneTrack* NewTrack = NewObject<UMovieSceneTrack>(
			MovieScene, UMovieSceneFloatTrack::StaticClass(), FName(TrackName), RF_Transactional);

		if (NewTrack)
		{
			const FGuid BindingID = Binding->GetObjectGuid();

			MovieScene->AddGivenTrack(NewTrack, BindingID);


			FrameTrack = NewTrack;


			if (UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(FrameTrack))
			{
				FloatTrack->SetPropertyNameAndPath(TEXT("Frame"), TEXT("Frame"));
			}

			bIsNewTrackCreated = true;

		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("SetFrameTrackIfNeed: Failed to create new track."));
			return;
		}
	}

	if (bIsNewTrackCreated)
	{
		if (UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>(FrameTrack))
		{
			UMovieSceneFloatSection* FloatSection = nullptr;
			const TArray<UMovieSceneSection*>& Sections = FloatTrack->GetAllSections();
			if (Sections.Num() > 0)
			{
				FloatSection = Cast<UMovieSceneFloatSection>(Sections[0]);
			}
			else
			{
				FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->CreateNewSection());
				if (FloatSection)
				{
					FloatTrack->AddSection(*FloatSection);
					FloatSection->SetRange(TRange<FFrameNumber>::All());
#if WITH_EDITORONLY_DATA
					FloatSection->SetIsLocked(true);
#endif
				}
			}

			if (FloatSection)
			{

				const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
				const FFrameRate TickResolution = MovieScene->GetTickResolution();

				const FFrameNumber Frame0 = FFrameRate::TransformTime(FFrameTime(0), DisplayRate, TickResolution).RoundToFrame();
				const FFrameNumber Frame30 = FFrameRate::TransformTime(FFrameTime(30), DisplayRate, TickResolution).RoundToFrame();

				FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();

				Channel.PreInfinityExtrap = RCCE_Linear;
				Channel.PostInfinityExtrap = RCCE_Linear;

				auto Data = Channel.GetData();

				FMovieSceneFloatValue V0(0.0f);
				V0.InterpMode = RCIM_Linear;
				Data.AddKey(Frame0, V0);

				FMovieSceneFloatValue V1(30.0f);
				V1.InterpMode = RCIM_Linear;
				Data.AddKey(Frame30, V1);
			}
		}
	}
#endif
	return;
}