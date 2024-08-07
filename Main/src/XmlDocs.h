#ifndef MARKUPDOCUMENTS_H
#define MARKUPDOCUMENTS_H

	class CXmlDocument:public CComPtr<IXMLDOMDocument>{
	protected:
		class CXmlAttribute:public CComPtr<IXMLDOMAttribute>{
		public:
			// ctors
			inline CXmlAttribute(){}
			CXmlAttribute(const CXmlDocument &doc,LPCSTR attrName);

			// cloning with value setting
			CXmlAttribute operator()(LPCSTR format,...) const;
		};

		class CXmlElement:public CComPtr<IXMLDOMElement>{
		public:
			// ctors
			inline CXmlElement(){}
			CXmlElement(const CXmlDocument &doc,LPCSTR elementName);
			CXmlElement(CXmlElement &&element);

			inline void operator=(const CXmlElement &r){ __super::operator=(r); }

			// cloning
			inline CXmlElement operator/(const CXmlElement &element){ return AppendChildElement(element); }
			CXmlElement AppendChildElement(const CXmlElement &element);

			// node modifications
			CXmlElement &operator()(LPCTSTR format,va_list args);
			CXmlElement &operator()(LPCTSTR format,...);
			CXmlElement &operator()(const CXmlElement &wrapper,LPCTSTR text1,...);
			inline CXmlElement &operator()(const CXmlAttribute &attr){ return SetAttribute(attr); }
			CXmlElement &operator()(const CXmlAttribute &attr,LPCTSTR format,...);
			CXmlElement &SetAttribute(const CXmlAttribute &attr);
			CXmlElement &SetAttribute(BSTR attrName,const VARIANT &attrValue);

			// adding extra text subnodes
			inline void operator<<(BSTR text) const{ AppendChildText(text); }
			inline void operator<<(LPCTSTR text) const{ AppendChildText(text); }
			void AppendChildText(BSTR text) const;
			void AppendChildText(LPCSTR text) const;
			void AppendChildTextDot(LPCTSTR text) const;
		};

		CXmlElement root;
	public:
		static CString EncodeXml(LPCTSTR text);

		CXmlDocument(LPCSTR rootName);
		CXmlDocument(const CXmlElement &element);

		TStdWinError Save(LPCTSTR filename) const;
		std::unique_ptr<COleStreamFile> Save() const;
	};

#endif // MARKUPDOCUMENTS_H
