// // /*++

// //     Copyright (c) Microsoft Corporation.
// //     Licensed under the MIT License.

// // Abstract:
// //     This file implements the CubicProbe congestion control algorithm.
// //     This is the final, complete version with all interface functions implemented and bugs fixed.
// // --*/

// // #include "precomp.h"
// // #include <stdio.h> // For printf

// // // Constants for CUBIC
// // #define TEN_TIMES_BETA_CUBIC 7
// // #define TEN_TIMES_C_CUBIC 4

// // // Constants for the probing mechanism
// // #define PROBE_RTT_INTERVAL 5
// // #define PROBE_RTT_INCREASE_THRESHOLD 1.2

// // // Forward declarations of local functions
// // void CubicProbeCongestionControlOnCongestionEvent(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ BOOLEAN IsPersistentCongestion,
// //     _In_ BOOLEAN Ecn
// //     );
// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // BOOLEAN
// // CubicProbeCongestionControlUpdateBlockedState(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ BOOLEAN PreviousCanSendState
// //     );

// // // Helper function to reset the probe state.
// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // void
// // CubicProbeResetProbeState(
// //     _In_ QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe
// //     )
// // {
// //     CubicProbe->ProbeState = PROBE_INACTIVE;
// //     CubicProbe->CumulativeSuccessLevel = 0;
// //     CubicProbe->RttCount = 0;
// //     CubicProbe->RttAtProbeStartUs = 0;
// // }

// // // NOTE: CubeRoot function has been REMOVED as it is unused.

// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // BOOLEAN
// // CubicProbeCongestionControlCanSend(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc
// //     )
// // {
// //     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
// //     return CubicProbe->BytesInFlight < CubicProbe->CongestionWindow || CubicProbe->Exemptions > 0;
// // }


// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // void
// // CubicProbeCongestionControlReset(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ BOOLEAN FullReset
// //     )
// // {
// //     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
// //     QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc);
// //     const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&Connection->Paths[0]);

// //     CubicProbe->SlowStartThreshold = UINT32_MAX;
// //     CubicProbe->IsInRecovery = FALSE;
// //     CubicProbe->HasHadCongestionEvent = FALSE;
// //     CubicProbe->CongestionWindow = DatagramPayloadLength * CubicProbe->InitialWindowPackets;
// //     CubicProbe->BytesInFlightMax = CubicProbe->CongestionWindow / 2;
// //     if (FullReset) {
// //         CubicProbe->BytesInFlight = 0;
// //     }
// //     CubicProbeResetProbeState(CubicProbe);
// // }

// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // BOOLEAN
// // CubicProbeCongestionControlOnDataAcknowledged(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ const QUIC_ACK_EVENT* AckEvent
// //     )
// // {
// //     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
// //     const uint64_t TimeNowUs = AckEvent->TimeNow;
// //     QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc);
// //     BOOLEAN PreviousCanSendState = CubicProbeCongestionControlCanSend(Cc);
// //     uint32_t BytesAcked = AckEvent->NumRetransmittableBytes;

// //     CXPLAT_DBG_ASSERT(CubicProbe->BytesInFlight >= BytesAcked);
// //     CubicProbe->BytesInFlight -= BytesAcked;

// //     if (CubicProbe->IsInRecovery) {
// //         if (AckEvent->LargestAck > CubicProbe->RecoverySentPacketNumber) {
// //             CubicProbe->IsInRecovery = FALSE;
// //             CubicProbe->TimeOfCongAvoidStart = TimeNowUs;
// //         }
// //         goto Exit;
// //     }

// //     if (CubicProbe->CongestionWindow >= CubicProbe->SlowStartThreshold && AckEvent->MinRttValid) {
// //         switch (CubicProbe->ProbeState) {
// //         case PROBE_INACTIVE:
// //             if (CubicProbe->WindowMax > 0 && CubicProbe->CongestionWindow >= CubicProbe->WindowMax) {
// //                 CubicProbe->RttCount++;
// //                 if (CubicProbe->RttCount >= PROBE_RTT_INTERVAL) {
// //                     CubicProbe->ProbeState = PROBE_TEST;
// //                     CubicProbe->CumulativeSuccessLevel = 1;
// //                     CubicProbe->RttAtProbeStartUs = AckEvent->MinRtt;
// //                     CubicProbe->RttCount = 0;
// //                     printf("[CubicProbe] Probe Start -> TEST. RTT_base=%llu us, CWND=%u\n", (unsigned long long)CubicProbe->RttAtProbeStartUs, CubicProbe->CongestionWindow);
// //                 }
// //             } else {
// //                 CubicProbe->RttCount = 0;
// //             }
// //             break;
// //         case PROBE_TEST:
// //             CubicProbe->ProbeState = PROBE_JUDGMENT;
// //             // fallthrough
// //         case PROBE_JUDGMENT:
// //             if (CubicProbe->RttAtProbeStartUs > 0 && AckEvent->MinRtt <= (uint64_t)(CubicProbe->RttAtProbeStartUs * PROBE_RTT_INCREASE_THRESHOLD)) {
// //                 CubicProbe->ProbeState = PROBE_TEST;
// //                 CubicProbe->CumulativeSuccessLevel++;
// //                 printf("[CubicProbe] Probe Success -> TEST. Level=%u, NewRTT=%llu us, CWND=%u\n", CubicProbe->CumulativeSuccessLevel, (unsigned long long)AckEvent->MinRtt, CubicProbe->CongestionWindow);
// //             } else {
// //                 printf("[CubicProbe] Probe Failed -> INACTIVE. NewRTT=%llu us, BaseRTT=%llu us\n", (unsigned long long)AckEvent->MinRtt, (unsigned long long)CubicProbe->RttAtProbeStartUs);
// //                 CubicProbeCongestionControlOnCongestionEvent(Cc, FALSE, FALSE);
// //             }
// //             break;
// //         }
// //     }

