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

#include "stdafx.h"
#include "DSUtil/Filehandle.h"
#include "DSUtil/text.h"
#include "PlayerYouTubeDL.h"

#include "rapidjsonHelper.h"

#define bufsize (2ul * KILOBYTE)

namespace YT_DLP
{
#define YOUTUBE_PL_URL              L"youtube.com/playlist?"
#define YOUTUBE_USER_URL            L"youtube.com/user/"
#define YOUTUBE_USER_SHORT_URL      L"youtube.com/c/"
#define YOUTUBE_CHANNEL_URL         L"youtube.com/channel/"
#define YOUTUBE_URL                 L"youtube.com/watch?"
#define YOUTUBE_URL_A               L"www.youtube.com/attribution_link"
#define YOUTUBE_URL_V               L"youtube.com/v/"
#define YOUTUBE_URL_EMBED           L"youtube.com/embed/"
#define YOUTUBE_URL_SHORTS          L"youtube.com/shorts/"
#define YOUTUBE_URL_CLIP            L"youtube.com/clip/"
#define YOUTUBE_URL_LIBRARY         L"youtube.com/@"
#define YOUTUBE_URL_LIVE            L"youtube.com/live/"
#define YOUTU_BE_URL                L"youtu.be/"

	bool CheckYoutubeVideo(CStringW url)
	{
		url.MakeLower();

		if (url.Find(YOUTUBE_URL) != -1
			|| url.Find(YOUTUBE_URL_A) != -1
			|| url.Find(YOUTUBE_URL_V) != -1
			|| url.Find(YOUTUBE_URL_EMBED) != -1
			|| url.Find(YOUTUBE_URL_SHORTS) != -1
			|| url.Find(YOUTUBE_URL_CLIP) != -1
			|| url.Find(YOUTUBE_URL_LIVE) != -1
			|| url.Find(YOUTU_BE_URL) != -1) {
			return true;
		}

		return false;
	}

	bool CheckYoutubePlaylist(CString url)
	{
		url.MakeLower();

		if (url.Find(YOUTUBE_PL_URL) != -1
			|| url.Find(YOUTUBE_USER_URL) != -1
			|| url.Find(YOUTUBE_CHANNEL_URL) != -1
			|| url.Find(YOUTUBE_USER_SHORT_URL) != -1
			|| url.Find(YOUTUBE_URL_LIBRARY) != -1
			|| (url.Find(YOUTUBE_URL) != -1 && url.Find(L"&list=") != -1)
			|| (url.Find(YOUTUBE_URL_A) != -1 && url.Find(L"/watch_videos?video_ids") != -1)
			|| ((url.Find(YOUTUBE_URL_V) != -1 || url.Find(YOUTUBE_URL_EMBED) != -1
				|| url.Find(YOUTU_BE_URL) != -1 || url.Find(YOUTUBE_URL_SHORTS) != -1) && url.Find(L"list=") != -1)) {
			return true;
		}

		return false;
	}

