/*
 * (C) 2018-2026 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "PlayerYouTube.h"

namespace YT_DLP
{
	enum yt_vcodec_type {
		vcodec_unknoun = -1,
		vcodec_h264 = 0,
		vcodec_vp9,
		vcodec_av1,
	};
	enum yt_acodec_type {
		acodec_none = -2,
		acodec_unknoun = -1,
		acodec_aac = 0,
		acodec_opus,
	};
	enum yt_protocol_type {
		protocol_unknoun = -1,
		protocol_http = 0,
		protocol_dash,
		protocol_hls,
	};

	struct yt_vformat_t {
		CStringA id;
		yt_protocol_type protocol = protocol_unknoun;
		yt_vcodec_type codec = vcodec_unknoun;
		yt_acodec_type audio = acodec_none;
		int height = 0;
		float bitrate = 0;
		int fps = 0;
		bool hdr = false;
		CStringA url;
		CStringA user_agent;
		CStringW desc;
	};

	struct yt_aformat_t {
		CStringA id;
		yt_protocol_type protocol = protocol_unknoun;
		yt_acodec_type codec = acodec_unknoun;
		float bitrate = 0;
		CStringA language;
		int language_preference = 0;
		CStringA url;
		CStringA user_agent;
		CStringW desc;
	};

	bool Parse_URL(
		const CStringW& url,        // input parameter
		Youtube::YoutubeFields& y_fields,
		Youtube::YoutubeUrllist& youtubeUrllist,
		Youtube::YoutubeUrllist& youtubeAudioUrllist,
		OpenFileData* pOFD
	);
	void Clear();
}
