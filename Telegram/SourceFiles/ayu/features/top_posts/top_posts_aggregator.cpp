// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/top_posts/top_posts_aggregator.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <algorithm>

namespace TopPosts {

ChannelTopPost CoalesceMediaGroup(const std::vector<ChannelTopPost> &group) {
	if (group.empty()) {
		return ChannelTopPost();
	}
	if (group.size() == 1) {
		auto post = group.front();
		if (post.groupedId != 0) {
			post.setType(TopPostMediaType::Grouped);
		}
		return post;
	}

	auto result = group.front();
	result.setType(TopPostMediaType::Grouped);

	auto totalReactions = 0;
	auto maxComments = 0;
	auto maxViews = 0;
	auto minMessageId = group.front().messageId;
	auto anchorDate = group.front().date;
	auto latestScanTime = group.front().scanTime;
	auto leadSnippet = QString();
	auto mergedReactions = QJsonObject();

	for (const auto &item : group) {
		totalReactions += item.reactionsCount;
		maxComments = std::max(maxComments, item.commentsCount);
		maxViews = std::max(maxViews, item.viewsCount);
		latestScanTime = std::max(latestScanTime, item.scanTime);

		if (item.messageId < minMessageId) {
			minMessageId = item.messageId;
			anchorDate = item.date;
		}

		if (leadSnippet.isEmpty() && !item.textSnippet.empty()) {
			leadSnippet = QString::fromStdString(item.textSnippet);
		}

		if (!item.reactionsJson.empty()) {
			const auto doc = QJsonDocument::fromJson(
				QByteArray::fromRawData(
					item.reactionsJson.data(),
					item.reactionsJson.size()));
			const auto obj = doc.object();
			for (auto it = obj.begin(); it != obj.end(); ++it) {
				const auto key = it.key();
				const auto count = it.value().toInt();
				mergedReactions[key] = mergedReactions.value(key).toInt() + count;
			}
		}
	}

	if (leadSnippet.size() > 120) {
		leadSnippet = leadSnippet.left(120);
	}

	result.messageId = minMessageId;
	result.date = anchorDate;
	result.reactionsCount = totalReactions;
	result.commentsCount = maxComments;
	result.viewsCount = maxViews;
	result.scanTime = latestScanTime;
	result.textSnippet = leadSnippet.toStdString();
	result.reactionsJson = QString::fromUtf8(
		QJsonDocument(mergedReactions).toJson(QJsonDocument::Compact)
	).toStdString();

	return result;
}

std::vector<ChannelTopPost> AggregateMediaGroups(
		const std::vector<ChannelTopPost> &posts) {
	if (posts.empty()) {
		return {};
	}

	auto coalesced = std::vector<ChannelTopPost>();
	auto currentGroup = std::vector<ChannelTopPost>();
	auto currentGroupId = ID(0);

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

std::vector<ChannelTopPost> MediaGroupAggregator::feed(
		const std::vector<ChannelTopPost> &batch) {
	auto output = std::vector<ChannelTopPost>();
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

std::vector<ChannelTopPost> MediaGroupAggregator::flush() {
	auto output = std::vector<ChannelTopPost>();
	if (!_pendingGroup.empty()) {
		output.push_back(CoalesceMediaGroup(_pendingGroup));
		_pendingGroup.clear();
		_currentGroupId = 0;
	}
	return output;
}

void MediaGroupAggregator::reset() {
	_pendingGroup.clear();
	_currentGroupId = 0;
}

} // namespace TopPosts
