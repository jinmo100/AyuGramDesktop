// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <string>

using ID = long long;

class AyuMessageBase
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	ID groupedId;
	ID peerId;
	ID fromId;
	ID topicId;
	int messageId;
	int date;
	int flags;
	int editDate;
	int views;
	int fwdFlags;
	ID fwdFromId;
	std::string fwdName;
	int fwdDate;
	std::string fwdPostAuthor;
	std::string postAuthor;
	int replyFlags;
	int replyMessageId;
	ID replyPeerId;
	int replyTopId;
	bool replyForumTopic;
	std::vector<char> replySerialized;
	std::vector<char> replyMarkupSerialized;
	int entityCreateDate;
	std::string text;
	std::vector<char> textEntities;
	std::string mediaPath;
	std::string hqThumbPath;
	int documentType;
	std::vector<char> documentSerialized;
	std::vector<char> thumbsSerialized;
	std::vector<char> documentAttributesSerialized;
	std::string mimeType;
};

class DeletedMessage : public AyuMessageBase
{
};

class EditedMessage : public AyuMessageBase
{
};

class DeletedDialog
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	ID peerId;
	std::unique_ptr<int> folderId; // nullable
	int topMessage;
	int lastMessageDate;
	int flags;
	int entityCreateDate;
};

class RegexFilter
{
public:
	std::vector<char> id;
	std::string text;
	bool enabled;
	bool reversed;
	bool caseInsensitive;
	std::optional<ID> dialogId; // nullable

	bool operator==(const RegexFilter &other) const {
		return id == other.id &&
			text == other.text &&
			caseInsensitive == other.caseInsensitive &&
			reversed == other.reversed &&
			dialogId == other.dialogId &&
			enabled == other.enabled;
	}
	[[nodiscard]] QJsonObject toJson() const {
		QJsonObject json;
		json["id"] = QString::fromUtf8(id.data());
		json["text"] = QString::fromStdString(text);
		json["enabled"] = enabled;
		json["reversed"] = reversed;
		json["caseInsensitive"] = caseInsensitive;
		if (dialogId.has_value()) {
			json["dialogId"] = dialogId.value();
		}
		return json;
	}
};

class RegexFilterGlobalExclusion
{
public:
	ID fakeId;
	ID dialogId;
	std::vector<char> filterId;

	bool operator==(const RegexFilterGlobalExclusion& other) const {
		return dialogId == other.dialogId && filterId == other.filterId;
	}
};

class SpyMessageRead
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	int messageId;
	int entityCreateDate;
};

class SpyMessageContentsRead
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	int messageId;
	int entityCreateDate;
};

enum class TopPostMediaType : int {
	None = 0,
	Photo = 1,
	Video = 2,
	Document = 3,
	Grouped = 4,
};

class ChannelTopPost
{
public:
	ID id = 0;
	ID channelId = 0;
	ID groupedId = 0;
	std::string textSnippet;
	std::string reactionsJson;
	int messageId = 0;
	int date = 0;
	int reactionsCount = 0;
	int commentsCount = 0;
	int viewsCount = 0;
	int mediaType = 0;
	int scanTime = 0;
	int groupedCount = 1;
	int positiveCount = 0;
	int negativeCount = 0;

	[[nodiscard]] TopPostMediaType type() const {
		return static_cast<TopPostMediaType>(mediaType);
	}
	void setType(TopPostMediaType t) {
		mediaType = static_cast<int>(t);
	}
	[[nodiscard]] bool isGrouped() const {
		return type() == TopPostMediaType::Grouped || groupedId != 0;
	}

	bool operator==(const ChannelTopPost &other) const {
		return channelId == other.channelId
			&& messageId == other.messageId
			&& date == other.date
			&& reactionsCount == other.reactionsCount
			&& commentsCount == other.commentsCount
			&& viewsCount == other.viewsCount
			&& textSnippet == other.textSnippet
			&& mediaType == other.mediaType
			&& reactionsJson == other.reactionsJson
			&& scanTime == other.scanTime
			&& groupedId == other.groupedId
			&& groupedCount == other.groupedCount;
	}

};

class ChannelTopPostState
{
public:
	ID channelId = 0;
	int sortOrder = 0;
	int filter = 0;
	int scanWindow = 0;
	int oldestScannedId = 0;
	int lastScanTime = 0;
	int lastAccessTime = 0;

	bool operator==(const ChannelTopPostState &other) const {
		return channelId == other.channelId
			&& sortOrder == other.sortOrder
			&& filter == other.filter
			&& scanWindow == other.scanWindow
			&& oldestScannedId == other.oldestScannedId
			&& lastScanTime == other.lastScanTime
			&& lastAccessTime == other.lastAccessTime;
	}

};

class ChannelTopPostScanRange
{
public:
	ID id = 0;
	ID channelId = 0;
	int fromDate = 0;
	int toDate = 0;
	int fromId = 0;
	int toId = 0;
	int messagesScanned = 0;
	int lastScanTime = 0;

	bool operator==(const ChannelTopPostScanRange &other) const {
		return channelId == other.channelId
			&& fromDate == other.fromDate
			&& toDate == other.toDate
			&& fromId == other.fromId
			&& toId == other.toId
			&& messagesScanned == other.messagesScanned
			&& lastScanTime == other.lastScanTime;
	}

};
