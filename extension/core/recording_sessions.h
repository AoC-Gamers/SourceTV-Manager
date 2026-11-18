// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../natives/recording_path.h"

namespace sourcetv::recording {

enum class StartResult : int32_t {
	Started = 0, RecorderUnavailable = 1, ServerInactive = 2, NotMaster = 3,
	AlreadyRecording = 4, InvalidPath = 5, StartFailed = 6, OwnerTokenUnavailable = 7
};

struct Plugin {
	const void* identity;
	unsigned serial;

	bool operator==(const Plugin& other) const
	{
		return identity == other.identity && serial == other.serial;
	}
};

struct Revision {
	uint64_t lifetime = 0;
	uint64_t sequence = 0;
	bool start = false;

	bool operator==(const Revision& other) const
	{
		return lifetime == other.lifetime && sequence == other.sequence;
	}
};

struct Snapshot {
	bool active = false;
	int instance = -1;
	int tick = -1;
	std::string filename;
};

class Sessions {
public:
	explicit Sessions(uint32_t firstToken = 1) : nextToken_(firstToken) {}

	void Attach(const void* recorder)
	{
		if (recorder && entries_.find(recorder) == entries_.end()) {
			Entry entry;
			entry.revision.lifetime = ++lastLifetime_;
			entries_.emplace(recorder, std::move(entry));
		}
	}

	void Forget(const void* recorder)
	{
		if (owner_.recorder == recorder)
			owner_ = {};
		entries_.erase(recorder);
	}

	void Reset()
	{
		owner_ = {};
		entries_.clear();
	}

	Revision Current(const void* recorder) const
	{
		auto it = entries_.find(recorder);
		return it == entries_.end() ? Revision{} : it->second.revision;
	}

	bool StartStillCurrent(const void* recorder, Revision before) const
	{
		const Revision after = Current(recorder);
		return before.lifetime && before.lifetime == after.lifetime &&
			(after.sequence == before.sequence ||
			 (after.start && after.sequence == before.sequence + 1));
	}

	int32_t Reserve(Plugin plugin, const void* recorder)
	{
		if (!plugin.identity || !Current(recorder).lifetime || owner_.token ||
			!nextToken_ || nextToken_ > (std::numeric_limits<int32_t>::max)())
			return 0;
		owner_ = {plugin, recorder, static_cast<int32_t>(nextToken_++)};
		return owner_.token;
	}

	bool Owns(Plugin plugin, const void* recorder, int32_t token) const
	{
		return token > 0 && owner_.token == token && owner_.recorder == recorder && owner_.plugin == plugin;
	}

	int32_t Token(const void* recorder) const
	{
		return owner_.recorder == recorder ? owner_.token : 0;
	}

	void Release(const void* recorder, int32_t token)
	{
		if (token && Token(recorder) == token)
			owner_ = {};
	}

	void Revoke(Plugin plugin)
	{
		if (plugin.identity && owner_.plugin == plugin)
			owner_ = {};
	}

	void BeginStart(const void* recorder, Snapshot before)
	{
		auto it = entries_.find(recorder);
		if (it == entries_.end())
			return;
		auto& entry = it->second;
		Advance(entry, true);
		entry.starts.push_back({entry.revision, std::move(before)});
	}

	struct StartFrame {
		Revision revision;
		Snapshot before;
	};

	std::optional<StartFrame> EndStart(const void* recorder)
	{
		auto it = entries_.find(recorder);
		if (it == entries_.end() || it->second.starts.empty())
			return {};
		auto& stack = it->second.starts;
		auto frame = std::move(stack.back());
		stack.pop_back();
		return frame;
	}

	void ConfirmNewFile(const void* recorder, Revision revision)
	{
		auto it = entries_.find(recorder);
		if (it != entries_.end() && it->second.revision == revision)
			++it->second.file;
	}

	bool HasPendingClose(const void* recorder) const
	{
		auto it = entries_.find(recorder);
		return it != entries_.end() && !it->second.closes.empty();
	}

	void BeginClose(const void* recorder, Snapshot before)
	{
		auto it = entries_.find(recorder);
		if (it == entries_.end())
			return;
		auto& entry = it->second;
		// StartRecording calls StopRecording before opening the new file.
		// Keep the callback frame without superseding the start if no file is open.
		if (before.active)
			Advance(entry, false);
		entry.closes.push_back({entry.file, Token(recorder), false, std::move(before)});
	}

	bool CloseStillCurrent(const void* recorder, Revision before) const
	{
		auto it = entries_.find(recorder);
		if (it == entries_.end() || it->second.revision.lifetime != before.lifetime ||
			it->second.closes.empty())
			return false;
		const auto& entry = it->second;
		const auto& pending = entry.closes.back();
		return !pending.handled && pending.file == entry.file;
	}

