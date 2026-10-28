#pragma once
class ACXPlayerController;

// Opt-in development-only integration test. Inactive in ordinary play.
void ScheduleCXNetworkSmokeTest(ACXPlayerController* Controller);
