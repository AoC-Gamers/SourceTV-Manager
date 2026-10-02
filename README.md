# SourceTV Manager
Interface to interact with the SourceTV server from SourcePawn.

This is a [SourceMod](http://www.sourcemod.net/) extension providing an API for plugins to access the [SourceTV](https://developer.valvesoftware.com/wiki/SourceTV) server instance(s).
[Forum thread](https://forums.alliedmods.net/showthread.php?t=280402)

#### API
There are natives and forwards to access
  * Basic SourceTV information (stats, delay, active)
  * Interacting with spectators (chat/console messages, kick, ip, name, tv title)
  * Forcing camera shots on the director
  * Demo recording (filename, recording tick, print message to demo console)

Have a look at the [include file](scripting/include/sourcetvmanager.inc).

#### Recording lifecycle API

`SourceTV_StartRecordingEx(filename, continuous, ownerToken)` returns a
`SourceTVRecordingStartResult`: `Started`, `RecorderUnavailable`,
`ServerInactive`, `NotMaster`, `AlreadyRecording`, `InvalidPath`,
`StartFailed`, or `OwnerTokenUnavailable`. A successful call confirms
`IsRecording()` and the effective filename after the engine call. It also
returns a positive token bound to the calling plugin. The extension allocates
tokens monotonically and never reuses one during its lifetime.

Use `SourceTV_StopOwnedRecording(ownerToken)` to stop only a recording owned by
the caller. The older `SourceTV_StopRecording()` keeps its signature but refuses
to stop a managed recording owned by another plugin. Calls through engine
commands can still bypass this API. On plugin unload, the token is revoked; an
active demo is left running and appears ownerless in snapshots. A later manager
may stop that demo with the legacy native after checking the snapshot.

`SourceTV_GetRecordingSnapshot(...)` is a synchronous read of the selected
recorder. Its return value says whether the recorder is available. The output
fields describe its recording state, selected instance, current path, and tick.
Only the owning plugin receives its owner token; other plugins receive zero.
Unavailable output defaults are `false`, `0`, `-1`, empty path, and `-1` tick.

`SourceTV_OnRecorderAvailabilityChanged(instance, available)` reports recorder
appearance and removal. Late-loaded plugins must query the snapshot because
they may have missed the forward. `SourceTV_OnRecordingFileStarted` fires after
an engine start call returns with an active recorder and includes the engine
`continuous` argument. This reports calls; it does not guarantee automatic
L4D2 file rotation or continuity.

The existing `SourceTV_OnStopRecording` remains a pre-close event. The new
`SourceTV_OnRecordingFileClosed(instance, filename, finalTick)` fires after
`StopRecording` returns. Its path and tick are copied before the recorder closes
its internal file object. An engine crash cannot deliver this forward.
If StopRecording returns while the recorder still reports the same active
filename, the manager suppresses the close event. If the recorder becomes
inactive, ownership ends. It never assigns the old token to a later recording,
including across map transitions; L4D2 continuity and segment rotation need a
runtime smoke before they can be promised.

## Building

The extension targets Linux x86 for Left 4 Dead 2. See
[docs/BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md) for the local build layout and
commands:

```bash
make deps-linux
make build-linux
```

To build and verify the package in the pinned Docker toolchain instead, run
`make build-linux-container`. The container writes the same package layout
under `.build/linux-l4d2/` without connecting to a game server. See
[docs/BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md) for details.

#### Steam authentication for spectators
The extension adds a convar `tv_force_steamauth` (defaults to 0) which lets you enable steam validation on SourceTV clients.
Currently when this is enabled SourceTV relays are rejected.

#### `status` command fixes
There are several quirks around recording the `status` command output in a SourceTV demo.
While this extension is running, you're going to be able to do
```sourcepawn
FakeClientCommand(SourceTV_GetBotIndex(), "status");
```
and have the output recorded in the demo. This can help quickly identifying the players in the demo while playing it back.

Note: Doesn't run on relay proxies yet.
