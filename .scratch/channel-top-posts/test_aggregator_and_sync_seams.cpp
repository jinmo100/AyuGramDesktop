#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <sstream>

namespace TestSeams {

using ID = long long;

enum class TopPostMediaType : int {
	None = 0,
	Photo = 1,
	Video = 2,
	Document = 3,
	Grouped = 4,
};

struct MockTopPost {
	ID id = 0;
	ID channelId = 0;
	int messageId = 0;
	int date = 0;
	int reactionsCount = 0;
	int commentsCount = 0;
	int viewsCount = 0;
	std::string textSnippet;
	int mediaType = 0;
	std::string reactionsJson;
	int scanTime = 0;
	ID groupedId = 0;
	int groupedCount = 1;

	[[nodiscard]] TopPostMediaType type() const {
		return static_cast<TopPostMediaType>(mediaType);
	}
	void setType(TopPostMediaType t) {
		mediaType = static_cast<int>(t);
	}
};

std::map<std::string, int> ParseReactionsJson(const std::string &json) {
	std::map<std::string, int> result;
	if (json.empty() || json == "{}") {
		return result;
	}
	size_t pos = 0;
	while (pos < json.size()) {
		auto keyStart = json.find('"', pos);
		if (keyStart == std::string::npos) break;
		auto keyEnd = json.find('"', keyStart + 1);
		if (keyEnd == std::string::npos) break;
		std::string key = json.substr(keyStart + 1, keyEnd - keyStart - 1);

		auto colon = json.find(':', keyEnd);
		if (colon == std::string::npos) break;
		auto valStart = colon + 1;
		while (valStart < json.size() && (json[valStart] == ' ' || json[valStart] == '\t')) {
			++valStart;
		}
		auto valEnd = valStart;
		while (valEnd < json.size() && (isdigit(json[valEnd]) || json[valEnd] == '-')) {
			++valEnd;
		}
		if (valEnd > valStart) {
			int count = std::stoi(json.substr(valStart, valEnd - valStart));
			result[key] = count;
		}
		pos = valEnd;
	}
	return result;
}

std::string SerializeReactionsJson(const std::map<std::string, int> &reactions) {
	if (reactions.empty()) {
		return "{}";
	}
	std::string out = "{";
	bool first = true;
	for (const auto &[emoji, count] : reactions) {
		if (!first) out += ",";
		out += "\"" + emoji + "\":" + std::to_string(count);
		first = false;
	}
	out += "}";
	return out;
}

MockTopPost CoalesceMediaGroup(const std::vector<MockTopPost> &group) {
	assert(!group.empty());
	if (group.size() == 1) {
		auto post = group.front();
		if (post.groupedId != 0) {
			post.setType(TopPostMediaType::Grouped);
		}
		post.groupedCount = 1;
		return post;
	}

	auto result = group.front();
	result.setType(TopPostMediaType::Grouped);
	result.groupedCount = static_cast<int>(group.size());

	int totalReactions = 0;
	int maxComments = 0;
	int maxViews = 0;
	int minMessageId = group.front().messageId;
	int anchorDate = group.front().date;
	int latestScanTime = group.front().scanTime;
	std::string leadSnippet;
	std::map<std::string, int> aggregatedReactions;

	for (const auto &item : group) {
		totalReactions += item.reactionsCount;
		maxComments = std::max(maxComments, item.commentsCount);
		maxViews = std::max(maxViews, item.viewsCount);
		latestScanTime = std::max(latestScanTime, item.scanTime);

		if (item.messageId < minMessageId) {
			minMessageId = item.messageId;
			anchorDate = item.date;
		}

		if (leadSnippet.empty() && !item.textSnippet.empty()) {
			leadSnippet = item.textSnippet;
		}

		auto parsed = ParseReactionsJson(item.reactionsJson);
		for (const auto &[emoji, count] : parsed) {
			aggregatedReactions[emoji] += count;
		}
	}

	result.messageId = minMessageId;
	result.date = anchorDate;
	result.reactionsCount = totalReactions;
	result.commentsCount = maxComments;
	result.viewsCount = maxViews;
	result.scanTime = latestScanTime;
	result.textSnippet = leadSnippet;
	result.reactionsJson = SerializeReactionsJson(aggregatedReactions);

	return result;
}

std::vector<MockTopPost> AggregateMediaGroups(const std::vector<MockTopPost> &posts) {
	if (posts.empty()) {
		return {};
	}

	std::vector<MockTopPost> coalesced;
	std::vector<MockTopPost> currentGroup;
	ID currentGroupId = 0;

	for (const auto &post : posts) {
		if (post.groupedId != 0) {
			if (currentGroupId == 0) {
				currentGroupId = post.groupedId;
				currentGroup.push_back(post);
			} else if (currentGroupId == post.groupedId) {
				currentGroup.push_back(post);
			} else {
				coalesced.push_back(CoalesceMediaGroup(currentGroup));
				currentGroup.clear();
				currentGroupId = post.groupedId;
				currentGroup.push_back(post);
			}
		} else {
			if (!currentGroup.empty()) {
				coalesced.push_back(CoalesceMediaGroup(currentGroup));
				currentGroup.clear();
				currentGroupId = 0;
			}
			coalesced.push_back(post);
		}
	}

	if (!currentGroup.empty()) {
		coalesced.push_back(CoalesceMediaGroup(currentGroup));
	}

	return coalesced;
}

class MediaGroupAggregator {
public:
	std::vector<MockTopPost> feed(const std::vector<MockTopPost> &batch) {
		std::vector<MockTopPost> output;
		for (const auto &post : batch) {
			if (post.groupedId != 0) {
				if (_currentGroupId == 0) {
					_currentGroupId = post.groupedId;
					_pendingGroup.push_back(post);
				} else if (_currentGroupId == post.groupedId) {
					_pendingGroup.push_back(post);
				} else {
					output.push_back(CoalesceMediaGroup(_pendingGroup));
					_pendingGroup.clear();
					_currentGroupId = post.groupedId;
					_pendingGroup.push_back(post);
				}
			} else {
				if (!_pendingGroup.empty()) {
					output.push_back(CoalesceMediaGroup(_pendingGroup));
					_pendingGroup.clear();
					_currentGroupId = 0;
				}
				output.push_back(post);
			}
		}
		return output;
	}

