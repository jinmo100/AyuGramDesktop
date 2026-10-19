// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/entities.h"

#include <vector>

namespace TopPosts {

[[nodiscard]] ChannelTopPost CoalesceMediaGroup(
	const std::vector<ChannelTopPost> &group);

[[nodiscard]] std::vector<ChannelTopPost> AggregateMediaGroups(
	const std::vector<ChannelTopPost> &posts);

[[nodiscard]] std::string MergeReactionsJson(
	const std::string &first,
	const std::string &second);

class MediaGroupAggregator final {
public:
	[[nodiscard]] std::vector<ChannelTopPost> feed(
		const std::vector<ChannelTopPost> &batch);
	[[nodiscard]] std::vector<ChannelTopPost> flush();
	void reset();

private:
	std::vector<ChannelTopPost> _pendingGroup;
	ID _currentGroupId = 0;

};

} // namespace TopPosts
