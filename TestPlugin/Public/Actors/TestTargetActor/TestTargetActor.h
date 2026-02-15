// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "TestTargetActor.generated.h"


class ATestActor;


USTRUCT(BlueprintType)
struct FOpenSequencerLocalTimeResult
{
	GENERATED_BODY()

	// RuntimeでもOK（LevelSequence モジュール）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<ULevelSequence> Sequence = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LocalTimeSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bValid = false;
};

UCLASS()
class TESTPLUGIN_API ATestTargetActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestTargetActor();

	void RunTargetActor();


private:

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<ULevelSequence> LevelSequence;


	UPROPERTY(VisibleAnywhere, Interp)
	float Frame;



};
