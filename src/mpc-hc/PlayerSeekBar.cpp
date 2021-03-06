/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "mplayerc.h"
#include "PlayerSeekBar.h"
#include "MainFrm.h"
#include "DSMPropertyBag.h"


// CPlayerSeekBar

IMPLEMENT_DYNAMIC(CPlayerSeekBar, CDialogBar)

CPlayerSeekBar::CPlayerSeekBar()
    : m_start(0)
    , m_stop(100)
    , m_pos(0)
    , m_posreal(0)
    , m_fEnabled(false)
    , m_tooltipPos(0)
    , m_tooltipState(TOOLTIP_HIDDEN)
    , m_tooltipLastPos(-1)
    , m_tooltipTimer(1)
    , m_pChapterBag(NULL)
{
}

CPlayerSeekBar::~CPlayerSeekBar()
{
}

BOOL CPlayerSeekBar::Create(CWnd* pParentWnd)
{
    if (!CDialogBar::Create(pParentWnd, IDD_PLAYERSEEKBAR, WS_CHILD | WS_VISIBLE | CBRS_ALIGN_BOTTOM, IDD_PLAYERSEEKBAR)) {
        return FALSE;
    }

    // Should never be RTLed
    ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    m_tooltip.Create(this, TTS_NOPREFIX | TTS_ALWAYSTIP);

    m_tooltip.SetMaxTipWidth(SHRT_MAX);
    // SetDelayTime seems to be ignored but we set it anyway for safety.
    m_tooltip.SetDelayTime(TTDT_AUTOPOP, SHRT_MAX);
    m_tooltip.SetDelayTime(TTDT_INITIAL, 0);
    m_tooltip.SetDelayTime(TTDT_RESHOW, 0);

    memset(&m_ti, 0, sizeof(TOOLINFO));
    m_ti.cbSize = sizeof(TOOLINFO);
    m_ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
    m_ti.hwnd = m_hWnd;
    m_ti.hinst = AfxGetInstanceHandle();
    m_ti.lpszText = NULL;
    m_ti.uId = (UINT_PTR)m_hWnd;

    m_tooltip.SendMessage(TTM_ADDTOOL, 0, (LPARAM)&m_ti);

    return TRUE;
}

BOOL CPlayerSeekBar::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CDialogBar::PreCreateWindow(cs)) {
        return FALSE;
    }

    m_dwStyle &= ~CBRS_BORDER_TOP;
    m_dwStyle &= ~CBRS_BORDER_BOTTOM;
    m_dwStyle |= CBRS_SIZE_FIXED;

    return TRUE;
}

void CPlayerSeekBar::Enable(bool fEnable)
{
    m_fEnabled = fEnable;
    Invalidate();
}

void CPlayerSeekBar::GetRange(__int64& start, __int64& stop) const
{
    start = m_start;
    stop = m_stop;
}

void CPlayerSeekBar::SetRange(__int64 start, __int64 stop)
{
    if (start > stop) {
        start ^= stop, stop ^= start, start ^= stop;
    }
    m_start = start;
    m_stop = stop;
    if (m_pos < m_start || m_pos >= m_stop) {
        SetPos(m_start);
    }
}

__int64 CPlayerSeekBar::GetPos() const
{
    return m_pos;
}

__int64 CPlayerSeekBar::GetPosReal() const
{
    return m_posreal;
}

void CPlayerSeekBar::SetPos(__int64 pos)
{
    CWnd* w = GetCapture();
    if (w && w->m_hWnd == m_hWnd) {
        return;
    }

    SetPosInternal(pos);
}

void CPlayerSeekBar::SetPosInternal(__int64 pos)
{
    if (m_pos == pos) {
        return;
    }

    CRect before = GetThumbRect();
    m_pos = min(max(pos, m_start), m_stop);
    m_posreal = pos;
    CRect after = GetThumbRect();

    if (before != after) {
        InvalidateRect(before | after);

        CMainFrame* pFrame = ((CMainFrame*)GetParentFrame());
        if (pFrame && (AfxGetAppSettings().fUseWin7TaskBar && pFrame->m_pTaskbarList)) {
            pFrame->m_pTaskbarList->SetProgressValue(pFrame->m_hWnd, pos, m_stop);
        }
    }
}

CRect CPlayerSeekBar::GetChannelRect() const
{
    CRect r;
    GetClientRect(&r);
    r.DeflateRect(8, 9, 9, 0);
    r.bottom = r.top + 5;
    return r;
}

