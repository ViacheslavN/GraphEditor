#pragma once
#include "SymbolBase.h"
#include "SymbolsLoader.h"

namespace GraphEngine {
    namespace Display {

        // symbol made of layers drawn one over another (ported from UniGIS MultiLayer*Symbol).
        // UseCache: the features are collected and drawn by FlushBuffers layer by layer,
        // so the first layer of all features is under the second one (road casing)
        template<class I>
        class CMultiLayerSymbolBase : public CSymbolBase<I>, public IMultiLayerSymbol
        {
        public:
            typedef CSymbolBase<I> TSymbolBase;

            CMultiLayerSymbolBase() : m_bUseCache(false)
            {}

            virtual ~CMultiLayerSymbolBase()
            {}

            // IMultiLayerSymbol
            virtual int AddLayer(ISymbolPtr ptrSymbol)
            {
                if(!ptrSymbol.get())
                    throw CommonLib::CExcBase("MultiLayerSymbol: empty layer");
                CheckLayer(ptrSymbol);
                m_vecLayers.push_back(ptrSymbol);
                this->m_bDirty = true;
                return (int)m_vecLayers.size() - 1;
            }

            virtual void DeleteLayer(int nIndex)
            {
                CheckIndex(nIndex);
                m_vecLayers.erase(m_vecLayers.begin() + nIndex);
                this->m_bDirty = true;
            }

            virtual void ClearLayers()
            {
                m_vecLayers.clear();
                ClearCache();
            }

            virtual int GetCount() const
            {
                return (int)m_vecLayers.size();
            }

            virtual ISymbolPtr GetLayer(int nIndex) const
            {
                CheckIndex(nIndex);
                return m_vecLayers[nIndex];
            }

            virtual void MoveLayer(int nIndexFrom, int nIndexTo)
            {
                CheckIndex(nIndexFrom);
                CheckIndex(nIndexTo);
                ISymbolPtr ptrSymbol = m_vecLayers[nIndexFrom];
                m_vecLayers.erase(m_vecLayers.begin() + nIndexFrom);
                m_vecLayers.insert(m_vecLayers.begin() + nIndexTo, ptrSymbol);
            }

            virtual bool GetUseCache() const
            {
                return m_bUseCache;
            }

            virtual void SetUseCache(bool bUse)
            {
                m_bUseCache = bUse;
                ClearCache();
            }

            // ISymbol
            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const
            {
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                {
                    if(m_vecLayers[i]->CanDraw(ptrShape))
                        return true;
                }
                return false;
            }

            virtual void Prepare(IDisplayPtr ptrDisplay)
            {
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->Prepare(ptrDisplay);
            }

            virtual void Reset()
            {
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->Reset();
            }

            virtual void Draw(IDisplayPtr ptrDisplay, CommonLib::IGeoShapePtr ptrShape)
            {
                if(m_bUseCache)
                {
                    TSymbolBase::Draw(ptrDisplay, ptrShape);   // -> DrawGeometryEx, stores the device geometry
                    return;
                }

                // every layer gets the shape itself (a text layer needs it, not only the device points)
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                {
                    if(m_vecLayers[i]->CanDraw(ptrShape))
                        m_vecLayers[i]->Draw(ptrDisplay, ptrShape);
                }
            }

            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount)
            {
                if(m_bUseCache)
                {
                    SCachedGeometry geometry;
                    int nTotal = 0;
                    for(int part = 0; part < polyCount; ++part)
                        nTotal += polyCounts[part];
                    geometry.vecPoints.assign(points, points + nTotal);
                    geometry.vecParts.assign(polyCounts, polyCounts + polyCount);
                    m_vecCache.push_back(geometry);
                    return;
                }

                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->DrawGeometryEx(ptrDisplay, points, polyCounts, polyCount);
            }

            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount)
            {
                DrawGeometryEx(ptrDisplay, lpPoints, lpPolyCounts, nCount);
            }

            // draws the collected geometries layer by layer (UseCache)
            virtual void FlushBuffers(IDisplayPtr ptrDisplay, ITrackCancelPtr ptrTrackCancel)
            {
                if(m_bUseCache && !m_vecCache.empty())
                {
                    for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    {
                        if(ptrTrackCancel.get() && !ptrTrackCancel->Continue())
                            break;

                        m_vecLayers[i]->Prepare(ptrDisplay);
                        for(size_t g = 0; g < m_vecCache.size(); ++g)
                        {
                            const SCachedGeometry& geometry = m_vecCache[g];
                            m_vecLayers[i]->DrawGeometryEx(ptrDisplay, geometry.vecPoints.data(), geometry.vecParts.data(), (int)geometry.vecParts.size());
                        }
                        m_vecLayers[i]->Reset();
                    }
                }
                ClearCache();

                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->FlushBuffers(ptrDisplay, ptrTrackCancel);
            }