	static bool RunYtDlp(const CStringW& ydl_path, const CStringW& url, CStringA& buf_out)
	{
		HANDLE hStdout_r, hStdout_w;
		HANDLE hStderr_r, hStderr_w;

		SECURITY_ATTRIBUTES sec_attrib = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
		if (!CreatePipe(&hStdout_r, &hStdout_w, &sec_attrib, bufsize)) {
			return false;
		}
		if (!CreatePipe(&hStderr_r, &hStderr_w, &sec_attrib, bufsize)) {
			CloseHandle(hStdout_r);
			CloseHandle(hStdout_w);
			return false;
		}

		STARTUPINFOW startup_info = {};
		startup_info.cb = sizeof(STARTUPINFO);
		startup_info.hStdOutput = hStdout_w;
		startup_info.hStdError = hStderr_w;
		startup_info.wShowWindow = SW_HIDE;
		startup_info.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

		PROCESS_INFORMATION proc_info = {};
		CStringW args;
		args.Format(LR"(%s -j --all-subs --sub-format vtt --no-check-certificates "%s")", ydl_path.GetString(), url.GetString());

		const BOOL bSuccess = CreateProcessW(
			nullptr, args.GetBuffer(), nullptr, nullptr, TRUE, 0,
			nullptr, nullptr, &startup_info, &proc_info
		);
		CloseHandle(hStdout_w);
		CloseHandle(hStderr_w);

		if (!bSuccess) {
			CloseHandle(proc_info.hProcess);
			CloseHandle(proc_info.hThread);
			CloseHandle(hStdout_r);
			CloseHandle(hStderr_r);
			return false;
		}

		CStringA buf_err;

		std::thread stdout_thread;
		std::thread stderr_thread;

		if (hStdout_r) {
			stdout_thread = std::thread([&]() {
				char buffer[bufsize] = { 0 };
				DWORD dwBytesRead = 0;
				while (ReadFile(hStdout_r, buffer, bufsize, &dwBytesRead, nullptr)) {
					if (!dwBytesRead) {
						break;
					}
					buf_out.Append(buffer, dwBytesRead);
				}
				});
		}

		if (hStderr_r) {
			stderr_thread = std::thread([&]() {
				char buffer[bufsize] = { 0 };
				DWORD dwBytesRead = 0;
				while (ReadFile(hStderr_r, buffer, bufsize, &dwBytesRead, nullptr)) {
					if (!dwBytesRead) {
						break;
					}
					buf_err.Append(buffer, dwBytesRead);
				}
				});
		}

		WaitForSingleObject(proc_info.hProcess, INFINITE);

		if (stdout_thread.joinable()) {
			stdout_thread.join();
		}
		if (stderr_thread.joinable()) {
			stderr_thread.join();
		}

		DWORD exitcode;
		GetExitCodeProcess(proc_info.hProcess, &exitcode);

		CloseHandle(proc_info.hProcess);
		CloseHandle(proc_info.hThread);
		CloseHandle(hStdout_r);
		CloseHandle(hStderr_r);

		if (exitcode) {
			if (buf_err.Find("Unsupported URL:") == -1) {
				const CStringW ydl_fname = GetFileName(ydl_path);
				MessageBoxW(AfxGetApp()->GetMainWnd()->m_hWnd, CStringW(buf_err), ydl_fname, MB_ICONERROR | MB_OK);
			}
			return false;
		}

		return true;
	}

	int vformat_cmp(const yt_vformat_t& a, const yt_vformat_t& b)
	{
		if (a.protocol < b.protocol) {
			return -1;
		}
		if (a.protocol > b.protocol) {
			return +1;
		}

		if (a.codec < b.codec) {
			return -1;
		}
		if (a.codec > b.codec) {
			return +1;
		}

		if (a.height > b.height) {
			return -1;
		}
		if (a.height < b.height) {
			return +1;
		}

		if (a.fps > b.fps) {
			return -1;
		}
		if (a.fps < b.fps) {
			return +1;
		}

		if (a.hdr > b.hdr) {
			return -1;
		}
		if (a.hdr < b.hdr) {
			return +1;
		}

		if (a.bitrate > b.bitrate) {
			return -1;
		}
		if (a.bitrate < b.bitrate) {
			return +1;
		}

		return 0;
	}

	int aformat_cmp(const yt_aformat_t& a, const yt_aformat_t& b)
	{
		if (a.protocol < b.protocol) {
			return -1;
		}
		if (a.protocol > b.protocol) {
			return +1;
		}

		if (a.codec > b.codec) {
			return -1;
		}
		if (a.codec < b.codec) {
			return +1;
		}

		if (a.bitrate > b.bitrate) {
			return -1;
		}
		if (a.bitrate < b.bitrate) {
			return +1;
		}

		return 0;
	}

	static bool IsVideoFormat(const rapidjson::Value& format)
	{
		CStringA value_str;
		if (getJsonValue(format, "vcodec", value_str) && value_str != "none") {
			return true;
		}
		if (getJsonValue(format, "video_ext", value_str) && value_str != "none") {
			return true;
		}
		return false;
	}

	static bool FormatHasAudio(const rapidjson::Value& format)
	{
		CStringA value_str;
		if (getJsonValue(format, "acodec", value_str) && value_str != "none") {
			return true;
		}
		if (getJsonValue(format, "audio_ext", value_str) && value_str != "none") {
			return true;
		}
		return false;
	}

	yt_acodec_type GetAudioCodec(const rapidjson::Value& format)
	{
		yt_acodec_type acodec = acodec_none;

		CStringA value_str;

		if (getJsonValue(format, "acodec", value_str) && value_str != "none") {
			if (StartsWith(value_str, "mp4a")) {
				acodec = acodec_aac;
			}
			else if (StartsWith(value_str, "opus")) {
				acodec = acodec_opus;
			}
			else {
				acodec = acodec_unknoun;
			}
		}
		else if (getJsonValue(format, "audio_ext", value_str) && value_str != "none") {
			acodec = acodec_unknoun;
		}

		return acodec;
	}