CRect CPlayerSeekBar::GetThumbRect() const
{
    //  bool fEnabled = m_fEnabled || m_start >= m_stop;

    CRect r = GetChannelRect();

    int x = r.left + (int)((m_start < m_stop /*&& fEnabled*/) ? (__int64)r.Width() * (m_pos - m_start) / (m_stop - m_start) : 0);
    int y = r.CenterPoint().y;

    r.SetRect(x, y, x, y);
    r.InflateRect(6, 7, 7, 8);

    return r;
}

CRect CPlayerSeekBar::GetInnerThumbRect() const
{
    CRect r = GetThumbRect();

    bool fEnabled = m_fEnabled && m_start < m_stop;
    r.DeflateRect(3, fEnabled ? 5 : 4, 3, fEnabled ? 5 : 4);

    return r;
}

__int64 CPlayerSeekBar::CalculatePosition(REFERENCE_TIME rt)
{
    if (rt >= m_start && rt < m_stop) {
        return (__int64)(GetChannelRect().Width() * ((double)(rt) / m_stop) + 1);
    } else {
        return -1;
    }
}

__int64 CPlayerSeekBar::CalculatePosition(CPoint point)
{
    __int64 pos = -1;
    CRect r = GetChannelRect();

    if (r.left >= r.right) {
        pos = -1;
    } else if (point.x < r.left) {
        pos = m_start;
    } else if (point.x >= r.right) {
        pos = m_stop;
    } else {
        __int64 w = r.right - r.left;
        if (m_start < m_stop) {
            pos = m_start + ((m_stop - m_start) * (point.x - r.left) + (w / 2)) / w;
        }
    }

    return pos;
}

void CPlayerSeekBar::MoveThumb(CPoint point)
{
    __int64 pos = CalculatePosition(point);

    if (pos >= 0) {
        SetPosInternal(pos);
    }
}

BEGIN_MESSAGE_MAP(CPlayerSeekBar, CDialogBar)
    //{{AFX_MSG_MAP(CPlayerSeekBar)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_ERASEBKGND()
    ON_WM_SETCURSOR()
    ON_WM_TIMER()
    //}}AFX_MSG_MAP
    ON_COMMAND_EX(ID_PLAY_STOP, OnPlayStop)
END_MESSAGE_MAP()


// CPlayerSeekBar message handlers

void CPlayerSeekBar::OnPaint()
{
    CPaintDC dc(this); // device context for painting

    bool fEnabled = m_fEnabled && m_start < m_stop;

    COLORREF
    dark   = GetSysColor(COLOR_GRAYTEXT),
    white  = GetSysColor(COLOR_WINDOW),
    shadow = GetSysColor(COLOR_3DSHADOW),
    light  = GetSysColor(COLOR_3DHILIGHT),
    bkg    = GetSysColor(COLOR_BTNFACE);

    // thumb
    {
        CRect r = GetThumbRect(), r2 = GetInnerThumbRect();
        CRect rt = r, rit = r2;

        dc.Draw3dRect(&r, light, 0);
        r.DeflateRect(0, 0, 1, 1);
        dc.Draw3dRect(&r, light, shadow);
        r.DeflateRect(1, 1, 1, 1);

        CBrush b(bkg);

        dc.FrameRect(&r, &b);
        r.DeflateRect(0, 1, 0, 1);
        dc.FrameRect(&r, &b);

        r.DeflateRect(1, 1, 0, 0);
        dc.Draw3dRect(&r, shadow, bkg);

        if (fEnabled) {
            r.DeflateRect(1, 1, 1, 2);
            CPen white(PS_INSIDEFRAME, 1, white);
            CPen* old = dc.SelectObject(&white);
            dc.MoveTo(r.left, r.top);
            dc.LineTo(r.right, r.top);
            dc.MoveTo(r.left, r.bottom);
            dc.LineTo(r.right, r.bottom);
            dc.SelectObject(old);
            dc.SetPixel(r.CenterPoint().x, r.top, 0);
            dc.SetPixel(r.CenterPoint().x, r.bottom, 0);
        }

        dc.SetPixel(r.CenterPoint().x + 5, r.top - 4, bkg);

        {
            CRgn rgn1, rgn2;
            rgn1.CreateRectRgnIndirect(&rt);
            rgn2.CreateRectRgnIndirect(&rit);
            ExtSelectClipRgn(dc, rgn1, RGN_DIFF);
            ExtSelectClipRgn(dc, rgn2, RGN_OR);
        }
    }

    // chapter position

    {
        // Start of critical section
        CAutoLock lock(&m_CBLock);

        if (m_pChapterBag && m_pChapterBag->ChapGetCount() > 1) {
            CRect cr = GetChannelRect();
            REFERENCE_TIME rt;
            CComBSTR name;
            for (DWORD i = 0; i < m_pChapterBag->ChapGetCount(); ++i) {
                if (SUCCEEDED(m_pChapterBag->ChapGet(i, &rt, &name))) {
                    __int64 pos = CalculatePosition(rt);
                    if (pos < 0) {
                        continue;
                    }

                    LONG chanPos = cr.left + (LONG)pos;
                    CRect chan = GetChannelRect();
                    CRect r;
                    if (chanPos >= chan.right) {
                        r = CRect(chanPos - 1, cr.top, chanPos, cr.bottom); // 1 px width
                    } else {
                        r = CRect(chanPos - 1, cr.top, chanPos + 1, cr.bottom); // 2 px width
                    }

                    dc.FillSolidRect(&r, dark);
                    dc.ExcludeClipRect(&r);
                }
            }
        }
    } // End of critical section

    // channel
    {
        CRect r = GetChannelRect();

        dc.FillSolidRect(&r, fEnabled ? white : bkg);
        r.InflateRect(1, 1);
        dc.Draw3dRect(&r, shadow, light);
        dc.ExcludeClipRect(&r);
    }

    // background
    {
        CRect r;
        GetClientRect(&r);
        CBrush b(bkg);
        dc.FillRect(&r, &b);
    }


    // Do not call CDialogBar::OnPaint() for painting messages
}