	std::vector<MockTopPost> flush() {
		std::vector<MockTopPost> output;
		if (!_pendingGroup.empty()) {
			output.push_back(CoalesceMediaGroup(_pendingGroup));
			_pendingGroup.clear();
			_currentGroupId = 0;
		}
		return output;
	}

	void reset() {
		_pendingGroup.clear();
		_currentGroupId = 0;
	}

private:
	std::vector<MockTopPost> _pendingGroup;
	ID _currentGroupId = 0;
};

std::vector<std::vector<int>> ChunkMessageIds(const std::vector<int> &ids, size_t chunkSize = 100) {
	std::vector<std::vector<int>> chunks;
	if (ids.empty()) {
		return chunks;
	}
	for (size_t i = 0; i < ids.size(); i += chunkSize) {
		const auto end = std::min(i + chunkSize, ids.size());
		chunks.emplace_back(ids.begin() + i, ids.begin() + end);
	}
	return chunks;
}

void ReRankAndTruncate(std::vector<MockTopPost> &posts, size_t maxCount = 300) {
	std::sort(posts.begin(), posts.end(), [](const MockTopPost &a, const MockTopPost &b) {
		if (a.reactionsCount != b.reactionsCount) {
			return a.reactionsCount > b.reactionsCount;
		}
		if (a.commentsCount != b.commentsCount) {
			return a.commentsCount > b.commentsCount;
		}
		if (a.viewsCount != b.viewsCount) {
			return a.viewsCount > b.viewsCount;
		}
		return a.date > b.date;
	});

	if (posts.size() > maxCount) {
		posts.resize(maxCount);
	}
}

} // namespace TestSeams