	void SetVFormatDesc(yt_vformat_t& v)
	{
		auto& desc = v.desc;

		switch (v.protocol) {
		case protocol_dash:
			desc = L"DASH ";
			break;
		case protocol_hls:
			desc = L"HLS ";
			break;
		default:
			desc.Empty();
		}

		switch (v.codec) {
		case vcodec_h264:
			desc.Append(L"H.264 ");
			break;
		case vcodec_vp9:
			desc.Append(L"VP9 ");
			break;
		case vcodec_av1:
			desc.Append(L"AV1 ");
			break;
		case vcodec_unknoun:
			desc.Append(L"Video ");
			break;
		}

		if (desc.IsEmpty()) {
			desc = v.id;
			desc.AppendChar(' ');
		}

		if (v.height > 0) {
			desc.AppendFormat(L"%dp ", v.height);
		}

		if (v.fps >= 48) {
			desc.AppendFormat(L"%dfps ", v.fps);
		}

		if (v.hdr) {
			desc.Append(L"HDR ");
		}

		switch (v.audio) {
		case acodec_aac:
			desc.Append(L"AAC ");
			break;
		case acodec_opus:
			desc.Append(L"OPUS ");
			break;
		case acodec_unknoun:
			desc.Append(L"Audio ");
			break;
		}

		desc.TrimRight(' ');
	}

	void SetAFormatDesc(yt_aformat_t& a)
	{
		auto& desc = a.desc;

		switch (a.protocol) {
		case protocol_dash:
			desc = L"DASH ";
			break;
		case protocol_hls:
			desc = L"HLS ";
			break;
		default:
			desc.Empty();
		}

		switch (a.codec) {
		case acodec_aac:
			desc.Append(L"AAC ");
			break;
		case acodec_opus:
			desc.Append(L"OPUS ");
			break;
		case acodec_unknoun:
			desc.Append(L"Audio ");
			break;
		}

		if (desc.IsEmpty()) {
			desc = a.id;
			desc.AppendChar(' ');
		}

		if (a.bitrate > 0) {
			desc.AppendFormat(L"%.0f kbps", a.bitrate);
		}

		desc.TrimRight(' ');
	}


