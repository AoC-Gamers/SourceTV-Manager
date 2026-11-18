// SPDX-License-Identifier: GPL-3.0-only
#include "recording_sessions.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace recording = sourcetv::recording;
using recording::StartResult;

static void Check(bool condition, const char* description)
{
	if (!condition) {
		std::fprintf(stderr, "recording_sessions: %s\n", description);
		std::exit(1);
	}
}

struct Port {
	bool active = true;
	bool master = true;
	bool recording = false;
	bool acceptStart = true;
	bool acceptStop = true;
	bool allowPath = true;
	bool continuous = false;
	int starts = 0;
	int stops = 0;
	std::string filename;
	std::string alternatePath;
	std::function<void()> onStart;

	bool IsActive() const { return active; }
	bool IsMaster() const { return master; }
	bool IsRecording() const { return recording; }
	const char* FileName() const { return filename.c_str(); }
	int Instance() const { return 7; }
	int Tick() const { return 42; }
	bool CanUsePath(const char*) const { return allowPath; }
	bool HasDemoSuffix(const char* path) const
	{
		const auto size = std::strlen(path);
		return size >= 4 && (Equal(path + size - 4, ".dem") || Equal(path + size - 4, ".DEM"));
	}
	bool Equal(const char* left, const char* right) const { return std::strcmp(left, right) == 0; }

	void Start(const char* path, bool keep)
	{
		++starts;
		continuous = keep;
		recording = acceptStart;
		filename = alternatePath.empty() ? path : alternatePath;
		if (onStart)
			onStart();
	}

	void Stop()
	{
		++stops;
		if (acceptStop) {
			recording = false;
			filename.clear();
		}
	}
};