int main() {
	using namespace TestSeams;

	// Test 1: Empty input
	{
		auto res = AggregateMediaGroups({});
		assert(res.empty());
		std::cout << "Test 1 (Empty input) passed.\n";
	}

	// Test 2: Single non-grouped messages pass through
	{
		MockTopPost p1;
		p1.messageId = 10;
		p1.groupedId = 0;
		p1.reactionsCount = 5;

		MockTopPost p2;
		p2.messageId = 9;
		p2.groupedId = 0;
		p2.reactionsCount = 8;

		auto res = AggregateMediaGroups({p1, p2});
		assert(res.size() == 2);
		assert(res[0].messageId == 10);
		assert(res[1].messageId == 9);
		std::cout << "Test 2 (Non-grouped passthrough) passed.\n";
	}

	// Test 3: Contiguous messages with same grouped_id coalesced
	{
		MockTopPost p1;
		p1.messageId = 103;
		p1.date = 1700000003;
		p1.groupedId = 8888;
		p1.reactionsCount = 10;
		p1.commentsCount = 2;
		p1.viewsCount = 100;
		p1.reactionsJson = "{\"👍\":10}";
		p1.textSnippet = "Album caption";

		MockTopPost p2;
		p2.messageId = 102;
		p2.date = 1700000002;
		p2.groupedId = 8888;
		p2.reactionsCount = 5;
		p2.commentsCount = 3;
		p2.viewsCount = 120;
		p2.reactionsJson = "{\"🔥\":5}";

		MockTopPost p3;
		p3.messageId = 101;
		p3.date = 1700000001;
		p3.groupedId = 8888;
		p3.reactionsCount = 7;
		p3.commentsCount = 1;
		p3.viewsCount = 110;
		p3.reactionsJson = "{\"👍\":3,\"❤️\":4}";

		auto res = AggregateMediaGroups({p1, p2, p3});
		assert(res.size() == 1);
		const auto &agg = res[0];

		// Preserves lead messageId (first/lead in set: min id = 101)
		assert(agg.messageId == 101);
		assert(agg.date == 1700000001);

		// Reactions aggregated: 10 + 5 + 7 = 22
		assert(agg.reactionsCount == 22);

		// Max comments and views preserved
		assert(agg.commentsCount == 3);
		assert(agg.viewsCount == 120);

		// Media type set to Grouped
		assert(agg.type() == TopPostMediaType::Grouped);

		// Text snippet preserved from item with caption
		assert(agg.textSnippet == "Album caption");
		assert(agg.groupedCount == 3);

		// Reactions JSON parsed and merged
		auto parsed = ParseReactionsJson(agg.reactionsJson);
		assert(parsed["👍"] == 13);
		assert(parsed["🔥"] == 5);
		assert(parsed["❤️"] == 4);

		std::cout << "Test 3 (Contiguous grouped coalescing & metrics aggregation) passed.\n";
	}

	// Test 4: Streaming aggregator across batch boundaries
	{
		MediaGroupAggregator aggregator;

		MockTopPost p1; p1.messageId = 200; p1.groupedId = 0;
		MockTopPost p2; p2.messageId = 199; p2.groupedId = 777; p2.reactionsCount = 10;
		MockTopPost p3; p3.messageId = 198; p3.groupedId = 777; p3.reactionsCount = 15;

		// Batch 1 ends with group 777
		auto out1 = aggregator.feed({p1, p2, p3});
		assert(out1.size() == 1);
		assert(out1[0].messageId == 200);

		// Batch 2 continues group 777 then normal post
		MockTopPost p4; p4.messageId = 197; p4.groupedId = 777; p4.reactionsCount = 5;
		MockTopPost p5; p5.messageId = 196; p5.groupedId = 0;

		auto out2 = aggregator.feed({p4, p5});
		assert(out2.size() == 2);
		// First item of out2 is coalesced group 777!
		assert(out2[0].messageId == 197); // min of 199, 198, 197
		assert(out2[0].reactionsCount == 30); // 10 + 15 + 5
		assert(out2[1].messageId == 196);

		auto out3 = aggregator.flush();
		assert(out3.empty());

		std::cout << "Test 4 (Streaming aggregator across batch boundaries) passed.\n";
	}

	// Test 5: Chunking cached IDs for messages.getMessages (max 100 per chunk)
	{
		std::vector<int> cached300;
		for (int i = 1; i <= 250; ++i) {
			cached300.push_back(i);
		}
		auto chunks = ChunkMessageIds(cached300, 100);
		assert(chunks.size() == 3);
		assert(chunks[0].size() == 100);
		assert(chunks[1].size() == 100);
		assert(chunks[2].size() == 50);
		assert(chunks[0].front() == 1);
		assert(chunks[0].back() == 100);
		assert(chunks[1].front() == 101);
		assert(chunks[2].back() == 250);

		std::cout << "Test 5 (Chunking cached IDs) passed.\n";
	}

	// Test 6: Re-ranking and truncation to Top 300
	{
		std::vector<MockTopPost> posts;
		for (int i = 1; i <= 350; ++i) {
			MockTopPost p;
			p.messageId = i;
			p.reactionsCount = i; // 1 to 350
			p.commentsCount = 0;
			p.viewsCount = 0;
			p.date = 1000 + i;
			posts.push_back(p);
		}

		ReRankAndTruncate(posts, 300);
		assert(posts.size() == 300);
		assert(posts.front().messageId == 350);
		assert(posts.front().reactionsCount == 350);
		assert(posts.back().messageId == 51);
		assert(posts.back().reactionsCount == 51);

		std::cout << "Test 6 (Re-ranking and Top 300 truncation) passed.\n";

	// Test 7: Incremental sync preserves media group status and aggregate counts
	{
		MockTopPost cached;
		cached.messageId = 101;
		cached.setType(TopPostMediaType::Grouped);
		cached.groupedId = 8888;
		cached.groupedCount = 3;
		cached.reactionsCount = 22;
		cached.commentsCount = 3;
		cached.viewsCount = 120;
		cached.reactionsJson = "{\"👍\":13,\"🔥\":5,\"❤️\":4}";

		// Incremental sync receives updated anchor message 101 with single-message metrics
		MockTopPost anchorUpdate;
		anchorUpdate.messageId = 101;
		anchorUpdate.setType(TopPostMediaType::Photo);
		anchorUpdate.groupedId = 8888;
		anchorUpdate.groupedCount = 1;
		anchorUpdate.reactionsCount = 12; // anchor alone grew
		anchorUpdate.commentsCount = 2;
		anchorUpdate.viewsCount = 130;
		anchorUpdate.reactionsJson = "{\"👍\":12}";

		// Apply sync preservation logic
		if (cached.type() == TopPostMediaType::Grouped) {
			anchorUpdate.setType(TopPostMediaType::Grouped);
			anchorUpdate.groupedId = cached.groupedId;
			anchorUpdate.groupedCount = cached.groupedCount;
			anchorUpdate.reactionsCount = std::max(cached.reactionsCount, anchorUpdate.reactionsCount);
			anchorUpdate.commentsCount = std::max(cached.commentsCount, anchorUpdate.commentsCount);
			anchorUpdate.viewsCount = std::max(cached.viewsCount, anchorUpdate.viewsCount);
		}

		assert(anchorUpdate.type() == TopPostMediaType::Grouped);
		assert(anchorUpdate.groupedCount == 3);
		assert(anchorUpdate.reactionsCount == 22);
		assert(anchorUpdate.viewsCount == 130);
		std::cout << "Test 7 (Incremental sync preserves media group status and aggregate counts) passed.\n";
	}
	}

	std::cout << "All aggregator and sync seam tests passed successfully!\n";
	return 0;
}