// //     // Slow Start
// //     if (CubicProbe->CongestionWindow < CubicProbe->SlowStartThreshold) {
// //         CubicProbe->CongestionWindow += BytesAcked;
// //     } else { // Congestion Avoidance
// //         const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&Connection->Paths[0]);
// //         if (CubicProbe->ProbeState == PROBE_TEST && CubicProbe->CumulativeSuccessLevel > 0) {
// //             uint32_t PrevCwnd = CubicProbe->CongestionWindow;
// //             uint32_t GrowthInBytes = (uint32_t)(DatagramPayloadLength * (CubicProbe->CumulativeSuccessLevel / 2.0 + 1.0));
// //             CubicProbe->CongestionWindow += GrowthInBytes;
// //             printf("[CubicProbe] Aggressive Growth! CWND: %u -> %u (+%u)\n", PrevCwnd, CubicProbe->CongestionWindow, GrowthInBytes);
// //         } else {
// //             // Standard CUBIC growth logic (simplified AIMD for this example)
// //             if (CubicProbe->CongestionWindow > 0) {
// //                 // Using a simplified Reno-friendly growth for stability in non-probing congestion avoidance.
// //                 CubicProbe->CongestionWindow += (DatagramPayloadLength * DatagramPayloadLength) / CubicProbe->CongestionWindow;
// //             }
// //         }
// //     }

// // Exit:
// //     return CubicProbeCongestionControlUpdateBlockedState(Cc, PreviousCanSendState);
// // }


// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // void
// // CubicProbeCongestionControlOnCongestionEvent(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ BOOLEAN IsPersistentCongestion,
// //     _In_ BOOLEAN Ecn
// //     )
// // {
// //     UNREFERENCED_PARAMETER(IsPersistentCongestion);
// //     UNREFERENCED_PARAMETER(Ecn);
// //     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
// //     const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&QuicCongestionControlGetConnection(Cc)->Paths[0]);

// //     CubicProbe->WindowMax = CubicProbe->CongestionWindow;
// //     CubicProbe->SlowStartThreshold = (CubicProbe->CongestionWindow * TEN_TIMES_BETA_CUBIC) / 10;
// //     CubicProbe->CongestionWindow = CXPLAT_MAX(CubicProbe->SlowStartThreshold, DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS);

// //     printf("[CubicProbe] Congestion Event! New CWND = %u, SSThresh = %u\n",
// //         CubicProbe->CongestionWindow, CubicProbe->SlowStartThreshold);
    
// //     CubicProbeResetProbeState(CubicProbe);
// // }

// // // ---▼▼▼ [수정된 부분] 빠졌던 함수들의 구현 추가 및 NULL이 아니도록 수정 ▼▼▼---

// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // void
// // CubicProbeCongestionControlOnDataLost(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ const QUIC_LOSS_EVENT* LossEvent
// //     )
// // {
// //     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;

// //     if (!CubicProbe->HasHadCongestionEvent || LossEvent->LargestPacketNumberLost > CubicProbe->RecoverySentPacketNumber) {
// //         CubicProbe->RecoverySentPacketNumber = LossEvent->LargestSentPacketNumber;
// //         CubicProbeCongestionControlOnCongestionEvent(Cc, LossEvent->PersistentCongestion, FALSE);
// //     }

// //     CXPLAT_DBG_ASSERT(CubicProbe->BytesInFlight >= LossEvent->NumRetransmittableBytes);
// //     CubicProbe->BytesInFlight -= LossEvent->NumRetransmittableBytes;

// //     CubicProbeCongestionControlUpdateBlockedState(Cc, CubicProbeCongestionControlCanSend(Cc));
// // }

// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // void
// // CubicProbeCongestionControlSetExemption(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ uint8_t NumPackets
// //     )
// // {
// //     Cc->CubicProbe.Exemptions = NumPackets;
// // }

// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // uint32_t CubicProbeCongestionControlGetSendAllowance(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid) { UNREFERENCED_PARAMETER(TimeSinceLastSend); UNREFERENCED_PARAMETER(TimeSinceLastSendValid); QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe; if (CubicProbe->BytesInFlight >= CubicProbe->CongestionWindow) return 0; return CubicProbe->CongestionWindow - CubicProbe->BytesInFlight; }
// // _IRQL_requires_max_(PASSIVE_LEVEL) void CubicProbeCongestionControlOnDataSent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes) { BOOLEAN CanSend = CubicProbeCongestionControlCanSend(Cc); Cc->CubicProbe.BytesInFlight += NumRetransmittableBytes; if (Cc->CubicProbe.BytesInFlightMax < Cc->CubicProbe.BytesInFlight) { Cc->CubicProbe.BytesInFlightMax = Cc->CubicProbe.BytesInFlight; } if (Cc->CubicProbe.Exemptions > 0) { --Cc->CubicProbe.Exemptions; } CubicProbeCongestionControlUpdateBlockedState(Cc, CanSend); }
// // _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlOnDataInvalidated( _In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes) { BOOLEAN CanSend = CubicProbeCongestionControlCanSend(Cc); CXPLAT_DBG_ASSERT(Cc->CubicProbe.BytesInFlight >= NumRetransmittableBytes); Cc->CubicProbe.BytesInFlight -= NumRetransmittableBytes; return CubicProbeCongestionControlUpdateBlockedState(Cc, CanSend); }
// // _IRQL_requires_max_(DISPATCH_LEVEL) void CubicProbeCongestionControlOnEcn(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent) { UNREFERENCED_PARAMETER(EcnEvent); CubicProbeCongestionControlOnCongestionEvent(Cc, FALSE, TRUE); }
// // _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); return FALSE; }
// // void CubicProbeCongestionControlLogOutFlowStatus(_In_ const QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); }
// // uint32_t CubicProbeCongestionControlGetBytesInFlightMax(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.BytesInFlightMax; }
// // _IRQL_requires_max_(DISPATCH_LEVEL) uint8_t CubicProbeCongestionControlGetExemptions(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.Exemptions; }
// // uint32_t CubicProbeCongestionControlGetCongestionWindow(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.CongestionWindow; }
// // void CubicProbeCongestionControlGetNetworkStatistics(_In_ const QUIC_CONNECTION* const Connection, _In_ const QUIC_CONGESTION_CONTROL* const Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics) { const QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe; const QUIC_PATH* Path = &Connection->Paths[0]; NetworkStatistics->BytesInFlight = CubicProbe->BytesInFlight; NetworkStatistics->CongestionWindow = CubicProbe->CongestionWindow; NetworkStatistics->SmoothedRTT = Path->SmoothedRtt; NetworkStatistics->Bandwidth = (Path->SmoothedRtt > 0) ? (CubicProbe->CongestionWindow * 1000000ull / Path->SmoothedRtt) : 0; }
// // _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlIsAppLimited(const QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); return FALSE; }
// // _IRQL_requires_max_(DISPATCH_LEVEL) void CubicProbeCongestionControlSetAppLimited(QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); }
// // _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlUpdateBlockedState(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN PreviousCanSendState) { QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc); if (PreviousCanSendState != CubicProbeCongestionControlCanSend(Cc)) { if (PreviousCanSendState) { QuicConnAddOutFlowBlockedReason(Connection, QUIC_FLOW_BLOCKED_CONGESTION_CONTROL); } else { QuicConnRemoveOutFlowBlockedReason(Connection, QUIC_FLOW_BLOCKED_CONGESTION_CONTROL); } } return FALSE; }