	template <class Equal>
	std::optional<Snapshot> EndClose(const void* recorder, bool active, const char* filename, Equal equal)
	{
		auto it = entries_.find(recorder);
		if (it == entries_.end() || it->second.closes.empty())
			return {};
		auto& entry = it->second;
		auto frame = std::move(entry.closes.back());
		entry.closes.pop_back();
		if (!frame.before.active || frame.handled ||
			(active && entry.file == frame.file && (!filename || equal(frame.before.filename.c_str(), filename))))
			return {};

		for (auto& pending : entry.closes) {
			if (pending.file == frame.file)
				pending.handled = true;
		}
		if (!active || entry.file != frame.file)
			Release(recorder, frame.token);
		return std::move(frame.before);
	}

private:
	struct CloseFrame {
		uint64_t file;
		int32_t token;
		bool handled;
		Snapshot before;
	};
	struct Entry {
		Revision revision;
		uint64_t file = 0;
		std::vector<StartFrame> starts;
		std::vector<CloseFrame> closes;
	};
	struct Owner {
		Plugin plugin{};
		const void* recorder = nullptr;
		int32_t token = 0;
	};

	static void Advance(Entry& entry, bool start)
	{
		++entry.revision.sequence;
		entry.revision.start = start;
	}

	std::unordered_map<const void*, Entry> entries_;
	Owner owner_;
	uint64_t lastLifetime_ = 0;
	uint32_t nextToken_;
};

struct RecordingStatus {
	bool available = false;
	Snapshot current;
	int32_t token = 0;
};

template <class Port>
RecordingStatus ReadStatus(const Sessions& sessions, Port& port, const void* recorder, Plugin plugin, bool readFilename)
{
	RecordingStatus status;
	if (!recorder)
		return status;

	status.available = true;
	status.current.instance = port.Instance();
	status.current.active = port.IsRecording();
	if (!status.current.active)
		return status;

	status.current.tick = port.Tick();
	if (readFilename) {
		const char* filename = port.FileName();
		if (filename)
			status.current.filename = filename;
	}
	const int32_t token = sessions.Token(recorder);
	if (sessions.Owns(plugin, recorder, token))
		status.token = token;
	return status;
}

inline bool PreparePath(const char* input, bool hasSuffix, bool allowTruncation, char* path, size_t capacity)
{
	if (!capacity || !IsValidPath(input))
		return false;
	const char* suffix = hasSuffix ? "" : ".dem";
	const auto length = std::strlen(input);
	if (!allowTruncation && (length >= capacity || std::strlen(suffix) >= capacity - length))
		return false;

	const size_t available = capacity - 1;
	const size_t prefix = length < available ? length : available;
	const size_t suffixLength = std::strlen(suffix);
	const size_t suffixBytes = suffixLength < available - prefix ? suffixLength : available - prefix;
	std::memcpy(path, input, prefix);
	std::memcpy(path + prefix, suffix, suffixBytes);
	path[prefix + suffixBytes] = '\0';
	return true;
}

struct StartOutcome {
	StartResult result;
	int32_t token = 0;
};

template <class Port>
StartOutcome Start(Sessions& sessions, Port& port, const void* recorder, Plugin plugin, bool owned,
	bool continuous, const char* input, char* path, size_t capacity)
{
	const Revision before = sessions.Current(recorder);
	if (!before.lifetime)
		return {StartResult::RecorderUnavailable};
	if (!port.IsActive())
		return {StartResult::ServerInactive};
	if (!port.IsMaster())
		return {StartResult::NotMaster};
	if (port.IsRecording())
		return {StartResult::AlreadyRecording};
	if (!PreparePath(input, port.HasDemoSuffix(input), !owned, path, capacity) || !port.CanUsePath(path))
		return {StartResult::InvalidPath};

	const int32_t token = owned ? sessions.Reserve(plugin, recorder) : 0;
	if (owned && !token)
		return {StartResult::OwnerTokenUnavailable};

	port.Start(path, continuous);
	if (!sessions.StartStillCurrent(recorder, before) || (owned && !sessions.Owns(plugin, recorder, token))) {
		sessions.Release(recorder, token);
		return {StartResult::StartFailed};
	}

	const bool active = port.IsRecording();
	const char* actual = active ? port.FileName() : nullptr;
	if (!actual || !port.Equal(actual, path)) {
		if (active)
			port.Stop();
		sessions.Release(recorder, token);
		return {StartResult::StartFailed};
	}
	return {StartResult::Started, token};
}

template <class Port>
bool Stop(Sessions& sessions, Port& port, const void* recorder, Plugin plugin, int32_t token, bool owned)
{
	const Revision before = sessions.Current(recorder);
	if (!before.lifetime || !port.IsRecording())
		return false;
	const int32_t currentToken = sessions.Token(recorder);
	if ((owned && !sessions.Owns(plugin, recorder, token)) ||
		(!owned && currentToken && !sessions.Owns(plugin, recorder, currentToken)))
		return false;

	port.Stop();
	// The legacy native reports that it invoked Stop. The owned native reports
	// whether that recorder remains inactive; never dereference a replaced one.
	return !owned || (sessions.Current(recorder).lifetime == before.lifetime && !port.IsRecording());
}

} // namespace sourcetv::recording
