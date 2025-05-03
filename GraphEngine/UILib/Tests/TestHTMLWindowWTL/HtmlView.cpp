// TestGraphicsView.cpp : implementation of the CMapView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "HtmlView.h"

CHtmlView::CHtmlView()
{

}

CHtmlView::~CHtmlView()
{

}

HINSTANCE CHtmlView::get_resource_instance()
{
    return _Module.m_hInst;
}