// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/ayu_database.h"

#include "ayu/data/entities.h"
#include "ayu/libs/sqlite/sqlite_orm.h"
#include "base/unixtime.h"
#include "crl/crl_on_main.h"
#include "rpl/event_stream.h"

#include <algorithm>
#include <map>
#include <mutex>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

using namespace sqlite_orm;

std::mutex databaseMutex;

auto storage = make_storage(
	"./tdata/ayudata.db",
	make_table<SchemaVersion>(
		"SchemaVersion",
		make_column("id", &SchemaVersion::id, primary_key()),
		make_column("version", &SchemaVersion::version)
	),
	make_index("idx_deleted_message_userId_dialogId_topicId_messageId",
			   column<DeletedMessage>(&DeletedMessage::userId),
			   column<DeletedMessage>(&DeletedMessage::dialogId),
			   column<DeletedMessage>(&DeletedMessage::topicId),
			   column<DeletedMessage>(&DeletedMessage::messageId)),
	make_index("idx_edited_message_userId_dialogId_messageId",
			   column<EditedMessage>(&EditedMessage::userId),
			   column<EditedMessage>(&EditedMessage::dialogId),
			   column<EditedMessage>(&EditedMessage::messageId)),
	make_table<DeletedMessage>(
		"DeletedMessage",
		make_column("fakeId", &DeletedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedMessage::userId),
		make_column("dialogId", &DeletedMessage::dialogId),
		make_column("groupedId", &DeletedMessage::groupedId),
		make_column("peerId", &DeletedMessage::peerId),
		make_column("fromId", &DeletedMessage::fromId),
		make_column("topicId", &DeletedMessage::topicId),
		make_column("messageId", &DeletedMessage::messageId),
		make_column("date", &DeletedMessage::date),
		make_column("flags", &DeletedMessage::flags),
		make_column("editDate", &DeletedMessage::editDate),
		make_column("views", &DeletedMessage::views),
		make_column("fwdFlags", &DeletedMessage::fwdFlags),
		make_column("fwdFromId", &DeletedMessage::fwdFromId),
		make_column("fwdName", &DeletedMessage::fwdName),
		make_column("fwdDate", &DeletedMessage::fwdDate),
		make_column("fwdPostAuthor", &DeletedMessage::fwdPostAuthor),
		make_column("replyFlags", &DeletedMessage::replyFlags),
		make_column("replyMessageId", &DeletedMessage::replyMessageId),
		make_column("replyPeerId", &DeletedMessage::replyPeerId),
		make_column("replyTopId", &DeletedMessage::replyTopId),
		make_column("replyForumTopic", &DeletedMessage::replyForumTopic),
		make_column("replySerialized", &DeletedMessage::replySerialized),
		make_column("entityCreateDate", &DeletedMessage::entityCreateDate),
		make_column("text", &DeletedMessage::text),
		make_column("textEntities", &DeletedMessage::textEntities),
		make_column("mediaPath", &DeletedMessage::mediaPath),
		make_column("hqThumbPath", &DeletedMessage::hqThumbPath),
		make_column("documentType", &DeletedMessage::documentType),
		make_column("documentSerialized", &DeletedMessage::documentSerialized),
		make_column("thumbsSerialized", &DeletedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &DeletedMessage::documentAttributesSerialized),
		make_column("mimeType", &DeletedMessage::mimeType)
	),
	make_table<EditedMessage>(
		"EditedMessage",
		make_column("fakeId", &EditedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &EditedMessage::userId),
		make_column("dialogId", &EditedMessage::dialogId),
		make_column("groupedId", &EditedMessage::groupedId),
		make_column("peerId", &EditedMessage::peerId),
		make_column("fromId", &EditedMessage::fromId),
		make_column("topicId", &EditedMessage::topicId),
		make_column("messageId", &EditedMessage::messageId),
		make_column("date", &EditedMessage::date),
		make_column("flags", &EditedMessage::flags),
		make_column("editDate", &EditedMessage::editDate),
		make_column("views", &EditedMessage::views),
		make_column("fwdFlags", &EditedMessage::fwdFlags),
		make_column("fwdFromId", &EditedMessage::fwdFromId),
		make_column("fwdName", &EditedMessage::fwdName),
		make_column("fwdDate", &EditedMessage::fwdDate),
		make_column("fwdPostAuthor", &EditedMessage::fwdPostAuthor),
		make_column("replyFlags", &EditedMessage::replyFlags),
		make_column("replyMessageId", &EditedMessage::replyMessageId),
		make_column("replyPeerId", &EditedMessage::replyPeerId),
		make_column("replyTopId", &EditedMessage::replyTopId),
		make_column("replyForumTopic", &EditedMessage::replyForumTopic),
		make_column("replySerialized", &EditedMessage::replySerialized),
		make_column("entityCreateDate", &EditedMessage::entityCreateDate),
		make_column("text", &EditedMessage::text),
		make_column("textEntities", &EditedMessage::textEntities),
		make_column("mediaPath", &EditedMessage::mediaPath),
		make_column("hqThumbPath", &EditedMessage::hqThumbPath),
		make_column("documentType", &EditedMessage::documentType),
		make_column("documentSerialized", &EditedMessage::documentSerialized),
		make_column("thumbsSerialized", &EditedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &EditedMessage::documentAttributesSerialized),
		make_column("mimeType", &EditedMessage::mimeType)
	),
	make_table<DeletedDialog>(
		"DeletedDialog",
		make_column("fakeId", &DeletedDialog::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedDialog::userId),
		make_column("dialogId", &DeletedDialog::dialogId),
		make_column("peerId", &DeletedDialog::peerId),
		make_column("folderId", &DeletedDialog::folderId),
		make_column("topMessage", &DeletedDialog::topMessage),
		make_column("lastMessageDate", &DeletedDialog::lastMessageDate),
		make_column("flags", &DeletedDialog::flags),
		make_column("entityCreateDate", &DeletedDialog::entityCreateDate)
	),
	make_table<RegexFilter>(
		"RegexFilter",
		make_column("id", &RegexFilter::id, primary_key()),
		make_column("text", &RegexFilter::text),
		make_column("enabled", &RegexFilter::enabled),
		make_column("reversed", &RegexFilter::reversed),
		make_column("caseInsensitive", &RegexFilter::caseInsensitive),
		make_column("dialogId", &RegexFilter::dialogId)
	),
	make_table<RegexFilterGlobalExclusion>(
		"RegexFilterGlobalExclusion",
		make_column("fakeId", &RegexFilterGlobalExclusion::fakeId, primary_key().autoincrement()),
		make_column("dialogId", &RegexFilterGlobalExclusion::dialogId),
		make_column("filterId", &RegexFilterGlobalExclusion::filterId)
	),
	make_table<SpyMessageRead>(
		"SpyMessageRead",
		make_column("fakeId", &SpyMessageRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageRead::userId),
		make_column("dialogId", &SpyMessageRead::dialogId),
		make_column("messageId", &SpyMessageRead::messageId),
		make_column("entityCreateDate", &SpyMessageRead::entityCreateDate)
	),
	make_table<SpyMessageContentsRead>(
		"SpyMessageContentsRead",
		make_column("fakeId", &SpyMessageContentsRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageContentsRead::userId),
		make_column("dialogId", &SpyMessageContentsRead::dialogId),
		make_column("messageId", &SpyMessageContentsRead::messageId),
		make_column("entityCreateDate", &SpyMessageContentsRead::entityCreateDate)
	),
	make_unique_index(
		"idx_channel_top_post_channel_message",
		column<ChannelTopPost>(&ChannelTopPost::channelId),
		column<ChannelTopPost>(&ChannelTopPost::messageId)
	),
	make_index(
		"idx_channel_top_post_channel_reactions",
		column<ChannelTopPost>(&ChannelTopPost::channelId),
		column<ChannelTopPost>(&ChannelTopPost::reactionsCount),
		column<ChannelTopPost>(&ChannelTopPost::commentsCount)
	),
	make_index(
		"idx_channel_top_post_channel_comments",
		column<ChannelTopPost>(&ChannelTopPost::channelId),
		column<ChannelTopPost>(&ChannelTopPost::commentsCount),
		column<ChannelTopPost>(&ChannelTopPost::reactionsCount)
	),
	make_index(
		"idx_channel_top_post_channel_scanTime",
		column<ChannelTopPost>(&ChannelTopPost::channelId),
		column<ChannelTopPost>(&ChannelTopPost::scanTime)
	),
	make_index(
		"idx_channel_top_post_scanTime",
		column<ChannelTopPost>(&ChannelTopPost::scanTime)
	),
	make_table<ChannelTopPost>(
		"ChannelTopPost",
		make_column("id", &ChannelTopPost::id, primary_key().autoincrement()),
		make_column("channelId", &ChannelTopPost::channelId),
		make_column("messageId", &ChannelTopPost::messageId),
		make_column("date", &ChannelTopPost::date),
		make_column("reactionsCount", &ChannelTopPost::reactionsCount),
		make_column("commentsCount", &ChannelTopPost::commentsCount),
		make_column("viewsCount", &ChannelTopPost::viewsCount),
		make_column("textSnippet", &ChannelTopPost::textSnippet),
		make_column("mediaType", &ChannelTopPost::mediaType),
		make_column("reactionsJson", &ChannelTopPost::reactionsJson),
		make_column("scanTime", &ChannelTopPost::scanTime),
		make_column("groupedId", &ChannelTopPost::groupedId),
		make_column("groupedCount", &ChannelTopPost::groupedCount)
	),
	make_table<ChannelTopPostState>(
		"ChannelTopPostState",
		make_column("channelId", &ChannelTopPostState::channelId, primary_key()),
		make_column("sortOrder", &ChannelTopPostState::sortOrder),
		make_column("filter", &ChannelTopPostState::filter),
		make_column("scanWindow", &ChannelTopPostState::scanWindow),
		make_column("oldestScannedId", &ChannelTopPostState::oldestScannedId),
		make_column("lastScanTime", &ChannelTopPostState::lastScanTime),
		make_column("lastAccessTime", &ChannelTopPostState::lastAccessTime)
	)
);

