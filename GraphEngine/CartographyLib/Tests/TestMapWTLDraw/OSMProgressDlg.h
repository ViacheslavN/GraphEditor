// OSMProgressDlg.h : modal progress window of an OSM task (reading the map / conversion) running in a background thread
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef HAVE_OSM_CONVERTOR

#include "../../../Convertors/OSM/OSMConvertorLib/OSMConvertorLib.h"
#include "../../drawer/TrackCancel.h"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

#define WM_OSM_PROGRESS     (WM_APP + 10)
#define WM_OSM_TASK_DONE    (WM_APP + 11)

class COSMProgressDlg : public CDialogImpl<COSMProgressDlg>
{
public:
	enum { IDD = IDD_OSM_PROGRESS };

	// runs in the background thread, the exceptions are shown as the error of the task
	typedef std::function<void(GraphEngine::Convertors::IProgressUpdaterPtr, GraphEngine::Display::ITrackCancelPtr)> TTask;

	// the totals (from the OSM map) give the percentage, 0 - unknown (reading the map)
	COSMProgressDlg(const std::wstring& sCaption, TTask task, uint64_t nTotalNodes = 0, uint64_t nTotalWays = 0, uint64_t nTotalRelations = 0);
	~COSMProgressDlg();

	// DoModal: IDOK - done, IDCANCEL - canceled by the user, IDABORT - failed (GetError)
	const std::wstring& GetError() const { return m_sError; }
	double GetElapsedSeconds() const { return m_dElapsed; }

	BEGIN_MSG_MAP(COSMProgressDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_OSM_PROGRESS, OnProgress)
		MESSAGE_HANDLER(WM_OSM_TASK_DONE, OnTaskDone)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnProgress(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTaskDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	// called by the converter thread: keeps the last state and posts one message at a time to the dialog
	class CUpdater : public GraphEngine::Convertors::IProgressUpdater
	{
	public:
		explicit CUpdater(HWND hWnd) : m_hWnd(hWnd), m_bPosted(false) {}

		virtual void UpdateStatusInfo(const GraphEngine::Convertors::SOSMProgress& progress)
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_progress = progress;
			}
			if(!m_bPosted.exchange(true))
				::PostMessage(m_hWnd, WM_OSM_PROGRESS, 0, 0);
		}

		// UI thread: the last state, the next update posts again
		GraphEngine::Convertors::SOSMProgress Take()
		{
			m_bPosted = false;
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_progress;
		}

	private:
		HWND m_hWnd;
		std::mutex m_mutex;
		GraphEngine::Convertors::SOSMProgress m_progress;
		std::atomic<bool> m_bPosted;
	};

	void ShowProgress(const GraphEngine::Convertors::SOSMProgress& progress);
	int  Percent(const GraphEngine::Convertors::SOSMProgress& progress) const;   // -1 - unknown
	void ShowElapsed();

private:
	std::wstring m_sCaption;
	TTask m_task;
	uint64_t m_nTotalNodes;
	uint64_t m_nTotalWays;
	uint64_t m_nTotalRelations;

	std::shared_ptr<CUpdater> m_ptrUpdater;
	GraphEngine::Cartography::CTrackCancelPtr m_ptrCancel;
	std::thread m_thread;
	std::string m_sThreadError;     // set by the thread before WM_OSM_TASK_DONE
	std::wstring m_sError;
	bool m_bCanceling;
	bool m_bMarquee;
	std::chrono::steady_clock::time_point m_start;
	double m_dElapsed;

	CProgressBarCtrl m_progressBar;
};

#endif
