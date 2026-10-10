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
			case OSMStageReadMap:       return L"Reading the map";
			case OSMStageNodes:         return L"Reading nodes and relations";
			case OSMStageSortNodes:     return L"Indexing node coordinates";
			case OSMStageWays:          return L"Reading ways";
			case OSMStageMultipolygons: return L"Building multipolygons";
			default:                    return L"Saving tables";
		}
	}

	double Fraction(uint64_t nValue, uint64_t nTotal)
	{
		return nTotal ? (std::min)(1., (double)nValue / (double)nTotal) : 0.;
	}
}

COSMProgressDlg::COSMProgressDlg(const std::wstring& sCaption, TTask task, uint64_t nTotalNodes, uint64_t nTotalWays, uint64_t nTotalRelations) :
	m_sCaption(sCaption), m_task(task), m_nTotalNodes(nTotalNodes), m_nTotalWays(nTotalWays), m_nTotalRelations(nTotalRelations),
	m_ptrCancel(std::make_shared<Cartography::CTrackCancel>()), m_bCanceling(false), m_bMarquee(false), m_dElapsed(0.)
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

	m_start = std::chrono::steady_clock::now();
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

int COSMProgressDlg::Percent(const SOSMProgress& progress) const
{
	// stages of the conversion: nodes + relations 45%, node index 10%, ways 40%, multipolygons and saving 5%
	double dDone = -1.;
	switch(progress.stage)
	{
		case OSMStageReadMap:
			return -1;
		case OSMStageNodes:
			dDone = 0.45 * Fraction(progress.nNodes + progress.nRelations, m_nTotalNodes + m_nTotalRelations);
			break;
		case OSMStageSortNodes:
			dDone = 0.45 + 0.10 * Fraction(progress.nNodes, m_nTotalNodes);
			break;
		case OSMStageWays:
			dDone = 0.55 + 0.40 * Fraction(progress.nWays, m_nTotalWays);
			break;
		case OSMStageMultipolygons:
			dDone = 0.96;
			break;
		default:
			dDone = 0.98;
			break;
	}
	if(m_nTotalNodes + m_nTotalWays + m_nTotalRelations == 0)
		return -1;
	return (int)(dDone * ProgressRange);
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

	int nPercent = Percent(progress);
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

	std::wstring sCounts = L"Nodes: " + FormatCount(progress.nNodes);
	if(m_nTotalNodes)
		sCounts += L" / " + FormatCount(m_nTotalNodes);
	sCounts += L"    Ways: " + FormatCount(progress.nWays);
	if(m_nTotalWays)
		sCounts += L" / " + FormatCount(m_nTotalWays);
	sCounts += L"    Relations: " + FormatCount(progress.nRelations);
	if(m_nTotalRelations)
		sCounts += L" / " + FormatCount(m_nTotalRelations);
	if(progress.stage != OSMStageReadMap)
		sCounts += L"\nWritten features / records: " + FormatCount(progress.nFeatures);
	SetDlgItemText(IDC_PROGRESS_COUNTS, sCounts.c_str());
}

void COSMProgressDlg::ShowElapsed()
{
	m_dElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
	int nSeconds = (int)m_dElapsed;
	wchar_t szText[64];
	swprintf(szText, 64, L"Elapsed: %02d:%02d:%02d", nSeconds / 3600, (nSeconds / 60) % 60, nSeconds % 60);
	SetDlgItemText(IDC_PROGRESS_TIME, szText);
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
		ShowProgress(m_ptrUpdater->Take());
	return 0;
}

LRESULT COSMProgressDlg::OnTaskDone(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if(m_thread.joinable())
		m_thread.join();
	KillTimer(TimeTimerId);
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