// // static const QUIC_CONGESTION_CONTROL QuicCongestionControlCubicProbe = {
// //     .Name = "CubicProbe",
// //     .QuicCongestionControlCanSend = CubicProbeCongestionControlCanSend,
// //     .QuicCongestionControlSetExemption = CubicProbeCongestionControlSetExemption,
// //     .QuicCongestionControlReset = CubicProbeCongestionControlReset,
// //     .QuicCongestionControlGetSendAllowance = CubicProbeCongestionControlGetSendAllowance,
// //     .QuicCongestionControlOnDataSent = CubicProbeCongestionControlOnDataSent,
// //     .QuicCongestionControlOnDataInvalidated = CubicProbeCongestionControlOnDataInvalidated,
// //     .QuicCongestionControlOnDataAcknowledged = CubicProbeCongestionControlOnDataAcknowledged,
// //     .QuicCongestionControlOnDataLost = CubicProbeCongestionControlOnDataLost,
// //     .QuicCongestionControlOnEcn = CubicProbeCongestionControlOnEcn,
// //     .QuicCongestionControlOnSpuriousCongestionEvent = CubicProbeCongestionControlOnSpuriousCongestionEvent,
// //     .QuicCongestionControlLogOutFlowStatus = CubicProbeCongestionControlLogOutFlowStatus,
// //     .QuicCongestionControlGetExemptions = CubicProbeCongestionControlGetExemptions,
// //     .QuicCongestionControlGetBytesInFlightMax = CubicProbeCongestionControlGetBytesInFlightMax,
// //     .QuicCongestionControlIsAppLimited = CubicProbeCongestionControlIsAppLimited,
// //     .QuicCongestionControlSetAppLimited = CubicProbeCongestionControlSetAppLimited,
// //     .QuicCongestionControlGetCongestionWindow = CubicProbeCongestionControlGetCongestionWindow,
// //     .QuicCongestionControlGetNetworkStatistics = CubicProbeCongestionControlGetNetworkStatistics
// // };
// // // ---▲▲▲ [여기까지 수정] ▲▲▲---

// // _IRQL_requires_max_(DISPATCH_LEVEL)
// // void
// // CubicProbeCongestionControlInitialize(
// //     _In_ QUIC_CONGESTION_CONTROL* Cc,
// //     _In_ const QUIC_SETTINGS_INTERNAL* Settings
// //     )
// // {
// //     *Cc = QuicCongestionControlCubicProbe;
// //     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
// //     CubicProbe->SendIdleTimeoutMs = Settings->SendIdleTimeoutMs;
// //     CubicProbe->InitialWindowPackets = Settings->InitialWindowPackets;
// //     CubicProbeCongestionControlReset(Cc, TRUE);
// // }



// /*++

//     Copyright (c) Microsoft Corporation.
//     Licensed under the MIT License.

// Abstract:
//     This file implements the CubicProbe congestion control algorithm.
//     This is the final, complete version with all interface functions implemented to prevent crashes.
// --*/

// #include "precomp.h"
// #include <stdio.h> // For printf

// // Constants for CUBIC
// #define TEN_TIMES_BETA_CUBIC 7
// #define TEN_TIMES_C_CUBIC 4

// // Constants for the probing mechanism
// #define PROBE_RTT_INTERVAL 5
// #define PROBE_RTT_INCREASE_THRESHOLD 1.2

// // Forward declarations of local functions
// void CubicProbeCongestionControlOnCongestionEvent(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ BOOLEAN IsPersistentCongestion,
//     _In_ BOOLEAN Ecn
//     );
// _IRQL_requires_max_(DISPATCH_LEVEL)
// BOOLEAN
// CubicProbeCongestionControlUpdateBlockedState(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ BOOLEAN PreviousCanSendState
//     );

// // Helper function to reset the probe state.
// _IRQL_requires_max_(DISPATCH_LEVEL)
// void
// CubicProbeResetProbeState(
//     _In_ QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe
//     )
// {
//     CubicProbe->ProbeState = PROBE_INACTIVE;
//     CubicProbe->CumulativeSuccessLevel = 0;
//     CubicProbe->RttCount = 0;
//     CubicProbe->RttAtProbeStartUs = 0;
// }

// _IRQL_requires_max_(DISPATCH_LEVEL)
// BOOLEAN
// CubicProbeCongestionControlCanSend(
//     _In_ QUIC_CONGESTION_CONTROL* Cc
//     )
// {
//     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
//     return CubicProbe->BytesInFlight < CubicProbe->CongestionWindow || CubicProbe->Exemptions > 0;
// }

