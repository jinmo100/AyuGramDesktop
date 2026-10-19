#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <sstream>
#include <iomanip>

namespace UiCardSeams {

using ID = long long;

enum class TopPostMediaType : int {
	None = 0,
	Photo = 1,
	Video = 2,
	Document = 3,
	Grouped = 4,
};

enum class TopPostsSortOrder {
	Reactions,
	Comments,
	Views,
};

enum class TopPostsFilter {
	All,
	Documents,
	Media,
};

enum class BannerState {
	Hidden,
	Scanning,
	Syncing,
	FloodWait,
	Failed,
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

// Formats metric count into compact human readable format (e.g., 950, 1.2K, 3.4M).
std::string FormatCount(int count) {
	if (count < 1000) {
		return std::to_string(count);
	} else if (count < 1000000) {
		double val = count / 1000.0;
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(1) << val;
		std::string str = ss.str();
		if (str.size() >= 2 && str.substr(str.size() - 2) == ".0") {
			str = str.substr(0, str.size() - 2);
		}
		return str + "K";
	} else {
		double val = count / 1000000.0;
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(1) << val;
		std::string str = ss.str();
		if (str.size() >= 2 && str.substr(str.size() - 2) == ".0") {
			str = str.substr(0, str.size() - 2);
		}
		return str + "M";
	}
}

// Parses primary reaction emoji from JSON, defaulting to 🔥 if empty or invalid.
std::string ExtractPrimaryReactionEmoji(const std::string &json) {
	if (json.empty() || json == "{}") {
		return "🔥";
	}
	auto start = json.find('"');
	if (start == std::string::npos) return "🔥";
	auto end = json.find('"', start + 1);
	if (end == std::string::npos) return "🔥";
	std::string emoji = json.substr(start + 1, end - start - 1);
	return emoji.empty() ? "🔥" : emoji;
}

std::string FormatReactionBadge(const MockTopPost &post) {
	std::string emoji = ExtractPrimaryReactionEmoji(post.reactionsJson);
	return emoji + " " + FormatCount(post.reactionsCount);
}

std::string FormatGroupBadge(const MockTopPost &post) {
	if (post.type() == TopPostMediaType::Grouped && post.groupedCount > 1) {
		return "📦 " + std::to_string(post.groupedCount);
	}
	return "";
}

std::string FormatDate(int timestamp) {
	time_t t = static_cast<time_t>(timestamp);
	struct tm *timeinfo = gmtime(&t);
	char buf[32];
	strftime(buf, sizeof(buf), "%b %d, %Y", timeinfo);
	return std::string(buf);
}

// Filter predicate
bool MatchesFilter(const MockTopPost &post, TopPostsFilter filter) {
	switch (filter) {
	case TopPostsFilter::All:
		return true;
	case TopPostsFilter::Documents:
		return post.type() == TopPostMediaType::Document;
	case TopPostsFilter::Media:
		return post.type() == TopPostMediaType::Photo
			|| post.type() == TopPostMediaType::Video
			|| post.type() == TopPostMediaType::Grouped;
	}
	return true;
}

// Sort comparator
bool ComparePosts(const MockTopPost &a, const MockTopPost &b, TopPostsSortOrder order) {
	switch (order) {
	case TopPostsSortOrder::Reactions:
		if (a.reactionsCount != b.reactionsCount) {
			return a.reactionsCount > b.reactionsCount;
		}
		break;
	case TopPostsSortOrder::Comments:
		if (a.commentsCount != b.commentsCount) {
			return a.commentsCount > b.commentsCount;
		}
		break;
	case TopPostsSortOrder::Views:
		if (a.viewsCount != b.viewsCount) {
			return a.viewsCount > b.viewsCount;
		}
		break;
	}
	if (a.date != b.date) {
		return a.date > b.date;
	}
	return a.messageId > b.messageId;
}

std::vector<MockTopPost> QueryPosts(
	const std::vector<MockTopPost> &allPosts,
	TopPostsSortOrder sortOrder,
	TopPostsFilter filter) {
	std::vector<MockTopPost> result;
	for (const auto &post : allPosts) {
		if (MatchesFilter(post, filter)) {
			result.push_back(post);
		}
	}
	std::sort(result.begin(), result.end(), [&](const MockTopPost &a, const MockTopPost &b) {
		return ComparePosts(a, b, sortOrder);
	});
	return result;
}

// Discussion group comment capability & tooltip
struct CommentSortCapability {
	bool enabled = false;
	std::string tooltip;
};

CommentSortCapability ResolveCommentSortCapability(bool hasDiscussionGroup) {
	if (hasDiscussionGroup) {
		return { true, "" };
	} else {
		return { false, "This channel has no discussion group" };
	}
}

// Progress banner text helper
struct BannerInfo {
	BannerState state = BannerState::Hidden;
	std::string text;
	bool canCancel = false;
};

BannerInfo ResolveBannerInfo(
	BannerState state,
	int current,
	int target,
	int floodWaitSeconds,
	const std::string &error = "") {
	BannerInfo info;
	info.state = state;
	switch (state) {
	case BannerState::Hidden:
		info.text = "";
		info.canCancel = false;
		break;
	case BannerState::Scanning:
		info.text = "Scanning " + std::to_string(current) + "/" + std::to_string(target) + " messages...";
		info.canCancel = true;
		break;
	case BannerState::Syncing:
		info.text = "Syncing channel updates (" + std::to_string(current) + " fetched)...";
		info.canCancel = true;
		break;
	case BannerState::FloodWait:
		info.text = "Rate limited. Resuming in " + std::to_string(floodWaitSeconds) + "s...";
		info.canCancel = true;
		break;
	case BannerState::Failed:
		info.text = error.empty() ? "Scan failed." : ("Scan error: " + error);
		info.canCancel = false;
		break;
	}
	return info;
}

// Empty state message helper
std::string ResolveEmptyStateMessage(bool hasAnyCachedPosts, TopPostsFilter filter) {
	if (!hasAnyCachedPosts) {
		return "No top posts scanned yet.\nClick Rescan to discover popular content.";
	}
	switch (filter) {
	case TopPostsFilter::Documents:
		return "No document files found in top posts.";
	case TopPostsFilter::Media:
		return "No photos or videos found in top posts.";
	default:
		return "No posts match current criteria.";
	}
}

} // namespace UiCardSeams

int main() {
	using namespace UiCardSeams;

	// Test 1: Count and badge formatting
	{
		assert(FormatCount(0) == "0");
		assert(FormatCount(999) == "999");
		assert(FormatCount(1000) == "1K");
		assert(FormatCount(1234) == "1.2K");
		assert(FormatCount(10500) == "10.5K");
		assert(FormatCount(1000000) == "1M");
		assert(FormatCount(2500000) == "2.5M");

		MockTopPost post;
		post.reactionsCount = 42;
		post.reactionsJson = "{\"❤️\":42}";
		assert(FormatReactionBadge(post) == "❤️ 42");

		post.reactionsJson = "";
		assert(FormatReactionBadge(post) == "🔥 42");

		post.setType(TopPostMediaType::Grouped);
		post.groupedCount = 5;
		assert(FormatGroupBadge(post) == "📦 5");
		std::cout << "Test 1 (Formatting & badges) passed." << std::endl;
	}

	// Test 2: Filter matching
	{
		MockTopPost pDoc, pPhoto, pVideo, pGroup, pText;
		pDoc.setType(TopPostMediaType::Document);
		pPhoto.setType(TopPostMediaType::Photo);
		pVideo.setType(TopPostMediaType::Video);
		pGroup.setType(TopPostMediaType::Grouped);
		pText.setType(TopPostMediaType::None);

		assert(MatchesFilter(pDoc, TopPostsFilter::All));
		assert(MatchesFilter(pPhoto, TopPostsFilter::All));
		assert(MatchesFilter(pText, TopPostsFilter::All));

		assert(MatchesFilter(pDoc, TopPostsFilter::Documents));
		assert(!MatchesFilter(pPhoto, TopPostsFilter::Documents));
		assert(!MatchesFilter(pGroup, TopPostsFilter::Documents));

		assert(MatchesFilter(pPhoto, TopPostsFilter::Media));
		assert(MatchesFilter(pVideo, TopPostsFilter::Media));
		assert(MatchesFilter(pGroup, TopPostsFilter::Media));
		assert(!MatchesFilter(pDoc, TopPostsFilter::Media));
		assert(!MatchesFilter(pText, TopPostsFilter::Media));
		std::cout << "Test 2 (Filter matching) passed." << std::endl;
	}

	// Test 3: Multi-dimensional sorting
	{
		MockTopPost p1, p2, p3;
		p1.messageId = 1;
		p1.reactionsCount = 100;
		p1.commentsCount = 5;
		p1.viewsCount = 1000;
		p1.date = 1000;

		p2.messageId = 2;
		p2.reactionsCount = 50;
		p2.commentsCount = 20;
		p2.viewsCount = 3000;
		p2.date = 2000;

		p3.messageId = 3;
		p3.reactionsCount = 200;
		p3.commentsCount = 10;
		p3.viewsCount = 500;
		p3.date = 1500;

		std::vector<MockTopPost> all = { p1, p2, p3 };

		// Sort by reactions
		auto byReactions = QueryPosts(all, TopPostsSortOrder::Reactions, TopPostsFilter::All);
		assert(byReactions.size() == 3);
		assert(byReactions[0].messageId == 3); // 200
		assert(byReactions[1].messageId == 1); // 100
		assert(byReactions[2].messageId == 2); // 50

		// Sort by comments
		auto byComments = QueryPosts(all, TopPostsSortOrder::Comments, TopPostsFilter::All);
		assert(byComments[0].messageId == 2); // 20
		assert(byComments[1].messageId == 3); // 10
		assert(byComments[2].messageId == 1); // 5

		// Sort by views
		auto byViews = QueryPosts(all, TopPostsSortOrder::Views, TopPostsFilter::All);
		assert(byViews[0].messageId == 2); // 3000
		assert(byViews[1].messageId == 1); // 1000
		assert(byViews[2].messageId == 3); // 500
		std::cout << "Test 3 (Sorting) passed." << std::endl;
	}

	// Test 4: Discussion group capability and tooltip
	{
		auto withGroup = ResolveCommentSortCapability(true);
		assert(withGroup.enabled == true);
		assert(withGroup.tooltip.empty());

		auto withoutGroup = ResolveCommentSortCapability(false);
		assert(withoutGroup.enabled == false);
		assert(!withoutGroup.tooltip.empty());
		assert(withoutGroup.tooltip == "This channel has no discussion group");
		std::cout << "Test 4 (Discussion capability & tooltip) passed." << std::endl;
	}

	// Test 5: Progress banner resolution
	{
		auto bScan = ResolveBannerInfo(BannerState::Scanning, 600, 1000, 0);
		assert(bScan.state == BannerState::Scanning);
		assert(bScan.text == "Scanning 600/1000 messages...");
		assert(bScan.canCancel == true);

		auto bFlood = ResolveBannerInfo(BannerState::FloodWait, 0, 0, 42);
		assert(bFlood.state == BannerState::FloodWait);
		assert(bFlood.text == "Rate limited. Resuming in 42s...");
		assert(bFlood.canCancel == true);

		auto bHidden = ResolveBannerInfo(BannerState::Hidden, 0, 0, 0);
		assert(bHidden.state == BannerState::Hidden);
		assert(bHidden.text.empty());
		assert(bHidden.canCancel == false);
		std::cout << "Test 5 (Banner resolution) passed." << std::endl;
	}

	// Test 6: Empty state messaging
	{
		std::string emptyNoCache = ResolveEmptyStateMessage(false, TopPostsFilter::All);
		assert(emptyNoCache.find("No top posts scanned yet") != std::string::npos);

		std::string emptyDocs = ResolveEmptyStateMessage(true, TopPostsFilter::Documents);
		assert(emptyDocs.find("No document files") != std::string::npos);

		std::string emptyMedia = ResolveEmptyStateMessage(true, TopPostsFilter::Media);
		assert(emptyMedia.find("No photos or videos") != std::string::npos);
		std::cout << "Test 6 (Empty states) passed." << std::endl;
	}

	std::cout << "All UI and cards seam tests passed successfully!" << std::endl;
	return 0;
}