namespace AyuMigrations {

void migrateToV1(decltype(storage) &storage) {
	// drop RegexFilter table as we've added primary_key()
	try {
		storage.drop_table_if_exists("RegexFilter");
		LOG(("Migration to V1 successful."));
	} catch (const std::exception &ex) {
		LOG(("Migration to V1 failed: %1").arg(ex.what()));
	}
}

void migrateToV2(decltype(storage) &storage) {
	// WHY: v1 scanned under the selected filter only, so its rows are partial.
	try {
		storage.remove_all<ChannelTopPost>();
		storage.remove_all<ChannelTopPostState>();
		LOG(("Migration to V2 successful."));
	} catch (const std::exception &ex) {
		LOG(("Migration to V2 failed: %1").arg(ex.what()));
	}
}

}

void runMigrations(decltype(storage) &storage) {
	constexpr int kLatestVersion = 2;

	const std::map<int, Fn<void(decltype(storage) &)>> migrations = {
		{1, AyuMigrations::migrateToV1},
		{2, AyuMigrations::migrateToV2},
	};

	int currentVersion = 0;
	try {
		if (auto versionRow = storage.get_pointer<SchemaVersion>(1)) {
			currentVersion = versionRow->version;
		} else {
			storage.insert(SchemaVersion{1, 0});
		}
	} catch (...) {
		LOG(("No SchemaVersion, assuming 0"));
		storage.insert(SchemaVersion{1, 0});
	}

	if (currentVersion >= kLatestVersion) {
		LOG(("Database is ok"));
		return;
	}

	LOG(("Database version: %1. Latest version: %2.").arg(currentVersion).arg(kLatestVersion));

	for (int v = currentVersion + 1; v <= kLatestVersion; ++v) {
		if (migrations.contains(v)) {
			try {
				LOG(("Migration for version: %1").arg(v));
				storage.begin_transaction();

				migrations.at(v)(storage);

				storage.update_all(set(c(&SchemaVersion::version) = v), where(c(&SchemaVersion::id) == 1));
				storage.commit();
				LOG(("Applied migration for version: %1.").arg(v));
			} catch (...) {
				storage.rollback();
				LOG(("Failed to apply migration for version: %1.").arg(v));
				AyuDatabase::moveCurrentDatabase();

				return;
			}
		}
	}
}

