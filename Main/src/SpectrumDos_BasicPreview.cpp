#include "stdafx.h"

	CSpectrumDos::CBasicPreview *CSpectrumDos::CBasicPreview::pSingleInstance;

	#define INI_PREVIEW	_T("ZxBasic")

	#define PREVIEW_WIDTH_DEFAULT	750
	#define PREVIEW_HEIGHT_DEFAULT	300

	#define INI_APPLY_COLORS			_T("clr")
	#define INI_SHOW_NONPRINTABLE_CHARS	_T("prn")
	#define INI_SHOW_INTERNAL_BINARY	_T("bin")

	CSpectrumDos::CBasicPreview::CBasicPreview(const CFileManagerView &rFileManager)
		// ctor
		// - base
		: CFilePreview(	&listingView, INI_PREVIEW, rFileManager, PREVIEW_WIDTH_DEFAULT, PREVIEW_HEIGHT_DEFAULT, IDR_SPECTRUM_PREVIEW_BASIC )
		, listingView(_T(""))
		, applyColors( app.GetProfileInt(INI_PREVIEW,INI_APPLY_COLORS,true) )
		, showNonprintableChars( app.GetProfileInt(INI_PREVIEW,INI_SHOW_NONPRINTABLE_CHARS,false) )
		, binaryAfter0x14( (TBinaryAfter0x14)app.GetProfileInt(INI_PREVIEW,INI_SHOW_INTERNAL_BINARY,TBinaryAfter0x14::DONT_SHOW) ) {
		// - initialization
		pSingleInstance=this;
		// - creating the TemporaryFile to store HTML-formatted BASIC Listing
		::GetTempPath(MAX_PATH,tmpFileName);
		::GetTempFileName( tmpFileName, NULL, TRUE, tmpFileName );
		// - creating the ListingView
		listingView.Create( NULL, NULL, WS_CHILD|WS_VISIBLE, rectDefault, this, AFX_IDW_PANE_FIRST, NULL );
		listingView.OnInitialUpdate();
		// - showing the first File
		__showNextFile__();
	}

	CSpectrumDos::CBasicPreview::~CBasicPreview(){
		// dtor
		// - saving the settings
		app.WriteProfileInt(INI_PREVIEW,INI_APPLY_COLORS,applyColors);
		app.WriteProfileInt(INI_PREVIEW,INI_SHOW_NONPRINTABLE_CHARS,showNonprintableChars);
		app.WriteProfileInt(INI_PREVIEW,INI_SHOW_INTERNAL_BINARY,binaryAfter0x14);
		// - uninitialization
		::DeleteFile(tmpFileName);
		pSingleInstance=NULL;
	}







	#define DOS		rFileManager.tab.dos
	#define IMAGE	DOS->image

	#define PREVIEW_LABEL	"BASIC listing"

	void CSpectrumDos::CBasicPreview::__parseBasicFileAndGenerateHtmlFormattedContent__(PCFile file) const{
		// generates HTML-formatted BASIC listing of the input File into a temporary file
		// - creating the File reader
		CFileReaderWriter frw(DOS,pdt->entry);
		// - opening the temporary HTML file for writing
		class CFormattedBasicListingFile sealed:public CFile{
			const CBasicPreview &rBasicPreview;
			struct{
				BYTE ink;	// color index from {0..7}
				BYTE paper;	// color index from {0..7}
				bool flash;
				bool bright;
				bool inverse;
				//bool over; // commented out as not applicable
			} attributes;
			enum TSpecialFormatting{
				NONE,
				NONPRINTABLE_CHARS,
				STD_UDG_SYMBOLS
			} specialFormattingOpen;
		public:
			CFormattedBasicListingFile &operator<<(LPCTSTR t){
				// a shorthand for writing HTML-formatted text into this file
				// . closing any SpecialFormatting currently -Open
				if (specialFormattingOpen!=TSpecialFormatting::NONE){
					Utils::WriteToFile(*this,_T("</span>"));
					specialFormattingOpen=TSpecialFormatting::NONE;
				}
				// . writing the supplied HTML-formatted text
				return (CFormattedBasicListingFile &)Utils::WriteToFile(*this,t);
			}
			CFormattedBasicListingFile &operator<<(int i){
				// a shorthand for writing HTML-formatted integer into this file
				TCHAR buf[16];
				return operator<<(_itot(i,buf,10));
			}

			CFormattedBasicListingFile(const CBasicPreview &_rBasicPreview)
				// ctor
				// . base
				: CFile( _rBasicPreview.tmpFileName, CFile::modeWrite|CFile::modeCreate )
				// . initialization
				, rBasicPreview(_rBasicPreview)
				, specialFormattingOpen(TSpecialFormatting::NONE) {
				__resetColors__();
				// . writing the opening HTML tags
				(CFormattedBasicListingFile &)Utils::WriteToFile(
					*this << _T("<html><body style=\"background-color:#"),
					*(PINT)&Colors[7], _T("%06x")
				) << _T("\">");
			}
			~CFormattedBasicListingFile(){
				// dtor
				// . writing the closing HTML tags
				*this << _T("</body></html>");
			}

			void __startApplicationOfColors__(){
				// writes opening tag that changes the subsequent text display
				if (rBasicPreview.applyColors){
					TCHAR buf[128];
					::wsprintf(	buf,
								_T("<span style=\"color:#%06x;background:#%06x%s\">"),
								Colors[8*attributes.bright+(attributes.inverse?attributes.paper:attributes.ink)],
								Colors[8*attributes.bright+(attributes.inverse?attributes.ink:attributes.paper)],
								attributes.flash ? _T(";text-decoration:blink") : _T("")
							);
					*this << buf; // the "<<" operator automatically closes any previous SpecialFormatting
				}
			}
			void __endApplicationOfColors__(){
				// writes closing tag that changes the subsequent text display to default again
				if (rBasicPreview.applyColors)
					*this << _T("</span>"); // the "<<" operator automatically closes any previous SpecialFormatting
			}

			void __writeNonprintableChar__(BYTE c){
				// describes the (non-printable) Character using printable HTML-formatted text
				if (rBasicPreview.showNonprintableChars){
					// . opening the SpecialFormatting
					if (specialFormattingOpen!=TSpecialFormatting::NONPRINTABLE_CHARS){
						*this << _T("<span style=\"border:1pt solid\">"); // the "<<" operator automatically closes any previous SpecialFormatting
						specialFormattingOpen=TSpecialFormatting::NONPRINTABLE_CHARS;
					// . writing the (non-printable) Character
					}else
						Utils::WriteToFile(*this,',');
					Utils::WriteToFile(*this,c,_T("0x%02X"));
				}
			}

			void __writeStdUdgSymbol__(BYTE s){
				// imitating the standard UDG symbol as two semigraphic characters
				// . opening the SpecialFormatting
				if (specialFormattingOpen!=TSpecialFormatting::STD_UDG_SYMBOLS){
					*this << _T("<span style=\"font-size:50%\">"); // the "<<" operator automatically closes any previous SpecialFormatting
					specialFormattingOpen=TSpecialFormatting::STD_UDG_SYMBOLS;
				}
				// . writing the standard UDG symbol as two semigraphic characters
				for( BYTE i=2,udg=s-128; i-->0; udg<<=1 ){
					Utils::WriteToFile(*this,'&');
					switch (udg&10){
						case 0: // neither upper nor lower half has Ink color
							Utils::WriteToFile(*this,_T("nbsp")); // non-breakable space
							break;
						case 2: // upper half only
							Utils::WriteToFile( Utils::WriteToFile(*this,'#'), 9600 ); // 9600 = suitable Courier-New semigraphic symbol
							break;
						case 8: // lower half only
							Utils::WriteToFile( Utils::WriteToFile(*this,'#'), 9604 ); // 9604 = suitable Courier-New semigraphic symbol
							break;
						case 10: // both halves
							Utils::WriteToFile( Utils::WriteToFile(*this,'#'), 9608 ); // 9608 = suitable Courier-New semigraphic symbol
							break;
					}
					Utils::WriteToFile(*this,';');
				}
			}

			void __resetColors__(){
				// resets Attributes to their defaults
				::ZeroMemory(&attributes,sizeof(attributes)); // black non-flashing, non-bright text ...
				attributes.paper=7; // ... at white background
			}

			bool __changeInk__(BYTE newInk){
				// sets new Ink color index
				if (newInk<=7){
					attributes.ink=newInk;
					return true;
				}else
					return false;
			}
			bool __changePaper__(BYTE newPaper){
				// sets new Ink color index
				if (newPaper<=7){
					attributes.paper=newPaper;
					return true;
				}else
					return false;
			}
			bool __changeFlash__(BYTE newFlash){
				// sets new Ink color index
				if (newFlash<=1){
					attributes.flash=newFlash;
					return true;
				}else
					return false;
			}
			bool __changeBright__(BYTE newBright){
				// sets new Ink color index
				if (newBright<=1){
					attributes.bright=newBright;
					return true;
				}else
					return false;
			}
			bool __changeInverse__(BYTE newInverse){
				// sets new Ink color index
				if (newInverse<=1){
					attributes.inverse=newInverse;
					return true;
				}else
					return false;
			}

			void __parseBasicLine__(PCBYTE pLineStart,WORD nBytesOfLine){
				// parses a BASIC line of given length and writes corresponding HTML-formatted text into this file
				__startApplicationOfColors__();
					bool isLastWrittenCharNbsp=false; // True <=> the last character written to the TemporaryFile is a non-breakable space, otherwise False
					for( const PCBYTE pLineEnd=pLineStart+nBytesOfLine; pLineStart<pLineEnd; ){
						const BYTE b=*pLineStart++;
						switch (b){
							case 13:
								// Enter - breaking the current line
								*this << _T("<br>");
								break;
							case 14:
								// displaying the five-Byte internal form of constant
								if (rBasicPreview.binaryAfter0x14==TBinaryAfter0x14::DONT_SHOW)
									// skipping the five-Byte internal form
									pLineStart+=5;
								else{
									// displaying the five-Byte internal form
									*this << _T("<span style=\"border:1pt dashed\">"); // the "<<" operator automatically closes any previous SpecialFormatting
										switch (rBasicPreview.binaryAfter0x14){
											case TBinaryAfter0x14::SHOW_AS_RAW_BYTES:
												Utils::WriteToFile(*this,*pLineStart++,_T("0x%02X"));
												for( BYTE n=4; n-->0; Utils::WriteToFile(*this,*pLineStart++,_T(",0x%02X")) );
												break;
											case TBinaryAfter0x14::SHOW_AS_NUMBER:
												Utils::WriteToFile( *this, ((TZxRom::PCNumberInternalForm)pLineStart)->ToDouble() );
												pLineStart+=5;
												break;
											default:
												ASSERT(FALSE);
										}
									*this << _T("</span>");
								}
								continue;
							case 16:
								// changing the Ink color
								if (__changeInk__(*pLineStart)){ // successfully changed
applyChangedColorAttributes:		if (pLineStart<pLineEnd){ // color information exists (there's no harm to consuming a non-existing color Byte above)
										__writeNonprintableChar__(b);
										__writeNonprintableChar__(*pLineStart); // any of {Ink,Paper,Flash,Bright,Inverse}
										__endApplicationOfColors__();
										__startApplicationOfColors__();
										pLineStart++; // color information just consumed
									}
									continue;
								}else
									goto defaultPrinting;
							case 17:
								// changing the Paper color
								if (__changePaper__(*pLineStart)) // successfully changed
									goto applyChangedColorAttributes;
								else
									goto defaultPrinting;
							case 18:
								// changing the Flash attribute
								if (__changeFlash__(*pLineStart)) // successfully changed
									goto applyChangedColorAttributes;
								else
									goto defaultPrinting;
							case 19:
								// changing the Bright attribute
								if (__changeBright__(*pLineStart)) // successfully changed
									goto applyChangedColorAttributes;
								else
									goto defaultPrinting;
							case 20:
								// changing the Inverse attribute
								if (__changeInverse__(*pLineStart)) // successfully changed
									goto applyChangedColorAttributes;
								else
									goto defaultPrinting;
							case 21:
								// changing the Over attribute
								if (*pLineStart<=1) // valid value for the Over attribute
									goto applyChangedColorAttributes; // to display the non-printable characters
								else
									goto defaultPrinting;
							case 22:
								// changing the printing position at the screen
								if (pLineStart+1<pLineEnd && *pLineStart<24 && *(pLineStart+1)<32){ // valid [Row,Column] coordinates
writeTwoNonprintableChars:			__writeNonprintableChar__(b);
									__writeNonprintableChar__(*pLineStart++); // X-coordinate information just consumed
									__writeNonprintableChar__(*pLineStart++); // Y-coordinate information just consumed
									continue;
								}else
									goto defaultPrinting; // to display the non-printable characters
							case 23:
								// displaying the Tab character
								if (pLineStart+1<pLineEnd) // a Word value must follow
									goto writeTwoNonprintableChars;
								else
									goto defaultPrinting; // to display the non-printable characters
							case ' ':
								// each explicit space character is output
								*this << _T("&nbsp;"); // non-breakable space
								break;
							case '&':
								// the "ampersand" symbol is not an escape for a hexadecimal literal
								*this << _T("&amp;");
								break;
							case '<':
								// the "less than" symbol is not an opening character of a HTML tag
								*this << _T("&lt;");
								break;
							case '>':
								// the "greater than" symbol is not a closing character of a HTML tag
								*this << _T("&gt;");
								break;
							case 199:
								// the "less than or equal to" symbol is not an opening character of a HTML tag
								*this << _T("&lt;=");
								break;
							case 200:
								// the "greater than or equal to" symbol is not a closing character of a HTML tag
								*this << _T("&gt;=");
								break;
							case 201:
								// the "inequal to" symbol is not an opening character of a HTML tag
								*this << _T("&lt;&gt;");
								break;
							default:
defaultPrinting:				if (b<' ')
									// non-printable character
									__writeNonprintableChar__(b);
								else if (TZxRom::IsStdUdgSymbol(b))
									// standard UDG symbol 
									__writeStdUdgSymbol__(b);
								else if (const LPCSTR keywordTranscript=TZxRom::GetKeywordTranscript(b))
									// writing a Keyword including its start and trail spaces (will be correctly formatted by the HTML parser when displayed)
									*this << ( keywordTranscript + (int)(isLastWrittenCharNbsp&&*keywordTranscript==' ') ); // skipping initial space should it be preceeded by a non-breakable space in the TemporaryFile (as incorrectly two spaces would be displayed in the Listing)
								else{
									// a character that doesn't require a special treatment - just converting between ZX Spectrum and Ascii charsets
									TCHAR buf[16];
									*this << TZxRom::ZxToAscii((LPCSTR)&b,1,buf);
								}
								break;
						}
						isLastWrittenCharNbsp=b==' ';
					}
				__endApplicationOfColors__();
			}

		} listing(*this);
		// - generating the HTML-formatted BASIC Listing
		bool error=false; // assumption (no parsing error of the input File)
		listing << _T("<h2>BASIC Listing</h2>");
		#define HTML_TABLE_BEGIN _T("<table cellpadding=3 cellspacing=0 style=\"font-family:'Courier New'\">")
		listing << HTML_TABLE_BEGIN;
			if (frw.GetPosition()>=frw.GetLength())
				listing << _T("None");
			else
				do{
					TBigEndianWord lineNumber;
					if (error=frw.Read(&lineNumber,sizeof(WORD))!=sizeof(WORD)) // error
						break;
					if (lineNumber>0x39ff){ // invalid LineNumber ...
						frw.Seek( -sizeof(WORD), CFile::current ); // put the LineNumber back
						break; // ... is a correct end of BASIC program
					}
					WORD nBytesOfLine;
					if (error=frw.Read(&nBytesOfLine,sizeof(WORD))!=sizeof(WORD)) // error
						break;
					//if (!nBytesOfLine) // if line has no content ...
						//continue; // ... then simply doing nothing; commented out to write out at least the empty line number
					BYTE lineBytes[65536];
					const WORD nBytesOfLineRead=frw.Read(lineBytes,nBytesOfLine);
					if (nBytesOfLineRead<nBytesOfLine){
						// sometimes, the number of Bytes of line is intentionally reported wrongly (e.g., when part of a copy-protection scheme)
						lineBytes[nBytesOfLineRead]='\r';
						nBytesOfLine =	(PCBYTE)::memchr(lineBytes,'\r',sizeof(lineBytes))
										- // Bytes "after" the '\r' (or 0x0d) Byte are not considered as Bytes of the current line
										lineBytes
										+ // the '\r' (or 0x0d) Byte counts as a Byte of the current line
										1;
						frw.Seek( nBytesOfLine-nBytesOfLineRead, CFile::current ); // rolling back those Bytes that "don't look like Bytes of a line"
					}
					listing << _T("<tr>");
						// | adding a new cell to the "Line Number" column
						listing << _T("<td align=right valign=top style=\"padding-right:5pt\"><b>");
							Utils::WriteToFile(listing,lineNumber);
						//listing << _T("</td>"); // commented out as written in the following command
						// | adding a new cell to the "BASIC Listing" column
						listing << _T("</b></td><td style=\"padding-left:5pt\">");
							listing.__parseBasicLine__( lineBytes, nBytesOfLine-1 ); // "-1" = skipping the terminating Enter character (0x0d)
						//Utils::WriteToFile(fTmp,_T("</td>")); // commented out as written in the following command
					listing << _T("</td></tr>");
				} while (frw.GetPosition()<frw.GetLength());
		listing << _T("</table>");
		listing.__resetColors__();
		// - Error in BASIC Listing
		if (error){
errorInBasic:listing << _T("<p style=\"color:red\">Error in BASIC file structure!</p>");
			return;
		}
		// - generating the HTML-formatted list of BASIC variables
		listing << _T("<h2>Variables (Run-time States)</h2>");
		listing << HTML_TABLE_BEGIN;
			if (frw.GetPosition()>=frw.GetLength())
				listing << _T("None");
			else
				do{
					BYTE variableType;
					if (error=frw.Read(&variableType,sizeof(variableType))!=sizeof(variableType)) // error
						break;
					listing << _T("<tr><td align=right valign=top><b>");
						TCHAR varName[256],*pVarName=varName;
						*pVarName++=(variableType&31)+'@'; // extracting Variable's name first capital letter
						*pVarName='\0'; // terminating the Variable name as most of the names may consist only of just one letter (further chars eventually added below)
						#define HTML_END_OF_NAME_AND_START_OF_VALUE _T("</b></td><td valign=top>=</td><td valign=top>")
						switch (variableType&0xe0){ // masking out the part of the first Byte that determines the Type of the Variable
							case 0x40:{
								// string Variable (always with a single-letter name)
								WORD strLength;
								if (error=frw.Read(&strLength,sizeof(strLength))!=sizeof(strLength)) // error
									break;
								BYTE strBytes[65536];
								if (error=frw.Read(strBytes,strLength)!=strLength) // error
									break;
								(  listing << ::CharUpper(varName) << _T(":string(") << (int)strLength << _T(")") HTML_END_OF_NAME_AND_START_OF_VALUE ).__parseBasicLine__( strBytes, strLength );
								break;
							}
							case 0x80:{
								// number array Variable (always with a single-letter name)
								WORD nBytesInTotal;
								if (error=frw.Read(&nBytesInTotal,sizeof(nBytesInTotal))!=sizeof(nBytesInTotal)) // error
									break;
								BYTE nDimensions;
								if (error=frw.Read(&nDimensions,sizeof(nDimensions))!=sizeof(nDimensions)) // error
									break;
								if (error=!nDimensions)
									break;
								int nItems=1;
								WORD dimensionSizes[(BYTE)-1];
								for( WORD n=0,tmp; n<nDimensions; nItems*=(dimensionSizes[n++]=tmp) )
									if (error=frw.Read(&tmp,sizeof(tmp))!=sizeof(tmp)) // error
										break;
								if (error|=!nItems || nItems*sizeof(TZxRom::TNumberInternalForm)>frw.GetLength()-frw.GetPosition())
									break;
								listing << ::CharUpper(varName) << _T(":number");
								for( BYTE n=0; n<nDimensions; listing << _T("[") << (int)dimensionSizes[n++] << _T("]") );
								listing << HTML_END_OF_NAME_AND_START_OF_VALUE << _T(" { ");
									for( TZxRom::TNumberInternalForm number; nItems-->0; ){
										frw.Read(&number,sizeof(number));
										Utils::WriteToFile( listing, number.ToDouble() );
										listing << _T(", ");
									}
									listing.Seek(-2,CFile::current); // dismissing the ", " string added after the last Item in the array
								listing << _T(" }");
								break;
							}
							case 0xA0:{
								// number Variable with a multi-letter name
								char c=0;
								do{
									if (error=frw.Read(&c,sizeof(c))!=sizeof(c)) // error
										break;
									*pVarName++=c&127;
								}while (c>0);
								if (error=c>=0) // if the last letter in Variable's name doesn't have the highest bit set ...
									break; // ... then it's an error
								*pVarName='\0';
								//fallthrough
							}
							case 0x60:{
								// number Variable with a single-letter name
								TZxRom::TNumberInternalForm number;
								if (error=frw.Read(&number,sizeof(number))!=sizeof(number)) // error
									break;
								Utils::WriteToFile(	listing << ::CharUpper(varName) << _T(":number") << HTML_END_OF_NAME_AND_START_OF_VALUE,
														number.ToDouble()
													);
								break;
							}
							case 0xC0:{
								// character array Variable (always with a single-letter name)
								WORD nBytesInTotal;
								if (error=frw.Read(&nBytesInTotal,sizeof(nBytesInTotal))!=sizeof(nBytesInTotal)) // error
									break;
								BYTE nDimensions;
								if (error=frw.Read(&nDimensions,sizeof(nDimensions))!=sizeof(nDimensions)) // error
									break;
								if (error=!nDimensions)
									break;
								int nItems=1;
								WORD dimensionSizes[(BYTE)-1];
								for( WORD n=0,tmp; n<nDimensions; nItems*=(dimensionSizes[n++]=tmp) )
									if (error=frw.Read(&tmp,sizeof(tmp))!=sizeof(tmp)) // error
										break;
								if (error|=!nItems || nItems*sizeof(BYTE)>frw.GetLength()-frw.GetPosition())
									break;
								listing << ::CharUpper(varName) << _T(":char");
								for( BYTE n=0; n<nDimensions; listing << _T("[") << (int)dimensionSizes[n++] << _T("]") );
								listing << HTML_END_OF_NAME_AND_START_OF_VALUE << _T(" { ");
									for( BYTE byte; nItems-->0; ){
										frw.Read(&byte,sizeof(byte));
										listing << _T("'");
											listing.__parseBasicLine__(&byte,1);
										listing << _T("', ");
									}
									listing.Seek(-2,CFile::current); // dismissing the ", " string added after the last Item in the array
								listing << _T(" }");
								break;
							}
							case 0xE0:{
								// FOR-cycle control Variable (always with a single-letter name)
								TZxRom::TNumberInternalForm from,to,step;
								if (error=frw.Read(&from,sizeof(from))!=sizeof(from)) // error
									break;
								if (error=frw.Read(&to,sizeof(to))!=sizeof(to)) // error
									break;
								if (error=frw.Read(&step,sizeof(step))!=sizeof(step)) // error
									break;
								BYTE tmp[sizeof(WORD)+sizeof(BYTE)]; // Word = line number to jump to after the Next clause, Byte = command number in the line indicated by the Word
								if (error=frw.Read(tmp,sizeof(tmp))!=sizeof(tmp)) // error
									break;
								listing << ::CharUpper(varName) << _T(":iterator") << HTML_END_OF_NAME_AND_START_OF_VALUE << _T("for( ");
								Utils::WriteToFile(	listing << varName << _T(":number="),
														from.ToDouble()
													);
								const double d=step.ToDouble();
								Utils::WriteToFile(	listing << _T("; ") << varName << (d>=0?_T("<="):_T(">=")),
														to.ToDouble()
													);
								Utils::WriteToFile(	listing << _T("; ") << varName << _T("+="),
														d
													);
								listing << _T(" )");
								break;
							}
							default:
								error=true;
								break;
						}
					listing << _T("</td></tr>");
				} while (frw.GetPosition()<frw.GetLength());
		listing << _T("</table>");
		// - Error in variables
		if (error)
			goto errorInBasic;
	}

	void CSpectrumDos::CBasicPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			// . describing the BASIC file using HTML-formatted text
			__parseBasicFileAndGenerateHtmlFormattedContent__(file);
			// . opening the HTML-formatted content
			listingView.Navigate2(tmpFileName);
			// . updating the window caption
			TCHAR buf[MAX_PATH],bufCaption[20+MAX_PATH];
			::wsprintf(	bufCaption,
						PREVIEW_LABEL " (%s)",
						TZxRom::ZxToAscii( DOS->GetFileNameWithAppendedExt(file,buf),-1, bufCaption+20 )
					);
			SetWindowText(bufCaption);
		}else
			SetWindowText(PREVIEW_LABEL);
		SetWindowPos( NULL, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED );
	}

	BOOL CSpectrumDos::CBasicPreview::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_COLOR:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(applyColors);
						return TRUE;
					case ID_PRINT:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(showNonprintableChars);
						return TRUE;
					case ID_ZX_BASIC_BINARY_DONTSHOW:
					case ID_ZX_BASIC_BINARY_SHOWRAW:
					case ID_ZX_BASIC_BINARY_SHOWNUMBER:
						((CCmdUI *)pExtra)->Enable(TRUE);
						((CCmdUI *)pExtra)->SetCheck(nID==binaryAfter0x14+ID_ZX_BASIC_BINARY_DONTSHOW);
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_COLOR:
						applyColors=!applyColors;
						RefreshPreview();
						return TRUE;
					case ID_PRINT:
						showNonprintableChars=!showNonprintableChars;
						RefreshPreview();
						return TRUE;
					case ID_ZX_BASIC_BINARY_DONTSHOW:
					case ID_ZX_BASIC_BINARY_SHOWRAW:
					case ID_ZX_BASIC_BINARY_SHOWNUMBER:
						binaryAfter0x14=(TBinaryAfter0x14)(nID-ID_ZX_BASIC_BINARY_DONTSHOW);
						RefreshPreview();
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}