	void GetFormatsInfo(const rapidjson::Value* formats, const bool getUserAgent,
		std::vector<yt_vformat_t>& vformats, std::vector<yt_aformat_t>& aformats)
	{
		CStringA value_str;
		int   value_int = 0;
		float value_float = 0.0f;

		bool vcodec_defined = false;
		bool acodec_defined = false;

		for (const auto& format : formats->GetArray()) {
			// mandatory parameters

			CStringA format_id;
			CStringA url;
			yt_protocol_type protocol = protocol_unknoun;
			int bitrate = 0;

			if (!getJsonValue(format, "format_id", format_id)) {
				ASSERT(0);
				continue;
			}

			if (getJsonValue(format, "protocol", value_str)) {
				if (StartsWith(value_str, "http")) {
					if (StartsWith(format_id, "dash")) {
						protocol = protocol_dash;
					} else if (getJsonValue(format, "container", value_str) && EndsWith(value_str, "_dash")) {
						protocol = protocol_dash;
					} else {
						protocol = protocol_http;
					}
				}
				else if (StartsWith(value_str, "m3u8")) {
					protocol = protocol_hls;
				}
				else {
					continue;
				}
			}
			else {
				ASSERT(0);
				continue;
			}

			if (!getJsonValue(format, "url", url)) {
				ASSERT(0);
				continue;
			}

			// optional parameters

			if (IsVideoFormat(format)) {
				yt_vformat_t vformat;
				vformat.id = format_id;
				vformat.protocol = protocol;
				vformat.url = url;
				vformat.bitrate = bitrate;

				getJsonValue(format, "height", vformat.height);

				if (getJsonValue(format, "vcodec", value_str)) {
					if (StartsWith(value_str, "avc1") || value_str == "h264") {
						vformat.codec = vcodec_h264;
					}
					else if (StartsWith(value_str, "vp9")) {
						vformat.codec = vcodec_vp9;
					}
					else if (StartsWith(value_str, "av01")) {
						vformat.codec = vcodec_av1;
					}
					vcodec_defined = (vformat.codec != vcodec_unknoun);
				}

				if (getJsonValue(format, "fps", value_int) && value_int > 0) {
					vformat.fps = value_int;
				}

				if (getJsonValue(format, "dynamic_range", value_str) && value_str != "SDR") {
					vformat.hdr = true;
				}

				getJsonValue(format, "tbr", vformat.bitrate);

				vformat.audio = GetAudioCodec(format);

				if (getUserAgent) {
					if (auto http_headers = GetJsonObject(format, "http_headers")) {
						getJsonValue(format, "User-Agent", vformat.user_agent);
					}
				}

				vformats.emplace_back(vformat);
			}
			else if (FormatHasAudio(format)) {
				if (EndsWith(format_id, "-drc")) {
					continue;
				}

				yt_aformat_t aformat;
				aformat.id = format_id;
				aformat.protocol = protocol;
				aformat.url = url;
				aformat.bitrate = bitrate;

				aformat.codec = GetAudioCodec(format);
				if (aformat.codec >= 0) {
					acodec_defined = true;
				}

				getJsonValue(format, "tbr", aformat.bitrate);

				if (getUserAgent) {
					if (auto http_headers = GetJsonObject(format, "http_headers")) {
						getJsonValue(format, "User-Agent", aformat.user_agent);
					}
				}

				aformats.emplace_back(aformat);
			}
		}

		// If some codecs are identified, then remove formats with unknown codecs.
		if (vcodec_defined) {
			vformats.erase(std::remove_if(vformats.begin(), vformats.end(), [](yt_vformat_t v) { return v.codec == vcodec_unknoun; }), vformats.end());
		}
		if (acodec_defined) {
			aformats.erase(std::remove_if(aformats.begin(), aformats.end(), [](yt_aformat_t a) { return a.codec == acodec_unknoun; }), aformats.end());
		}

		std::sort(vformats.begin(), vformats.end(), [](const yt_vformat_t& a, const yt_vformat_t& b) {
			return (vformat_cmp(a, b) < 0);
			});
		std::sort(aformats.begin(), aformats.end(), [](const yt_aformat_t& a, const yt_aformat_t& b) {
			return (aformat_cmp(a, b) < 0);
			});

		for (auto& v : vformats) {
			SetVFormatDesc(v);
		}
		for (auto& a : aformats) {
			SetAFormatDesc(a);
		}
	}

	struct yt_vformats_stats {
		bool live = false;
		bool containsDASH = false;
		bool containsHLS = false;
		int heightMin = INT_MAX;
		int heightMax = 0;
		int fpsMin = INT_MAX;
		int fpsMax = 0;
	};

	///////////////////////////////////////////////////////////

