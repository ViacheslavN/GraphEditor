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


        };

        class IHtmlWindow : public IWindow
        {
        public:
            IHtmlWindow(){}
            virtual ~IHtmlWindow(){}
            virtual void CreateFromHtml(const std::string& html, IWindowPtr ptrParentWindow = IWindowPtr(), uint64_t flags = 0, uint64_t flagsEx = 0) = 0;
            virtual void CreateFromResource(const std::string& resource, IWindowPtr ptrParentWindow = IWindowPtr(), uint64_t flags = 0, uint64_t flagsEx = 0) = 0;
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