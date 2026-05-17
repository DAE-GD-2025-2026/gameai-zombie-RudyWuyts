#include "BTTask_SpinAroundTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBTSpin, Log, All);

UBTTask_SpinAroundTarget::UBTTask_SpinAroundTarget()
{
    NodeName = TEXT("Spin Around Target");
    bNotifyTick = true;
    RotationSpeedDegPerSec = 180.f;

    // Restrict the blackboard key to Actor types in editor
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SpinAroundTarget, TargetKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_SpinAroundTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTSpinMemory* MyMemory = (FBTSpinMemory*)NodeMemory;
    MyMemory->AccumulatedDeg = 0.f;

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: Blackboard component missing"));
        return EBTNodeResult::Failed;
    }

    UObject* Obj = nullptr;
    if (TargetKey.SelectedKeyName.IsNone())
    {
        // fallback to common default key name
        Obj = BlackboardComp->GetValueAsObject(FName("SelfActor"));
    }
    else
    {
        Obj = BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName);
    }

    AActor* Target = Cast<AActor>(Obj);
    if (!Target)
    {
        UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: Target is null"));
        return EBTNodeResult::Failed;
    }

    MyMemory->TargetActor = Target;

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (AICon)
    {
        APawn* Pawn = AICon->GetPawn();
        if (Pawn)
        {
            AICon->StopMovement();
        }
    }

    UE_LOG(LogBTSpin, Log, TEXT("SpinAround: Starting spin around %s"), *Target->GetName());

    return EBTNodeResult::InProgress;
}

void UBTTask_SpinAroundTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FBTSpinMemory* MyMemory = (FBTSpinMemory*)NodeMemory;

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AActor* Target = MyMemory->TargetActor.Get();
    if (!Target)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    const float Step = RotationSpeedDegPerSec * DeltaSeconds;
    MyMemory->AccumulatedDeg += Step;

    Pawn->AddActorWorldRotation(FRotator(0.f, Step, 0.f));
    if (UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->Velocity = FVector::ZeroVector;
    }

    if (MyMemory->AccumulatedDeg >= 360.f)
    {
        UE_LOG(LogBTSpin, Log, TEXT("SpinAround: Completed 360 degrees"));
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