namespace AyuDatabase {

void moveCurrentDatabase() {
	const auto time = base::unixtime::now();

	if (QFile::exists("./tdata/ayudata.db")) {
		QFile::rename("./tdata/ayudata.db", QString("./tdata/ayudata_%1.db").arg(time));
	}

	if (QFile::exists("./tdata/ayudata.db-shm")) {
		QFile::rename("./tdata/ayudata.db-shm", QString("./tdata/ayudata_%1.db-shm").arg(time));
	}

	if (QFile::exists("./tdata/ayudata.db-wal")) {
		QFile::rename("./tdata/ayudata.db-wal", QString("./tdata/ayudata_%1.db-wal").arg(time));
	}
}

void initialize() {
	{
		std::lock_guard lock(databaseMutex);
		try {
			storage.sync_schema(true);

			runMigrations(storage);

			storage.sync_schema(true);
		} catch (const std::exception &ex) {
			LOG(("Database initialization failed: %1").arg(ex.what()));
			moveCurrentDatabase();

			storage.sync_schema(true);
			if (!storage.get_pointer<SchemaVersion>(1)) {
				storage.insert(SchemaVersion{1, 0});
			}
		}

		try {
			storage.open_forever();
		} catch (const std::exception &ex) {
			LOG(("Failed to keep database open: %1").arg(ex.what()));
		}
	}

	pruneExpiredChannelPosts();
	evictLruChannels();
}

void addEditedMessage(const EditedMessage &message) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.begin_transaction();
		storage.insert(message);
		storage.commit();
	} catch (std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
	}
}

