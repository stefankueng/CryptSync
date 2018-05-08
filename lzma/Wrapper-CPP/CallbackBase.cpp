// This file is based on the following file from the LZMA SDK (http://www.7-zip.org/sdk.html):
//   ./CPP/7zip/UI/Client7z/Client7z.cpp
#include "StdAfx.h"
#include "CallbackBase.h"

namespace SevenZip
{
CallbackBase::CallbackBase()
    : m_callback(nullptr)
    , m_progress(0)
    , m_total(0)
{
}

CallbackBase::~CallbackBase()
{
}
}
