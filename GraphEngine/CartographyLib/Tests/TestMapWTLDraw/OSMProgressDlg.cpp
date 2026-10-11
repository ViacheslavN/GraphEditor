// OSMProgressDlg.cpp : implementation of the COSMProgressDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#ifdef HAVE_OSM_CONVERTOR

#include "OSMProgressDlg.h"
#include "DialogUtils.h"

using namespace GraphEngine;
using namespace GraphEngine::Convertors;

namespace
{
	const UINT_PTR TimeTimerId = 1;
	const int ProgressRange = 1000;

	// 1234567 -> "1 234 567"
	std::wstring FormatCount(uint64_t nValue)
	{
		std::wstring sText = std::to_wstring(nValue);
		for(int i = (int)sText.size() - 3; i > 0; i -= 3)
			sText.insert(i, 1, L' ');
		return sText;
	}

	const wchar_t* StageText(eOSMStage stage)
	{
		switch(stage)
		{
			case OSMStageAnalyze:       return L"Analyzing the file";
			case OSMStageNodes:         return L"Reading nodes and relations";
			case OSMStageSortNodes:     return L"Indexing node coordinates";
			case OSMStageWays:          return L"Reading ways";
			case OSMStageMultipolygons: return L"Building multipolygons";
			case OSMStageSplit:         return L"Splitting the file";
			default:                    return L"Saving tables";
		}
	}

	double Fraction(uint64_t nValue, uint64_t nTotal)
	{
		return nTotal ? (std::min)(1., (double)nValue / (double)nTotal) : 0.;
	}

	// 1.5 GB, 236 MB, 12 KB
	std::wstring FormatBytes(uint64_t nBytes)
	{
		wchar_t szText[64];
		if(nBytes >= 1024ull * 1024 * 1024)
			swprintf(szText, 64, L"%.2f GB", nBytes / (1024. * 1024 * 1024));
		else if(nBytes >= 1024 * 1024)
			swprintf(szText, 64, L"%.0f MB", nBytes / (1024. * 1024));
		else
			swprintf(szText, 64, L"%.0f KB", nBytes / 1024.);
		return szText;
	}

	std::wstring FormatTime(double dSeconds)
	{
		int nSeconds = (int)(dSeconds + 0.5);
		wchar_t szText[64];
		swprintf(szText, 64, L"%02d:%02d:%02d", nSeconds / 3600, (nSeconds / 60) % 60, nSeconds % 60);
		return szText;
	}

	// the parts of the conversion time (central-america: nodes and relations 14%, ways 69%, multipolygons 5%, indexes 12%)
	const double NodesEnd = 0.15;       // 2 passes; 1 pass (no ways) - the nodes until FinishStart1
	const double SortEnd = 0.17;
	const double WaysEnd = 0.82;
	const double MultipolygonsEnd = 0.88;
	const double FinishStart1 = 0.85;

	enum eItemColumns { ColWork, ColCount, ColData, ColBusy, ColInfo };

	// 12.3 GB, 185 MB/s (of the stage time)
	std::wstring DataItemText(uint64_t nBytes, double dStageSeconds)
	{
		if(nBytes == 0)
			return std::wstring();
		std::wstring sText = FormatBytes(nBytes);
		if(dStageSeconds >= 1.)
		{
			wchar_t szRate[64];
			swprintf(szRate, 64, L", %.0f MB/s", nBytes / (1024. * 1024) / dStageSeconds);
			sText += szRate;
		}
		return sText;
	}

	// 12.3 s, 45% of the stage; a pool of threads: the CPU time and how many threads were busy on average
	std::wstring BusyItemText(double dSeconds, bool bThreads, double dStageSeconds)
	{
		if(dSeconds <= 0.)
			return std::wstring();
		wchar_t szText[96];
		if(bThreads)
			swprintf(szText, 96, L"%.1f s CPU, x%.1f", dSeconds, dStageSeconds > 0 ? dSeconds / dStageSeconds : 0.);
		else
			swprintf(szText, 96, L"%.1f s, %.0f%%", dSeconds, dStageSeconds > 0 ? (std::min)(100., 100. * dSeconds / dStageSeconds) : 0.);
		return szText;
	}
}

