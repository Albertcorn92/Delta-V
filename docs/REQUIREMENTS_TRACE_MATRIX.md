# Requirements Traceability Matrix

| Requirement | DAL | Status | Test Evidence | Other Evidence |
|---|---|---|---|---|
| DV-ARCH-01 | B | PASS | RateGroupExecutive.InitAllCallsComponentInit<br>Scheduler.RegisterAndInit | - |
| DV-ARCH-02 | B | PASS | ActiveComponent.ThreadLifecycleStartStop<br>ImuComponent.StepPublishesTelemetryOnReadSuccess | analysis:src/Os.hpp |
| DV-ARCH-03 | C | PASS | Architecture.SingletonServiceIdentity | analysis:docs/ARCHITECTURE.md<br>analysis:src/Requirements.hpp |
| DV-COMM-01 | B | PASS | Serializer.CRC16ArrayOverload<br>Types.PacketSizes | - |
| DV-COMM-02 | B | PASS | Cobs.DecodeFailsOnTruncated<br>Cobs.DelimiterIsLastByte<br>Cobs.NoZerosInEncodedData<br>Cobs.RoundtripShortPayload | - |
| DV-COMM-03 | A | PASS | TelemetryBridge.RejectsNonCanonicalFrameLength | - |
| DV-COMM-04 | A | PASS | TelemetryBridge.RejectsReplaySequence | - |
| DV-COMM-05 | B | PASS | TelemetryBridge.EnforcesMaxCommandsPerTick | - |
| DV-COMM-06 | A | PASS | TelemetryBridge.PersistsReplaySequenceAcrossRestart | - |
| DV-DATA-01 | A | PASS | ParamDb.ConcurrentReadWrite<br>ParamDb.IntegrityAfterWrite | - |
| DV-DATA-02 | B | PASS | TmrStore.MajorityVoteHealsOneSEU<br>TmrStore.WriteAndRead | - |
| DV-DATA-03 | B | PASS | TmrStore.TmrRegistryScrubAll<br>WdFixture.PeriodicTmrScrubRepairsSingleFault | - |
| DV-FDIR-01 | A | PASS | WdFixture.InjectBatteryLevel_v3<br>WdFixture.SafeModeOnLowBattery | - |
| DV-FDIR-02 | A | PASS | WdFixture.EmergencyOnVeryLowBattery | - |
| DV-FDIR-03 | B | PASS | WdFixture.DegradedFromWarning | - |
| DV-FDIR-04 | A | PASS | WdFixture.RestartsCriticalActiveComponent | - |
| DV-FDIR-05 | B | PASS | WdFixture.DegradedAutoRecovery | - |
| DV-FDIR-06 | B | PASS | WdFixture.HeartbeatAtCycle10 | - |
| DV-FDIR-07 | C | PASS | WdFixture.SchedulerHealthEmitsOnNewDrops<br>WdFixture.SchedulerHealthSilentWhenNoNewDrops | - |
| DV-FDIR-08 | B | PASS | CommandHubFixture.DuplicateRouteRegistrationIsRejected<br>CommandHubFixture.NackOnUnknownTarget<br>CommandHubFixture.NackWhenRouteQueueIsFull<br>CommandHubFixture.NullRouteRegistrationIsRejected | - |
| DV-FDIR-09 | B | PASS | WatchdogThresholds.InvalidThresholdConfigFallsBackToDefaults<br>WatchdogThresholds.UsesConfiguredBatteryThresholds | - |
| DV-LOG-01 | C | PASS | LoggerComponent.DrainsAll | analysis:src/LoggerComponent.hpp |
| DV-LOG-02 | C | PASS | LoggerComponent.EventFlushesImmediately<br>LoggerComponent.TelemetryFlushesAtInterval | analysis:src/LoggerComponent.hpp |
| DV-MEM-01 | A | PASS | HeapGuard.ArmAndIsArmed<br>HeapGuard.NewAfterArmTriggersFatal | - |
| DV-MEM-02 | A | PASS | MemorySafety.StaticBoundsAreNonZero | analysis:src/Port.hpp<br>analysis:src/Types.hpp |
| DV-MEM-03 | B | PASS | RingBuffer.DrainOverflowCount<br>RingBuffer.OverflowCounted | - |
| DV-OPS-01 | B | PASS | OtaComponent.SizeMismatchPreventsActivation<br>OtaComponent.StageWritesArtifactAndManifest<br>OtaComponent.VerifiesImageAndRequestsReboot | - |
| DV-OPS-02 | B | PASS | TimeService.UtcSyncHelpers<br>TimeSyncComponent.ApplySyncFromWordCommands | - |
| DV-OPS-03 | C | PASS | PlaybackComponent.LoadsAndReplaysHistoricalSamples | - |
| DV-OPS-04 | B | PASS | MemoryDwellComponent.PatchAndDwellAddressWord<br>MemoryDwellComponent.SampleNowEmitsTelemetry | - |
| DV-SEC-01 | A | PASS | TelemetryBridge.RejectsInvalidHeaderFields | - |
| DV-SEC-02 | B | PASS | CommandHubFixture.BlocksOperationalInSafeMode<br>MissionFsm.SafeModeBlocksOperational | - |
| DV-SEC-03 | A | PASS | TelemetryBridge.RejectsNonCanonicalFrameLength<br>TelemetryBridge.RejectsTruncatedFrame | - |
| DV-TIME-01 | B | PASS | ActiveComponent.ThreadLifecycleStartStop | analysis:src/Os.hpp |
| DV-TIME-02 | C | PASS | TimeService.OverflowWarningThreshold | - |
| DV-TMR-01 | A | PASS | TmrStore.MajorityVoteHealsOneSEU | - |
| DV-TMR-02 | B | PASS | TmrStore.AllThreeDifferIsNotSane | - |