std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit) {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.get_all<EditedMessage>(
			where(
				column<EditedMessage>(&EditedMessage::userId) == userId and
				column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
				column<EditedMessage>(&EditedMessage::messageId) == messageId and
				(column<EditedMessage>(&EditedMessage::fakeId) > minId or minId == 0) and
				(column<EditedMessage>(&EditedMessage::fakeId) < maxId or maxId == 0)
			),
			order_by(column<EditedMessage>(&EditedMessage::fakeId)).desc(),
			limit(totalLimit)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get edited messages: %1").arg(ex.what()));
		return {};
	}
}

bool hasRevisions(ID userId, ID dialogId, ID messageId) {
	std::lock_guard lock(databaseMutex);
	try {
		return !storage.select(
			columns(column<EditedMessage>(&EditedMessage::messageId)),
			where(
				column<EditedMessage>(&EditedMessage::userId) == userId and
				column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
				column<EditedMessage>(&EditedMessage::messageId) == messageId
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if message has revisions: %1").arg(ex.what()));
		return false;
	}
}

void addDeletedMessage(const DeletedMessage &message) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.begin_transaction();
		storage.insert(message);
		storage.commit();
	} catch (std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
	}
}

std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit, const std::string &searchQuery) {
	std::lock_guard lock(databaseMutex);
	try {
		if (searchQuery.empty()) {
			return storage.get_all<DeletedMessage>(
				where(
					column<DeletedMessage>(&DeletedMessage::userId) == userId and
					column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
					(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
					(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
					(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0)
				),
				order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
				limit(totalLimit)
			);
		}

		std::string escaped;
		escaped.reserve(searchQuery.size());
		for (const auto c : searchQuery) {
			if (c == '%' || c == '_' || c == '\\') {
				escaped += '\\';
			}
			escaped += c;
		}
		const auto pattern = "%" + escaped + "%";
		return storage.get_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0) and
				like(column<DeletedMessage>(&DeletedMessage::text), pattern, "\\")
			),
			order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
			limit(totalLimit)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get deleted messages: %1").arg(ex.what()));
		return {};
	}
}

bool hasDeletedMessages(ID userId, ID dialogId, ID topicId) {
	std::lock_guard lock(databaseMutex);
	try {
		return !storage.select(
			columns(column<DeletedMessage>(&DeletedMessage::dialogId)),
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if dialog has deleted message: %1").arg(ex.what()));
		return false;
	}
}

void removeDeletedMessage(ID userId, ID dialogId, ID messageId) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				column<DeletedMessage>(&DeletedMessage::messageId) == messageId
			)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to remove deleted message: %1").arg(ex.what()));
	}
}

void clearDeletedMessages(ID userId, ID dialogId, ID topicId) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			)
		);
	} catch (std::exception &) {
	}
}

template<typename T>
std::vector<T> getAllT() {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.get_all<T>();
	} catch (std::exception &ex) {
		LOG(("Failed to get all: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getAllRegexFilters() {
	return getAllT<RegexFilter>();
}

std::vector<RegexFilterGlobalExclusion> getAllFiltersExclusions() {
	return getAllT<RegexFilterGlobalExclusion>();
}

std::vector<RegexFilter> getExcludedByDialogId(ID dialogId) {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.get_all<RegexFilter>(
			where(in(&RegexFilter::id,
					 storage.select(columns(&RegexFilterGlobalExclusion::filterId),
									where(is_equal(&RegexFilterGlobalExclusion::dialogId, dialogId))
					 )
			))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get excluded by dialog id: %1").arg(ex.what()));
		return {};
	}
}

int getCount() {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.count<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to get count: %1").arg(ex.what()));
		return 0;
	}
}

RegexFilter getById(std::vector<char> id) {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.get<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == std::move(id))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by id: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getShared() {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.get_all<RegexFilter>(
			where(is_null(column<RegexFilter>(&RegexFilter::dialogId)))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get shared filters: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getByDialogId(ID dialogId) {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.get_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::dialogId) == dialogId)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by dialog id: %1").arg(ex.what()));
		return {};
	}
}

void addRegexFilter(const RegexFilter &filter) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.begin_transaction();
		storage.replace(filter); // we're using replace as we set std::vector<char> as primary key
		storage.commit();
	} catch (std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to save regex filter for some reason: %1").arg(ex.what()));
	}
}

void addRegexExclusion(const RegexFilterGlobalExclusion &exclusion) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.begin_transaction();
		storage.insert(exclusion);
		storage.commit();
	} catch (std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to save regex filter exclusion for some reason: %1").arg(ex.what()));
	}
}