COSMProgressDlg::COSMProgressDlg(const std::wstring& sCaption, TTask task, uint64_t nTotalNodes, uint64_t nTotalWays, uint64_t nTotalRelations) :
	m_sCaption(sCaption), m_task(task), m_nTotalNodes(nTotalNodes), m_nTotalWays(nTotalWays), m_nTotalRelations(nTotalRelations),
	m_ptrCancel(std::make_shared<Cartography::CTrackCancel>()), m_bCanceling(false), m_bMarquee(false), m_dElapsed(0.), m_dFraction(-1.), m_nPass(0)
{

}

COSMProgressDlg::~COSMProgressDlg()
{
	// the dialog doesn't end before the thread, this is only a safety net
	if(m_thread.joinable())
	{
		m_ptrCancel->Cancel();
		m_thread.join();
	}
}

LRESULT COSMProgressDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());
	SetWindowText(m_sCaption.c_str());

	m_progressBar = GetDlgItem(IDC_PROGRESS_BAR);
	m_progressBar.SetRange32(0, ProgressRange);

	m_listItems = GetDlgItem(IDC_PROGRESS_ITEMS);
	m_listItems.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
	m_listItems.InsertColumn(ColWork, L"Work", LVCFMT_LEFT, 190);
	m_listItems.InsertColumn(ColCount, L"Count", LVCFMT_RIGHT, 100);
	m_listItems.InsertColumn(ColData, L"Data", LVCFMT_RIGHT, 140);
	m_listItems.InsertColumn(ColBusy, L"Busy", LVCFMT_RIGHT, 150);
	m_listItems.InsertColumn(ColInfo, L"Details", LVCFMT_LEFT, 600);

	m_start = std::chrono::steady_clock::now();
	m_passStart = m_start;
	SetTimer(TimeTimerId, 500);
	ShowElapsed();

	m_ptrUpdater = std::make_shared<CUpdater>(m_hWnd);
	IProgressUpdaterPtr ptrUpdater = m_ptrUpdater;
	Display::ITrackCancelPtr ptrCancel = m_ptrCancel;
	HWND hWnd = m_hWnd;
	m_thread = std::thread([this, hWnd, ptrUpdater, ptrCancel]()
	{
		try
		{
			m_task(ptrUpdater, ptrCancel);
		}
		catch (std::exception& exc)
		{
			m_sThreadError = exc.what();
		}
		catch (...)
		{
			m_sThreadError = "Unknown error";
		}
		::PostMessage(hWnd, WM_OSM_TASK_DONE, 0, 0);
	});

	return TRUE;
}

double COSMProgressDlg::TaskFraction(const SOSMProgress& progress) const
{
	// the part of the pass: the bytes of a .pbf file, the objects by the counts of the map (Analyze) for an .osm file
	const bool bTwoPasses = progress.nPassCount > 1;
	const bool bBytes = progress.nBytesTotal > 0;
	const bool bCounts = m_nTotalNodes + m_nTotalWays + m_nTotalRelations > 0;
	double dPass = -1.;
	switch(progress.stage)
	{
		case OSMStageAnalyze:
			return bBytes ? Fraction(progress.nBytesRead, progress.nBytesTotal) : -1.;
		case OSMStageNodes:
			if(bBytes)
				dPass = Fraction(progress.nBytesRead, progress.nBytesTotal);
			else if(bCounts)
				dPass = Fraction(progress.nNodes + progress.nRelations, m_nTotalNodes + m_nTotalRelations);
			return dPass < 0 ? -1. : dPass * (bTwoPasses ? NodesEnd : FinishStart1);
		case OSMStageSortNodes:
			return NodesEnd + (SortEnd - NodesEnd) * Fraction(progress.nNodes, m_nTotalNodes);
		case OSMStageWays:
			if(bBytes)
				dPass = Fraction(progress.nBytesRead, progress.nBytesTotal);
			else if(m_nTotalWays)
				dPass = Fraction(progress.nWays, m_nTotalWays);
			return dPass < 0 ? -1. : SortEnd + (WaysEnd - SortEnd) * dPass;
		case OSMStageMultipolygons:
			return WaysEnd + (MultipolygonsEnd - WaysEnd) * Fraction(progress.nStepDone, progress.nStepTotal);
		case OSMStageSplit:
			// one pass over a sorted file, the second pass of an unsorted one (the nodes before it)
			if(!bBytes)
				return -1.;
			dPass = Fraction(progress.nBytesRead, progress.nBytesTotal);
			return bTwoPasses ? SortEnd + (0.98 - SortEnd) * dPass : 0.98 * dPass;
		default:
		{
			double dStart = bTwoPasses ? MultipolygonsEnd : FinishStart1;
			return dStart + (1. - dStart) * Fraction(progress.nStepDone, progress.nStepTotal);
		}
	}
}

