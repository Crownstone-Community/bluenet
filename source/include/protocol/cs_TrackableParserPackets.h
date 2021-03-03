/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Mar 2, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

// ------------------ Command values -----------------

struct __attribute__((__packed__)) trackable_parser_cmd_upload_filter_t {
	uint8_t filterId;
	uint16_t chunkStartIndex;
	uint16_t totalSize;
	uint16_t chunkSize;
	uint8_t chunk[]; // flexible array, sizeof packet depends on chunkSize.
};

struct __attribute__((__packed__)) trackable_parser_cmd_remove_filter_t {
	uint8_t filterId;
};

struct __attribute__((__packed__)) trackable_parser_cmd_commit_filter_changes_t {
	uint16_t masterVersion;
	uint16_t masterCrc;
};

struct __attribute__((__packed__)) trackable_parser_cmd_get_filer_summaries_t {
	// empty
};

// ------------------ Return values ------------------


struct __attribute__((__packed__)) trackable_parser_cmd_upload_filter_ret_t {

};

struct __attribute__((__packed__)) trackable_parser_cmd_remove_filter_ret_t {

};

struct __attribute__((__packed__)) trackable_parser_cmd_commit_filter_changes_ret_t {

};

struct __attribute__((__packed__)) trackable_parser_cmd_get_filer_summaries_ret_t {

};