// _IRQL_requires_max_(DISPATCH_LEVEL)
// void
// CubicProbeCongestionControlReset(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ BOOLEAN FullReset
//     )
// {
//     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
//     QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc);
//     const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&Connection->Paths[0]);

//     CubicProbe->SlowStartThreshold = UINT32_MAX;
//     CubicProbe->IsInRecovery = FALSE;
//     CubicProbe->HasHadCongestionEvent = FALSE;
//     CubicProbe->CongestionWindow = DatagramPayloadLength * CubicProbe->InitialWindowPackets;
//     CubicProbe->BytesInFlightMax = CubicProbe->CongestionWindow / 2;
//     if (FullReset) {
//         CubicProbe->BytesInFlight = 0;
//     }
//     CubicProbeResetProbeState(CubicProbe);
// }

// _IRQL_requires_max_(DISPATCH_LEVEL)
// BOOLEAN
// CubicProbeCongestionControlOnDataAcknowledged(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ const QUIC_ACK_EVENT* AckEvent
//     )
// {
//     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
//     const uint64_t TimeNowUs = AckEvent->TimeNow;
//     QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc);
//     BOOLEAN PreviousCanSendState = CubicProbeCongestionControlCanSend(Cc);
//     uint32_t BytesAcked = AckEvent->NumRetransmittableBytes;

//     CXPLAT_DBG_ASSERT(CubicProbe->BytesInFlight >= BytesAcked);
//     CubicProbe->BytesInFlight -= BytesAcked;

//     if (CubicProbe->IsInRecovery) {
//         if (AckEvent->LargestAck > CubicProbe->RecoverySentPacketNumber) {
//             CubicProbe->IsInRecovery = FALSE;
//             CubicProbe->TimeOfCongAvoidStart = TimeNowUs;
//         }
//         goto Exit;
//     }

//     if (CubicProbe->CongestionWindow >= CubicProbe->SlowStartThreshold && AckEvent->MinRttValid) {
//         switch (CubicProbe->ProbeState) {
//         case PROBE_INACTIVE:
//             if (CubicProbe->WindowMax > 0 && CubicProbe->CongestionWindow >= CubicProbe->WindowMax) {
//                 CubicProbe->RttCount++;
//                 if (CubicProbe->RttCount >= PROBE_RTT_INTERVAL) {
//                     CubicProbe->ProbeState = PROBE_TEST;
//                     CubicProbe->CumulativeSuccessLevel = 1;
//                     CubicProbe->RttAtProbeStartUs = AckEvent->MinRtt;
//                     CubicProbe->RttCount = 0;
//                     printf("[SERVER-CubicProbe] Probe Start -> TEST. RTT_base=%llu us, CWND=%u\n", (unsigned long long)CubicProbe->RttAtProbeStartUs, CubicProbe->CongestionWindow);
//                 }
//             } else {
//                 CubicProbe->RttCount = 0;
//             }
//             break;
//         case PROBE_TEST:
//             CubicProbe->ProbeState = PROBE_JUDGMENT;
//             // fallthrough
//         case PROBE_JUDGMENT:
//             if (CubicProbe->RttAtProbeStartUs > 0 && AckEvent->MinRtt <= (uint64_t)(CubicProbe->RttAtProbeStartUs * PROBE_RTT_INCREASE_THRESHOLD)) {
//                 CubicProbe->ProbeState = PROBE_TEST;
//                 CubicProbe->CumulativeSuccessLevel++;
//                 printf("[SERVER-CubicProbe] Probe Success -> TEST. Level=%u, NewRTT=%llu us, CWND=%u\n", CubicProbe->CumulativeSuccessLevel, (unsigned long long)AckEvent->MinRtt, CubicProbe->CongestionWindow);
//             } else {
//                 printf("[SERVER-CubicProbe] Probe Failed -> INACTIVE. NewRTT=%llu us, BaseRTT=%llu us\n", (unsigned long long)AckEvent->MinRtt, (unsigned long long)CubicProbe->RttAtProbeStartUs);
//                 CubicProbeCongestionControlOnCongestionEvent(Cc, FALSE, FALSE);
//             }
//             break;
//         }
//     }

//     // Slow Start
//     if (CubicProbe->CongestionWindow < CubicProbe->SlowStartThreshold) {
//         CubicProbe->CongestionWindow += BytesAcked;
//     } else { // Congestion Avoidance
//         const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&Connection->Paths[0]);
//         if (CubicProbe->ProbeState == PROBE_TEST && CubicProbe->CumulativeSuccessLevel > 0) {
//             uint32_t PrevCwnd = CubicProbe->CongestionWindow;
//             uint32_t GrowthInBytes = (uint32_t)(DatagramPayloadLength * (CubicProbe->CumulativeSuccessLevel / 2.0 + 1.0));
//             CubicProbe->CongestionWindow += GrowthInBytes;
//             printf("[SERVER-CubicProbe] Aggressive Growth! CWND: %u -> %u (+%u)\n", PrevCwnd, CubicProbe->CongestionWindow, GrowthInBytes);
//         } else {
//             // Standard simplified growth
//             if (CubicProbe->CongestionWindow > 0) {
//                 CubicProbe->CongestionWindow += (DatagramPayloadLength * DatagramPayloadLength) / CubicProbe->CongestionWindow;
//             }
//         }
//     }

// Exit:
//     return CubicProbeCongestionControlUpdateBlockedState(Cc, PreviousCanSendState);
// }


// _IRQL_requires_max_(DISPATCH_LEVEL)
// void
// CubicProbeCongestionControlOnCongestionEvent(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ BOOLEAN IsPersistentCongestion,
//     _In_ BOOLEAN Ecn
//     )
// {
//     UNREFERENCED_PARAMETER(IsPersistentCongestion);
//     UNREFERENCED_PARAMETER(Ecn);
//     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
//     const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&QuicCongestionControlGetConnection(Cc)->Paths[0]);