std::wstring COSMProgressDlg::DataText(const SOSMProgress& progress)
{
	if(progress.nPass > 0)
	{
		// the speed is measured from the start of the pass
		auto now = std::chrono::steady_clock::now();
		if(progress.nPass != m_nPass)
		{
			m_nPass = progress.nPass;
			m_passStart = now;
		}

		std::wstring sText = L"Pass " + std::to_wstring(progress.nPass) + L" of " + std::to_wstring((std::max)(progress.nPass, progress.nPassCount));
		if(progress.nBytesTotal == 0)
		{
			if(progress.nFileSize)
				sText += L", the file is " + FormatBytes(progress.nFileSize);
			return sText;
		}

		sText += L": " + FormatBytes(progress.nBytesRead) + L" of " + FormatBytes(progress.nBytesTotal);
		wchar_t szPercent[32];
		swprintf(szPercent, 32, L" (%.0f%%)", 100. * Fraction(progress.nBytesRead, progress.nBytesTotal));
		sText += szPercent;
		// a pass over the blocks of some objects (the ways) reads a part of the file
		if(progress.nFileSize && progress.nBytesTotal < progress.nFileSize * 98 / 100)
			sText += L", file " + FormatBytes(progress.nFileSize);

		double dSeconds = std::chrono::duration<double>(now - m_passStart).count();
		if(dSeconds >= 2. && progress.nBytesRead > 0)
		{
			double dRate = progress.nBytesRead / dSeconds;
			wchar_t szRate[64];
			swprintf(szRate, 64, L"\n%.1f MB/s", dRate / (1024. * 1024));
			sText += szRate;
			if(progress.nBytesRead < progress.nBytesTotal)
				sText += L", the pass ends in about " + FormatTime((progress.nBytesTotal - progress.nBytesRead) / dRate);
		}
		return sText;
	}

	m_nPass = 0;
	if(progress.nStepTotal > 0)
	{
		std::wstring sDone = FormatCount(progress.nStepDone) + L" of " + FormatCount(progress.nStepTotal);
		if(progress.stage == OSMStageMultipolygons)
			return L"Multipolygons: " + sDone;
		if(progress.stage == OSMStageFinish)
			return L"Layers saved (spatial index): " + sDone;
	}
	return std::wstring();
}

void COSMProgressDlg::ShowProgress(const SOSMProgress& progress)
{
	if(!m_bCanceling)
	{
		std::wstring sStage = StageText(progress.stage);
		if(!progress.sMessage.empty())
			sStage = DialogUtils::Utf8ToWide(progress.sMessage);
		SetDlgItemText(IDC_PROGRESS_STAGE, sStage.c_str());
	}

	m_dFraction = TaskFraction(progress);
	int nPercent = m_dFraction < 0 ? -1 : (int)(m_dFraction * ProgressRange);
	bool bMarquee = nPercent < 0;
	if(bMarquee != m_bMarquee)
	{
		// the marquee style is switched at run time (common controls 6)
		if(bMarquee)
		{
			m_progressBar.ModifyStyle(0, PBS_MARQUEE);
			m_progressBar.SetMarquee(TRUE, 30);
		}
		else
		{
			m_progressBar.SetMarquee(FALSE);
			m_progressBar.ModifyStyle(PBS_MARQUEE, 0);
			m_progressBar.SetRange32(0, ProgressRange);
		}
		m_bMarquee = bMarquee;
	}
	if(!bMarquee)
		m_progressBar.SetPos(nPercent);

	SetDlgItemText(IDC_PROGRESS_DATA, DataText(progress).c_str());

	std::wstring sCounts = L"Nodes: " + FormatCount(progress.nNodes);
	if(m_nTotalNodes)
		sCounts += L" / " + FormatCount(m_nTotalNodes);
	sCounts += L"    Ways: " + FormatCount(progress.nWays);
	if(m_nTotalWays)
		sCounts += L" / " + FormatCount(m_nTotalWays);
	sCounts += L"    Relations: " + FormatCount(progress.nRelations);
	if(m_nTotalRelations)
		sCounts += L" / " + FormatCount(m_nTotalRelations);
	if(progress.stage != OSMStageAnalyze)
		sCounts += L"\nWritten features / records: " + FormatCount(progress.nFeatures);
	if(progress.dStageSeconds > 0)
		sCounts += L"    The stage: " + FormatTime(progress.dStageSeconds);
	SetDlgItemText(IDC_PROGRESS_COUNTS, sCounts.c_str());
	ShowItems(progress);
}