void CPlayerSeekBar::OnSize(UINT nType, int cx, int cy)
{
    HideToolTip();

    CDialogBar::OnSize(nType, cx, cy);

    Invalidate();
}

void CPlayerSeekBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    CRect r;
    GetClientRect(&r);
    if (m_fEnabled && r.PtInRect(point)) {
        SetCapture();
        MoveThumb(point);
        GetParent()->PostMessage(WM_HSCROLL, MAKEWPARAM((short)m_pos, SB_THUMBPOSITION), (LPARAM)m_hWnd);
    } else {
        CMainFrame* pFrame = ((CMainFrame*)GetParentFrame());
        if (!pFrame->m_fFullScreen) {
            MapWindowPoints(pFrame, &point, 1);
            pFrame->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
        }
    }

    CDialogBar::OnLButtonDown(nFlags, point);
}

void CPlayerSeekBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    ReleaseCapture();

    CDialogBar::OnLButtonUp(nFlags, point);
}

void CPlayerSeekBar::UpdateTooltip(CPoint point)
{
    m_tooltipPos = CalculatePosition(point);
    CRect r;
    GetClientRect(&r);

    if (m_fEnabled && m_start < m_stop && r.PtInRect(point)) {
        if (m_tooltipState == TOOLTIP_HIDDEN && m_tooltipPos != m_tooltipLastPos) {
            // Request notification when the mouse leaves.
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT) };
            tme.hwndTrack = m_hWnd;
            tme.dwFlags = TME_LEAVE;
            TrackMouseEvent(&tme);

            m_tooltipState = TOOLTIP_TRIGGERED;
            m_tooltipTimer = SetTimer(m_tooltipTimer, SHOW_DELAY, NULL);
        }
    } else {
        HideToolTip();
    }

    if (m_tooltipState == TOOLTIP_VISIBLE && m_tooltipPos != m_tooltipLastPos) {
        UpdateToolTipText();
        UpdateToolTipPosition(point);
        // Reset the timer
        m_tooltipTimer = SetTimer(m_tooltipTimer, AUTOPOP_DELAY, NULL);
    }
}

void CPlayerSeekBar::OnMouseMove(UINT nFlags, CPoint point)
{
    CWnd* w = GetCapture();
    if (w && w->m_hWnd == m_hWnd && (nFlags & MK_LBUTTON)) {
        MoveThumb(point);
        GetParent()->PostMessage(WM_HSCROLL, MAKEWPARAM((short)m_pos, SB_THUMBTRACK), (LPARAM)m_hWnd);
    }

    if (AfxGetAppSettings().fUseTimeTooltip) {
        UpdateTooltip(point);
    }

    CDialogBar::OnMouseMove(nFlags, point);
}

LRESULT CPlayerSeekBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_MOUSELEAVE) {
        HideToolTip();
    }

    return CWnd::WindowProc(message, wParam, lParam);
}

