// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"


UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto PerceptionComp = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);
	}

	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (PawnOwner)
	{
		CachedController = Cast<AAIController>(PawnOwner->GetController());
		if (CachedController)
		{
			CachedBlackboard = CachedController->GetBlackboardComponent();
			CachedBehaviorTree = Cast<UBehaviorTreeComponent>(CachedController->BrainComponent);
		}
	}
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	FString StimulusInfo = FString::Printf(
		TEXT("Stimulus - Tag: %s, Strength: %.2f, Age: %.2f, StimulusLocation: (%.1f,%.1f,%.1f), ReceiverLocation: (%.1f,%.1f,%.1f), WasSensed: %s, Expired: %s"),
		*Stimulus.Tag.ToString(),
		Stimulus.Strength,
		Stimulus.GetAge(),
		Stimulus.StimulusLocation.X, Stimulus.StimulusLocation.Y, Stimulus.StimulusLocation.Z,
		Stimulus.ReceiverLocation.X, Stimulus.ReceiverLocation.Y, Stimulus.ReceiverLocation.Z,
		Stimulus.WasSuccessfullySensed() ? TEXT("true") : TEXT("false"),
		Stimulus.IsExpired() ? TEXT("true") : TEXT("false")
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, StimulusInfo);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *StimulusInfo);

	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Saw Something!"));

	if (CachedBlackboard)
	{
		CachedBlackboard->SetValueAsObject(BlackboardTargetKey, Actor);
	}
	else
	{
		APawn* PawnOwner = Cast<APawn>(GetOwner());
		if (PawnOwner)
		{
			if (AAIController* AICon = Cast<AAIController>(PawnOwner->GetController()))
			{
				if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				{
					BB->SetValueAsObject(BlackboardTargetKey, Actor);
					CachedBlackboard = BB;
					CachedController = AICon;
					CachedBehaviorTree = Cast<UBehaviorTreeComponent>(AICon->BrainComponent);
				}
			}
		}
	}

	if (CachedBehaviorTree)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("BehaviorTreeComponent found"));
	}
}