void updateRegexFilter(const RegexFilter &filter) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.update_all(
			set(
				c(&RegexFilter::text) = filter.text,
				c(&RegexFilter::enabled) = filter.enabled,
				c(&RegexFilter::reversed) = filter.reversed,
				c(&RegexFilter::caseInsensitive) = filter.caseInsensitive,
				c(&RegexFilter::dialogId) = filter.dialogId
			),
			where(c(&RegexFilter::id) == filter.id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to update regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteFilter(const std::vector<char> &id) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteExclusionsByFilterId(const std::vector<char> &id) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<RegexFilterGlobalExclusion>(
			where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter exclusion by filter id for some reason: %1").arg(ex.what()));
	}
}

void deleteExclusion(ID dialogId, std::vector<char> filterId) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<RegexFilterGlobalExclusion>(
			where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == filterId and
				column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::dialogId) == dialogId
			)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter exclusion for some reason: %1").arg(ex.what()));
	}
}

void deleteAllFilters() {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to delete all regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteAllExclusions() {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<RegexFilterGlobalExclusion>();
	} catch (std::exception &ex) {
		LOG(("Failed to delete all regex filter exclusions for some reason: %1").arg(ex.what()));
	}
}

bool hasFilters() {
	std::lock_guard lock(databaseMutex);
	try {
		return !storage.select(
			columns(column<RegexFilter>(&RegexFilter::id)),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

bool hasPerDialogFilters() {
	std::lock_guard lock(databaseMutex);
	try {
		return
			!storage.select(
				columns(column<RegexFilter>(&RegexFilter::id)),
				where(is_not_null(column<RegexFilter>(&RegexFilter::dialogId))),
				limit(1)
			).empty() ||
			!storage.select(
				columns(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::fakeId)),
				limit(1)
			).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

namespace {

void evictLruChannelsLocked(int maxChannels) {
	auto rows = storage.select(
		columns(&ChannelTopPost::channelId, max(&ChannelTopPost::scanTime)),
		group_by(&ChannelTopPost::channelId)
	);
	if (static_cast<int>(rows.size()) <= maxChannels) {
		return;
	}

	std::map<ID, int> lastAccess;
	for (const auto &row : storage.select(
			columns(
				&ChannelTopPostState::channelId,
				&ChannelTopPostState::lastAccessTime))) {
		lastAccess.emplace(std::get<0>(row), std::get<1>(row));
	}
	const auto visited = [&](ID channelId, int scanTime) {
		const auto i = lastAccess.find(channelId);
		return (i != lastAccess.end()) ? std::max(i->second, scanTime) : scanTime;
	};

	std::sort(rows.begin(), rows.end(), [&](const auto &a, const auto &b) {
		return visited(std::get<0>(a), std::get<1>(a) ? *std::get<1>(a) : 0)
			> visited(std::get<0>(b), std::get<1>(b) ? *std::get<1>(b) : 0);
	});

	std::vector<ID> channelsToEvict;
	channelsToEvict.reserve(rows.size() - maxChannels);
	for (auto i = static_cast<size_t>(maxChannels); i < rows.size(); ++i) {
		channelsToEvict.push_back(std::get<0>(rows[i]));
	}

	storage.begin_transaction();
	for (const auto channelId : channelsToEvict) {
		storage.remove_all<ChannelTopPost>(
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId)
		);
		storage.remove_all<ChannelTopPostState>(
			where(column<ChannelTopPostState>(&ChannelTopPostState::channelId) == channelId)
		);
	}
	storage.commit();
}

template<typename WhereExpr, typename OrderExpr>
std::vector<ChannelTopPost> queryTopPosts(
	WhereExpr &&whereClause,
	OrderExpr &&orderClause,
	int totalLimit,
	int offset) {
	if (offset > 0) {
		return storage.get_all<ChannelTopPost>(
			where(std::forward<WhereExpr>(whereClause)),
			std::forward<OrderExpr>(orderClause),
			limit(totalLimit, sqlite_orm::offset(offset))
		);
	}
	return storage.get_all<ChannelTopPost>(
		where(std::forward<WhereExpr>(whereClause)),
		std::forward<OrderExpr>(orderClause),
		limit(totalLimit)
	);
}

} // namespace

void saveChannelPosts(const std::vector<ChannelTopPost> &posts) {
	if (posts.empty()) {
		return;
	}
	std::lock_guard lock(databaseMutex);
	try {
		storage.begin_transaction();
		for (auto post : posts) {
			const auto existingIds = storage.select(
				&ChannelTopPost::id,
				where(
					column<ChannelTopPost>(&ChannelTopPost::channelId) == post.channelId
					&& column<ChannelTopPost>(&ChannelTopPost::messageId) == post.messageId
				),
				limit(1)
			);
			if (!existingIds.empty()) {
				post.id = existingIds.front();
				storage.update(post);
			} else {
				storage.insert(post);
			}
		}
		storage.commit();
		evictLruChannelsLocked(100);
	} catch (const std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to save channel top posts: %1").arg(ex.what()));
	}
}

void saveChannelPost(const ChannelTopPost &post) {
	saveChannelPosts({ post });
}

void truncateChannelPosts(ID channelId, int maxCount) {
	std::lock_guard lock(databaseMutex);
	try {
		if (maxCount <= 0) {
			storage.remove_all<ChannelTopPost>(
				where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId)
			);
			return;
		}
		const auto allIds = storage.select(
			&ChannelTopPost::id,
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId),
			multi_order_by(
				order_by(column<ChannelTopPost>(&ChannelTopPost::reactionsCount)).desc(),
				order_by(column<ChannelTopPost>(&ChannelTopPost::commentsCount)).desc(),
				order_by(column<ChannelTopPost>(&ChannelTopPost::viewsCount)).desc(),
				order_by(column<ChannelTopPost>(&ChannelTopPost::date)).desc()
			)
		);
		if (static_cast<int>(allIds.size()) > maxCount) {
			storage.begin_transaction();
			const auto idsToDelete = std::vector<ID>(
				allIds.begin() + maxCount,
				allIds.end());
			for (size_t i = 0; i < idsToDelete.size(); i += 300) {
				const auto end = std::min(i + 300, idsToDelete.size());
				const auto chunk = std::vector<ID>(
					idsToDelete.begin() + i,
					idsToDelete.begin() + end);
				storage.remove_all<ChannelTopPost>(
					where(in(&ChannelTopPost::id, chunk))
				);
			}
			storage.commit();
		}
	} catch (const std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to truncate channel top posts: %1").arg(ex.what()));
	}
}

void pruneExpiredChannelPosts(int maxAgeSeconds) {
	std::lock_guard lock(databaseMutex);
	try {
		const auto cutoff = base::unixtime::now() - maxAgeSeconds;
		std::map<ID, int> lastAccess;
		for (const auto &row : storage.select(
				columns(
					&ChannelTopPostState::channelId,
					&ChannelTopPostState::lastAccessTime))) {
			lastAccess.emplace(std::get<0>(row), std::get<1>(row));
		}

		std::vector<ID> expired;
		for (const auto &row : storage.select(
				columns(
					&ChannelTopPost::channelId,
					max(&ChannelTopPost::scanTime)),
				group_by(&ChannelTopPost::channelId))) {
			const auto channelId = std::get<0>(row);
			const auto scanned = std::get<1>(row) ? *std::get<1>(row) : 0;
			const auto i = lastAccess.find(channelId);
			const auto seen = (i != lastAccess.end())
				? std::max(i->second, scanned)
				: scanned;
			if (seen < cutoff) {
				expired.push_back(channelId);
			}
		}
		if (expired.empty()) {
			return;
		}

		storage.begin_transaction();
		for (const auto channelId : expired) {
			storage.remove_all<ChannelTopPost>(
				where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId)
			);
			storage.remove_all<ChannelTopPostState>(
				where(column<ChannelTopPostState>(&ChannelTopPostState::channelId) == channelId)
			);
		}
		storage.commit();
	} catch (const std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to prune expired channel top posts: %1").arg(ex.what()));
	}
}

