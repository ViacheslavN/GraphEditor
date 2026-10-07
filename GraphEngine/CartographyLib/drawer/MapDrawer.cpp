#include "MapDrawer.h"
#include "../../DisplayLib/Transformation/DisplayTransformation2D.h"
#include "../../DisplayLib/Transformation/DisplayTransformation3D.h"

#include <cmath>

namespace GraphEngine {
    namespace Cartography {

        namespace
        {
            const eDrawPhase MapTaskPhases = (eDrawPhase)((int)DrawPhaseGeography | (int)DrawPhaseSelection);
        }

        CMapDrawer::CMapDrawer(double dpi) :
                m_dDpi(dpi > 0. ? dpi : 96.)
                , m_b3D(false)
                , m_dTilt(55.)
                , m_nWidth(0)
                , m_nHeight(0)
                , m_nFlags(0)
                , m_backgroundColor(255, 255, 255, 255)
                , m_nProgressInterval(500)
                , m_bTimerActive(false)
                , m_bTimerStop(false)
                , m_mapTask(this)
        {
            m_panStart.x = m_panStart.y = 0;
            m_panOffset.x = m_panOffset.y = 0;
            m_timerThread = std::thread(&CMapDrawer::TimerProc, this);
        }

        CMapDrawer::~CMapDrawer()
        {
            StopDraw(true);
            m_drawThread.StopThread();
            {
                std::lock_guard<std::mutex> lock(m_timerMutex);
                m_bTimerStop = true;
            }
            m_timerEvent.notify_all();
            if(m_timerThread.joinable())
                m_timerThread.join();
        }

        void CMapDrawer::SetOnInvalidate(OnInvalidate* pFunck, bool bAdd)
        {
            if(bAdd)
                m_OnInvalidateEvent += pFunck;
            else
                m_OnInvalidateEvent -= pFunck;
        }

        void CMapDrawer::SetOnFinishMapDrawing(OnFinishMapDrawing* pFunck, bool bAdd)
        {
            if(bAdd)
                m_OnFinishMapDrawingEvent += pFunck;
            else
                m_OnFinishMapDrawingEvent -= pFunck;
        }

        void CMapDrawer::SetProgressInterval(uint32_t nMilliseconds)
        {
            std::lock_guard<std::mutex> lock(m_timerMutex);
            m_nProgressInterval = nMilliseconds;
        }

        Display::IDisplayTransformationPtr CMapDrawer::GetTransformation() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrDispTran;
        }

