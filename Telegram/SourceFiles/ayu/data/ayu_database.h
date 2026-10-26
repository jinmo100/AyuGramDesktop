// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/entities.h"
#include "rpl/producer.h"

#include <functional>
#include <vector>

#include <QtCore/QString>

class SchemaVersion
{
public:
	int id;
	int version;
};

namespace AyuDatabase {

inline constexpr auto kTopPostsDisplayLimit = 300;
inline constexpr auto kTopPostsStorageLimit = 3000;

void initialize();

void addEditedMessage(const EditedMessage &message);
std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit);
bool hasRevisions(ID userId, ID dialogId, ID messageId);

void addDeletedMessage(const DeletedMessage &message);
std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit, const std::string &searchQuery = "");
bool hasDeletedMessages(ID userId, ID dialogId, ID topicId);
void removeDeletedMessage(ID userId, ID dialogId, ID messageId);
void clearDeletedMessages(ID userId, ID dialogId, ID topicId);

std::vector<RegexFilter> getAllRegexFilters();
RegexFilter getById(std::vector<char> id);
std::vector<RegexFilter> getShared();
std::vector<RegexFilter> getByDialogId(ID dialogId);
std::vector<RegexFilterGlobalExclusion> getAllFiltersExclusions();
std::vector<RegexFilter> getExcludedByDialogId(ID dialogId);

int getCount();


void addRegexFilter(const RegexFilter &filter);
void addRegexExclusion(const RegexFilterGlobalExclusion &exclusion);

void updateRegexFilter(const RegexFilter &filter);

void deleteFilter(const std::vector<char> &id);
void deleteExclusionsByFilterId(const std::vector<char> &id);
void deleteExclusion(ID dialogId, std::vector<char> filterId);

void deleteAllFilters();
void deleteAllExclusions();

bool hasFilters();
bool hasPerDialogFilters();

enum class TopPostsSortOrder {
	Reactions,
	Comments,
	Views,
	Positive,
	Negative,
};

enum class TopPostsFilter {
	All,
	Documents,
	Media,
};

struct TopPostsReactionLists {
	std::vector<QString> positive;
	std::vector<QString> negative;
};

void saveChannelPosts(const std::vector<ChannelTopPost> &posts);
void saveChannelPost(const ChannelTopPost &post);

void truncateChannelPosts(ID channelId, int maxCount = 300);
void pruneExpiredChannelPosts(int maxAgeSeconds = 30 * 86400);
void evictLruChannels(int maxChannels = 100);

void clearChannelPosts(ID channelId);
void clearAllChannelPosts();
void removePost(ID channelId, int messageId);
void removePosts(ID channelId, const std::vector<int> &messageIds);
[[nodiscard]] rpl::producer<std::pair<ID, int>> channelPostRemoved();
[[nodiscard]] rpl::producer<ID> channelPostsCleared();

ChannelTopPostState getChannelTopPostState(ID channelId);
void saveChannelTopPostState(const ChannelTopPostState &state);
void saveChannelTopPostView(ID channelId, int sortOrder, int filter);
void extendChannelTopPostCoverage(
	ID channelId,
	int scanWindow,
	int oldestScannedId);
void touchChannelTopPostAccess(ID channelId);

void saveChannelTopPostScanRange(const ChannelTopPostScanRange &range);
std::vector<ChannelTopPostScanRange> getChannelTopPostScanRanges(ID channelId);
void clearChannelTopPostScanRanges(ID channelId);

std::vector<ChannelTopPost> getChannelTopPosts(
	ID channelId,
	TopPostsSortOrder sortOrder = TopPostsSortOrder::Reactions,
	TopPostsFilter filter = TopPostsFilter::All,
	int totalLimit = 300,
	int offset = 0,
	const TopPostsReactionLists &lists = TopPostsReactionLists(),
	int fromDate = 0,
	int toDate = 0);
bool hasChannelTopPosts(ID channelId);
int getChannelTopPostsCount(ID channelId);
std::vector<int> getChannelTopPostMessageIds(ID channelId);
int getChannelTopPostsMaxMessageId(ID channelId);

void moveCurrentDatabase();

}