void evictLruChannels(int maxChannels) {
	std::lock_guard lock(databaseMutex);
	try {
		evictLruChannelsLocked(maxChannels);
	} catch (const std::exception &ex) {
		try {
			storage.rollback();
		} catch (...) {
		}
		LOG(("Failed to evict LRU channel top posts: %1").arg(ex.what()));
	}
}

rpl::event_stream<std::pair<ID, int>> channelPostRemovedStream;
rpl::event_stream<ID> channelPostsClearedStream;

rpl::producer<std::pair<ID, int>> channelPostRemoved() {
	return channelPostRemovedStream.events();
}

rpl::producer<ID> channelPostsCleared() {
	return channelPostsClearedStream.events();
}

void clearChannelPosts(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<ChannelTopPost>(
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId)
		);
		storage.update_all(
			set(
				c(&ChannelTopPostState::scanWindow) = 0,
				c(&ChannelTopPostState::oldestScannedId) = 0,
				c(&ChannelTopPostState::lastScanTime) = 0
			),
			where(column<ChannelTopPostState>(&ChannelTopPostState::channelId) == channelId)
		);
		crl::on_main([=] {
			channelPostsClearedStream.fire_copy(channelId);
		});
	} catch (const std::exception &ex) {
		LOG(("Failed to clear channel top posts: %1").arg(ex.what()));
	}
}

