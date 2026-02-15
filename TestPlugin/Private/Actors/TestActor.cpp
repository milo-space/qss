// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/TestActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

// ControlRig
#include "ControlRigComponent.h"
#include "ControlRig.h"
#include "ControlRigDefines.h" 


#include "Actors/TestTargetActor/TestTargetActor.h"


// Sets default values
ATestActor::ATestActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMeshComp->SetupAttachment(Root);


	ControlRigComp = CreateDefaultSubobject<UControlRigComponent>(TEXT("ControlRig"));
	ControlRigComp->SetupAttachment(Root);


}


#if WITH_EDITOR
void ATestActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (IsTemplateObjectSafeToSkip())
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] PostEditChangeProperty Canceled"), *GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] PostEditChangeProperty"), *GetName());


	const FName PropName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	const bool bRelevant =
		(PropName == GET_MEMBER_NAME_CHECKED(ATestActor, SkeletalMeshAsset)) ||
		(PropName == GET_MEMBER_NAME_CHECKED(ATestActor, ControlRigClass));


	if (bRelevant) {
		ApplyAssetsToComponents();
	}

}
#endif

bool ATestActor::IsTemplateObjectSafeToSkip() const
{
	return (IsTemplate() || GetWorld() == nullptr);
}


void ATestActor::ApplyAssetsToComponents()
{
	if (!SkeletalMeshComp || !ControlRigComp)
	{
		return;
	}

	if (SkeletalMeshComp->GetSkeletalMeshAsset() != SkeletalMeshAsset)
	{
		SkeletalMeshComp->SetSkeletalMesh(SkeletalMeshAsset);
	}

	// ControlRigClass
	UControlRig* CurrentRig = ControlRigComp->GetControlRig();
	const UClass* CurrentClass = CurrentRig ? CurrentRig->GetClass() : nullptr;
	const UClass* NewClass = ControlRigClass.Get(); // nullptr の可能性あり

	if (CurrentClass != NewClass)
	{
		ControlRigComp->SetControlRigClass(ControlRigClass); // None ならクリア方向
	}

	//UE_LOG(LogTemp, Log, TEXT("[%s] ApplyAssetsToComponents"), *GetName());

}


void ATestActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();


	// CDO やテンプレートにはバインドしない（BP編集時の事故防止）
	if (IsTemplateObjectSafeToSkip())
	{
		//UE_LOG(LogTemp, Log, TEXT("[%s] PostRegisterAllComponents Canceled"), *GetName());
		return;
	}

	//UE_LOG(LogTemp, Log, TEXT("[%s] PostRegisterAllComponents"), *GetName());

	BindControlRigDelegatesIfNeeded();
}



void ATestActor::BindControlRigDelegatesIfNeeded()
{
	if (!ControlRigComp || bPreInitBound)
	{
		return;
	}


	ControlRigComp->OnPreInitializeDelegate.RemoveDynamic(
		this, &ATestActor::HandleControlRigPreInitialize);
	ControlRigComp->OnPreInitializeDelegate.AddDynamic(
		this, &ATestActor::HandleControlRigPreInitialize);


	ControlRigComp->OnPreForwardsSolveDelegate.RemoveDynamic(
		this, &ATestActor::HandleControlRigPreForwardsSolve);
	ControlRigComp->OnPreForwardsSolveDelegate.AddDynamic(
		this, &ATestActor::HandleControlRigPreForwardsSolve);


	ControlRigComp->OnPreForwardsSolveDelegate.RemoveDynamic(
		this, &ATestActor::HandleControlRigPostForwardsSolve);
	ControlRigComp->OnPreForwardsSolveDelegate.AddDynamic(
		this, &ATestActor::HandleControlRigPostForwardsSolve);


	bPreInitBound = true;
}



void ATestActor::HandleControlRigPreInitialize(UControlRigComponent* Component)
{

	if (!Component || !SkeletalMeshComp)
	{
		return;
	}

	Component->ClearMappedElements();

	Component->AddMappedCompleteSkeletalMesh(
		SkeletalMeshComp,
		EControlRigComponentMapDirection::Output
	);

	UE_LOG(LogTemp, Log, TEXT("[%s] HandleControlRigPreInitialize"), *GetName());
}


void ATestActor::HandleControlRigPreForwardsSolve(UControlRigComponent* Component)
{
	if (!Component)
	{
		return;
	}


	
	//UE_LOG(LogTemp, Log, TEXT("[%s] PreForwordsSolve2 %s %f"), *GetName(), *GetWorld()->GetName(), GetWorld()->TimeSeconds);

	if (TestTargetActor) {
		
		//UE_LOG(LogTemp, Log, TEXT("[%d] "), TestTargetActor->IsHidden());
		TestTargetActor->RunTargetActor();
	}

	// 例: 毎フレーム/毎評価で、外部入力値をリグに流す・Transformを読む/書く等
	// ※Mapping追加や重い探索、アセットロードは避ける（ここは頻度が高い）
}


void ATestActor::HandleControlRigPostForwardsSolve(UControlRigComponent* Component)
{
	if (!Component)
	{
		return;
	}


	//UE_LOG(LogTemp, Log, TEXT("[%s] HandleControlRigPreForwordsSolve"), *GetName());
	// 例: 毎フレーム/毎評価で、外部入力値をリグに流す・Transformを読む/書く等
	// ※Mapping追加や重い探索、アセットロードは避ける（ここは頻度が高い）
}