            virtual void QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount, GRect &rect) const
            {
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->QueryBoundaryRectEx(ptrDisplay, points, polyCounts, polyCount, rect);
            }

            void SetScaleDependent(bool bFlag)
            {
                TSymbolBase::SetScaleDependent(bFlag);
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->SetScaleDependent(bFlag);
            }

            size_t GetCachedCount() const
            {
                return m_vecCache.size();
            }

            void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                TSymbolBase::Save(pObj);
                pObj->AddPropertyBool("UseCache", m_bUseCache);
                CommonLib::ISerializeObjPtr ptrLayers = pObj->CreateChildNode("Layers");
                for(size_t i = 0; i < m_vecLayers.size(); ++i)
                    m_vecLayers[i]->Save(ptrLayers->CreateChildNode("Layer"));
            }

            void Load(CommonLib::ISerializeObjPtr pObj)
            {
                TSymbolBase::Load(pObj);
                m_bUseCache = pObj->GetPropertyBool("UseCache", m_bUseCache);
                m_vecLayers.clear();
                ClearCache();
                if(pObj->IsChildExists("Layers"))
                {
                    std::vector<CommonLib::ISerializeObjPtr> vecLayers = pObj->GetChild("Layers")->GetChilds("Layer");
                    for(size_t i = 0; i < vecLayers.size(); ++i)
                        AddLayer(CSymbolsLoader::LoadSymbol(vecLayers[i]));
                }
            }

        protected:
            // a layer must have the interface of the multi layer symbol (marker in a marker symbol ...)
            void CheckLayer(ISymbolPtr ptrSymbol) const
            {
                if(!std::dynamic_pointer_cast<I>(ptrSymbol))
                    throw CommonLib::CExcBase("MultiLayerSymbol: wrong type of the layer symbol, id: {0}", ptrSymbol->GetSymbolID());
            }

            void CheckIndex(int nIndex) const
            {
                if(nIndex < 0 || nIndex >= (int)m_vecLayers.size())
                    throw CommonLib::CExcBase("MultiLayerSymbol: layer index out of range: {0}", nIndex);
            }

            void ClearCache()
            {
                m_vecCache.clear();
            }

            template<class T>
            std::shared_ptr<T> LayerAs(size_t nIndex) const
            {
                return std::dynamic_pointer_cast<T>(m_vecLayers[nIndex]);
            }

        protected:
            struct SCachedGeometry
            {
                std::vector<GPoint> vecPoints;
                std::vector<int>    vecParts;
            };

            std::vector<ISymbolPtr>      m_vecLayers;
            std::vector<SCachedGeometry> m_vecCache;
            bool                         m_bUseCache;
        };

        class CMultiLayerMarkerSymbol : public CMultiLayerSymbolBase<IMarkerSymbol>
        {
        public:
            typedef CMultiLayerSymbolBase<IMarkerSymbol> TBase;

            CMultiLayerMarkerSymbol();
            virtual ~CMultiLayerMarkerSymbol();

            // IMarkerSymbol: get - the first layer, set - all layers (size keeps the proportions of the layers)
            virtual double GetAngle() const;
            virtual void   SetAngle(double dAngle);
            virtual Color  GetColor() const;
            virtual void   SetColor(const Color &color);
            virtual double GetSize() const;
            virtual void   SetSize(double dSize);
            virtual double GetXOffset() const;
            virtual void   SetXOffset(double dOffset);
            virtual double GetYOffset() const;
            virtual void   SetYOffset(double dOffset);
            virtual bool   GetIgnoreRotation() const;
            virtual void   SetIgnoreRotation(bool bIgnore);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);
        };

        class CMultiLayerLineSymbol : public CMultiLayerSymbolBase<ILineSymbol>
        {
        public:
            typedef CMultiLayerSymbolBase<ILineSymbol> TBase;

            CMultiLayerLineSymbol();
            virtual ~CMultiLayerLineSymbol();

            // ILineSymbol: get - the first layer, set - all layers (width keeps the proportions of the layers)
            virtual Color  GetColor() const;
            virtual void   SetColor(const Color &color);
            virtual double GetWidth() const;
            virtual void   SetWidth(double dWidth);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);
        };

        class CMultiLayerFillSymbol : public CMultiLayerSymbolBase<IFillSymbol>
        {
        public:
            typedef CMultiLayerSymbolBase<IFillSymbol> TBase;

            CMultiLayerFillSymbol();
            virtual ~CMultiLayerFillSymbol();

            // IFillSymbol: the outline is drawn over all layers
            virtual ILineSymbolPtr GetOutlineSymbol() const;
            virtual void           SetOutlineSymbol(ILineSymbolPtr ptrLine);
            virtual void           FillRect(IDisplayPtr ptrDisplay, const GRect& rect);
            virtual Color          GetColor() const;     // the first layer
            virtual void           SetColor(const Color &color);

            virtual void Prepare(IDisplayPtr ptrDisplay);
            virtual void Reset();
            virtual void DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void FlushBuffers(IDisplayPtr ptrDisplay, ITrackCancelPtr ptrTrackCancel);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            ILineSymbolPtr m_ptrOutline;
        };

    }
}
