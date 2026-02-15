// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestActor.generated.h"


class USceneComponent;
class USkeletalMeshComponent;
class UControlRigComponent;
class UControlRig;
class ATestTargetActor;

UCLASS(meta = (PrioritizeCategories = "TestActor"))
class TESTPLUGIN_API ATestActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestActor();

protected:
	virtual void PostRegisterAllComponents() override;


#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif


private:


	UPROPERTY(VisibleDefaultsOnly, Category = "TestActor|Components")
	TObjectPtr<USceneComponent> Root;


	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComp;


	UPROPERTY()
	TObjectPtr<UControlRigComponent> ControlRigComp;


	/** SkeletalMesh アセット（BPデフォルトでのみ編集したいなら EditDefaultsOnly 推奨） */
	UPROPERTY(EditDefaultsOnly, Category = "TestActor")
	TObjectPtr<USkeletalMesh> SkeletalMeshAsset;

	/** ControlRig クラス（BPデフォルトでのみ編集したいなら EditDefaultsOnly 推奨） */
	UPROPERTY(EditDefaultsOnly, Category = "TestActor")
	TSubclassOf<UControlRig> ControlRigClass;


	UPROPERTY(EditAnywhere, Interp, Category = "TestActor")
	TObjectPtr<ATestTargetActor> TestTargetActor;


	UFUNCTION()
	void HandleControlRigPreInitialize(UControlRigComponent* Component);


	UFUNCTION()
	void HandleControlRigPreForwardsSolve(UControlRigComponent* Component);


	UFUNCTION()
	void HandleControlRigPostForwardsSolve(UControlRigComponent* Component);


	bool bPreInitBound = false;

	void BindControlRigDelegatesIfNeeded();


	/** 変数→コンポーネントへ反映（エディタでも呼ぶ） */
	void ApplyAssetsToComponents();

	/** CDO/Archetype ガード */
	bool IsTemplateObjectSafeToSkip() const;

};