BOOL CPlayerSeekBar::PreTranslateMessage(MSG* pMsg)
{
    CRect r;
    GetClientRect(&r);
    POINT ptWnd(pMsg->pt);
    this->ScreenToClient(&ptWnd);
    if (m_fEnabled && AfxGetAppSettings().fUseTimeTooltip && m_start < m_stop && r.PtInRect(ptWnd)) {
        m_tooltip.RelayEvent(pMsg);
    }

    return CDialogBar::PreTranslateMessage(pMsg);
}

BOOL CPlayerSeekBar::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

BOOL CPlayerSeekBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (m_fEnabled && m_start < m_stop && m_stop != 100) {
        ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
        return TRUE;
    }

    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

BOOL CPlayerSeekBar::OnPlayStop(UINT nID)
{
    SetPos(0);
    return FALSE;
}

void CPlayerSeekBar::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_tooltipTimer) {
        switch (m_tooltipState) {
            case TOOLTIP_TRIGGERED: {
                CPoint point;

                GetCursorPos(&point);
                ScreenToClient(&point);
                CRect r;
                GetClientRect(&r);

                if (m_fEnabled && m_start < m_stop && r.PtInRect(point)) {
                    m_tooltipTimer = SetTimer(m_tooltipTimer, AUTOPOP_DELAY, NULL);
                    m_tooltipPos = CalculatePosition(point);
                    UpdateToolTipText();
                    m_tooltip.SendMessage(TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_ti);
                    UpdateToolTipPosition(point);
                    m_tooltipState = TOOLTIP_VISIBLE;
                }
            }
            break;
            case TOOLTIP_VISIBLE:
                HideToolTip();
                break;
        }

    }

    CWnd::OnTimer(nIDEvent);
}

void CPlayerSeekBar::HideToolTip()
{
    if (m_tooltipState > TOOLTIP_HIDDEN) {
        KillTimer(m_tooltipTimer);
        m_tooltip.SendMessage(TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_ti);
        m_tooltipState = TOOLTIP_HIDDEN;
    }
}

void CPlayerSeekBar::UpdateToolTipPosition(CPoint& point)
{
    static CSize size;
    static CRect r;
    size = m_tooltip.GetBubbleSize(&m_ti);
    GetWindowRect(r);

    if (AfxGetAppSettings().nTimeTooltipPosition == TIME_TOOLTIP_ABOVE_SEEKBAR) {
        point.x -= size.cx / 2 - 2;
        point.y = GetChannelRect().TopLeft().y - (size.cy + 13);
    } else {
        point.x += 10;
        point.y += 20;
    }
    point.x = max(0, min(point.x, r.Width() - size.cx));
    ClientToScreen(&point);

    m_tooltip.SendMessage(TTM_TRACKPOSITION, 0, MAKELPARAM(point.x, point.y));
    m_tooltipLastPos = m_tooltipPos;
}

void CPlayerSeekBar::UpdateToolTipText()
{
    DVD_HMSF_TIMECODE tcNow = RT2HMS_r(m_tooltipPos);

    CString time;
    if (tcNow.bHours > 0) {
        time.Format(_T("%02d:%02d:%02d"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds);
    } else {
        time.Format(_T("%02d:%02d"), tcNow.bMinutes, tcNow.bSeconds);
    }

    CComBSTR chapterName;
    {
        // Start of critical section
        CAutoLock lock(&m_CBLock);

        if (m_pChapterBag) {
            REFERENCE_TIME rt = m_tooltipPos;
            m_pChapterBag->ChapLookup(&rt, &chapterName);
        }
    } // End of critical section

    if (chapterName.Length() == 0) {
        m_tooltipText = time;
    } else {
        m_tooltipText.Format(_T("%s - %s"), time, chapterName);
    }

    m_ti.lpszText = (LPTSTR)(LPCTSTR)m_tooltipText;
    m_tooltip.SendMessage(TTM_SETTOOLINFO, 0, (LPARAM)&m_ti);
}

void CPlayerSeekBar::SetChapterBag(CComPtr<IDSMChapterBag>& pCB)
{
    if (!pCB) {
        RemoveChapters();
    }

    {
        // Start of critical section
        CAutoLock lock(&m_CBLock);

        RemoveChapters();
        pCB.CopyTo(&m_pChapterBag);
    } // End of critical section
}

void CPlayerSeekBar::RemoveChapters()
{
    {
        // Start of critical section
        CAutoLock lock(&m_CBLock);

        m_pChapterBag.Release();
    } // End of critical section
}