//     CubicProbe->WindowMax = CubicProbe->CongestionWindow;
//     CubicProbe->SlowStartThreshold = (CubicProbe->CongestionWindow * TEN_TIMES_BETA_CUBIC) / 10;
//     CubicProbe->CongestionWindow = CXPLAT_MAX(CubicProbe->SlowStartThreshold, DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS);

//     printf("[SERVER-CubicProbe] Congestion Event! New CWND = %u, SSThresh = %u\n",
//         CubicProbe->CongestionWindow, CubicProbe->SlowStartThreshold);
    
//     CubicProbeResetProbeState(CubicProbe);
// }

// _IRQL_requires_max_(DISPATCH_LEVEL)
// void
// CubicProbeCongestionControlOnDataLost(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ const QUIC_LOSS_EVENT* LossEvent
//     )
// {
//     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
//     printf("[SERVER-CubicProbe] Packet Loss Event! LostBytes=%u\n", LossEvent->NumRetransmittableBytes);
//     if (!CubicProbe->HasHadCongestionEvent || LossEvent->LargestPacketNumberLost > CubicProbe->RecoverySentPacketNumber) {
//         CubicProbe->RecoverySentPacketNumber = LossEvent->LargestSentPacketNumber;
//         CubicProbeCongestionControlOnCongestionEvent(Cc, LossEvent->PersistentCongestion, FALSE);
//     }
//     CXPLAT_DBG_ASSERT(CubicProbe->BytesInFlight >= LossEvent->NumRetransmittableBytes);
//     CubicProbe->BytesInFlight -= LossEvent->NumRetransmittableBytes;
//     CubicProbeCongestionControlUpdateBlockedState(Cc, CubicProbeCongestionControlCanSend(Cc));
// }

// //
// // Boilerplate functions to fully implement the CC interface.
// //
// _IRQL_requires_max_(DISPATCH_LEVEL)
// void
// CubicProbeCongestionControlSetExemption(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ uint8_t NumPackets
//     )
// {
//     Cc->CubicProbe.Exemptions = NumPackets;
// }

// _IRQL_requires_max_(DISPATCH_LEVEL)
// uint32_t CubicProbeCongestionControlGetSendAllowance(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid) { UNREFERENCED_PARAMETER(TimeSinceLastSend); UNREFERENCED_PARAMETER(TimeSinceLastSendValid); QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe; if (CubicProbe->BytesInFlight >= CubicProbe->CongestionWindow) return 0; return CubicProbe->CongestionWindow - CubicProbe->BytesInFlight; }
// _IRQL_requires_max_(PASSIVE_LEVEL) void CubicProbeCongestionControlOnDataSent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes) { BOOLEAN CanSend = CubicProbeCongestionControlCanSend(Cc); Cc->CubicProbe.BytesInFlight += NumRetransmittableBytes; if (Cc->CubicProbe.BytesInFlightMax < Cc->CubicProbe.BytesInFlight) { Cc->CubicProbe.BytesInFlightMax = Cc->CubicProbe.BytesInFlight; } if (Cc->CubicProbe.Exemptions > 0) { --Cc->CubicProbe.Exemptions; } CubicProbeCongestionControlUpdateBlockedState(Cc, CanSend); }
// _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlOnDataInvalidated( _In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes) { BOOLEAN CanSend = CubicProbeCongestionControlCanSend(Cc); CXPLAT_DBG_ASSERT(Cc->CubicProbe.BytesInFlight >= NumRetransmittableBytes); Cc->CubicProbe.BytesInFlight -= NumRetransmittableBytes; return CubicProbeCongestionControlUpdateBlockedState(Cc, CanSend); }
// _IRQL_requires_max_(DISPATCH_LEVEL) void CubicProbeCongestionControlOnEcn(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent) { UNREFERENCED_PARAMETER(EcnEvent); CubicProbeCongestionControlOnCongestionEvent(Cc, FALSE, TRUE); }
// _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); return FALSE; }
// void CubicProbeCongestionControlLogOutFlowStatus(_In_ const QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); }
// uint32_t CubicProbeCongestionControlGetBytesInFlightMax(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.BytesInFlightMax; }
// _IRQL_requires_max_(DISPATCH_LEVEL) uint8_t CubicProbeCongestionControlGetExemptions(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.Exemptions; }
// uint32_t CubicProbeCongestionControlGetCongestionWindow(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.CongestionWindow; }
// void CubicProbeCongestionControlGetNetworkStatistics(_In_ const QUIC_CONNECTION* const Connection, _In_ const QUIC_CONGESTION_CONTROL* const Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics) { const QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe; const QUIC_PATH* Path = &Connection->Paths[0]; NetworkStatistics->BytesInFlight = CubicProbe->BytesInFlight; NetworkStatistics->CongestionWindow = CubicProbe->CongestionWindow; NetworkStatistics->SmoothedRTT = Path->SmoothedRtt; NetworkStatistics->Bandwidth = (Path->SmoothedRtt > 0) ? (CubicProbe->CongestionWindow * 1000000ull / Path->SmoothedRtt) : 0; }
// _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlIsAppLimited(const QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); return FALSE; }
// _IRQL_requires_max_(DISPATCH_LEVEL) void CubicProbeCongestionControlSetAppLimited(QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); }
// _IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlUpdateBlockedState(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN PreviousCanSendState) { QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc); if (PreviousCanSendState != CubicProbeCongestionControlCanSend(Cc)) { if (PreviousCanSendState) { QuicConnAddOutFlowBlockedReason(Connection, QUIC_FLOW_BLOCKED_CONGESTION_CONTROL); } else { QuicConnRemoveOutFlowBlockedReason(Connection, QUIC_FLOW_BLOCKED_CONGESTION_CONTROL); } } return FALSE; }

