// XMLBlob.cpp
#ifndef FORTE_NO_XML

#include "XMLBlob.h"
#include "Exception.h"
#include <wctype.h>

using namespace Forte;

XMLBlob::XMLBlob(const char *rootName
    ) :
    readOnly(false)
{
    doc = xmlNewDoc(reinterpret_cast<xmlChar *>(const_cast<char *>("1.0")));

    // create the root node
    root = xmlNewNode(NULL, reinterpret_cast<xmlChar *>(const_cast<char *>(rootName)));
    xmlDocSetRootElement(doc, root);

    current = root;
    lastData=NULL;
}

XMLBlob::~XMLBlob()
{
    xmlFreeDoc(doc);
}

XMLBlob::XMLBlob(const FString &in) :
    readOnly(true)
{
    // parse the incoming XML string
    throw EUnimplemented("XML parsing not implemented");
}

void XMLBlob::BeginChild(const char *name)
{
    current = xmlNewChild(current, NULL, reinterpret_cast<xmlChar *>(const_cast<char *>(name)), NULL);
}

void XMLBlob::EndChild(void)
{
    if (current != NULL)
        current = current->parent;
}

void XMLBlob::AddAttribute(const char *name, const char *value)
{
    FString stripped;
    stripControls(stripped, value);
    xmlNewProp(current, reinterpret_cast<xmlChar *>(const_cast<char *>(name)), reinterpret_cast<xmlChar *>(const_cast<char *>(stripped.c_str())));
}

void XMLBlob::AddDataAttribute(const char *name, const char *value)
{
    if (lastData)
    {
        FString stripped;
        stripControls(stripped, value);
        xmlNewProp(lastData, reinterpret_cast<xmlChar *>(const_cast<char *>(name)), reinterpret_cast<xmlChar *>(const_cast<char *>(stripped.c_str())));
    }
}

void XMLBlob::AddData(const char *name, const char *value)
{
    FString stripped;
    AddDataToNode(current, name, value);
}
void XMLBlob::AddDataRaw(const char *name, const char *value)
{
    FString stripped;
    AddDataToNodeRaw(current, name, value);
}

void XMLBlob::AddDataToNode(xmlNodePtr node, const char *name, const char *value)
{
    FString stripped;
    stripControls(stripped, value);
    lastData = xmlNewTextChild(current, NULL, reinterpret_cast<xmlChar *>(const_cast<char *>(name)), reinterpret_cast<xmlChar *>(const_cast<char *>(stripped.c_str())));
}
void XMLBlob::AddDataToNodeRaw(xmlNodePtr node, const char *name, const char *value)
{
    FString stripped;
    lastData = xmlNewTextChild(current, NULL, reinterpret_cast<xmlChar *>(const_cast<char *>(name)), reinterpret_cast<xmlChar *>(const_cast<char *>(value)));
}

void XMLBlob::ToString(FString &out,
                       bool pretty
    )
{
    xmlChar *buf;
    int bufsize;
    xmlDocDumpFormatMemoryEnc(doc, &buf, &bufsize, "UTF-8", (pretty) ? 1 : 0);
    out.assign(reinterpret_cast<const char *>(buf), bufsize);
    xmlFree(buf);
}

void XMLBlob::stripControls(FString &dest,
                            const char *src
    )
{
    if (src == NULL)
    {
        dest.assign("");
        return;
    }

    // By default all C programs have the "C" locale set,
    // which is a rather neutral locale with minimal locale information.
    // We need to set the locale to the default system locale which
    // for us is usually "en_US.UTF-8". The will make sure that we
    // can appropiately decode UTF-8 strings.
    setlocale(LC_ALL,"");

    int wc_len = mbstowcs(NULL,src,0);
    if (wc_len == -1)
    {
        throw EXMLBlobStripControlsFailed("mbstowcs call failed");
    }
    wchar_t *wc_src = new wchar_t[wc_len+1];
    wchar_t *wc_dest = new wchar_t[wc_len+1];
    mbstowcs(wc_src, src, wc_len);
    int i = 0;
    int j = 0;
    for (; i < wc_len; i++)
    {
        if (!iswctype(wc_src[i], wctype("cntrl")))
            wc_dest[j++] = wc_src[i];
    }
    wc_dest[j] = 0;

    // convert back to mbs
    int len = wcstombs(NULL,wc_dest,0);
    char *pDest = new char[len+1];

    wcstombs(pDest, wc_dest, len);
    pDest[len] = 0;
    dest = pDest;
    delete [] pDest;
    delete [] wc_dest;
    delete [] wc_src;
}

#endif // FORTE_NO_XML
