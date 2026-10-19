#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

namespace TestSeams {

int ParseFloodWaitSeconds(const std::string &errorType) {
	const std::string prefix1 = "FLOOD_WAIT_";
	const std::string prefix2 = "FLOOD_PREMIUM_WAIT_";
	if (errorType.rfind(prefix1, 0) == 0 || errorType.rfind(prefix2, 0) == 0) {
		const auto underscore = errorType.rfind('_');
		if (underscore != std::string::npos && underscore + 1 < errorType.size()) {
			try {
				const auto sec = std::stoi(errorType.substr(underscore + 1));
				return std::max(sec, 1);
			} catch (...) {
				return 1;
			}
		}
		return 1;
	}
	return 0;
}

int CalculateThrottleDelay(int minMs = 150, int maxMs = 250) {
	if (maxMs <= minMs) {
		return minMs;
	}
	static std::mt19937 gen(1337);
	std::uniform_int_distribution<int> dist(minMs, maxMs);
	return dist(gen);
}

int ClampScanWindow(int window, int minW = 1, int maxW = 5000) {
	return std::clamp(window, minW, maxW);
}

struct MockReaction {
	std::string emoji;
	int count = 0;
};

struct MockHistoryItem {
	int id = 0;
	int date = 0;
	std::vector<MockReaction> reactions;
	int repliesCount = 0;
	bool repliesAreComments = false;
	int viewsCount = 0;
	uint64_t groupId = 0;
	std::string text;
	std::string filename;
	int mediaType = 0; // 0=None, 1=Photo, 2=Video, 3=Document, 4=Grouped
};

struct MockTopPost {
	int channelId = 0;
	int messageId = 0;
	int date = 0;
	int reactionsCount = 0;
	int commentsCount = 0;
	int viewsCount = 0;
	std::string textSnippet;
	int mediaType = 0;
	std::string reactionsJson;
	int scanTime = 0;
	int64_t groupedId = 0;
};

MockTopPost ExtractMetrics(const MockHistoryItem &item, int channelId, int scanTime) {
	MockTopPost post;
	post.channelId = channelId;
	post.messageId = item.id;
	post.date = item.date;
	post.scanTime = scanTime;

	// Reactions
	int totalReactions = 0;
	std::string json = "{";
	bool first = true;
	for (const auto &r : item.reactions) {
		totalReactions += r.count;
		if (!first) json += ",";
		json += "\"" + r.emoji + "\":" + std::to_string(r.count);
		first = false;
	}
	json += "}";
	post.reactionsCount = totalReactions;
	post.reactionsJson = json;

	// Comments: strictly verify repliesAreComments
	post.commentsCount = item.repliesAreComments ? item.repliesCount : 0;
	post.viewsCount = item.viewsCount;

	// Grouping
	post.groupedId = static_cast<int64_t>(item.groupId);
	if (item.groupId != 0) {
		post.mediaType = 4; // Grouped
	} else {
		post.mediaType = item.mediaType;
	}

	// Snippet
	std::string snippet = item.text;
	if (snippet.empty() && !item.filename.empty()) {
		snippet = item.filename;
	}
	if (snippet.size() > 120) {
		snippet = snippet.substr(0, 120);
	}
	post.textSnippet = snippet;

	return post;
}

} // namespace TestSeams

int main() {
	using namespace TestSeams;

	// Test 1: ParseFloodWaitSeconds
	assert(ParseFloodWaitSeconds("FLOOD_WAIT_15") == 15);
	assert(ParseFloodWaitSeconds("FLOOD_PREMIUM_WAIT_30") == 30);
	assert(ParseFloodWaitSeconds("FLOOD_WAIT_0") == 1);
	assert(ParseFloodWaitSeconds("FLOOD_WAIT_invalid") == 1);
	assert(ParseFloodWaitSeconds("SEARCH_QUERY_EMPTY") == 0);
	assert(ParseFloodWaitSeconds("PEER_ID_INVALID") == 0);
	assert(ParseFloodWaitSeconds("") == 0);
	std::cout << "Test 1 (Flood wait parsing) passed.\n";

	// Test 2: CalculateThrottleDelay
	for (int i = 0; i < 500; ++i) {
		int delay = CalculateThrottleDelay(150, 250);
		assert(delay >= 150 && delay <= 250);
	}
	assert(CalculateThrottleDelay(200, 200) == 200);
	std::cout << "Test 2 (Throttle delay) passed.\n";

	// Test 3: ClampScanWindow
	assert(ClampScanWindow(500) == 500);
	assert(ClampScanWindow(1000) == 1000);
	assert(ClampScanWindow(2000) == 2000);
	assert(ClampScanWindow(5000) == 5000);
	assert(ClampScanWindow(6000) == 5000);
	assert(ClampScanWindow(-5) == 1);
	assert(ClampScanWindow(0) == 1);
	std::cout << "Test 3 (Scan window clamp) passed.\n";

	// Test 4: ExtractMetrics
	MockHistoryItem item;
	item.id = 101;
	item.date = 1700000000;
	item.reactions = {{"👍", 10}, {"🔥", 25}, {"❤️", 5}};
	item.repliesCount = 42;
	item.repliesAreComments = true;
	item.viewsCount = 1200;
	item.groupId = 0;
	item.text = "A very useful resource file description";
	item.mediaType = 3; // Document

	auto post = ExtractMetrics(item, 100123456, 1700001000);
	assert(post.channelId == 100123456);
	assert(post.messageId == 101);
	assert(post.date == 1700000000);
	assert(post.reactionsCount == 40);
	assert(post.commentsCount == 42);
	assert(post.viewsCount == 1200);
	assert(post.mediaType == 3);
	assert(post.textSnippet == "A very useful resource file description");
	assert(post.groupedId == 0);

	// Test 5: Comments ignored when repliesAreComments is false
	item.repliesAreComments = false;
	auto post2 = ExtractMetrics(item, 100123456, 1700001000);
	assert(post2.commentsCount == 0);

	// Test 6: Text snippet clamping > 120 chars
	std::string longText(200, 'x');
	item.text = longText;
	auto post3 = ExtractMetrics(item, 100123456, 1700001000);
	assert(post3.textSnippet.size() == 120);

	// Test 7: Grouped ID overrides media type
	item.groupId = 987654321;
	auto post4 = ExtractMetrics(item, 100123456, 1700001000);
	assert(post4.groupedId == 987654321);
	assert(post4.mediaType == 4);

	// Test 8: Empty text falls back to filename
	item.text = "";
	item.filename = "archive.tar.gz";
	auto post5 = ExtractMetrics(item, 100123456, 1700001000);
	assert(post5.textSnippet == "archive.tar.gz");

	std::cout << "All seam unit tests passed successfully!\n";
	return 0;
}