// static const QUIC_CONGESTION_CONTROL QuicCongestionControlCubicProbe = {
//     .Name = "CubicProbe",
//     .QuicCongestionControlCanSend = CubicProbeCongestionControlCanSend,
//     .QuicCongestionControlSetExemption = CubicProbeCongestionControlSetExemption,
//     .QuicCongestionControlReset = CubicProbeCongestionControlReset,
//     .QuicCongestionControlGetSendAllowance = CubicProbeCongestionControlGetSendAllowance,
//     .QuicCongestionControlOnDataSent = CubicProbeCongestionControlOnDataSent,
//     .QuicCongestionControlOnDataInvalidated = CubicProbeCongestionControlOnDataInvalidated,
//     .QuicCongestionControlOnDataAcknowledged = CubicProbeCongestionControlOnDataAcknowledged,
//     .QuicCongestionControlOnDataLost = CubicProbeCongestionControlOnDataLost,
//     .QuicCongestionControlOnEcn = CubicProbeCongestionControlOnEcn,
//     .QuicCongestionControlOnSpuriousCongestionEvent = CubicProbeCongestionControlOnSpuriousCongestionEvent,
//     .QuicCongestionControlLogOutFlowStatus = NULL,
//     .QuicCongestionControlGetExemptions = CubicProbeCongestionControlGetExemptions,
//     .QuicCongestionControlGetBytesInFlightMax = CubicProbeCongestionControlGetBytesInFlightMax,
//     .QuicCongestionControlIsAppLimited = CubicProbeCongestionControlIsAppLimited,
//     .QuicCongestionControlSetAppLimited = CubicProbeCongestionControlSetAppLimited,
//     .QuicCongestionControlGetCongestionWindow = CubicProbeCongestionControlGetCongestionWindow,
//     .QuicCongestionControlGetNetworkStatistics = CubicProbeCongestionControlGetNetworkStatistics
// };

// _IRQL_requires_max_(DISPATCH_LEVEL)
// void
// CubicProbeCongestionControlInitialize(
//     _In_ QUIC_CONGESTION_CONTROL* Cc,
//     _In_ const QUIC_SETTINGS_INTERNAL* Settings
//     )
// {
//     *Cc = QuicCongestionControlCubicProbe;
//     QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
//     CubicProbe->SendIdleTimeoutMs = Settings->SendIdleTimeoutMs;
//     CubicProbe->InitialWindowPackets = Settings->InitialWindowPackets;
//     CubicProbeCongestionControlReset(Cc, TRUE);
// }









/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:
    This file implements the CubicProbe congestion control algorithm.
    This is the final, complete version with all interface functions implemented to prevent crashes.
--*/

#include "precomp.h"
#include <stdio.h> // For printf

// Constants for CUBIC
#define TEN_TIMES_BETA_CUBIC 7
#define TEN_TIMES_C_CUBIC 4

// Constants for the probing mechanism
#define PROBE_RTT_INTERVAL 5
#define PROBE_RTT_INCREASE_THRESHOLD 1.2

// Forward declarations of local functions
void CubicProbeCongestionControlOnCongestionEvent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN IsPersistentCongestion,
    _In_ BOOLEAN Ecn
    );
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
CubicProbeCongestionControlUpdateBlockedState(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN PreviousCanSendState
    );

// Helper function to reset the probe state.
_IRQL_requires_max_(DISPATCH_LEVEL)
void
CubicProbeResetProbeState(
    _In_ QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe
    )
{
    CubicProbe->ProbeState = PROBE_INACTIVE;
    CubicProbe->CumulativeSuccessLevel = 0;
    CubicProbe->RttCount = 0;
    CubicProbe->RttAtProbeStartUs = 0;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
CubicProbeCongestionControlCanSend(
    _In_ QUIC_CONGESTION_CONTROL* Cc
    )
{
    QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
    return CubicProbe->BytesInFlight < CubicProbe->CongestionWindow || CubicProbe->Exemptions > 0;
}


_IRQL_requires_max_(DISPATCH_LEVEL)
void
CubicProbeCongestionControlReset(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN FullReset
    )
{
    QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
    QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc);
    const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&Connection->Paths[0]);

    CubicProbe->SlowStartThreshold = UINT32_MAX;
    CubicProbe->IsInRecovery = FALSE;
    CubicProbe->HasHadCongestionEvent = FALSE;
    CubicProbe->CongestionWindow = DatagramPayloadLength * CubicProbe->InitialWindowPackets;
    CubicProbe->BytesInFlightMax = CubicProbe->CongestionWindow / 2;
    if (FullReset) {
        CubicProbe->BytesInFlight = 0;
    }
    CubicProbeResetProbeState(CubicProbe);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