int main()
{
	recording::Sessions state;
	Port port;
	const recording::Plugin owner{&state, 4};
	const recording::Plugin reloaded{&state, 5};
	const recording::Plugin different{&port, 4};
	char path[12];
	auto start = [&](const char* name = "match", bool managed = true) {
		return recording::Start(state, port, &port, owner, managed, true, name, path, sizeof(path));
	};

	const auto unavailable = recording::ReadStatus(state, port, nullptr, owner, true);
	Check(!unavailable.available && !unavailable.current.active && !unavailable.token &&
		unavailable.current.instance == -1 && unavailable.current.tick == -1 && unavailable.current.filename.empty(),
		"snapshot defaults without a recorder");

	Check(start().result == StartResult::RecorderUnavailable, "unregistered recorder");
	state.Attach(&port);
	port.active = false;
	Check(start().result == StartResult::ServerInactive, "inactive server");
	port.active = true;
	port.master = false;
	Check(start().result == StartResult::NotMaster, "relay");
	port.master = true;
	port.recording = true;
	Check(start().result == StartResult::AlreadyRecording, "already active");
	port.recording = false;
	Check(start("../match").result == StartResult::InvalidPath, "invalid path");
	Check(start("12345678").result == StartResult::InvalidPath, "reject owned truncation");
	port.allowPath = false;
	Check(start().result == StartResult::InvalidPath && port.starts == 0, "path collision before effects");
	port.allowPath = true;

	auto first = start("1234567.DEM");
	Check(first.result == StartResult::Started && first.token > 0 && port.continuous, "confirmed start");
	Check(port.filename == "1234567.DEM", "preserve suffix with exact capacity");
	const auto ownSnapshot = recording::ReadStatus(state, port, &port, owner, true);
	const auto otherSnapshot = recording::ReadStatus(state, port, &port, different, false);
	Check(ownSnapshot.available && ownSnapshot.current.active && ownSnapshot.current.instance == 7 &&
		ownSnapshot.current.tick == 42 && ownSnapshot.current.filename == "1234567.DEM" && ownSnapshot.token == first.token,
		"active snapshot carries confirmed file and owner's token");
	Check(otherSnapshot.current.active && otherSnapshot.current.filename.empty() && otherSnapshot.token == 0,
		"snapshot does not expose another plugin's token");

	Check(!state.Owns(different, &port, first.token) && !state.Owns(reloaded, &port, first.token), "token isolation");
	Check(!recording::Stop(state, port, &port, different, 0, false) && port.stops == 0, "legacy cannot stop another owner's file");
	port.acceptStop = false;
	Check(!recording::Stop(state, port, &port, owner, first.token, true) && port.stops == 1, "engine stayed active");
	state.Revoke(reloaded);
	Check(state.Token(&port) == first.token, "wrong serial cannot revoke");
	state.Revoke(owner);
	Check(!state.Token(&port) && port.recording, "revocation leaves recording running");
	port.acceptStop = true;
	Check(recording::Stop(state, port, &port, different, 0, false), "legacy stops orphan");

	port.acceptStart = false;
	Check(start().result == StartResult::StartFailed && !state.Token(&port), "engine failed start");
	port.acceptStart = true;
	port.alternatePath = "other.dem";
	const int beforeMismatch = port.stops;
	Check(start().result == StartResult::StartFailed && port.stops == beforeMismatch + 1, "wrong path rolls back own start");
	port.alternatePath.clear();

	// The L4D2 engine invokes StopRecording on the inactive recorder before
	// opening the new file. That internal stop must not invalidate the start.
	port.onStart = [&] {
		state.BeginStart(&port, {});
		const auto beforeCleanup = state.Current(&port);
		state.BeginClose(&port, {});
		Check(state.CloseStillCurrent(&port, beforeCleanup), "inactive stop can call the original");
		Check(!state.EndClose(&port, false, nullptr, [&](const char* a, const char* b) { return port.Equal(a, b); }),
			"inactive cleanup emits no close");
		const auto opening = state.EndStart(&port);
		Check(opening && state.Current(&port) == opening->revision, "internal inactive stop keeps the start revision");
		state.ConfirmNewFile(&port, opening->revision);
	};
	const auto cleanedStart = start();
	Check(cleanedStart.result == StartResult::Started && cleanedStart.token > first.token,
		"start retains ownership after inactive cleanup");
	Check(recording::Stop(state, port, &port, owner, cleanedStart.token, true), "owner can stop after inactive cleanup");
	state.Release(&port, cleanedStart.token);

	port.onStart = [&] {
		state.BeginStart(&port, {});
		state.EndStart(&port);
		state.BeginClose(&port, {true, 2, 30, "match.dem"});
		state.EndClose(&port, false, nullptr, [&](const char* a, const char* b) { return port.Equal(a, b); });
		state.BeginStart(&port, {});
		auto nested = state.EndStart(&port);
		state.ConfirmNewFile(&port, nested->revision);
		Check(state.Reserve(different, &port) > first.token, "replacement receives a fresh token");
		port.filename = "match.dem";
	};
	const int beforeNested = port.stops;
	Check(start().result == StartResult::StartFailed && port.stops == beforeNested && port.recording,
		"nested same-name recording survives outer rollback");
	Check(state.Owns(different, &port, state.Token(&port)), "outer rollback cannot release the replacement token");
	port.onStart = {};

	auto equals = [&](const char* a, const char* b) { return port.Equal(a, b); };
	state.BeginClose(&port, {true, 7, 80, "last.dem"});
	Check(!state.EndClose(&port, true, "last.dem", equals), "no close while same file active");
	const auto beforeClose = state.Current(&port);
	state.BeginClose(&port, {true, 7, 81, "last.dem"});
	Check(state.CloseStillCurrent(&port, beforeClose), "current file may be closed");
	state.BeginClose(&port, {true, 7, 82, "last.dem"});
	auto closed = state.EndClose(&port, false, nullptr, equals);
	Check(closed && closed->instance == 7 && closed->tick == 82 && closed->filename == "last.dem", "pre-close snapshot survives nested stop");
	Check(!state.CloseStillCurrent(&port, beforeClose), "nested close invalidates outer original call");
	Check(!state.EndClose(&port, false, nullptr, equals) && !state.Token(&port), "no duplicate close");
	Check(!state.EndClose(&port, false, nullptr, equals), "post without pre");

	const auto oldLifetime = state.Current(&port);
	state.BeginClose(&port, {true, 7, 83, "old.dem"});
	state.Forget(&port);
	state.Attach(&port);
	Check(!state.StartStillCurrent(&port, oldLifetime) && !state.EndClose(&port, false, nullptr, equals),
		"reused address has a fresh lifetime");
	port.recording = false;
	Check(start("12345678", false).result == StartResult::Started && port.filename == "12345678.de", "legacy truncation");
	port.recording = false;
	port.onStart = [&] {
		state.BeginStart(&port, {});
		state.EndStart(&port);
		state.BeginStart(&port, {});
		auto inner = state.EndStart(&port);
		state.ConfirmNewFile(&port, inner->revision);
	};
	const int beforeLegacyNested = port.stops;
	Check(start("match", false).result == StartResult::StartFailed && port.recording && port.stops == beforeLegacyNested,
		"legacy start also rejects a superseded operation");

	recording::Sessions lastToken(INT_MAX);
	lastToken.Attach(&port);
	Check(lastToken.Reserve(owner, &port) == INT_MAX, "last positive token");
	lastToken.Revoke(owner);
	Check(!lastToken.Reserve(owner, &port), "token exhaustion without reuse");
	std::printf("recording_sessions: checks passed (%zu-bit)\n", sizeof(void*) * CHAR_BIT);
}