	bool Parse_URL(
		const CStringW& url,        // input parameter
		yt_info_t& ytInfo,
		std::vector<yt_vformat_t>& vformats,
		std::vector<yt_aformat_t>& aformats,
		OpenFileData* pOFD
	)
	{
		ytInfo.Clear();
		vformats.clear();
		aformats.clear();
		pOFD->Clear();

		// get a copy of the settings

		CAppSettings& s = AfxGetAppSettings();

		const CStringW ydl_path = GetFullExePath(s.strYdlExePath, true);
		if (ydl_path.IsEmpty()) {
			return false;
		}

		const int  iMaxHeight   = s.iYdlMaxHeight;
		const bool bHighFps     = s.bYdlHighFps;
		const bool bHighBitrate = s.bYdlHighBitrate;
		CStringA lang(s.strYdlAudioLang);

		// run yt-dlp.exe

		CStringA buf_out;
		bool ok = RunYtDlp(ydl_path, url, buf_out);
		if (!ok || buf_out.IsEmpty()) {
			return false;
		}

		// processing a JSON file

		rapidjson::Document doc;
		const int k = buf_out.Find("\n{\"", 64); // check presence of second JSON root element and ignore it
		if (doc.Parse(buf_out.GetString(), k > 0 ? k : buf_out.GetLength()).HasParseError()) {
			return false;
		}

		auto formats = GetJsonArray(doc, "formats");
		if (!formats) {
			return false;
		}

		GetFormatsInfo(formats, ytInfo.userAgent.IsEmpty(), vformats, aformats);
		if (vformats.empty() && aformats.empty()) {
			return false;
		}


#ifdef _DEBUG
		for (const auto& v : vformats) {
			DLog(v.desc);
		}
		for (const auto& a : aformats) {
			DLog(a.desc);
		}
#endif

		yt_vformats_stats vstats = {};
		{
			CStringA liveStatus;
			getJsonValue(doc, "live_status", liveStatus);
			vstats.live = (liveStatus == "is_live");

			for (const auto& v : vformats) {
				switch (v.protocol) {
				case protocol_dash:
					vstats.containsDASH = true;
					break;
				case protocol_hls:
					vstats.containsHLS = true;
					break;
				}

				if (v.height > 0) {
					if (v.height < vstats.heightMin) {
						vstats.heightMin = v.height;
					}
					if (v.height > vstats.heightMax) {
						vstats.heightMax = v.height;
					}
				}

				if (v.fps > 0) {
					if (v.fps < vstats.fpsMin) {
						vstats.fpsMin = v.fps;
					}
					if (v.fps > vstats.fpsMax) {
						vstats.fpsMax = v.fps;
					}
				}
			}

			if (vstats.heightMax == 0) {
				vstats.heightMin = 0;
			}
			if (vstats.fpsMax == 0) {
				vstats.fpsMin = 0;
			}
		}

		bool bIsYoutube = CheckYoutubeVideo(url);
		int iTag = 1;

		int vid_height = 0;
		bool bVideoOnly = false;

		float aud_bitrate = 0.0f;
		CStringA bestAudioUrl;
		struct audio_info_t {
			float tbr = 0;
			CStringA url;
		};
		std::map<CStringA, audio_info_t> audioUrls;

		CStringA bestUrl;
		getJsonValue(doc, "url", bestUrl);


		CStringA liveStatus;
		getJsonValue(doc, "live_status", liveStatus);
		bool bIsLive = liveStatus == "is_live";

		if (getJsonValue(doc, "title", ytInfo.title)) {
			CStringA ext;
			if (getJsonValue(doc, "ext", ext)) {
				ytInfo.fname.Format(L"%s.%hs", ytInfo.title.GetString(), ext.GetString());
			}
		}

		getJsonValue(doc, "uploader", ytInfo.author);

		getJsonValue(doc, "description", ytInfo.content);
		if (ytInfo.content.Find('\n') && ytInfo.content.Find(L"\r\n") == -1) {
			ytInfo.content.Replace(L"\n", L"\r\n");
		}

		CStringA upload_date;
		if (getJsonValue(doc, "upload_date", upload_date)) {
			WORD y, m, d;
			if (sscanf_s(upload_date.GetString(), "%04hu%02hu%02hu", &y, &m, &d) == 3) {
				ytInfo.dtime.wYear = y;
				ytInfo.dtime.wMonth = m;
				ytInfo.dtime.wDay = d;
			}
		}

		// subtitles
		if (auto requested_subtitles = GetJsonObject(doc, "requested_subtitles")) {
			for (const auto& subtitle : requested_subtitles->GetObj()) {
				CStringW sub_url;
				CStringW sub_name;
				getJsonValue(subtitle.value, "url", sub_url);
				getJsonValue(subtitle.value, "name", sub_name);
				CStringA sub_lang = subtitle.name.GetString();

				if (sub_url.GetLength() && sub_lang.GetLength()) {
					pOFD->subs.emplace_back(sub_url, sub_name, sub_lang);
				}
			}
		}

		// chapters
		if (auto chapters = GetJsonArray(doc, "chapters")) {
			for (const auto& chapter : chapters->GetArray()) {
				float start_time = 0.0f;
				CString title;
				if (getJsonValue(chapter, "title", title) && getJsonValue(chapter, "start_time", start_time)) {
					ytInfo.chapters.emplace_back(title, REFERENCE_TIME(start_time * UNITS));
				}
			}
		}

		// thumbnails
		if (auto thumbnails = GetJsonArray(doc, "thumbnails")) {
			CStringA thumbnailUrl;
			int maxHeight = 0;
			for (const auto& elem : thumbnails->GetArray()) {
				int height = 0;
				if (getJsonValue(elem, "height", height) && height > maxHeight) {
					if (getJsonValue(elem, "url", thumbnailUrl)) {
						maxHeight = height;
					}
				}
			}

			if (!thumbnailUrl.IsEmpty()) {
				ytInfo.thumbnailUrl = thumbnailUrl;
			}
		}

		return pOFD->fi.Valid();
	}
}
