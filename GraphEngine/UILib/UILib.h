#pragma once
#include "../CommonLib/CommonLib.h"




namespace GraphEngine {
    namespace UILib {



        enum SplitterType
        {
            VerticalSplitter,
            HorizontalSplitter
        };

        typedef std::shared_ptr<class IWindow> IWindowPtr;
        typedef std::shared_ptr<class IHtmlWindow> IHtmlWindowPtr;

        class IWindow
        {
        public:
            IWindow(){}
            virtual ~IWindow(){}

            virtual int64_t GetNativeHandle() const = 0;
          //  virtual void ShowWindow(bool bShow) = 0;


        };

        class IHtmlWindow : public IWindow
        {
        public:
            IHtmlWindow(){}
            virtual ~IHtmlWindow(){}
            virtual void LoadHtml(const std::wstring& html) = 0;
            virtual void LoadFromResource(const std::wstring& resource) = 0;
        };

        class ISplitterWindows : public IWindow
        {
        public:
            ISplitterWindows(){}
            virtual ~ISplitterWindows(){}

            virtual void Create(SplitterType type,  IWindowPtr ptrParentWindow = IWindowPtr(), uint64_t flags = 0, uint64_t flagsEx = 0) = 0;
            virtual void SetWindows(IWindowPtr left, IWindowPtr right) = 0;
        };



    }
}