void clearAllChannelPosts() {
	std::lock_guard lock(databaseMutex);
	try {
		storage.remove_all<ChannelTopPost>();
		storage.update_all(set(
			c(&ChannelTopPostState::scanWindow) = 0,
			c(&ChannelTopPostState::oldestScannedId) = 0,
			c(&ChannelTopPostState::lastScanTime) = 0));
		crl::on_main([=] {
			channelPostsClearedStream.fire(0);
		});
	} catch (const std::exception &ex) {
		LOG(("Failed to clear all channel top posts: %1").arg(ex.what()));
	}
}

void removePosts(ID channelId, const std::vector<int> &messageIds) {
	if (messageIds.empty()) {
		return;
	}
	std::lock_guard lock(databaseMutex);
	try {
		const auto count = storage.count<ChannelTopPost>(
			where(
				column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId
				&& in(&ChannelTopPost::messageId, messageIds)
			)
		);
		if (count == 0) {
			return;
		}
		storage.remove_all<ChannelTopPost>(
			where(
				column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId
				&& in(&ChannelTopPost::messageId, messageIds)
			)
		);
		crl::on_main([=] {
			for (const auto messageId : messageIds) {
				channelPostRemovedStream.fire({ channelId, messageId });
			}
		});
	} catch (const std::exception &ex) {
		LOG(("Failed to remove channel top posts: %1").arg(ex.what()));
	}
}

void removePost(ID channelId, int messageId) {
	removePosts(channelId, { messageId });
}

namespace {

int CountReactionQuality(
		const std::string &json,
		const std::vector<QString> &keys) {
	if (keys.empty() || json.empty()) {
		return 0;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(
		QByteArray::fromStdString(json),
		&error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return 0;
	}
	const auto object = document.object();
	auto result = 0;
	for (const auto &key : keys) {
		result += object.value(key).toInt();
	}
	return result;
}

} // namespace

std::vector<ChannelTopPost> getChannelTopPosts(
	ID channelId,
	TopPostsSortOrder sortOrder,
	TopPostsFilter filter,
	int totalLimit,
	int offset,
	const TopPostsReactionLists &lists) {
	std::lock_guard lock(databaseMutex);
	try {
		const auto isDoc = (column<ChannelTopPost>(&ChannelTopPost::mediaType)
			== static_cast<int>(TopPostMediaType::Document));
		const auto isMedia = (
			column<ChannelTopPost>(&ChannelTopPost::mediaType) == static_cast<int>(TopPostMediaType::Photo)
			|| column<ChannelTopPost>(&ChannelTopPost::mediaType) == static_cast<int>(TopPostMediaType::Video)
			|| column<ChannelTopPost>(&ChannelTopPost::mediaType) == static_cast<int>(TopPostMediaType::Grouped)
		);
		const auto baseWhere = (column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId);

		auto executeQuery = [&](auto whereClause) -> std::vector<ChannelTopPost> {
			if (sortOrder == TopPostsSortOrder::Reactions) {
				return queryTopPosts(
					whereClause,
					multi_order_by(
						order_by(column<ChannelTopPost>(&ChannelTopPost::reactionsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::commentsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::viewsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::date)).desc()
					),
					totalLimit,
					offset
				);
			} else if (sortOrder == TopPostsSortOrder::Comments) {
				return queryTopPosts(
					whereClause,
					multi_order_by(
						order_by(column<ChannelTopPost>(&ChannelTopPost::commentsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::reactionsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::viewsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::date)).desc()
					),
					totalLimit,
					offset
				);
			} else if (sortOrder == TopPostsSortOrder::Views) {
				return queryTopPosts(
					whereClause,
					multi_order_by(
						order_by(column<ChannelTopPost>(&ChannelTopPost::viewsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::reactionsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::commentsCount)).desc(),
						order_by(column<ChannelTopPost>(&ChannelTopPost::date)).desc()
					),
					totalLimit,
					offset
				);
			}
			const auto &keys = (sortOrder == TopPostsSortOrder::Negative)
				? lists.negative
				: lists.positive;
			auto scored = std::vector<std::pair<int, ChannelTopPost>>();
			for (auto &post : storage.get_all<ChannelTopPost>(where(whereClause))) {
				scored.emplace_back(
					CountReactionQuality(post.reactionsJson, keys),
					std::move(post));
			}
			std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
				if (a.first != b.first) {
					return a.first > b.first;
				}
				const auto &left = a.second;
				const auto &right = b.second;
				if (left.reactionsCount != right.reactionsCount) {
					return left.reactionsCount > right.reactionsCount;
				} else if (left.commentsCount != right.commentsCount) {
					return left.commentsCount > right.commentsCount;
				} else if (left.viewsCount != right.viewsCount) {
					return left.viewsCount > right.viewsCount;
				}
				return left.date > right.date;
			});
			auto result = std::vector<ChannelTopPost>();
			const auto from = std::min(
				static_cast<size_t>(std::max(offset, 0)),
				scored.size());
			const auto to = std::min(
				from + static_cast<size_t>(std::max(totalLimit, 0)),
				scored.size());
			result.reserve(to - from);
			for (auto i = from; i != to; ++i) {
				result.push_back(std::move(scored[i].second));
			}
			return result;
		};