CubicProbeCongestionControlOnDataAcknowledged(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ACK_EVENT* AckEvent
    )
{
    QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
    const uint64_t TimeNowUs = AckEvent->TimeNow;
    QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc);
    BOOLEAN PreviousCanSendState = CubicProbeCongestionControlCanSend(Cc);
    uint32_t BytesAcked = AckEvent->NumRetransmittableBytes;

    CXPLAT_DBG_ASSERT(CubicProbe->BytesInFlight >= BytesAcked);
    CubicProbe->BytesInFlight -= BytesAcked;

    if (CubicProbe->IsInRecovery) {
        if (AckEvent->LargestAck > CubicProbe->RecoverySentPacketNumber) {
            CubicProbe->IsInRecovery = FALSE;
            CubicProbe->TimeOfCongAvoidStart = TimeNowUs;
        }
        goto Exit;
    }

    if (CubicProbe->CongestionWindow >= CubicProbe->SlowStartThreshold && AckEvent->MinRttValid) {
        switch (CubicProbe->ProbeState) {
        case PROBE_INACTIVE:
            if (CubicProbe->WindowMax > 0 && CubicProbe->CongestionWindow >= CubicProbe->WindowMax) {
                CubicProbe->RttCount++;
                if (CubicProbe->RttCount >= PROBE_RTT_INTERVAL) {
                    CubicProbe->ProbeState = PROBE_TEST;
                    CubicProbe->CumulativeSuccessLevel = 1;
                    CubicProbe->RttAtProbeStartUs = AckEvent->MinRtt;
                    CubicProbe->RttCount = 0;
                    printf("[SERVER-CubicProbe] Probe Start -> TEST. RTT_base=%llu us, CWND=%u\n", (unsigned long long)CubicProbe->RttAtProbeStartUs, CubicProbe->CongestionWindow);
                }
            } else {
                CubicProbe->RttCount = 0;
            }
            break;
        case PROBE_TEST:
            CubicProbe->ProbeState = PROBE_JUDGMENT;
            // fallthrough
        case PROBE_JUDGMENT:
            if (CubicProbe->RttAtProbeStartUs > 0 && AckEvent->MinRtt <= (uint64_t)(CubicProbe->RttAtProbeStartUs * PROBE_RTT_INCREASE_THRESHOLD)) {
                CubicProbe->ProbeState = PROBE_TEST;
                CubicProbe->CumulativeSuccessLevel++;
                printf("[SERVER-CubicProbe] Probe Success -> TEST. Level=%u, NewRTT=%llu us, CWND=%u\n", CubicProbe->CumulativeSuccessLevel, (unsigned long long)AckEvent->MinRtt, CubicProbe->CongestionWindow);
            } else {
                printf("[SERVER-CubicProbe] Probe Failed -> INACTIVE. NewRTT=%llu us, BaseRTT=%llu us\n", (unsigned long long)AckEvent->MinRtt, (unsigned long long)CubicProbe->RttAtProbeStartUs);
                CubicProbeCongestionControlOnCongestionEvent(Cc, FALSE, FALSE);
            }
            break;
        }
    }

    // Slow Start
    if (CubicProbe->CongestionWindow < CubicProbe->SlowStartThreshold) {
        CubicProbe->CongestionWindow += BytesAcked;
    } else { // Congestion Avoidance
        const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&Connection->Paths[0]);
        if (CubicProbe->ProbeState == PROBE_TEST && CubicProbe->CumulativeSuccessLevel > 0) {
            uint32_t PrevCwnd = CubicProbe->CongestionWindow;
            uint32_t GrowthInBytes = (uint32_t)(DatagramPayloadLength * (CubicProbe->CumulativeSuccessLevel / 2.0 + 1.0));
            CubicProbe->CongestionWindow += GrowthInBytes;
            printf("[SERVER-CubicProbe] Aggressive Growth! CWND: %u -> %u (+%u)\n", PrevCwnd, CubicProbe->CongestionWindow, GrowthInBytes);
        } else {
            // Standard simplified growth
            if (CubicProbe->CongestionWindow > 0) {
                CubicProbe->CongestionWindow += (DatagramPayloadLength * DatagramPayloadLength) / CubicProbe->CongestionWindow;
            }
        }
    }

Exit:
    return CubicProbeCongestionControlUpdateBlockedState(Cc, PreviousCanSendState);
}


_IRQL_requires_max_(DISPATCH_LEVEL)
void
CubicProbeCongestionControlOnCongestionEvent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN IsPersistentCongestion,
    _In_ BOOLEAN Ecn
    )
{
    UNREFERENCED_PARAMETER(IsPersistentCongestion);
    UNREFERENCED_PARAMETER(Ecn);
    QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
    const uint16_t DatagramPayloadLength = QuicPathGetDatagramPayloadSize(&QuicCongestionControlGetConnection(Cc)->Paths[0]);

    CubicProbe->WindowMax = CubicProbe->CongestionWindow;
    CubicProbe->SlowStartThreshold = (CubicProbe->CongestionWindow * TEN_TIMES_BETA_CUBIC) / 10;
    CubicProbe->CongestionWindow = CXPLAT_MAX(CubicProbe->SlowStartThreshold, DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS);

    printf("[SERVER-CubicProbe] Congestion Event! New CWND = %u, SSThresh = %u\n",
        CubicProbe->CongestionWindow, CubicProbe->SlowStartThreshold);
    
    CubicProbeResetProbeState(CubicProbe);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
CubicProbeCongestionControlOnDataLost(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_LOSS_EVENT* LossEvent
    )
{
    QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;

    printf("[SERVER-CubicProbe] Packet Loss Event! LostBytes=%u\n", LossEvent->NumRetransmittableBytes);

    if (!CubicProbe->HasHadCongestionEvent || LossEvent->LargestPacketNumberLost > CubicProbe->RecoverySentPacketNumber) {
        CubicProbe->RecoverySentPacketNumber = LossEvent->LargestSentPacketNumber;
        CubicProbeCongestionControlOnCongestionEvent(Cc, LossEvent->PersistentCongestion, FALSE);
    }

    CXPLAT_DBG_ASSERT(CubicProbe->BytesInFlight >= LossEvent->NumRetransmittableBytes);
    CubicProbe->BytesInFlight -= LossEvent->NumRetransmittableBytes;

    CubicProbeCongestionControlUpdateBlockedState(Cc, CubicProbeCongestionControlCanSend(Cc));
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
CubicProbeCongestionControlSetExemption(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint8_t NumPackets
    )
{
    Cc->CubicProbe.Exemptions = NumPackets;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
uint32_t CubicProbeCongestionControlGetSendAllowance(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid) { UNREFERENCED_PARAMETER(TimeSinceLastSend); UNREFERENCED_PARAMETER(TimeSinceLastSendValid); QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe; if (CubicProbe->BytesInFlight >= CubicProbe->CongestionWindow) return 0; return CubicProbe->CongestionWindow - CubicProbe->BytesInFlight; }
_IRQL_requires_max_(PASSIVE_LEVEL) void CubicProbeCongestionControlOnDataSent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes) { BOOLEAN CanSend = CubicProbeCongestionControlCanSend(Cc); Cc->CubicProbe.BytesInFlight += NumRetransmittableBytes; if (Cc->CubicProbe.BytesInFlightMax < Cc->CubicProbe.BytesInFlight) { Cc->CubicProbe.BytesInFlightMax = Cc->CubicProbe.BytesInFlight; } if (Cc->CubicProbe.Exemptions > 0) { --Cc->CubicProbe.Exemptions; } CubicProbeCongestionControlUpdateBlockedState(Cc, CanSend); }
_IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlOnDataInvalidated( _In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes) { BOOLEAN CanSend = CubicProbeCongestionControlCanSend(Cc); CXPLAT_DBG_ASSERT(Cc->CubicProbe.BytesInFlight >= NumRetransmittableBytes); Cc->CubicProbe.BytesInFlight -= NumRetransmittableBytes; return CubicProbeCongestionControlUpdateBlockedState(Cc, CanSend); }
_IRQL_requires_max_(DISPATCH_LEVEL) void CubicProbeCongestionControlOnEcn(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent) { UNREFERENCED_PARAMETER(EcnEvent); CubicProbeCongestionControlOnCongestionEvent(Cc, FALSE, TRUE); }
_IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); return FALSE; }