        Display::IDisplayTransformationPtr CMapDrawer::GetCalcTransformation() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrDispCalcTran;
        }

        Display::IGraphicsPtr CMapDrawer::GetMapGraphics() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrMapGraphics;
        }

        Display::IGraphicsPtr CMapDrawer::GetLabelGraphics() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrLabelGraphics;
        }

        Display::IGraphicsPtr CMapDrawer::GetOutGraphics() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrOutGraphics;
        }

        IMapPtr CMapDrawer::GetMap() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrMap;
        }

        void CMapDrawer::SetMap(IMapPtr ptrMap)
        {
            StopDraw(true);

            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_ptrMap = ptrMap;
            Init(true); // new map - new units, spatial reference and full extent
        }

        void CMapDrawer::SetResolution(double dpi)
        {
            if(dpi <= 0.)
                return;

            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_dDpi = dpi;
            if(m_ptrDispTran.get())
            {
                m_ptrDispTran->SetResolution(dpi);
                m_ptrDispCalcTran->SetResolution(dpi);
            }
        }

        double CMapDrawer::GetResolution() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_dDpi;
        }

        void CMapDrawer::SetBackgroundColor(const Display::Color& color)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_backgroundColor = color;
            m_mapTask.SetBackgroundColor(color);
        }

        void CMapDrawer::SetSize(int cx , int cy, bool bDraw)
        {
            if(cx <= 0 || cy <= 0)
                return;

            StopDraw(true);
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                m_nWidth = (uint32_t)cx;
                m_nHeight = (uint32_t)cy;
                Init(false);
            }

            if(bDraw)
                Redraw();
        }

        void CMapDrawer::Init(bool bResetTransformation)
        {
            if(!m_nWidth || !m_nHeight)
                return;

            Display::GRect wndRect(0, 0, (Display::GUnits)m_nWidth , (Display::GUnits)m_nHeight);

            m_ptrMapGraphics = Display::IGraphics::CreateCGraphicsAgg((Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight, false);
            m_ptrLabelGraphics = Display::IGraphics::CreateCGraphicsAgg((Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight, false);
            m_ptrOutGraphics = Display::IGraphics::CreateCGraphicsAgg((Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight, false);
            m_ptrMapGraphics->Erase(m_backgroundColor);
            m_ptrOutGraphics->Erase(m_backgroundColor);
            SetFlag(0);

            if(!m_ptrMap.get())
            {
                m_ptrDispTran.reset();
                m_ptrDispCalcTran.reset();
                m_mapTask.Init(IMapPtr(), Display::IDisplayTransformationPtr(), MapTaskPhases, m_ptrMapGraphics);
                return;
            }

            if(bResetTransformation || !m_ptrDispTran.get())
            {
                Geometry::IEnvelopePtr ptrFullExtent = m_ptrMap->GetFullExtent(m_ptrMap->GetSpatialReference());

                Display::IDisplayTransformationPtr ptrTrans[2];
                for(int i = 0; i < 2; ++i)
                {
                    ptrTrans[i] = CreateTransformation(wndRect);
                    ptrTrans[i]->SetSpatialReference(m_ptrMap->GetSpatialReference());
                    ptrTrans[i]->SetVerticalFlip(m_ptrMap->GetVerticalFlip());
                    ptrTrans[i]->SetHorizontalFlip(m_ptrMap->GetHorizontalFlip());
                    ptrTrans[i]->SetDeviceClipRect(wndRect);
                    if(ptrFullExtent.get())
                        ptrTrans[i]->SetMapVisibleRect(ptrFullExtent->GetBoundingBox());
                }
                m_ptrDispTran = ptrTrans[0];
                m_ptrDispCalcTran = ptrTrans[1];
            }
            else
            {
                m_ptrDispTran->SetDeviceRect(wndRect);
                m_ptrDispTran->SetDeviceClipRect(wndRect);
                m_ptrDispCalcTran->SetDeviceRect(wndRect);
                m_ptrDispCalcTran->SetDeviceClipRect(wndRect);
            }

            m_mapTask.Init(m_ptrMap, m_ptrDispTran, MapTaskPhases, m_ptrMapGraphics);
        }

        void CMapDrawer::Update(Display::IGraphicsPtr ptrGraphics, const Display::GPoint *pPoint, const Display::GRect* pRect)
        {
            if(!ptrGraphics.get())
                return;

            std::lock_guard<std::recursive_mutex> lock(m_mutex);

            Display::GRect outRect(0, 0, (Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight);
            Display::GPoint outPoint(0, 0);
            if(pRect)
                outRect = *pRect;
            if(pPoint)
                outPoint = *pPoint;

            if(!m_ptrOutGraphics.get())
            {
                ptrGraphics->Erase(m_backgroundColor, &outRect);
                return;
            }

            Display::GRect fullRect(0, 0, (Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight);
            if(IsFlag(MapDrawerPanState))
            {
                // show the last picture moved by the pan offset
                ptrGraphics->Erase(m_backgroundColor, &outRect);
                Display::GRect dstRect(m_panOffset.x, m_panOffset.y, m_panOffset.x + (Display::GUnits)m_nWidth, m_panOffset.y + (Display::GUnits)m_nHeight);
                ptrGraphics->Copy(m_ptrOutGraphics, Display::GPoint(0, 0), dstRect, false);
                return;
            }

            if(IsFlag(MapDrawerDrawMap))
            {
                // drawing in progress: show what is already drawn
                m_ptrMapGraphics->Lock();
                m_ptrOutGraphics->Copy(m_ptrMapGraphics, Display::GPoint(0, 0), fullRect, false);
                m_ptrMapGraphics->UnLock();
            }

            ptrGraphics->Copy(m_ptrOutGraphics, outPoint, outRect, false);
        }

        void CMapDrawer::Redraw(Display::IGraphicsPtr ptrGraphics)
        {
            StopDraw(true);

            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!m_ptrMap.get() || !m_ptrDispTran.get())
                    return;

                CopyTrans();
                m_mapTask.SetDraw();

                if(ptrGraphics.get())
                {
                    Display::GRect fullRect(0, 0, (Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight);
                    m_ptrMapGraphics->Copy(ptrGraphics, Display::GPoint(0, 0), fullRect, false);
                    m_ptrOutGraphics->Copy(ptrGraphics, Display::GPoint(0, 0), fullRect, false);
                }

                m_mapTask.SetDrawPhase(MapTaskPhases);
                SetFlag(MapDrawerDrawMap);
            }

            StartTimer();
            m_drawThread.SetTask(&m_mapTask, true);
        }

        bool CMapDrawer::IsDrawing() const
        {
            return IsFlag(MapDrawerDrawMap);
        }

        std::string CMapDrawer::GetLastError() const
        {
            return m_mapTask.GetLastError();
        }

        void CMapDrawer::ZoomIn(const Display::GRect& rect)
        {
            Display::IDisplayTransformationPtr ptrTrans = GetCalcTransformation();
            if(!ptrTrans.get() || rect.Width() <= 0 || rect.Height() <= 0)
                return;

            CommonLib::bbox bb;
            ptrTrans->DeviceToMap(rect, bb);
            bb.type = CommonLib::bbox_type_normal;
            ZoomIn(bb);
        }

        void CMapDrawer::ZoomIn(const CommonLib::bbox& bb)
        {
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!m_ptrDispCalcTran.get())
                    return;

                m_ptrDispCalcTran->SetMapVisibleRect(bb);
            }
            Redraw();
        }

        void CMapDrawer::ZoomToFullExtent()
        {
            IMapPtr ptrMap = GetMap();
            if(!ptrMap.get())
                return;

            Geometry::IEnvelopePtr ptrFullExtent = ptrMap->GetFullExtent(ptrMap->GetSpatialReference());
            if(ptrFullExtent.get())
                ZoomIn(ptrFullExtent->GetBoundingBox());
        }

        void CMapDrawer::SetScale(double scale)
        {
            if(scale <= 0.)
                return;

            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!m_ptrDispCalcTran.get())
                    return;

                m_ptrDispCalcTran->SetMapPos(m_ptrDispCalcTran->GetMapPos(), scale);
            }
            Redraw();
        }

        void CMapDrawer::StartPan(const Display::GPoint& pt)
        {
            StopDraw(true);

            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if(!m_ptrOutGraphics.get())
                return;

            if(!IsFlag(MapDrawerFinishedDrawMap))
            {
                // the drawing was interrupted, move what is already drawn
                m_ptrOutGraphics->Copy(m_ptrMapGraphics, Display::GPoint(0, 0), Display::GRect(0, 0, (Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight), false);
            }

            m_panStart = pt;
            m_panOffset.x = m_panOffset.y = 0;
            AddFlags(MapDrawerPanState, MapDrawerDrawMap);
        }

        void CMapDrawer::MovePan(const Display::GPoint& pt)
        {
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!IsFlag(MapDrawerPanState))
                    return;

                m_panOffset.x = pt.x - m_panStart.x;
                m_panOffset.y = pt.y - m_panStart.y;
            }
            FireInvalidate();
        }

        void CMapDrawer::StopPan(const Display::GPoint& pt)
        {
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!IsFlag(MapDrawerPanState))
                    return;

                m_panOffset.x = pt.x - m_panStart.x;
                m_panOffset.y = pt.y - m_panStart.y;
                AddFlags(MapDrawerFinishedPan, MapDrawerPanState);

                if(m_ptrDispCalcTran.get() && (m_panOffset.x != 0 || m_panOffset.y != 0))
                {
                    // keep the moved picture on screen while the map is redrawn
                    Display::GRect fullRect(0, 0, (Display::GUnits)m_nWidth, (Display::GUnits)m_nHeight);
                    Display::GRect dstRect(m_panOffset.x, m_panOffset.y, m_panOffset.x + (Display::GUnits)m_nWidth, m_panOffset.y + (Display::GUnits)m_nHeight);
                    m_ptrMapGraphics->Erase(m_backgroundColor);
                    m_ptrMapGraphics->Copy(m_ptrOutGraphics, Display::GPoint(0, 0), dstRect, false);
                    m_ptrOutGraphics->Erase(m_backgroundColor);
                    m_ptrOutGraphics->Copy(m_ptrMapGraphics, Display::GPoint(0, 0), fullRect, false);

                    // move the map so that the point under the pan start is under the cursor now
                    // (in the 3D view the shift of the map depends on the distance from the viewer)
                    CommonLib::GisXYPoint mapStart, mapEnd;
                    m_ptrDispCalcTran->DeviceToMap(&m_panStart, &mapStart, 1);
                    m_ptrDispCalcTran->DeviceToMap(&pt, &mapEnd, 1);
                    CommonLib::GisXYPoint mapPos = m_ptrDispCalcTran->GetMapPos();
                    mapPos.x += mapStart.x - mapEnd.x;
                    mapPos.y += mapStart.y - mapEnd.y;
                    m_ptrDispCalcTran->SetMapPos(mapPos, m_ptrDispCalcTran->GetScale());
                }
                m_panOffset.x = m_panOffset.y = 0;
            }

            Redraw();
        }

        void CMapDrawer::StopDraw(bool bWait)
        {
            StopTimer();
            m_mapTask.StopDraw(bWait);
            m_drawThread.StopDraw(false, bWait);

            if(!IsFlag(MapDrawerFinishedDrawMap))
                AddFlags(0, MapDrawerDrawMap);
        }

        void CMapDrawer::OnFinishedDrawMapTask(CMapTask *pTask, bool bCanceled)
        {
            StopTimer();
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!bCanceled)
                {
                    AddFlags(MapDrawerFinishedDrawMap, MapDrawerDrawMap);
                    m_ptrMapGraphics->Lock();
                    m_ptrOutGraphics->Copy(m_ptrMapGraphics, Display::GPoint(0, 0), Display::GRect(0, 0,(Display::GUnits) m_nWidth, (Display::GUnits)m_nHeight), false);
                    m_ptrMapGraphics->UnLock();
                }
            }

            if(!bCanceled)
                FireInvalidate();

            m_OnFinishMapDrawingEvent.fire(bCanceled);
        }

        void CMapDrawer::CopyTrans()
        {
            m_ptrDispTran->SetMapPos(m_ptrDispCalcTran->GetMapPos() , m_ptrDispCalcTran->GetScale());
            m_ptrDispTran->SetRotation(m_ptrDispCalcTran->GetRotation());
            m_ptrDispTran->SetVerticalFlip(m_ptrDispCalcTran->GetVerticalFlip());
            m_ptrDispTran->SetHorizontalFlip(m_ptrDispCalcTran->GetHorizontalFlip());

            Display::CDisplayTransformation3DPtr ptrDst3D = std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(m_ptrDispTran);
            Display::CDisplayTransformation3DPtr ptrSrc3D = std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(m_ptrDispCalcTran);
            if(ptrDst3D.get() && ptrSrc3D.get())
                ptrDst3D->SetTilt(ptrSrc3D->GetTilt());
        }

        Display::IDisplayTransformationPtr CMapDrawer::CreateTransformation(const Display::GRect& wndRect) const
        {
            if(m_b3D)
                return std::make_shared<Display::CDisplayTransformation3D>(m_dDpi, m_ptrMap->GetMapUnits(), wndRect, 1.0, m_dTilt);

            return std::make_shared<Display::CDisplayTransformation2D>(m_dDpi, m_ptrMap->GetMapUnits(), wndRect);
        }

        void CMapDrawer::Set3DMode(bool b3D)
        {
            StopDraw(true);
            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(m_b3D == b3D)
                    return;

                m_b3D = b3D;
                if(!m_ptrMap.get() || !m_ptrDispCalcTran.get() || !m_nWidth || !m_nHeight)
                    return;

                // new transformations of the other type with the same position, scale and rotation
                Display::GRect wndRect(0, 0, (Display::GUnits)m_nWidth , (Display::GUnits)m_nHeight);
                Display::IDisplayTransformationPtr ptrOld = m_ptrDispCalcTran;
                Display::IDisplayTransformationPtr ptrTrans[2];
                for(int i = 0; i < 2; ++i)
                {
                    ptrTrans[i] = CreateTransformation(wndRect);
                    ptrTrans[i]->SetSpatialReference(ptrOld->GetSpatialReference());
                    ptrTrans[i]->SetVerticalFlip(ptrOld->GetVerticalFlip());
                    ptrTrans[i]->SetHorizontalFlip(ptrOld->GetHorizontalFlip());
                    ptrTrans[i]->SetRotation(ptrOld->GetRotation());
                    ptrTrans[i]->SetDeviceClipRect(wndRect);
                    ptrTrans[i]->SetMapPos(ptrOld->GetMapPos(), ptrOld->GetScale());
                }
                m_ptrDispTran = ptrTrans[0];
                m_ptrDispCalcTran = ptrTrans[1];
                m_mapTask.Init(m_ptrMap, m_ptrDispTran, MapTaskPhases, m_ptrMapGraphics);
            }
            Redraw();
        }

        bool CMapDrawer::Is3DMode() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_b3D;
        }

        void CMapDrawer::SetTilt(double degrees)
        {
            if(!(degrees >= 0.))
                degrees = 0.;
            if(degrees > Display::CDisplayTransformation3D::MaxTilt)
                degrees = Display::CDisplayTransformation3D::MaxTilt;

            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(m_dTilt == degrees)
                    return;

                m_dTilt = degrees;
                Display::CDisplayTransformation3DPtr ptr3D = std::dynamic_pointer_cast<Display::CDisplayTransformation3D>(m_ptrDispCalcTran);
                if(!ptr3D.get())
                    return; // used when the 3D mode is on

                ptr3D->SetTilt(degrees);
            }
            Redraw();
        }

        double CMapDrawer::GetTilt() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_dTilt;
        }

        void CMapDrawer::SetRotation(double degrees)
        {
            degrees = std::fmod(degrees, 360.);
            if(degrees < 0.)
                degrees += 360.;

            {
                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if(!m_ptrDispCalcTran.get())
                    return;

                m_ptrDispCalcTran->SetRotation(degrees);
            }
            Redraw();
        }

        double CMapDrawer::GetRotation() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_ptrDispCalcTran.get() ? m_ptrDispCalcTran->GetRotation() : 0.;
        }

        void CMapDrawer::FireInvalidate()
        {
            m_OnInvalidateEvent.fire((const Display::GPoint*)nullptr, (const Display::GRect*)nullptr, false);
        }

        void CMapDrawer::StartTimer()
        {
            {
                std::lock_guard<std::mutex> lock(m_timerMutex);
                m_bTimerActive = true;
            }
            m_timerEvent.notify_all();
        }

        void CMapDrawer::StopTimer()
        {
            std::lock_guard<std::mutex> lock(m_timerMutex);
            m_bTimerActive = false;
        }

        void CMapDrawer::TimerProc()
        {
            std::unique_lock<std::mutex> lock(m_timerMutex);
            while(!m_bTimerStop)
            {
                if(!m_bTimerActive || m_nProgressInterval == 0)
                {
                    m_timerEvent.wait(lock);
                    continue;
                }

                m_timerEvent.wait_for(lock, std::chrono::milliseconds(m_nProgressInterval));
                if(m_bTimerStop || !m_bTimerActive)
                    continue;

                lock.unlock();
                FireInvalidate();
                lock.lock();
            }
        }

        void CMapDrawer::AddFlags(uint32_t add_flag, uint32_t remove_flag)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nFlags |= add_flag;
            m_nFlags &= ~remove_flag;
        }

        void CMapDrawer::SetFlag(uint32_t flag)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_nFlags = flag;
        }

        bool CMapDrawer::IsFlag(uint32_t flag) const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return (m_nFlags & flag) == flag;
        }

    }
}