		auto posts = (filter == TopPostsFilter::Documents)
			? executeQuery(baseWhere and isDoc)
			: (filter == TopPostsFilter::Media)
			? executeQuery(baseWhere and isMedia)
			: executeQuery(baseWhere);
		for (auto &post : posts) {
			post.positiveCount = CountReactionQuality(post.reactionsJson, lists.positive);
			post.negativeCount = CountReactionQuality(post.reactionsJson, lists.negative);
		}
		return posts;
	} catch (const std::exception &ex) {
		LOG(("Failed to get channel top posts: %1").arg(ex.what()));
		return {};
	}
}

bool hasChannelTopPosts(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		return !storage.select(
			columns(column<ChannelTopPost>(&ChannelTopPost::id)),
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId),
			limit(1)
		).empty();
	} catch (const std::exception &ex) {
		LOG(("Failed to check if channel has top posts: %1").arg(ex.what()));
		return false;
	}
}

int getChannelTopPostsCount(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		return static_cast<int>(storage.count<ChannelTopPost>(
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId)
		));
	} catch (const std::exception &ex) {
		LOG(("Failed to get channel top posts count: %1").arg(ex.what()));
		return 0;
	}
}

std::vector<int> getChannelTopPostMessageIds(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		return storage.select(
			&ChannelTopPost::messageId,
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId),
			multi_order_by(
				order_by(column<ChannelTopPost>(&ChannelTopPost::reactionsCount)).desc(),
				order_by(column<ChannelTopPost>(&ChannelTopPost::commentsCount)).desc(),
				order_by(column<ChannelTopPost>(&ChannelTopPost::viewsCount)).desc(),
				order_by(column<ChannelTopPost>(&ChannelTopPost::date)).desc()
			)
		);
	} catch (const std::exception &ex) {
		LOG(("Failed to get channel top post message ids: %1").arg(ex.what()));
		return {};
	}
}

int getChannelTopPostsMaxMessageId(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		const auto rows = storage.select(
			max(&ChannelTopPost::messageId),
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId)
		);
		if (!rows.empty() && rows.front()) {
			return *rows.front();
		}
		return 0;
	} catch (const std::exception &ex) {
		LOG(("Failed to get channel top post max message id: %1").arg(ex.what()));
		return 0;
	}
}

ChannelTopPostState getChannelTopPostState(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		if (const auto row = storage.get_pointer<ChannelTopPostState>(channelId)) {
			return *row;
		}
	} catch (const std::exception &ex) {
		LOG(("Failed to get channel top post state: %1").arg(ex.what()));
	}
	return ChannelTopPostState{ .channelId = channelId };
}

void saveChannelTopPostState(const ChannelTopPostState &state) {
	std::lock_guard lock(databaseMutex);
	try {
		storage.replace(state);
	} catch (const std::exception &ex) {
		LOG(("Failed to save channel top post state: %1").arg(ex.what()));
	}
}

void touchChannelTopPostAccess(ID channelId) {
	std::lock_guard lock(databaseMutex);
	try {
		const auto hasPosts = !storage.select(
			columns(&ChannelTopPost::id),
			where(column<ChannelTopPost>(&ChannelTopPost::channelId) == channelId),
			limit(1)).empty();
		if (!hasPosts) {
			return;
		}
		const auto now = base::unixtime::now();
		if (storage.get_pointer<ChannelTopPostState>(channelId)) {
			storage.update_all(
				set(c(&ChannelTopPostState::lastAccessTime) = now),
				where(column<ChannelTopPostState>(&ChannelTopPostState::channelId) == channelId));
			return;
		}
		auto state = ChannelTopPostState();
		state.channelId = channelId;
		state.lastAccessTime = now;
		storage.replace(state);
	} catch (const std::exception &ex) {
		LOG(("Failed to touch channel top post access: %1").arg(ex.what()));
	}
}
}