void COSMProgressDlg::ShowItems(const SOSMProgress& progress)
{
	struct SRow
	{
		std::wstring sCells[ColInfo + 1];
	};
	std::vector<SRow> rows;
	for(const SOSMStageSummary& stage : progress.vecStages)
	{
		SRow row;
		row.sCells[ColWork] = L"[done] " + DialogUtils::Utf8ToWide(stage.sMessage);
		row.sCells[ColBusy] = FormatTime(stage.dSeconds);
		rows.push_back(row);
	}
	for(const SOSMProgressItem& item : progress.vecItems)
	{
		SRow row;
		row.sCells[ColWork] = DialogUtils::Utf8ToWide(item.sName);
		row.sCells[ColCount] = item.nCount ? FormatCount(item.nCount) : std::wstring();
		row.sCells[ColData] = DataItemText(item.nBytes, progress.dStageSeconds);
		row.sCells[ColBusy] = BusyItemText(item.dSeconds, item.bThreads, progress.dStageSeconds);
		row.sCells[ColInfo] = DialogUtils::Utf8ToWide(item.sInfo);
		rows.push_back(row);
	}

	// the rows are updated in place (no flicker), the number of rows changes with the stage
	m_listItems.SetRedraw(FALSE);
	while(m_listItems.GetItemCount() > (int)rows.size())
		m_listItems.DeleteItem(m_listItems.GetItemCount() - 1);
	for(size_t i = 0; i < rows.size(); ++i)
	{
		int nItem = (int)i;
		if(nItem >= m_listItems.GetItemCount())
			m_listItems.InsertItem(nItem, rows[i].sCells[ColWork].c_str());
		for(int nCol = ColWork; nCol <= ColInfo; ++nCol)
		{
			wchar_t szOld[1024] = {0};
			m_listItems.GetItemText(nItem, nCol, szOld, 1024);
			if(rows[i].sCells[nCol] != szOld)
				m_listItems.SetItemText(nItem, nCol, rows[i].sCells[nCol].c_str());
		}
	}
	m_listItems.SetRedraw(TRUE);
}

void COSMProgressDlg::ShowElapsed()
{
	m_dElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
	std::wstring sText = L"Elapsed: " + FormatTime((double)(int)m_dElapsed);
	// the whole task by the parts of the stages (the speed of a stage depends on the data: an estimate)
	if(m_dFraction >= 0.02 && m_dFraction < 1. && m_dElapsed >= 5. && !m_bCanceling)
		sText += L"      Left: about " + FormatTime(m_dElapsed * (1. - m_dFraction) / m_dFraction);
	SetDlgItemText(IDC_PROGRESS_TIME, sText.c_str());
}

LRESULT COSMProgressDlg::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if(wParam != TimeTimerId)
	{
		bHandled = FALSE;
		return 0;
	}
	ShowElapsed();
	return 0;
}

LRESULT COSMProgressDlg::OnProgress(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if(m_ptrUpdater.get())
	{
		m_lastProgress = m_ptrUpdater->Take();
		ShowProgress(m_lastProgress);
	}
	return 0;
}

LRESULT COSMProgressDlg::OnTaskDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if(m_thread.joinable())
		m_thread.join();
	KillTimer(TimeTimerId);
	if(m_ptrUpdater.get())
		m_lastProgress = m_ptrUpdater->Take();   // the final state (the summary of the last stage)
	ShowElapsed();

	// canceled: the task stops with the "canceled" exception (if it finished anyway, it is done)
	int nResult = IDOK;
	if(!m_sThreadError.empty())
	{
		m_sError = DialogUtils::Utf8ToWide(m_sThreadError);
		nResult = m_bCanceling ? IDCANCEL : IDABORT;
	}
	EndDialog(nResult);
	return 0;
}

LRESULT COSMProgressDlg::OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Cancel button, Esc, the close button: the dialog ends when the thread stops
	if(m_bCanceling)
		return 0;
	m_bCanceling = true;
	m_ptrCancel->Cancel();
	SetDlgItemText(IDC_PROGRESS_STAGE, L"Canceling (the transaction is rolled back)...");
	GetDlgItem(IDCANCEL).EnableWindow(FALSE);
	return 0;
}

#endif
