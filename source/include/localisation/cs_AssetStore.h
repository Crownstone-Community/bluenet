/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Aug 17, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#include <common/cs_Component.h>
#include <events/cs_EventListener.h>

#include <localisation/cs_AssetRecord.h>
#include <util/cs_Coroutine.h>

class AssetStore : public EventListener, public Component {
private:
	static constexpr auto MAX_REPORTS = 20u;
	static constexpr auto COUNTER_UPDATE_PERIOD_MS = 100;


public:
	AssetStore();
	cs_ret_code_t init() override;

	void handleEvent(event_t& evt) override;

	/**
	 * Get or create a record for the given assetId.
	 * Then update rssi values according to the incoming scan and
	 * revert the lastReceivedCounter to 0.
	 */
	void handleAcceptedAsset(const scanned_device_t& asset, const short_asset_id_t& assetId);

	/**
	 * returns a pointer of record if found,
	 * else tries to create a new blank record and return a pointer to that,
	 * else returns nullptr.
	 */
	asset_record_t* getOrCreateRecord(const short_asset_id_t& id);

	/**
	 * returns a pointer of record if found,
	 * else returns nullptr.
	 */
	asset_record_t* getRecord(const short_asset_id_t& id);


private:
	/**
	 * Relevant data for this algorithm per asset_id.
	 * Possible strategies to reduce memory:
	 *  - when full, remove worst personal_rssi.
	 *  - only store if observed the asset personally.
	 *  - ...
	 *
	 *  The array is 'front loaded'. entries with index < _assetRecordCount
	 *  are valid, other entries are not.
	 */
	asset_record_t _assetRecords[MAX_REPORTS];

	/**
	 * Current number of valid records in the _assetRecords array.
	 */
	uint8_t _assetRecordCount = 0;

	/**
	 * Assumes my_id is set to the stone id of this crownstone.
	 * Sets the reporter id of the personal report to my_id.
	 * Sets the reporter id of the winning report to 0.
	 * Sets the rssi value of both these reports to -127.
	 */
	void resetReports();

	/**
	 * Adds 1 to the update/sent counters of a record, until 0xff is reached.
	 */
	void incrementRecordCounters();

	Coroutine counterUpdateRoutine;

};
