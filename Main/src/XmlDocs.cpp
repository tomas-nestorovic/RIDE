#include "stdafx.h"
#include "XmlDocs.h"

	CXmlDocument::CXmlAttribute::CXmlAttribute(const CXmlDocument &doc,LPCSTR attrName){
		// ctor
		doc->createAttribute( COleVariant(attrName).bstrVal, &p );
		p->AddRef();
	}

	CXmlDocument::CXmlDocument(const CXmlElement &element){
		// ctor
		element->get_ownerDocument( (IXMLDOMDocument **)&p );
	}

	CXmlDocument::CXmlAttribute CXmlDocument::CXmlAttribute::operator()(LPCSTR format,...) const{
		// returns a clone without any modifications
		va_list argList;
		va_start( argList, format );
			CString value;
			value.FormatV( format, argList );
		va_end(argList);
		CXmlAttribute copy;
		p->cloneNode( VARIANT_TRUE, (IXMLDOMNode **)&copy.p );
		copy.p->AddRef();
		copy->put_text( COleVariant(value).bstrVal );
		return copy;
	}










	const CXmlDocument::CXmlElement CXmlDocument::CXmlElement::InvalidXmlElement;

	CXmlDocument::CXmlElement::CXmlElement(const CXmlDocument &doc,LPCSTR elementName){
		// ctor
		doc->createElement( COleVariant(elementName).bstrVal, &p );
	}

	CXmlDocument::CXmlElement::CXmlElement(CXmlElement &&element){
		// move ctor
		p=element.p;
		element.p=nullptr;
	}



	CXmlDocument::CXmlElement CXmlDocument::CXmlElement::AppendChildElement(const CXmlElement &element){
		//
		CComPtr<IXMLDOMNode> parent;
		if (FAILED(element->get_parentNode(&parent.p)))
			return InvalidXmlElement;
		CXmlElement result;
		if (parent) // already has a Parent?
			element->cloneNode( VARIANT_FALSE, (IXMLDOMNode **)&result.p ); // must create a copy
		else
			result=element;
		p->appendChild( result, (IXMLDOMNode **)&result.p );
		return result;
	}



	CXmlDocument::CXmlElement &CXmlDocument::CXmlElement::operator()(LPCTSTR format,va_list args){
		// appends formatted text
		CString tmp;
			tmp.FormatV( format, args );
		AppendChildText(tmp);
		return *this;
	}

	CXmlDocument::CXmlElement &CXmlDocument::CXmlElement::operator()(LPCTSTR format,...){
		// appends formatted text
		va_list argList;
		va_start( argList, format );
			operator()( format, argList );
		va_end(argList);
		return *this;
	}

	CXmlDocument::CXmlElement &CXmlDocument::CXmlElement::operator()(const CXmlElement &wrapper,LPCTSTR text1,...){
		// appends Wrapper nodes, each containing Text{1...N}
		for( LPCTSTR *pText=&text1; *pText; pText++ )
			AppendChildElement(wrapper)(*pText);
		return *this;
	}

	CXmlDocument::CXmlElement &CXmlDocument::CXmlElement::SetAttribute(const CXmlAttribute &attr){
		// sets Attribute
		CComPtr<IXMLDOMAttribute> tmp;
		p->setAttributeNode( attr, &tmp.p );
		return *this;
	}

	CXmlDocument::CXmlElement &CXmlDocument::CXmlElement::SetAttribute(BSTR attrName,const VARIANT &attrValue){
		// sets Attribute with explicit Name and Value
		p->setAttribute( attrName, attrValue );
		return *this;
	}



	void CXmlDocument::CXmlElement::AppendChildText(BSTR text) const{
		//
		CComPtr<IXMLDOMText> tmp;
		CXmlDocument(*this)->createTextNode( text, &tmp.p );
		CComPtr<IXMLDOMNode> node;
		p->appendChild( tmp, &node.p );
	}

	void CXmlDocument::CXmlElement::AppendChildText(LPCSTR text) const{
		//
		const int unicodeLength=::MultiByteToWideChar( CP_UTF8, 0, text,-1, nullptr,0 );
		CString unicode;
		const PWCHAR pw=(PWCHAR)unicode.GetBufferSetLength( sizeof(WCHAR)*unicodeLength );
		::MultiByteToWideChar( CP_UTF8, 0, text,-1, pw,unicodeLength );
		return AppendChildText(pw);
	}

	void CXmlDocument::CXmlElement::AppendChildTextDot(LPCTSTR text) const{
		//
		AppendChildText( CString(text)+'.' );
	}










	static IXMLDOMDocument *CreateXmlDoc(){
		::OleInitialize(nullptr); // may want to create a XmlDoc from any thread
		CComPtr<IXMLDOMDocument2> tmp;
		return	SUCCEEDED(tmp.CoCreateInstance( CLSID_DOMDocument60 )) //CLSID_DOMDocument60
				? tmp.Detach()
				: nullptr;
	}

	CXmlDocument::CXmlDocument(LPCSTR rootName)
		// ctor
		: CComPtr<IXMLDOMDocument>( CreateXmlDoc() )
		, root( *this, rootName ) {
		p->Release(); // because of 'Detach()' in CreateXmlDoc above
		p->putref_documentElement(root);
	}

	TStdWinError CXmlDocument::Save(LPCTSTR filename) const{
		return	SUCCEEDED( p->save(COleVariant(filename)) )
				? ERROR_SUCCESS
				: ::GetLastError();
	}

	std::unique_ptr<COleStreamFile> CXmlDocument::Save() const{
		std::unique_ptr<COleStreamFile> result(new COleStreamFile());
		if (!result->CreateMemoryStream())
			return nullptr;
		COleVariant v;
			v.vt=VT_UNKNOWN;
			( v.punkVal=result->m_lpStream )->AddRef();
		if (FAILED(p->save(v)))
			return nullptr;
		return result;
	}

	CString CXmlDocument::EncodeXml(LPCTSTR text){
		// returns the XML-compliant version of the Text
		static const CString Empty;
		struct TXmlDoc sealed:public CXmlDocument{
			TXmlDoc(LPCTSTR text)
				: CXmlDocument("x") {
				root.AppendChildText( const_cast<PTCHAR>(text) );
			}
		} doc(text);
		const auto stream=doc.Save();
		const auto streamLength=stream->GetLength();
		CString xml;
		stream->SeekToBegin();
		stream->Read( xml.GetBufferSetLength(streamLength), streamLength );
		const int iOpenPlusLength=xml.Find(_T("<x>"))+3;
		if (iOpenPlusLength<3)
			return Empty;
		return CString( (LPCTSTR)xml+iOpenPlusLength, xml.Find(_T("</x>"))-iOpenPlusLength );
	}