// ---▼▼▼ [수정된 부분] LogOutFlowStatus 함수 구현 추가 ▼▼▼---
void
CubicProbeCongestionControlLogOutFlowStatus(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
    )
{
    UNREFERENCED_PARAMETER(Cc);
    // This can be left empty for printf-based logging, but it must exist.
}
// ---▲▲▲ [여기까지 수정] ▲▲▲---

uint32_t CubicProbeCongestionControlGetBytesInFlightMax(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.BytesInFlightMax; }
_IRQL_requires_max_(DISPATCH_LEVEL) uint8_t CubicProbeCongestionControlGetExemptions(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.Exemptions; }
uint32_t CubicProbeCongestionControlGetCongestionWindow(_In_ const QUIC_CONGESTION_CONTROL* Cc) { return Cc->CubicProbe.CongestionWindow; }
void CubicProbeCongestionControlGetNetworkStatistics(_In_ const QUIC_CONNECTION* const Connection, _In_ const QUIC_CONGESTION_CONTROL* const Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics) { const QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe; const QUIC_PATH* Path = &Connection->Paths[0]; NetworkStatistics->BytesInFlight = CubicProbe->BytesInFlight; NetworkStatistics->CongestionWindow = CubicProbe->CongestionWindow; NetworkStatistics->SmoothedRTT = Path->SmoothedRtt; NetworkStatistics->Bandwidth = (Path->SmoothedRtt > 0) ? (CubicProbe->CongestionWindow * 1000000ull / Path->SmoothedRtt) : 0; }
_IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlIsAppLimited(const QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); return FALSE; }
_IRQL_requires_max_(DISPATCH_LEVEL) void CubicProbeCongestionControlSetAppLimited(QUIC_CONGESTION_CONTROL* Cc) { UNREFERENCED_PARAMETER(Cc); }
_IRQL_requires_max_(DISPATCH_LEVEL) BOOLEAN CubicProbeCongestionControlUpdateBlockedState(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN PreviousCanSendState) { QUIC_CONNECTION* Connection = QuicCongestionControlGetConnection(Cc); if (PreviousCanSendState != CubicProbeCongestionControlCanSend(Cc)) { if (PreviousCanSendState) { QuicConnAddOutFlowBlockedReason(Connection, QUIC_FLOW_BLOCKED_CONGESTION_CONTROL); } else { QuicConnRemoveOutFlowBlockedReason(Connection, QUIC_FLOW_BLOCKED_CONGESTION_CONTROL); } } return FALSE; }

static const QUIC_CONGESTION_CONTROL QuicCongestionControlCubicProbe = {
    .Name = "CubicProbe",
    .QuicCongestionControlCanSend = CubicProbeCongestionControlCanSend,
    .QuicCongestionControlSetExemption = CubicProbeCongestionControlSetExemption,
    .QuicCongestionControlReset = CubicProbeCongestionControlReset,
    .QuicCongestionControlGetSendAllowance = CubicProbeCongestionControlGetSendAllowance,
    .QuicCongestionControlOnDataSent = CubicProbeCongestionControlOnDataSent,
    .QuicCongestionControlOnDataInvalidated = CubicProbeCongestionControlOnDataInvalidated,
    .QuicCongestionControlOnDataAcknowledged = CubicProbeCongestionControlOnDataAcknowledged,
    .QuicCongestionControlOnDataLost = CubicProbeCongestionControlOnDataLost,
    .QuicCongestionControlOnEcn = CubicProbeCongestionControlOnEcn,
    .QuicCongestionControlOnSpuriousCongestionEvent = CubicProbeCongestionControlOnSpuriousCongestionEvent,
    .QuicCongestionControlLogOutFlowStatus = CubicProbeCongestionControlLogOutFlowStatus, // <--- NULL이 아닌 함수 포인터로 수정
    .QuicCongestionControlGetExemptions = CubicProbeCongestionControlGetExemptions,
    .QuicCongestionControlGetBytesInFlightMax = CubicProbeCongestionControlGetBytesInFlightMax,
    .QuicCongestionControlIsAppLimited = CubicProbeCongestionControlIsAppLimited,
    .QuicCongestionControlSetAppLimited = CubicProbeCongestionControlSetAppLimited,
    .QuicCongestionControlGetCongestionWindow = CubicProbeCongestionControlGetCongestionWindow,
    .QuicCongestionControlGetNetworkStatistics = CubicProbeCongestionControlGetNetworkStatistics
};

_IRQL_requires_max_(DISPATCH_LEVEL)
void
CubicProbeCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings
    )
{
    *Cc = QuicCongestionControlCubicProbe;
    QUIC_CONGESTION_CONTROL_CUBICPROBE* CubicProbe = &Cc->CubicProbe;
    CubicProbe->SendIdleTimeoutMs = Settings->SendIdleTimeoutMs;
    CubicProbe->InitialWindowPackets = Settings->InitialWindowPackets;
    CubicProbeCongestionControlReset(Cc, TRUE);
}